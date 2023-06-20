/* Skeleton code for Computer Networks 2022 @ LIACS */

/*
 * Sample WAV audio file player.
 */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "player.h"
#include "wave.h"

/* Signal handler for stopping playback */
sig_atomic_t stop = 0;
static void sigint_handler(int sig) {
    (void) sig;
    stop = 1;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <infile>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct wave_file wave;
    if (wave_open(&wave, argv[1]) != 0) {
        return EXIT_FAILURE;
    }

    const double duration = wave_duration(&wave);
    const double minutes = floor(duration / 60.);
    const double seconds = round(fmod(duration, 60.));
    const int minutes_digits = (minutes == 0 ? 0 : floor(log10(minutes))) + 1;

    /* Print some info */
    {
        printf("File:        %s\n",     argv[1]);
        printf("Channels:    %hu\n",    wave.fmt->channels);
        printf("Sample rate: %u\n",     wave.fmt->sample_rate);
        printf("Duration:    %.1f s\n", duration);
    }

    struct player player;
    if (player_open(&player, wave.fmt->channels, wave.fmt->sample_rate) != 0) {
        wave_close(&wave);
        return EXIT_FAILURE;
    }

    /* Install signal handler to cancel playback */
    struct sigaction sighandler = {
        .sa_handler = sigint_handler,
        .sa_flags = 0
    };
    sigemptyset(&sighandler.sa_mask);
    sigaction(SIGINT, &sighandler, NULL);

    /* Current position we're reading audio data from */
    uint8_t* cur = wave.data->data;

    /* Total number of bytes played */
    size_t played = 0;
    while (played < wave.data->riff.chunk_size && stop == 0) {
        /* 1 frame is 1 sample in each channel, so for stereo 16-bit this is 4 bytes */
        const size_t frames_to_play = 4096;
        size_t bytes_to_play = frames_to_play * wave.fmt->channels * sizeof(int16_t);

        /* Clamp to max size */
        if ((played + bytes_to_play) > wave.data->riff.chunk_size) {
            bytes_to_play = wave.data->riff.chunk_size - played;
        }

        /* Queue the frames */
        player_queue(&player, cur, bytes_to_play);

        /* Bookkeeping */
        played += bytes_to_play;
        cur += bytes_to_play;

        /* Print progress */
        {
            double percent = ((double)played) / wave.data->riff.chunk_size;
            double passed = duration * percent;
            double m_passed = floor(passed / 60.);
            double s_passed = round(fmod(passed, 60.));

            int printed = printf("\r| %*.f:%02.f / %.f:%.f | %3.f%% ", minutes_digits, m_passed, s_passed,
                                 minutes, seconds, percent * 100.);
            /* Ignore the carriage return */
            printed -= 1;

            /* Get terminal size */
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

            /* 2 characters on both sides for borders */
            int bar_width = (int)w.ws_col - 4 - printed;
            int cols_filled = round(bar_width * percent);

            /* Print the progress bar */
            printf("| ");
            for (int i = 0; i < cols_filled; ++i) {
                printf("#");
            }

            for (int i = 0; i < (bar_width - cols_filled); ++i) {
                printf(" ");
            }

            printf(" |");

            /* Ensure our progress bar gets updated nice and fast */
            fflush(stdout);
        }

        /* 
         * Wait until there's 1/8th second of audio remaining in the buffer,
         * this is effectively our ctrl+c latency
         */
        player_wait_for_queue_remaining(&player, 0.125);
    }

    printf("\n");

    player_close(&player);
    wave_close(&wave);

    return EXIT_SUCCESS;
}
