#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    ssize_t bytes_recv;
    char collected_segments[8000][PAYLOAD_SIZE];
    int received_flags[8000] = {0};
    int num_collected = 0;
    int final_packet_size = 0;
    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    
    while(1){
        bytes_recv = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size);
        /*printf("Received %zd bytes:\n", bytes_recv);
                for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
                    printf("%c", buffer.payload[i]);
                }
                printf("\n");
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
            printf("Timeout occurred. Retransmitting packet: %d\n", expected_seq_num);
            build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 1, "0");
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);
            continue;
        }
        if (buffer.seqnum < expected_seq_num && !(buffer.last == 1 && expected_seq_num - 1 == buffer.seqnum)){
            build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 1, "0");
            sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);
            continue;
        }         */


        
        if (!received_flags[buffer.seqnum]) {
                    // Copy the payload content to the collected_segments array
                    memcpy(collected_segments[buffer.seqnum], buffer.payload, buffer.length);
                    received_flags[buffer.seqnum] = 1;
                }
        
        if (buffer.seqnum > num_collected)
            num_collected = buffer.seqnum;
        
        if (expected_seq_num == buffer.seqnum) {
            while (received_flags[expected_seq_num] == 1)
                expected_seq_num++;
        }

        if (buffer.last == 1 && expected_seq_num - 1 == buffer.seqnum) {
            final_packet_size = buffer.length;
                    break;
                }
        build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 1, "0");
        sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size);
        //printRecv(&buffer);
    }
    
    for (int i = 0; i < num_collected; i++) {
            fwrite(collected_segments[i], 1, PAYLOAD_SIZE, fp);
        }

    if (final_packet_size > 0)
        fwrite(collected_segments[num_collected], 1, final_packet_size, fp);
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
