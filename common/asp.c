#include "asp.h"
#include "util.h"

int asp_init(struct asp_socket_info *sock)
{
    memset(sock, 0, sizeof(*sock));
    if ((sock->sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Error creating socket");
        return -1;
    } 

    memset(&(sock->local_addr), 0, sizeof(struct sockaddr_in6)); //initialize struct members to all 0, local
    sock->local_addr.sin6_family = AF_INET6;
    sock->local_addrlen = sizeof(struct sockaddr_in6);

    memset(&(sock->remote_addr), 0, sizeof(struct sockaddr_in6)); //...remote
    sock->remote_addr.sin6_family = AF_INET6;
    sock->remote_addrlen = sizeof(struct sockaddr_in6);

    sock->sequence_count = 0;
    sock->current_quality_level = 1;
    return 0;
}


int asp_bind(struct asp_socket_info* sock, const char* address, uint16_t port)
{
    sock->local_addr.sin6_port = htons(port);
    inet_pton(AF_INET6, address, &(sock->local_addr.sin6_addr)); //check return value
    if(bind(sock->sockfd, (struct sockaddr*) &(sock->local_addr), sizeof(struct sockaddr_in6)) < 0)
    {
        perror("Error binding socket");
        return -1;
    } 
    return 0;
}

int asp_send(struct asp_socket_info* sock, const void* packet, size_t size)
{
    if(sendto(sock->sockfd, packet, size, 0, 
        (struct sockaddr *) &(sock->remote_addr), sock->remote_addrlen) < 0)
    {
        perror("Error sending packet");
        return -1;
    }
    return 0;
}

int asp_receive(struct asp_socket_info* sock, void* packet, size_t size)
{
    if(recvfrom(sock->sockfd, packet, size, 0, 
        (struct sockaddr *) &(sock->remote_addr), (socklen_t *) &(sock->remote_addrlen)) < 0)
    {
        return -1;
    }
    return 0;
}

int asp_connect(struct asp_socket_info* sock, const char* address, uint16_t port)
{
    sock->remote_addr.sin6_port = htons(port);
    if (inet_pton(AF_INET6, address, &(sock->remote_addr.sin6_addr)) <= 0 || inet_pton(AF_INET6, "::1", &(sock->local_addr.sin6_addr)) <= 0) {
        fprintf(stderr, "Error: Failed to convert IPv6 Address\n");
        return -1;
    }
    if(connect(sock->sockfd, (struct sockaddr *) &(sock->remote_addr), sizeof(struct sockaddr_in6)) < 0){
        fprintf(stderr, "Error: Failed to open connection to socket\n");
        return -1;
    }
    return 0;
}


int asp_disconnect(struct asp_socket_info* sock) {
    if(close(sock->sockfd) < 0) 
    {
        perror("Error disconnecting socket");
        return -1;
    }
    return 0;
}


int asp_establish_connection(struct asp_socket_info* sock)
{
    struct asp_header msg;
    memset(&msg, 0, sizeof(msg));
    struct asp_header res;
    
    //send SYN packet
    msg.seq_number = htobe32(sock->sequence_count);
    msg.ack_number = htobe32(0);
    for(size_t i = 0; i < 4; ++i)
        msg.address[i] = htobe32(sock->local_addr.sin6_addr.__in6_u.__u6_addr32[i]);
    msg.packet_type = SYN;

    error(asp_send(sock, &msg, sizeof(msg)));

    //Receive ACK packet
    error(asp_receive(sock, &res, sizeof(res)));
    if(res.packet_type != ACK || be32toh(res.ack_number) != sock->sequence_count + 1)
    {
        fprintf(stderr, "Packet was not type ACK\n");
        return -1; //todo check the error and retransmit if packet was lost or wrong packet was sent, else refuse (in case already connected)
    }
        
    //Send ACK packet
    msg.seq_number = htobe32(++sock->sequence_count);
    msg.ack_number = htobe32(le32toh(res.seq_number) + 1);
    msg.packet_type = ACK;
    error(asp_send(sock, &msg, sizeof(msg)));

    sock->has_accepted = 1;
    return 0;
}

int asp_handle_connection(struct asp_socket_info* sock)
{
    struct asp_header msg;
    struct asp_header res;
    //receive SYN packet
    error(asp_receive(sock, &res, sizeof(res)));
    if(res.packet_type != SYN)
    {
        fprintf(stderr, "Packet was not type SYN\n");
        return -1; //TODO check the error and retransmit if packet was lost or wrong packet was sent, else refuse (in case already connected)
    }
        
    printf("Waiting on response\n");
    //Send ACK packet
    msg.seq_number = sock->sequence_count;
    msg.ack_number = be32toh(htole32(res.seq_number) + 1); // Acknowledge the received sequence number + 1
    msg.packet_type = ACK;

    error(asp_send(sock, &msg, sizeof(msg)));

    //Receive ACK packet
    error(asp_receive(sock, &res, sizeof(res)));
    if (res.packet_type != ACK || be32toh(res.ack_number) != sock->sequence_count + 1)
    {
        fprintf(stderr, "Packet was not type ACK\n");
        return -1; // Check the error and retransmit if packet was lost or wrong packet was sent, else refuse (in case already connected)
    }
        
    sock->is_connected = 1;
    return 0;
}


size_t get_audio_quality_data_size(size_t quality_level)
{
    if(quality_level == 1)
    {
        return QL1;
    } else if(quality_level == 2) {
        return QL2;
    } else if (quality_level == 3) {
        return QL3;
    } else {
        return QL4;
    }
}