/* Skeleton code for Computer Networks 2022 @ LIACS */
#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>
#include <stddef.h>

/* 
 * Some helper functionality for dealing with WAVE PCM files.
 * For more details, see: http://soundfile.sapp.org/doc/WaveFormat/
 */

/* RIFF header used in all chunks */
struct __attribute__((packed)) riff_header {
    char chunk_id[4];
    uint32_t chunk_size;
};

/* First chunk in a file */
struct __attribute__((packed)) wave_chunk {
    struct riff_header riff;
    char wave[4];
};

/* Mandatory second chunk describing audio */
struct __attribute__((packed)) format_chunk {
    struct riff_header riff;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct __attribute__((packed)) data_chunk {
    struct riff_header riff;
    uint8_t data[];
};

/* Describes a WAVE file */
struct wave_file {
    int fd;
    void* map;
    size_t map_size;

    /* fmt chunk to read audio parameters from */
    struct format_chunk* fmt;

    /* data chunk to read audio data from */
    struct data_chunk* data;
};

/* Open a wav file with the specified path */
int wave_open(struct wave_file* wave, const char* filename);
void wave_close(struct wave_file* wave);

/* Get a wav file's duration in seconds */
double wave_duration(struct wave_file* wave);

#endif /* WAVE_H */
