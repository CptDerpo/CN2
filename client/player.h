/* Skeleton code for Computer Networks 2022 @ LIACS */

#ifndef PLAYER
#define PLAYER

#include <stdint.h>
#include <stddef.h>

#include <SDL.h>

/* Total size, in seconds, of the audio buffer. # of frames is calculated automatically */
#define PLAYER_BUFFER_DURATON 0.5

/* What fraction of the buffer will be consumed each time audio is played, to prevent underruns */
#define PLAYER_BUFFER_CONSUME_RATIO 0.25

struct player {
    SDL_AudioDeviceID dev;
    uint16_t channels;
    uint32_t sample_rate;

    size_t buffer_frame_count;
};

/* Open an SDL-based audio player, with the specified parameters, to play 16-bit audio */
int player_open(struct player* player, uint16_t channels, uint32_t sample_rate);
void player_close(struct player* player);

/*
 * Queue a specified number of bytes to be played.
 * Note that this function copies the data and returns immediately, so any buffer can be reused.
 */
int player_queue(struct player* player, const uint8_t* data, uint32_t bytes);

/*
 * Sleep until the buffer has just the specified duration of audio remaining in it.
 * Note that this is not exact, so some margins should be considered for smooth playback.
 */
void player_wait_for_queue_remaining(struct player* player, double duration);

#endif
