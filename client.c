/*############################################################################################### 
 *
 *           Author         : 
 *           Date           : 
 *           File           : client.c©
 *           Descriptions   : Client side of tftp protocol
 *           Objective      : To send/recive file to/from server
 *           Usage          : ./client <server ip> <port> <filename> <output filename> [get|put]
 *           Output         :
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "udp.h"
#include "tftp.h"

/* Client code for tftp Protocol */

int main(int argc, char *argv[])
{
      if(argc != 6)
      {
            printf("Usage: ./client <server ip> <port> <filename> <output filename> [get | put]\n");
            return 1;
      }

      char *server_ip = argv[1];
      int port = atoi(argv[2]);
      char *filename = argv[3];
      char *operation = argv[5];

      int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(port);
      inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

      if(strcmp(operation, "get") == 0)
      {
            char packet[PACKETSIZE];
            int len = packet_form_rrq(packet, filename, MODE_OCTET);

            // Send RRQ to server
            sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
            printf("Requested %s from server.\n", filename);

            // TODO: Enter a loop calling recvform() to get DATA packets (Opcode 3).
            // For each block received, write the 512 bytes (or less) to output file using fwrite()
            // Then, construct an ACK packet (Opcode 4) with the matching block number and send iot back
            // If the received data block is less than 512 bytes, the transfer is complete.
            
            FILE *fp = fopen(argv[4], "wb"); //argv[4] is the output filename
            if(fp == NULL)
            {
                  perror("Failed to create a output file");
                  close(sockfd);
                  return 1;
            }

            unsigned short expected_block = 1;
            char recv_buffer[PACKETSIZE];
            socklen_t server_len = sizeof(server_addr);

            while(1)
            {

                  //Wait for DATA packet from server
                  int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                  // Extract Opcode and Block Number
                  short opcode, block_num;
                  memcpy(&opcode, recv_buffer, 2);
                  memcpy(&block_num, recv_buffer + 2, 2);
                  opcode = ntohs(opcode);
                  block_num = ntohs(block_num);

                  if(opcode == OPCODE_DATA && block_num == expected_block)
                  {
                        // Calculate how much actual file data in this packet (Total size - 4 for headers)
                        int data_len = bytes_received - 4;

                        //Write the data payload to the local file
                        fwrite(recv_buffer + 4, 1, data_len, fp);
                        printf("Received block %d (%d bytes)\n", block_num, data_len);

                        // Form and send the ACK packet
                        char ack_packet[4];
                        int ack_len = packet_form_ack(ack_packet, block_num);
                        sendto(sockfd, ack_packet, ack_len, 0, (struct sockaddr *)&server_addr, server_len);

                        //If received data is less than 512 bytes, the Transfer is complete.
                        if(data_len < BLIMIT)
                        {
                              printf("File transfer successful.\n");
                              break;
                        }

                        expected_block++;
                  }
            }
            fclose(fp);

      }

      close(sockfd);
      return 0;
}
