/*############################################################################################### 
 *
 *           Author         : 
 *           Date           : 
 *           File           : server.c©
 *           Descriptions   : Server side of tftp protocol
 *           Objective      : Ready to recive/send data to client 
 *           Usage          : ./server <port number>
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
#include "file.h"
#include "udp.h"
#include "tftp.h"

int main(int argc, char *argv[])
{
      if(argc != 2)
      {
            printf("Usage: ./server <port number>\n");
            return 1;
      }

      int port = atoi(argv[1]);
      int sockfd = create_bind_udp_socket(port);
      printf("TFTP Server listening on port %d ...\n", port);

      char buffer[PACKETSIZE];
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);

      while(1)
      {
            // Wait for a request
            int n = recvfrom(sockfd, buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);

            // Extract Opcode (first 2 bytes )
            short opcode;
            memcpy(&opcode, buffer, 2);
            opcode = ntohs(opcode);

            if(IS_RRQ(opcode))
            {
                  char filename[256];
                  strcpy(filename, buffer + 2); //filename comes after the opcode
                  printf("Client requested file: %s", filename);

                  // TODO: Open file, read in 512 byte chunks, form DATA packets (Opcode 2).
                  // send to client using sendto(), and wait for ACK (Opcode 4) before sending the next block.
                  //If the file doesn't exist, send an ERROR packet (Opcode 5, Error Code 1)

                  FILE *fp = fopen(filename, "rb");
                  if(fp == NULL)
                  {
                        printf("Error: File not found\n");
                        //For a complete project we need a ERROR packet (Opcode 5) in here
                        continue;
                  }

                  unsigned short block_num = 1;
                  char file_data[BLIMIT]; // BLIMIT is 512
                  int bytes_read;

                  while(1)
                  {

                        //Read up to 512 bytes from the file
                        bytes_read = fread(file_data, 1, BLIMIT, fp);

                        // Form the DATA packet (Opcode 3)
                        char data_packet[PACKETSIZE];
                        int packet_len = packet_form_data(data_packet, block_num, file_data, bytes_read);

                        //Send the DATA packet to client
                        sendto(sockfd, data_packet, packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Sent block %d (%d bytes)\n", block_num, bytes_read);

                        //Wait for ACK packet (Opcode 4)
                        char ack_buffer[PACKETSIZE];
                        recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);

                        //If the datawe just sent was less than 512 bytes, this marks the end of the transfer
                        if(bytes_read < BLIMIT)
                        {
                              printf("File transfer is complete\n");
                              break;
                        }

                        block_num++; // Move to the next block
                  }
                  fclose(fp);
            }
      }

      return 0;
}