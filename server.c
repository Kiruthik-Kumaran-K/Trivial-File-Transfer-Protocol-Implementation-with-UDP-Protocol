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
                        // printf("Error: File not found\n");
                        // //For a complete project we need a ERROR packet (Opcode 5) in here
                        // continue;

                        //Construct and send the ERROR packet
                        char err_packet[PACKETSIZE];

                        //Using ECODE_1 and ESTRING_1 is defined in tftp.h
                        int err_len = packet_form_error(err_packet, ECODE_1, ESTRING_1);

                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'File not found' to client.\n");
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
            else if (IS_WRQ(opcode)) 
            {
                  char filename[256];
                  strcpy(filename, buffer + 2);
                  printf("Client wants to upload file: %s\n", filename);

                  FILE *fp = fopen(filename, "wb");
                  if (fp == NULL) 
                  {
                        // printf("Error: Cannot create file.\n");
                        // continue;
                        //Construct and send the ERROR packet
                        char err_packet[PACKETSIZE];

                        //Using ECODE_1 and ESTRING_1 is defined in tftp.h
                        int err_len = packet_form_error(err_packet, ECODE_2, ESTRING_2);

                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'Access violation.' to client.\n");
                        continue;
                  }

                  // Acknowledge the WRQ with Block 0[cite: 7]
                  char ack_packet[4];
                  int ack_len = packet_form_ack(ack_packet, 0);
                  sendto(sockfd, ack_packet, ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                  unsigned short expected_block = 1;
                  char recv_buffer[PACKETSIZE];

                  while (1) {
                  // RESET client_len before every receive!
                  client_len = sizeof(client_addr);
                  int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                  
                  if (bytes_received < 4) continue; // Ignore malformed/empty packets

                  short rcv_opcode, block_num;
                  memcpy(&rcv_opcode, recv_buffer, 2);
                  memcpy(&block_num, recv_buffer + 2, 2);
                  rcv_opcode = ntohs(rcv_opcode);
                  block_num = ntohs(block_num);

                  // DEBUG: See exactly what the server receives
                  printf("DEBUG: Server received %d bytes | Opcode: %d | Block: %d\n", bytes_received, rcv_opcode, block_num);

                  if (rcv_opcode == OPCODE_DATA && block_num == expected_block) {
                        int data_len = bytes_received - 4;
                        
                        // Write payload to disk
                        fwrite(recv_buffer + 4, 1, data_len, fp);
                        printf("Received block %d (%d bytes)\n", block_num, data_len);

                        // Send ACK for received block[cite: 6]
                        ack_len = packet_form_ack(ack_packet, block_num);
                        sendto(sockfd, ack_packet, ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                        // End of transfer if data is less than 512 bytes[cite: 8]
                        if (data_len < BLIMIT) {
                              printf("File upload complete.\n");
                              break; // This breaks the loop so fclose() can run!
                        }
                        expected_block++;
                  }
                  }
                  fclose(fp); // Flushes the data to the file so it isn't empty!
        }

      }

      return 0;
}