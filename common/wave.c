#include "wave.h"

#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int wave_open(struct wave_file* wave, const char* filename) {
    /* Open file */
    wave->fd = open(filename, O_RDONLY);
    if (wave->fd < 0) {
        perror("failed to open file");
        return -1;
    }

    /* Retriefe filesize for mmap */
    struct stat statbuf;
    if (fstat(wave->fd, &statbuf) < 0) {
        perror("failed to stat");
        goto err_close;
    }

    wave->map_size = statbuf.st_size;

    wave->map = mmap(NULL, wave->map_size, PROT_READ, MAP_SHARED, wave->fd, 0);
    if (wave->map == MAP_FAILED) {
        perror("mmap failed");
        goto err_close;
    }

    struct wave_chunk* wave_hdr = wave->map;

    /* Validate WAVE RIFF */
    if (memcmp(wave_hdr, "RIFF", 4) != 0) {
        fprintf(stderr, "invalid RIFF header: %.*s\n", 4, wave_hdr->riff.chunk_id);
        goto err_unmap;
    }

    if (memcmp(wave_hdr->wave, "WAVE", 4) != 0) {
        fprintf(stderr, "invalid WAVE header: %.*s\n", 4, wave_hdr->wave);
        goto err_unmap;
    }

    /* Valid WAVE header, check fmt chunk */
    wave->fmt = (void*)(((char*)wave->map) + sizeof(struct wave_chunk));
    if (memcmp(wave->fmt->riff.chunk_id, "fmt ", 4) != 0) {
        fprintf(stderr, "invalid fmt header: %.*s\n", 4, wave->fmt->riff.chunk_id);
        goto err_unmap;
    }

    if (wave->fmt->audio_format != 1) {
        fprintf(stderr, "unsupported audio format: %hu\n", wave->fmt->audio_format);
        goto err_unmap;
    }

    if (wave->fmt->bits_per_sample != 16) {
        fprintf(stderr, "unsupported bits per sample: %hu\n", wave->fmt->bits_per_sample);
        goto err_unmap;
    }

    /* Valid format, search for data chunk */
    char* cur = wave->map;

    /* wave header is always the same size */
    cur += sizeof(struct wave_chunk);

    /* fmt allows for extra data */
    cur += (sizeof(struct riff_header) + wave->fmt->riff.chunk_size);

    while (cur < (((char*)wave->map) + wave->map_size)) {
        struct riff_header* hdr = (void*)cur;

        /* Return when data chunk is found */
        if (memcmp(hdr->chunk_id, "data", 4) == 0) {
            wave->data = (void*)cur;

            return 0;
        }

        /* Keep going */
        cur += sizeof(struct riff_header) + hdr->chunk_size;
    }

    fprintf(stderr, "didn't find data chunk\n");

err_unmap:
    munmap(wave->map, wave->map_size);

err_close:
    close(wave->fd);
    return -1;
}

void wave_close(struct wave_file* wave) {
    munmap(wave->map, wave->map_size);
    close(wave->fd);
}

double wave_duration(struct wave_file* wave) {
    return ((double)wave->data->riff.chunk_size)
                / wave->fmt->channels / wave->fmt->sample_rate / sizeof(int16_t);
}
