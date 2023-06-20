//Zahir Bingen s2436647

#ifndef ASP_H
#define ASP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <math.h>

//Standard info
#define MAX_PACKET_SIZE 16384
#define PACKET_QUEUE_SIZE 4
#define BIT_DEPTH 16
#define PORT 1235
#define PACKETS_CHECKED 20

//Packet sizes for each quality level
#define QL1 2048
#define QL2 1024
#define QL3 1024
#define QL4 512

//Packet Types
#define DAT 3
#define SYN 7
#define ACK 9
#define REQ 10
#define AIN 11
#define AEOF 12

//Request types
#define INFO 1
#define DATA 2

/*
 * An asp socket descriptor for information about the sockets current state.
 * This is just an example, you can make anything you want out of it
 */
struct asp_socket_info {
    int sockfd;

    struct sockaddr_in6 local_addr;
    socklen_t local_addrlen;

    struct sockaddr_in6 remote_addr;
    socklen_t remote_addrlen;

    int current_quality_level;
    uint32_t sequence_count;

    size_t packets_received;
    size_t packets_missing; //stores the amount of packets dropped per 20 packets received

    unsigned int is_connected : 1;
    unsigned int has_accepted : 1;
};


//Struct resembling our HEADER, as shown in the documentation
struct asp_header {
    uint32_t address[4];
    uint32_t seq_number;
    uint32_t ack_number;
    uint8_t packet_type;
    uint8_t resp;
    uint8_t request_type;
    uint8_t quality;
    uint32_t data_size;
};


//Struct resembling our AUDIO INFO PACKET, as shown in the documentation
struct asp_audio_info_packet {
    struct asp_header header;
    uint16_t sample_rate;
    uint8_t num_chans;
    uint8_t padding;
    uint32_t total_file_size;
    uint32_t total_duration_seconds;
    uint32_t padding2;
};


//Struct resembling our AUDIO DATA PACKET, as shown in the documentation
struct asp_audio_data_packet {
    struct asp_header header;
    uint8_t data[16384]; //MAX PACKET SIZE
};

/**
 * Initializes an ASP socket structure.
 *
 * This function initializes the given `asp_socket_info` structure, setting its fields to initial values and
 * allowing it to be used in other ASP Socket operations.
 *
 * @param socket    A pointer to the `asp_socket_info` structure to be initialized.
 *
 * @return          0 on success, or a negative value if an error occurs.
 **/
int asp_init(struct asp_socket_info* socket);

/**
 * Binds an ASP socket to a local address and port.
 *
 * This function binds the given ASP socket to the specified local address and port, allowing it to receive
 * incoming data on that address.
 *
 * @param socket    A pointer to the `asp_socket_info` structure.
 * @param address   The local IP address to bind the socket to, in string format.
 * @param port      The local port number to bind the socket to.
 *
 * @return          0 on success, or a negative value if an error occurs.
 **/
int asp_bind(struct asp_socket_info* socket, const char* address, uint16_t port);


/**
 * Connects an ASP socket to a remote address and port.
 *
 * This function connects the given ASP socket to the specified remote address and port.
 * It sets the remote address and port in the socket structure and attempts to connect to the remote server.
 *
 * @param socket    A pointer to the `asp_socket_info` structure.
 * @param address   The remote IP address to connect to, in string format.
 * @param port      The remote port number to connect to.
 *
 * @return          0 on success, or a negative value if an error occurs.
 **/
int asp_connect(struct asp_socket_info* socket, const char* address, uint16_t port);


/**
 * Disconnects an ASP socket from the remote server.
 *
 * This function closes the connection associated with the given ASP socket and
 * releases the resources associated with the socket.
 *
 * @param socket    A pointer to the `asp_socket_info` structure.
 *
 * @return          0 on success, or a negative value if an error occurs.
 **/
int asp_disconnect(struct asp_socket_info* socket);


/**
 * 
 * Sends an ASP packet through the specified ASP socket.
 *
 * This function sends the ASP packet through the ASP socket.
 *
 * @param socket   A pointer to the `asp_socket_info` structure.
 * @param packet   A pointer to the `asp_packet` structure containing the packet to be sent.
 *                 Must be casted to the correct type later. (Read the header)
 *
 * @return         Returns 0 on success, or a negative value if an error occurs.
 **/
int asp_send(struct asp_socket_info* sock, const void* packet, size_t size);


/**
 * Receives an ASP packet on the specified ASP socket.
 *
 * This function receives an ASP packet from the ASP socket and stores it in the packet structure.
 *
 * @param socket   A pointer to the any `asp_socket_info` structure.
 * @param packet   A pointer to the `asp_packet` structure to store the received packet.
 *                 Must be casted to the correct type later. (Read the header)
 *
 * @return         Returns 0 on success, or a negative value if an error occurs.
 **/
int asp_receive(struct asp_socket_info* sock, void* packet, size_t size);


/**
 * Handles an ASP connection.
 *
 * This function receives a SYN packet, sends an ACK packet, and receives an ACK packet in response.
 * It handles the connection on the specified ASP socket.
 *
 * @param sock      A pointer to the `asp_socket_info` structure.
 * @return Returns  0 on success, or a negative value if an error occurs.
 **/
int asp_handle_connection(struct asp_socket_info* sock);


/**
 * Establishes an ASP connection.
 *
 * This function sends a SYN packet, receives an ACK packet, and sends an ACK packet in response.
 * It establishes the connection on the specified ASP socket.
 *
 * @param sock      A pointer to the `asp_socket_info` structure.
 * @return Returns  0 on success, or a negative value if an error occurs.
 **/
int asp_establish_connection(struct asp_socket_info* sock);

/**
 * Returns the data size based on the audio quality level.
 *
 * @param quality_level     The audio quality level.
 * 
 * @return                  The data size corresponding to the specified quality level.
 **/
size_t get_audio_quality_data_size(size_t quality_level);

#endif