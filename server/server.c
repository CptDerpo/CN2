//Zahir Bingen s2436647

#include <stdlib.h>
#include <netinet/in.h>

#include <sys/types.h> //Sockets
#include <sys/socket.h> //sockets
#include <sys/time.h>
#include <stdio.h>

#include "asp.h"
#include "wave.h"
#include "util.h"
#include <string.h>
#include <stdbool.h>

//Randomly reorders the packet_queue to simulate out of order packet transmission
void reorder(struct asp_audio_data_packet* queue)
{
    for (size_t i = 0; i < PACKET_QUEUE_SIZE - 1; ++i)
    {
        size_t j = i + rand() / (RAND_MAX / (PACKET_QUEUE_SIZE - i) + 1);
        struct asp_audio_data_packet temp = queue[j];
        queue[j] = queue[i];
        queue[i] = temp;
    }
}

//Sends a packet to the client with audio info
void send_audio_info(struct asp_socket_info* sock, struct wave_file* wave)
{
    struct asp_audio_info_packet msg;
    memset(&msg, 0, sizeof(msg));
    sock->sequence_count += 1;
    msg.header.seq_number = htobe32(sock->sequence_count);
    msg.header.packet_type = AIN;
    msg.header.request_type = INFO;

    msg.num_chans = wave->fmt->channels;
    msg.sample_rate = wave->fmt->sample_rate;
    msg.total_duration_seconds = (uint32_t)wave_duration(wave);
    msg.total_file_size = wave->data->riff.chunk_size;

    error(asp_send(sock, &msg, sizeof(msg)));     
}

//Compresses the data by dividing the 8+8 bytes of each interleaved sample with 2^8
void compress_16_to_8(uint8_t *data, size_t size, uint8_t channels)
{
    uint8_t *temp = (uint8_t *)malloc(size / 2);
    for (size_t i = 0; i < size / (2 * channels); ++i)
    {
        for (size_t channel = 0; channel < channels; ++channel)
        {
            int16_t sample = (int16_t)((data[i * (2 * channels) + (2 * channel) + 1] << 8) | 
                            data[i * (2 * channels) + (2 * channel)]);
            int8_t sample_s8 = (int8_t)(sample >> 8);
            temp[i * channels + channel] = (uint8_t)(sample_s8 + 128);
        }
    }
    memcpy(data, temp, size / 2);
    free(temp);
}


//Fills queue with audio data packets and sets last packet in case of end of audio to AEOF
void fill_packet_queue(struct asp_socket_info* sock, struct asp_audio_data_packet *queue, 
                        const struct wave_file* wave, size_t quality, size_t data_size, size_t audio_data_size)
{
    static size_t offset = 0;
    for(int i = 0; i < PACKET_QUEUE_SIZE; ++i)
    {
        sock->sequence_count++;
        memset(&(queue[i]), 0, sizeof(queue[i]));
        queue[i].header.seq_number = sock->sequence_count;

        //reduce sample size by 50% (packet can fit 2x more data)
        if(quality == 3 || quality == 4) 
        {
            if(offset + (data_size * 2) > audio_data_size)
            {
                queue[i].header.packet_type = AEOF;
                queue[i].header.quality = quality;
                queue[i].header.data_size = audio_data_size - offset;
                memcpy(queue[i].data, (wave->data->data) + offset, queue[i].header.data_size);
                compress_16_to_8(queue[i].data, queue[i].header.data_size, wave->fmt->channels);
                return;            
            } else {
                queue[i].header.packet_type = DATA;
                queue[i].header.quality = quality;
                queue[i].header.data_size = data_size * 2;
                memcpy(queue[i].data, (wave->data->data) + offset, queue[i].header.data_size);
                compress_16_to_8(queue[i].data, queue[i].header.data_size, wave->fmt->channels);
                offset += queue[i].header.data_size;
            }
        } else {
            if(offset + data_size > audio_data_size)
            {
                queue[i].header.packet_type = AEOF;
                queue[i].header.quality = quality;
                queue[i].header.data_size = audio_data_size - offset;
                memcpy(queue[i].data, (wave->data->data) + offset, audio_data_size - offset);
                return;
            } else {
                queue[i].header.packet_type = DATA;
                queue[i].header.quality = quality;
                queue[i].header.data_size = data_size;
                memcpy(queue[i].data, (wave->data->data) + offset, data_size);
                offset += data_size;
            }
        }
    }
}



int main(int argc, char** argv) {
    //Parse command-line options 
    if (argc != 3 && argc !=6) 
    {
        printf("Usage: ./server <filename> <address> [-s <min_delay> <max_delay>]\n");
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];
    const char* address = argv[2];
    int min_delay = 0;
    int max_delay = 0;
    bool simulation = false;

    if (argc == 6 && strcmp(argv[3], "-s") == 0) 
    {
        simulation = true;
        min_delay = atoi(argv[4]);
        max_delay = atoi(argv[5]);
    }

    struct asp_audio_data_packet packet_queue[PACKET_QUEUE_SIZE];

    //Init network
    struct asp_socket_info asp_socket;
    error(asp_init(&asp_socket));
    error(asp_bind(&asp_socket, address, PORT));
    
    //Open the WAVE file
    struct wave_file wave;
    if (wave_open(&wave, filename) < 0) {
        return EXIT_FAILURE;
    }

    const size_t audio_data_size = wave.data->riff.chunk_size;
    
    //Wait for client to connect
    printf("Waiting for connection...\n");
    error(asp_handle_connection(&asp_socket));
    printf("Client has connected...\n");

    bool end_of_audio = false;
    while(true)
    {
        struct asp_header res; //wait for request from client
        memset(&res, 0, sizeof(res));
        memset(packet_queue, 0, sizeof(packet_queue));
        error(asp_receive(&asp_socket, &res, sizeof(res)));

        if(res.packet_type == REQ && res.request_type == INFO)
        {
            send_audio_info(&asp_socket, &wave);
        } else if (res.packet_type == REQ && res.request_type == DATA) {
            fill_packet_queue(&asp_socket, packet_queue, &wave, res.quality, get_audio_quality_data_size(res.quality), audio_data_size);
            if(simulation)
            {
                reorder(packet_queue);
                random_delay(min_delay, max_delay);
            }
            
            for(int i = 0; i < PACKET_QUEUE_SIZE; ++i) //send burst of packets
            {
                if(packet_queue[i].header.packet_type == AEOF)
                    end_of_audio = true;
                if(packet_queue[i].header.packet_type == DATA || packet_queue[i].header.packet_type == AEOF)
                    error(asp_send(&asp_socket, &(packet_queue[i]), sizeof(packet_queue[i])));
            }
            if(end_of_audio)
                break;
        }
    }
    printf("Finished serving file...\n");

    // Clean up
    wave_close(&wave); 
    asp_disconnect(&asp_socket);

    return EXIT_SUCCESS;
}
