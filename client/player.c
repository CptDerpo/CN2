/* Skeleton code for Computer Networks 2022 @ LIACS */

#include "player.h"

#include <stdio.h>
#include <time.h>
#include <stdbool.h>

int player_open(struct player* player, uint16_t channels, uint32_t sample_rate) {
    player->channels = channels;
    player->sample_rate = sample_rate;

    size_t target_buffer_frame_count = PLAYER_BUFFER_DURATON * player->sample_rate;

    player->buffer_frame_count = 4096; // Minimum of 4096 frames in the buffer
    while (player->buffer_frame_count < target_buffer_frame_count) {
        /* Geometric growth until we have enough */
        player->buffer_frame_count *= 2;
    }

    static bool initialized = false;
    if (!initialized) {
        if (SDL_Init(SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "failed to initialize SDL: %s\n", SDL_GetError());
            return -1;
        }
        
        atexit(SDL_Quit);
        initialized = true;
    }

    SDL_AudioSpec spec = {
        .freq     = player->sample_rate,

        /* Always signed 16-bit integers */
        .format   = AUDIO_S16LSB,

        .channels = player->channels,
        
        /*
         * This is the number of frames SDL consumes internally every time it actually
         * consumes any data. This means it's also SDL_GetQueuedAudioSize's resolution,
         * so by keeping it at a ratio, we can be notified before the buffer is entirely empty.
         */
        .samples  = player->buffer_frame_count * PLAYER_BUFFER_CONSUME_RATIO,
        .callback = NULL
    };
    SDL_AudioSpec have = { 0 };

    player->dev = SDL_OpenAudioDevice(NULL, 0, &spec, &have, 0);

    if (player->dev == 0) {
        fprintf(stderr, "failed to open audio device: %s\n", SDL_GetError());
        return -1;
    }

    /* Unpause, aka start playing */
    SDL_PauseAudioDevice(player->dev, 0);

    return 0;
}

void player_close(struct player* player) {
    /* Stop playbck immediately */
    SDL_ClearQueuedAudio(player->dev);
    SDL_CloseAudioDevice(player->dev);
}

int player_queue(struct player* player, const uint8_t* data, uint32_t bytes) {
    if (SDL_QueueAudio(player->dev, data, bytes) != 0) {
        fprintf(stderr, "failed to queue audio: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}

void player_wait_for_queue_remaining(struct player* player, double duration) {
    /* Target number of frames in the buffer */
    const size_t frames = duration * player->sample_rate;

    /* How many bytes should remain by the time this function returns */
    const size_t target_bytes = frames * player->channels * sizeof(int16_t);

    uint32_t queued;
    while ((queued = SDL_GetQueuedAudioSize(player->dev)) > target_bytes) {
        /* Currently queued frames in the buffer */
        size_t queued_frames = queued / player->channels / sizeof(int16_t);

        /* Early exit and prevent overflow */
        if (queued_frames <= frames) {
            return;
        }

        /* Store as double because we need to cast it directly afterwards anywy */
        double frames_to_sleep = queued_frames - frames;

        /* Calculate in terms of nanoseconds */
        struct timespec ts = {
            .tv_sec  = 0,
            .tv_nsec = (frames_to_sleep / player->sample_rate) * 1e9
        };

        /* Make ts valid */
        while (ts.tv_nsec > 1e9) {
            ts.tv_nsec -= 1e9;
            ts.tv_sec  += 1;
        }

        /* Sleep until (approx.) the desired time */
        (void) clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
}
