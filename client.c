#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "tftp.h"
#include <errno.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
      char server_ip[256] = {0};
      int connected = 0;
      int port = 3333; // Keeping our test port

      // Support starting the client with an IP argument directly
      if(argc > 1)
      {
            strcpy(server_ip, argv[1]);
            connected = 1;
            printf("INFO: Connected to Server\n");
      }

      printf("Type help to get supported features\n");

      char input[256];
      while(1)
      {
            printf("tftp> ");
            if(fgets(input, sizeof(input), stdin) == NULL) break;

            //Remove trailing newline character
            input[strcspn(input, "\n")] = 0;

            char cmd[32], filename[256];
            int parsed = sscanf(input, "%31s %255s", cmd, filename);

            if(parsed < 1) continue; // User just pressed enter

            // --- COMMAND: HELP ---
            if(strcmp(cmd, "help") == 0)
            {
                  printf("connect <server-ip>   : Connect to server\n");
                  printf("get <file-name>       : Receive a file from server\n");
                  printf("put <file-name>       : Send a file to the server\n");
                  printf("bye / quit            : Exit the applications\n");
            }

            // --- COMMAND : CONNECT ---
            else if(strcmp(cmd, "connect") == 0)
            {
                  if(parsed >= 2)
                  {
                        strcpy(server_ip, filename);
                        connected = 1;
                        printf("INFO: Connected to the server\n");
                  }
                  else
                  {
                        printf("Usage: connect <server-ip>\n");
                  }
            }

            // --- COMMAND: GET ---
            else if(strcmp(cmd, "get") == 0)
            {
                  if(!connected)
                  {
                        printf("ERROR: Not connected to server\n");
                        continue;
                  }
                  if(parsed < 2)
                  {
                        printf("Usage: get <file-name>\n");
                        continue;
                  }

                  //Set up the socket for this transfer
                  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

                  // Apply a 1 second receive timeout
                  struct timeval tv;
                  tv.tv_sec = TIMEOUT;
                  tv.tv_usec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  struct sockaddr_in server_addr;
                  memset(&server_addr, 0, sizeof(server_addr));
                  server_addr.sin_family = AF_INET;
                  server_addr.sin_port = htons(port);
                  inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

                  char packet[PACKETSIZE];
                  int len = packet_form_rrq(packet, filename, MODE_OCTET);

                  // Send RRQ to server
                  sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  printf("Requested %s from server.\n", filename);

                  FILE *fp = fopen(filename, "wb");
                  if(fp == NULL)
                  {
                        perror("Failed to create a outp[ut file");
                        close(sockfd);
                        continue;
                  }

                  unsigned short expected_block = 1;
                  char recv_buffer[PACKETSIZE];
                  // socklen_t server_len = sizeof(server_addr);

                  // int timeout_count = 0; // Track how many times we've timed out

                  // To retransmit, we need to remember the last packet we sent
                  // char last_ack_packet[4];
                  // int last_ack_len = 0;

                  // while(1)
                  // {
                  //       // wait for DATA packet from Server
                  //       int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                  //       // Check if recvfrom failed due to timeout
                  //       if(bytes_received < 0)
                  //       {
                  //             if(errno == EAGAIN || errno == EWOULDBLOCK)
                  //             {
                  //                   timeout_count++;
                  //                   if(timeout_count >= TIMEOUT_LIMIT)
                  //                   {
                  //                         printf("ERROR: Connection timed out completely\n");
                  //                         break; //give up
                  //                   }

                  //                   printf("Timeout %d! Retransmitting last packet ...\n", timeout_count);

                  //                   //Retransmit the RRQ if we haven't received the first block yet,
                  //                   //Otherwise retransmit the last ACK
                  //                   if(expected_block == 1)
                  //                   {
                  //                         sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  //                   }
                  //                   else
                  //                   {
                  //                         sendto(sockfd, last_ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  //                   }
                  //                   continue;
                  //             }
                  //             else
                  //             {
                  //                   perror("recvfrom error");
                  //                   break;
                  //             }
                  //       }

                  //       // If we succesfully received a packet, reset the time counter!
                  //       timeout_count = 0;

                  //       //Extract Opcode and Block Number
                  //       short opcode, block_num;
                  //       memcpy(&opcode, recv_buffer, 2);

                  //       if(opcode == OPCODE_ERROR)
                  //       {
                  //             //Extract error code and message
                  //             short err_code;
                  //             memcpy(&err_code, recv_buffer + 2, 2);
                  //             err_code = ntohs(err_code);

                  //             // The error string starts at the 4th byte
                  //             printf("Server Error %d:  %s\n", err_code, recv_buffer + 4);

                  //             //Delete the empty local file we just created
                  //             fclose(fp);
                  //             fp = NULL;
                  //             remove(filename);
                  //             break;
                  //       }

                  //       //Extract Block Number
                  //       memcpy(&block_num, recv_buffer + 2, 2);
                  //       opcode = ntohs(opcode);
                  //       block_num = ntohs(block_num);

                  //       if(opcode == OPCODE_DATA && block_num == expected_block)
                  //       {
                  //             // calculate how much actual file data in this packet. (Total size - $bytes of headers)
                  //             int data_len = bytes_received - 4;

                  //             //Write the data payload to the local file
                  //             fwrite(recv_buffer + 4, 1, data_len, fp);
                  //             printf("Received block %d (%d bytes)\n", block_num, data_len);

                  //             //Form and send the ACK packet
                  //             char ack_packet[4];
                  //             last_ack_len = packet_form_ack(last_ack_packet, block_num);
                  //             sendto(sockfd, ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

                  //             //If received dat is less than 512 bytes, the transfer is completed.
                  //             if(data_len < BLIMIT)
                  //             {
                  //                   printf("File transfer successful.\n");
                  //                   break;
                  //             }

                  //             expected_block++;
                  //       }
                  // }

                  int timeout_count = 0;
                  char last_ack_packet[4];
                  int last_ack_len = 0;
                  socklen_t server_len;

                  while(1)
                  {
                        // CRITICAL FIX 1: Reset the address size before EVERY receive!
                        server_len = sizeof(server_addr);
                        int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                        // --- TIMEOUT HANDLING ---
                        if (bytes_received < 0) 
                        {
                              if (errno == EAGAIN || errno == EWOULDBLOCK) 
                              {
                                    timeout_count++;
                                    if (timeout_count >= TIMEOUT_LIMIT) 
                                    {
                                          printf("ERROR: Connection timed out completely.\n");
                                          break;
                                    }
                                    printf("Timeout %d! Retransmitting last packet...\n", timeout_count);
                                    
                                    // Retransmit RRQ if no data received yet, else retransmit last ACK
                                    if (expected_block == 1) {
                                          sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    } else {
                                          sendto(sockfd, last_ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    }
                                    continue; 
                              } 
                              else 
                              {
                                    perror("recvfrom error");
                                    break;
                              }
                        }

                        if (bytes_received < 4) continue; 

                        short opcode, block_num;
                        memcpy(&opcode, recv_buffer, 2);
                        memcpy(&block_num, recv_buffer + 2, 2);
                        opcode = ntohs(opcode);
                        block_num = ntohs(block_num);

                        if (opcode == OPCODE_ERROR) 
                        {
                              short err_code;
                              memcpy(&err_code, recv_buffer + 2, 2);
                              printf("Server Error %d: %s\n", ntohs(err_code), recv_buffer + 4);
                              break;
                        }

                        // Only process if it is the EXACT DATA block we are waiting for
                        if (opcode == OPCODE_DATA && block_num == expected_block)
                        {
                              timeout_count = 0; // Reset timeout only on success
                              
                              int data_len = bytes_received - 4;
                              fwrite(recv_buffer + 4, 1, data_len, fp);
                              printf("Received block %d (%d bytes)\n", block_num, data_len);

                              last_ack_len = packet_form_ack(last_ack_packet, block_num);
                              
                              // CRITICAL FIX 2: Explicitly use sizeof(server_addr) to prevent port corruption!
                              sendto(sockfd, last_ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

                              if (data_len < BLIMIT)
                              {
                                    printf("File transfer successful.\n");
                                    break;
                              }

                              expected_block++;
                        }
                  }
                  if(fp != NULL)
                  {
                        fclose(fp);
                  }

                  close(sockfd);
            }

            // --- COMMAND: PUT ---
            else if(strcmp(cmd, "put") == 0)
            {
                  if(!connected)
                  {
                        printf("ERROR: Not conneceted to server\n");
                        continue;
                  }
                  if(parsed < 2)
                  {
                        printf("Usage: put <file-name>\n");
                        continue;
                  }

                  // Set up the socket for this transfer
                  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

                  // Apply a 1- seond receive timeout
                  struct timeval tv;
                  tv.tv_sec = TIMEOUT;
                  tv.tv_usec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  struct sockaddr_in server_addr;
                  memset(&server_addr, 0, sizeof(server_addr));
                  server_addr.sin_family = AF_INET;
                  server_addr.sin_port = htons(port);
                  inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

                  FILE *fp = fopen(filename, "rb");
                  if(fp == NULL)
                  {
                        perror("Failed to open local file");
                        close(sockfd);
                        continue;
                  }

                  char packet[PACKETSIZE];
                  int len = packet_form_wrq(packet, filename, MODE_OCTET);

                  // Send WRQ
                  sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  printf("Initiated upload for %s...\n", filename);

                  char ack_buffer[PACKETSIZE];
                  socklen_t server_len = sizeof(server_addr);
                  

                  unsigned short block_num = 1;
                  char file_data[BLIMIT];
                  int bytes_read;

                  int timeout_count = 0;
                  char last_packet[PACKETSIZE];
                  int last_packet_len = 0;

                  // Save the WRQ as the first "last_packet" in acse ACK 0 get lost
                  memcpy(last_packet, packet, len);
                  last_packet_len = len;

                  // Track the exact ACK we expect to receive (starts at 0 for WRQ)
                  unsigned short current_block = 0; 

                  // while(1)
                  // {
                  //       server_len = sizeof(server_addr);
                  //       int bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                  //       // --- Timeout Handling ---
                  //       if(bytes_received < 0)
                  //       {
                  //             if(errno == EAGAIN || errno == EWOULDBLOCK)
                  //             {
                  //                   timeout_count++;
                  //                   if(timeout_count >= TIMEOUT_LIMIT)
                  //                   {
                  //                         printf("ERROR: Connection timed out completely.\n");
                  //                         break;
                  //                   }

                  //                   printf("Timeout %d! Retransmitting last packet...\n", timeout_count);
                  //                   sendto(sockfd, last_packet, last_packet_len, 0 , (struct sockaddr *)&server_addr, sizeof(server_addr));
                  //                   continue; //Go back to waiting for recvfrom
                  //             }
                  //             else
                  //             {
                  //                   perror("recvfrom error");
                  //                   break;
                  //             }
                  //       }

                  //       timeout_count = 0;

                  //       if (bytes_received < 4) continue; // Ignore malformed packets

                  //       //Extract Opcode and Block Number from ACK
                  //       short rcv_opcode, ack_block;
                  //       memcpy(&rcv_opcode, ack_buffer, 2);
                  //       memcpy(&ack_buffer, ack_buffer + 2, 2);
                  //       rcv_opcode = ntohs(rcv_opcode);
                  //       ack_block = ntohs(ack_block);

                  //       printf("DEBUG Client: Received Opcode %d | ACK Block %d\n", rcv_opcode, ack_block);

                  //       //If it is an ACK and it matches the block we just sent (or block 0 for WRQ)
                  //       if(rcv_opcode == OPCODE_ACK && ack_block == current_block)
                  //       {

                  //             // If we just acknowledged a block that was less than 512 bytes, we are officially done!
                  //             if (current_block > 0 && bytes_read < BLIMIT) 
                  //             {
                  //                   printf("Upload successful.\n");
                  //                   break;
                  //             }

                  //             // Move to the next block
                  //             current_block++; 

                  //             // Dont erase this code
                  //             bytes_read = fread(file_data, 1, BLIMIT, fp);

                  //             //Form and send DATA packet
                  //             last_packet_len = packet_form_data(last_packet, current_block, file_data, bytes_read);

                  //             // Use sizeof(server_addr) directly to guarantee safety
                  //             sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  //             printf("Sent block %d (%d bytes)\n", current_block, bytes_read);

                  //       }

                  // }
                  while(1)
                  {
                        server_len = sizeof(server_addr);
                        int bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                        // --- TIMEOUT HANDLING ---
                        if (bytes_received < 0) 
                        {
                              if (errno == EAGAIN || errno == EWOULDBLOCK) 
                              {
                                    timeout_count++;
                                    if (timeout_count >= TIMEOUT_LIMIT) 
                                    {
                                          printf("ERROR: Connection timed out completely.\n");
                                          break;
                                    }
                                    printf("Timeout %d! Retransmitting packet block %d...\n", timeout_count, current_block);
                                    sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    continue; 
                              } 
                              else 
                              {
                                    perror("recvfrom error");
                                    break;
                              }
                        }

                        if (bytes_received < 4) continue; 

                        short rcv_opcode, ack_block;
                        memcpy(&rcv_opcode, ack_buffer, 2);
                        memcpy(&ack_block, ack_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        ack_block = ntohs(ack_block);

                        // Only process if it is the EXACT ACK we are waiting for
                        if (rcv_opcode == OPCODE_ACK && ack_block == current_block)
                        {
                              // CRITICAL FIX: Only reset timeout when we get the correct packet!
                              timeout_count = 0; 

                              if (current_block > 0 && bytes_read < BLIMIT) 
                              {
                                    printf("Upload successful.\n");
                                    break;
                              }

                              current_block++; 
                              bytes_read = fread(file_data, 1, BLIMIT, fp);
                              last_packet_len = packet_form_data(last_packet, current_block, file_data, bytes_read);
                              
                              sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                              printf("Sent block %d (%d bytes)\n", current_block, bytes_read);
                        }
                        else 
                        {
                              // It's a ghost packet or old ACK. Ignore it so the timeout keeps running!
                              printf("DEBUG: Ignored stray packet (Opcode %d, Block %d)\n", rcv_opcode, ack_block);
                        }
                  }
                  if(fp != NULL)
                        fclose(fp);
            }

            // --- COMMAND: QUIT ---
            else if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "bye") == 0)
            {
                  break;
            }
            else
            {
                  printf("Invalid command\n");
            }
      }
      return 0;
}