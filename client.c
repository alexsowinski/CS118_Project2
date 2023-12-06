#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "utils.h"

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    char last = 0;
    char ack = 0;
    int expected_ack = 1;

    //tv.tv_sec = TIMEOUT;
    //tv.tv_usec = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }
    
    int file_length;
    fseek(fp, 0, SEEK_END);
    file_length = ftell(fp);
    int last_packet_length = file_length % PAYLOAD_SIZE;
    int num_packets = file_length/PAYLOAD_SIZE + (file_length % PAYLOAD_SIZE != 0);

    fseek(fp, 0, SEEK_SET);

    ssize_t bytes_read;
    struct packet segments[num_packets];
    while((bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp)) > 0) {
        if (bytes_read < PAYLOAD_SIZE) {
            last = 1;
        }
        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
        segments[seq_num] = pkt;
        seq_num++;
    }
    close(fp);
    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    
    //for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
    //    printf("%c", segments[1].payload[i]);
    //}
    //printf("\n");

    
    // TODO: Read from file, and initiate reliable data transfer to the server
    ssize_t bytes_recv;
    seq_num = 0;
    float cwnd = 1;
    int ssthresh = 6;
    int dup_ack = 0;
    int curr_ack = 0;
    int in_transit = 0;
    int fr_phase = 0;
    while(1){
        //bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
        //build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, buffer);
        //if (bytes_read == -1) {
        //    perror("Error reading from file");
        //    fclose(fp);
        //    close(listen_sockfd);
        //    close(send_sockfd);
        //    return 1;
        //}
        //else if (bytes_read == 0)
        //    break;
        //else if (bytes_read < PAYLOAD_SIZE)
        //    pkt.last = 1;
        //sleep(0.001);
        //for (int i = 0; i < cwnd; i++){
        //    sendto(send_sockfd, &segments[seq_num + i], sizeof(segments[seq_num + i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
        //}
        if (/*(int)cwnd <= ssthresh && */in_transit < (int)cwnd){
            for (int i = seq_num; i < seq_num + (int)cwnd && i < num_packets; i++)
            {
                sendto(send_sockfd, &segments[i], sizeof(segments[i]), 0, (struct sockaddr *)&server_addr_to, addr_size);
                in_transit++;
            }
        }
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_sockfd, &read_fds);

        int select_result = select(listen_sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result == -1) {
                    perror("Error in select");
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 1;
        }
        else if (select_result == 0) {
            printf("Timeout occurred. Retransmitting packet: %d\n", seq_num);
            if (cwnd/2 > 2)
                ssthresh = cwnd/2;
            else
                ssthresh = 2;
            cwnd = 1;
            in_transit = 0; //possibly decrement by more for better efficiency
            continue;
        }
        bytes_recv = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
        printf("sequence number: %d - ack number: %d\n", seq_num, ack_pkt.acknum);
        if (ack_pkt.acknum != curr_ack){
            if (fr_phase == 1){
                cwnd = ssthresh;
                fr_phase = 0;
            }
            else{
                if ((int)cwnd <= ssthresh) {
                    cwnd += (ack_pkt.acknum - curr_ack); //possible this stretches slow start past the cwnd <= ssthresh condition
                }
                else {
                    cwnd += (float)(ack_pkt.acknum - curr_ack)/cwnd;
                }
            }
            in_transit -= (ack_pkt.acknum - curr_ack);
            curr_ack = ack_pkt.acknum;
            dup_ack = 0;
        }
        else
            dup_ack++;
        seq_num = ack_pkt.acknum;
        if (dup_ack == 3) {
            fr_phase = 1;
            if (cwnd/2 > 2)
                ssthresh = cwnd/2;
            else
                ssthresh = 2;
            cwnd = ssthresh + 3;
            sendto(send_sockfd, &segments[seq_num], sizeof(segments[seq_num]), 0, (struct sockaddr *)&server_addr_to, addr_size);
        }
        if (dup_ack > 3){
            cwnd++;
        }
            
        /*if (ack_pkt.acknum == seq_num + 1) {
            printf("Received acknowledgment for sequence number %hu\n", seq_num);
            seq_num = ack_pkt.acknum;
            cwnd++;
        }
        else {
            printf("Received acknowledgment for unexpected sequence number %hu (out-of-order acknowledgment)\n", ack_pkt.acknum);
        }*/
    }
    
    //fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

