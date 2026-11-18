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
#include <sys/time.h>

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

            // CRITICAL FIX: If recvfrom fails (e.g., client disconnected), ignore and wait again
            if (n < 0) 
            {
                  continue; 
            }

            // Extract Opcode (first 2 bytes)
            short opcode;
            memcpy(&opcode, buffer, 2);
            opcode = ntohs(opcode);

            if (IS_RRQ(opcode)) 
            {
                  char filename[256];
                  strcpy(filename, buffer + 2); 
                  printf("Client requested file: %s\n", filename);

                  FILE *fp = fopen(filename, "rb");
                  if (fp == NULL) 
                  {
                        // Construct and send ERROR packet
                        char err_packet[PACKETSIZE];
                        int err_len = packet_form_error(err_packet, ECODE_1, ESTRING_1); 
                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'File not found' to client.\n");
                        continue; 
                  }

                  // Turn ON the 1-second timeout for this transfer
                  struct timeval tv = {TIMEOUT, 0};
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  unsigned short block_num = 1;
                  char file_data[BLIMIT];
                  int bytes_read;
                  
                  int timeout_count = 0;
                  char last_packet[PACKETSIZE];
                  int last_packet_len = 0;

                  while (1) {
                  bytes_read = fread(file_data, 1, BLIMIT, fp);
                  last_packet_len = packet_form_data(last_packet, block_num, file_data, bytes_read);

                  sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                  printf("Sent block %d (%d bytes)\n", block_num, bytes_read);

                  // Wait for the correct ACK
                  while (1) {
                        client_len = sizeof(client_addr);
                        char ack_buffer[PACKETSIZE];
                        int bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                        
                        // --- TIMEOUT & ERROR HANDLING ---
                        if (bytes_received < 0) {
                              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                              timeout_count++;

                              if (timeout_count >= TIMEOUT_LIMIT) 
                              {
                                    // FINAL ACK FIX: If we are on the last block, assume the client finished and left.
                                    if (bytes_read < BLIMIT) 
                                    {
                                          printf("Client disconnected after final block. Transfer complete.\n");
                                    } 
                                    else 
                                    {
                                          printf("ERROR: Client timed out. Aborting transfer.\n");
                                    }
                                    goto transfer_done; 
                              }
                              printf("Timeout %d! Retransmitting DATA block %d...\n", timeout_count, block_num);
                              sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                              continue;
                              } 
                              else 
                              {
                              // Hard network error. Bail out safely!
                              goto transfer_done;
                              }
                        }
                        
                        if (bytes_received < 4) continue; // Ignore malformed packets
                        
                        short rcv_opcode, ack_block;
                        memcpy(&rcv_opcode, ack_buffer, 2);
                        memcpy(&ack_block, ack_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        ack_block = ntohs(ack_block);

                        // Process only if it is an ACK
                        if (rcv_opcode == OPCODE_ACK) {
                              if (ack_block == block_num) 
                              {
                                    // Correct ACK received! 
                                    timeout_count = 0; 
                                    break; // Break inner loop to send the next data block
                              } 
                              else if (ack_block < block_num) 
                              {
                                    // DESYNC FIX: Client sent an old ACK. Our DATA got lost. Resend it!
                                    sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                              }
                        }
                  } 
                  
                  // If we just sent the final block, the transfer is complete
                  if (bytes_read < BLIMIT) {
                        printf("File transfer complete.\n");
                        break;
                  }
                  block_num++;
                  }
                  
                  // Cleanup Label
                  transfer_done:
                  if (fp != NULL) {
                  fclose(fp);
                  }

                  // Turn OFF the timeout so the server can wait infinitely for the next client
                  tv.tv_sec = 0; 
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }
            else if (IS_WRQ(opcode)) 
            {
                  char filename[256];
                  strcpy(filename, buffer + 2);
                  printf("Client wants to upload file: %s\n", filename);

                  // --- ADD THIS ERROR 6 CHECK HERE ---
                  FILE *check_fp = fopen(filename, "rb");
                  if (check_fp != NULL) 
                  {
                        fclose(check_fp); // Close it immediately
                        char err_packet[PACKETSIZE];
                        int err_len = packet_form_error(err_packet, 6, "File already exists");
                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("Error: File already exists. Sent ERROR packet to client.\n");
                        continue; // Abort this request and wait for the next client
                  }
                  // -----------------------------------

                  FILE *fp = fopen(filename, "wb");
                  if (fp == NULL) 
                  {
                        char err_packet[PACKETSIZE];

                        //Using ECODE_1 and ESTRING_1 is defined in tftp.h
                        int err_len = packet_form_error(err_packet, ECODE_2, ESTRING_2);

                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'Access violation.' to client.\n");
                        continue;
                  }

                  // Turn ON the 1 - second timeout for this transfer
                  struct timeval tv = {TIMEOUT, 0};
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  // Acknowledge the WRQ with Block 0[cite: 7]
                  char last_ack[4];
                  int last_ack_len = packet_form_ack(last_ack, 0);
                  sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                  unsigned short expected_block = 1;
                  char recv_buffer[PACKETSIZE];
                  int timeout_count = 0;

                  while (1) 
                  {
                        client_len = sizeof(client_addr);
                        int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                        
                        if (bytes_received < 0) {
                              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    timeout_count++;
                                    if (timeout_count >= TIMEOUT_LIMIT) {
                                    printf("ERROR: Client timed out. Aborting transfer.\n");
                                    break;
                                    }
                                    printf("Timeout %d! Retransmitting ACK %d...\n", timeout_count, expected_block - 1);
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);
                                    continue; 
                              } else {
                                    break; 
                              }
                        }

                        if (bytes_received < 4) continue; 

                        short rcv_opcode, block_num;
                        memcpy(&rcv_opcode, recv_buffer, 2);
                        memcpy(&block_num, recv_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        block_num = ntohs(block_num);

                        if (rcv_opcode == OPCODE_DATA) {
                              if (block_num == expected_block) {
                                    // CRITICAL FIX: Only reset timeout when we get the correct data block!
                                    timeout_count = 0; 
                                    
                                    int data_len = bytes_received - 4;
                                    fwrite(recv_buffer + 4, 1, data_len, fp);
                                    printf("Received block %d (%d bytes)\n", block_num, data_len);

                                    last_ack_len = packet_form_ack(last_ack, block_num);
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                                    if (data_len < BLIMIT) {
                                    printf("File upload complete.\n");
                                    break;
                                    }
                                    expected_block++;
                              } 
                              else if (block_num < expected_block) {
                                    // Client sent an old block, meaning our ACK got lost. Resend it!
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);
                              }
                        }
                  }

                  // Clean up safely
                  if (fp != NULL) 
                  {
                  fclose(fp);
                  }

                  //Turn OFF the timer so Server can wait infinitely for next client
                  tv.tv_sec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }
            else 
            {
                  // If the opcode isn't recognized...
                  char err_packet[PACKETSIZE];
                  int err_len = packet_form_error(err_packet, 4, "Illegal TFTP operation");
                  sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                  printf("Error: Received unknown opcode %d. Sent Error 4 to client.\n", opcode);
            }
      }

      return 0;
}