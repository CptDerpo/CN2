// Zahir Bingen s2436647

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <sys/types.h> //Sockets
#include <sys/socket.h> //sockets

#include "player.h"
#include "util.h"
#include "asp.h"

#define LOCAL_ADDR "::1"

//Compares the sequence numbers of two packets, to sort them
int compare(const void* A, const void* B)
{
    const struct asp_audio_data_packet* packetA = (const struct asp_audio_data_packet*)A;
    const struct asp_audio_data_packet* packetB = (const struct asp_audio_data_packet*)B;
    if (packetA->header.seq_number == 0 || packetB->header.seq_number == 0)
        return 0;
    return (packetA->header.seq_number) - (packetB->header.seq_number);
}

//Randomly drop a packet in queue
void drop_packets(struct asp_socket_info* sock, struct asp_audio_data_packet* queue, int chance)
{
    int random_num = rand() % 100 + 1;
    if(random_num <= chance)
        {
            sock->packets_missing++;
            int index = rand() % 4;
            if(queue[index].header.packet_type != AEOF)
                memset(&(queue[index].data), 0, queue[index].header.data_size);
        }
}

//Requests a new burst of audio data packets
void request_audio_data(struct asp_socket_info *sock)
{
    struct asp_header req;
    memset(&req, 0, sizeof(req));

    req.packet_type = REQ;
    req.request_type = DATA;
    req.seq_number = htobe32(++sock->sequence_count);
    req.ack_number = htobe32(0);
    req.quality = sock->current_quality_level;

    for(int i = 0; i < 4; ++i)
        req.address[i] = htobe32(sock->local_addr.sin6_addr.__in6_u.__u6_addr32[i]);

    error(asp_send(sock, &req, sizeof(req)));
}

void set_quality_level(struct asp_socket_info *sock)
{
    if(sock->packets_received % PACKETS_CHECKED == 0)
    {
        if(sock->packets_missing == 0)
        {
            sock->current_quality_level = 1;
        } else if (sock->packets_missing == 1) {
            sock->current_quality_level = 2;
        } else if (sock->packets_missing > 1 && sock->packets_missing < 4) {
            sock->current_quality_level = 3;
        } else {
            sock->current_quality_level = 4;
        }
        sock->packets_missing = 0;
    }
}

//Expands 8 bit to 16 bit
void decompress_8_to_16(uint8_t *data, size_t size, size_t channels)
{
    uint8_t *temp = (uint8_t *)malloc(size * 2);
    memset(temp, 0, size * 2);
    for (size_t i = 0; i < size / (2 * channels); ++i)
    {
        for (size_t channel = 0; channel < channels; ++channel)
        {
            uint8_t sample_u8 = data[i * channels + channel];
            int8_t sample_s8 = (int8_t)(sample_u8 - 128);
            int16_t sample_s16 = (int16_t)(sample_s8) << 8;
            temp[i * (2 * channels) + (2 * channel)] = (uint8_t)(sample_s16 & 0xFF);
            temp[i * (2 * channels) + (2 * channel) + 1] = (uint8_t)(sample_s16 >> 8);
        }
    }
    memcpy(data, temp, size * 2);
    free(temp);
}

//Fills the buffer with audio data (~1s)
int fill_buffer(struct asp_socket_info* sock, uint8_t *buf, size_t size, size_t *audio_bytes, uint8_t channels, int chance)
{
    struct asp_audio_data_packet packet;
    struct asp_audio_data_packet packet_queue[PACKET_QUEUE_SIZE];
    size_t data_size = 0, offset = 0;
    bool end_of_audio = false;
    do 
    {
        set_quality_level(sock);
        if(sock->current_quality_level == 3 || sock->current_quality_level == 4)
            if(get_audio_quality_data_size(sock->current_quality_level) * 4 * 2 + offset > size) //compressed contains 2x more data
                break;
        if(get_audio_quality_data_size(sock->current_quality_level) * 4 + offset > size) //break if next burst won't fit in buf
            break;
        request_audio_data(sock);
        for(int i = 0; i < PACKET_QUEUE_SIZE; ++i)  //fill queue with audio packets from server
        {
            memset(&packet, 0, sizeof(packet));
            error(asp_receive(sock, &packet, sizeof(packet)));

            if(packet.header.packet_type == AEOF)   //check if it is the last audio packet
            {
                end_of_audio = true;
                break;
            } else if(packet.header.packet_type != DATA) {
                continue;
            }

            data_size = packet.header.data_size;
            packet_queue[i] = packet;
            sock->packets_received++;
        }
        qsort(packet_queue, PACKET_QUEUE_SIZE, sizeof(struct asp_audio_data_packet), compare);  //fix ordering of packets received
        drop_packets(sock, packet_queue, chance);   //drop a packet with a specific chance

        for(int i = 0; i < PACKET_QUEUE_SIZE; ++i)  //copy packets to buffer
        {
            if(packet_queue[i].header.quality == 3 || packet_queue[i].header.quality == 4)
            {
                decompress_8_to_16(packet_queue[i].data, packet_queue[i].header.data_size, channels);
            }
            memcpy(buf + offset, packet_queue[i].data, packet_queue[i].header.data_size);
            offset += packet_queue[i].header.data_size;
        }
        if(end_of_audio)
            break;
    }while((offset + data_size*4 < size));
    *audio_bytes = offset;
    return end_of_audio;
}

//Requests info about the audio file
void request_audio_info(struct asp_socket_info* sock, uint16_t *sample_rate, uint8_t *channels, 
                            uint32_t *tot_td, uint32_t *tot_fs)
{
    struct asp_header msg;
    memset(&msg, 0, sizeof(msg));
    struct asp_audio_info_packet res;

    msg.seq_number = htobe32(++sock->sequence_count);
    for(size_t i = 0; i < 4; ++i)
        msg.address[i] = htobe32(sock->local_addr.sin6_addr.__in6_u.__u6_addr32[i]);
    msg.packet_type = REQ;
    msg.request_type = INFO;

    error(asp_send(sock, &msg, sizeof(msg)));
    error(asp_receive(sock, &res, sizeof(res)));
        
    *sample_rate = res.sample_rate;
    *channels = res.num_chans;
    *tot_td = res.total_duration_seconds;
    *tot_fs = res.total_file_size;
}

//Main
int main(int argc, char** argv) {
    struct asp_socket_info asp_socket;
    int chance = 0;

    if (argc != 2 && argc !=4) 
    {
        printf("Usage: ./client <buffer_size> [-s <packet_drop_chance>\n");
        return EXIT_FAILURE;
    }

    if (argc == 4 && strcmp(argv[2], "-s") == 0) 
        chance = atoi(argv[3]);
    
    const size_t buffer_size = atoi(argv[1]);
    uint16_t samplerate;
    uint8_t channels;
    uint32_t tot_td;
    uint32_t tot_fs;

    //setup connection
    error(asp_init(&asp_socket));
    error(asp_connect(&asp_socket, LOCAL_ADDR, PORT));

    error(asp_establish_connection(&asp_socket));

    printf("Connection Established\n");

    request_audio_info(&asp_socket, &samplerate, &channels, &tot_td, &tot_fs);

    printf("Sample rate: %d, Channels: %d, Total time duration: %d:%d\n", samplerate, channels, (tot_td/60), (tot_td%60));

    if(buffer_size > (samplerate * BIT_DEPTH * channels)/8)
    {
        printf("Error: buffer_size can not exceed %d bytes\n", (samplerate * BIT_DEPTH * channels)/8);
        return EXIT_FAILURE;
    } else if (buffer_size < (samplerate * BIT_DEPTH * channels)/16){
        printf("Error: buffer_size too small, must be bigger than %d bytes\n", (samplerate * BIT_DEPTH * channels)/16);
        return EXIT_FAILURE;
    }


    struct player player;
    if (player_open(&player, channels, samplerate) != 0) {
        return EXIT_FAILURE;
    }

    //set up buffer/queue
    uint8_t* recvbuffer = malloc(buffer_size);

    bool end_of_audio = false;
    size_t audio_bytes = 0;
    //fills buffer with ~1 sec
    end_of_audio = (fill_buffer(&asp_socket, recvbuffer, buffer_size, &audio_bytes, channels, chance));

    //Play
    printf("playing...\n");
    printf("Total Playtime: %d:%d\n", (tot_td/60), (tot_td%60));
    player_queue(&player, recvbuffer, audio_bytes);

    size_t counter = 0;
    while(!end_of_audio)
    {
        end_of_audio = fill_buffer(&asp_socket, recvbuffer, buffer_size, &audio_bytes, channels, chance);
        player_queue(&player, recvbuffer, audio_bytes);
        printf("Playback Percentage: %.2f%%\n", ((double)(counter)/(double)tot_fs)*100);
        player_wait_for_queue_remaining(&player, 0.2); //sleep for 0.2 seconds gives us enough time to refill buffer
        counter += audio_bytes;
        if(end_of_audio)
        {
            printf("Playback Percentage: %.2f%%\n", 100.0);
            sleep(2);
            break;
        }
    }

    //Cleanup
    free(recvbuffer);
    player_close(&player);
    asp_disconnect(&asp_socket);

    return 0;
}
