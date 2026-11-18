/*############################################################################################### 
 *
 *           Author         : Kiruthik Kumaran k
 *           Date           : November 2026
 *           File           : server.c
 *           Description    : Server side of the TFTP (Trivial File Transfer Protocol)
 *           Objective      : Listen on a UDP port for RRQ (download) and WRQ (upload) requests,
 *                            serving and storing files with timeout recovery and RFC 1350 error handling.
 *           Usage          : ./server <port number>
 *           Output         : Live transfer logs, block acknowledgments, and error diagnostics
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // Core socket APIs (recvfrom, sendto, setsockopt)
#include <netinet/in.h> // sockaddr_in structure and byte-order conversions (ntohs)
#include <errno.h>      // Error codes for socket timeout detection (EAGAIN, EWOULDBLOCK)
#include <signal.h>     // Signal handling utilities
#include <string.h>     // Memory and string operations (memcpy, strcpy)
#include <unistd.h>     // POSIX system calls
#include <sys/time.h>   // struct timeval for SO_RCVTIMEO configuration
#include "udp.h"        // Custom UDP socket creation and binding helpers
#include "tftp.h"       // TFTP opcodes, packet builders, error codes, and buffer limits

int main(int argc, char *argv[])
{
      // Validate command-line arguments: require exactly one argument (port number)
      if(argc != 2)
      {
            printf("Usage: ./server <port number>\n");
            return 1;
      }

      // Convert port argument to integer and bind a UDP socket to it
      int port = atoi(argv[1]);
      int sockfd = create_bind_udp_socket(port);
      printf("TFTP Server listening on port %d ...\n", port);

      char buffer[PACKETSIZE];
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);

      // =======================================================================
      // MAIN SERVER LOOP: Wait indefinitely for incoming client requests
      // =======================================================================
      while(1)
      {
            // Reset address length before every recvfrom() to prevent value-result truncation
            client_len = sizeof(client_addr);

            // Block until a new TFTP request packet (RRQ or WRQ) arrives
            int n = recvfrom(sockfd, buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);

            // Guard against transient socket errors or abrupt client disconnects
            if (n < 0) 
            {
                  continue; 
            }

            // Extract the 2-byte TFTP Opcode and convert from Network to Host Byte Order
            short opcode;
            memcpy(&opcode, buffer, 2);
            opcode = ntohs(opcode);

            // =======================================================================
            // HANDLER 1: RRQ (Read Request - Client is downloading a file)
            // =======================================================================
            if (IS_RRQ(opcode)) 
            {
                  // Filename starts immediately after the 2-byte opcode and is null-terminated
                  char filename[256];
                  strcpy(filename, buffer + 2); 
                  printf("Client requested file: %s\n", filename);

                  // Attempt to open the requested file in binary read mode
                  FILE *fp = fopen(filename, "rb");
                  if (fp == NULL) 
                  {
                        // File does not exist: reply with TFTP Error Code 1 (File not found)
                        char err_packet[PACKETSIZE];
                        int err_len = packet_form_error(err_packet, ECODE_1, ESTRING_1); 
                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'File not found' to client.\n");
                        continue; 
                  }

                  // Enable a 1-second receive timeout for active transfer retransmission logic
                  struct timeval tv = {TIMEOUT, 0};
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  // Transfer state variables (TFTP data blocks are 1-indexed)
                  unsigned short block_num = 1;
                  char file_data[BLIMIT];
                  int bytes_read;
                  
                  int timeout_count = 0;
                  char last_packet[PACKETSIZE];
                  int last_packet_len = 0;

                  // Outer loop: Read file chunks and transmit DATA packets sequentially
                  while (1) 
                  {
                        // Read up to 512 bytes (BLIMIT) from disk and build the DATA packet
                        bytes_read = fread(file_data, 1, BLIMIT, fp);
                        last_packet_len = packet_form_data(last_packet, block_num, file_data, bytes_read);

                        // Transmit the current DATA block to the client
                        sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Sent block %d (%d bytes)\n", block_num, bytes_read);

                        // Inner loop: Wait for the matching ACK before advancing to the next block
                        while (1) 
                        {
                              // Reset client_len before each receive to keep the client address intact
                              client_len = sizeof(client_addr);
                              char ack_buffer[PACKETSIZE];
                              int bytes_received = recvfrom(sockfd, ack_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                              
                              // --- TIMEOUT & ERROR HANDLING ---
                              if (bytes_received < 0) 
                              {
                                    // Timeout expired: client did not ACK in time
                                    if (errno == EAGAIN || errno == EWOULDBLOCK) 
                                    {
                                          timeout_count++;

                                          if (timeout_count >= TIMEOUT_LIMIT) 
                                          {
                                                // Final ACK edge case: if the last block (< 512 bytes) was sent,
                                                // the client may have exited immediately after sending a lost final ACK.
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

                                          // Retransmit the last DATA packet and keep waiting
                                          printf("Timeout %d! Retransmitting DATA block %d...\n", timeout_count, block_num);
                                          sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                                          continue;
                                    } 
                                    else 
                                    {
                                          // Unrecoverable socket error: abort transfer safely
                                          goto transfer_done;
                                    }
                              }
                              
                              // Drop malformed packets smaller than the 4-byte TFTP ACK header
                              if (bytes_received < 4) continue; 
                              
                              // Parse Opcode (bytes 0-1) and Acknowledged Block Number (bytes 2-3)
                              short rcv_opcode, ack_block;
                              memcpy(&rcv_opcode, ack_buffer, 2);
                              memcpy(&ack_block, ack_buffer + 2, 2);
                              rcv_opcode = ntohs(rcv_opcode);
                              ack_block = ntohs(ack_block);

                              // Validate that the packet is an ACK
                              if (rcv_opcode == OPCODE_ACK) 
                              {
                                    if (ack_block == block_num) 
                                    {
                                          // Expected ACK received: reset timeout counter and send next block
                                          timeout_count = 0; 
                                          break; 
                                    } 
                                    else if (ack_block < block_num) 
                                    {
                                          // Duplicate old ACK indicates our latest DATA packet was lost; resend it
                                          sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&client_addr, client_len);
                                    }
                              }
                        } 
                        
                        // RFC 1350: A DATA packet with fewer than 512 bytes signals end-of-file
                        if (bytes_read < BLIMIT) 
                        {
                              printf("File transfer complete.\n");
                              break;
                        }
                        block_num++;
                  }
                  
                  // Centralized cleanup target for both normal completion and timeout aborts
                  transfer_done:
                  if (fp != NULL) 
                  {
                        fclose(fp);
                  }

                  // Disable socket timeout so the server blocks indefinitely for the next client
                  tv.tv_sec = 0; 
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }

            // =======================================================================
            // HANDLER 2: WRQ (Write Request - Client is uploading a file)
            // =======================================================================
            else if (IS_WRQ(opcode)) 
            {
                  char filename[256];
                  strcpy(filename, buffer + 2);
                  printf("Client wants to upload file: %s\n", filename);

                  // Check if the file already exists before opening in write mode (prevents overwrite)
                  FILE *check_fp = fopen(filename, "rb");
                  if (check_fp != NULL) 
                  {
                        fclose(check_fp);
                        char err_packet[PACKETSIZE];
                        int err_len = packet_form_error(err_packet, 6, "File already exists");
                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                        printf("Error: File already exists. Sent ERROR packet to client.\n");
                        continue; // Reject upload and return to main listening loop
                  }

                  // Open the destination file in binary write mode
                  FILE *fp = fopen(filename, "wb");
                  if (fp == NULL) 
                  {
                        // Failed to create file (e.g., permission denied): send Error Code 2
                        char err_packet[PACKETSIZE];
                        int err_len = packet_form_error(err_packet, ECODE_2, ESTRING_2);
                        sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                        printf("Error: Sent 'Access violation.' to client.\n");
                        continue;
                  }

                  // Enable 1-second receive timeout for the upload session
                  struct timeval tv = {TIMEOUT, 0};
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  // Acknowledge the WRQ with ACK Block 0 to authorize the client to send Block 1
                  char last_ack[4];
                  int last_ack_len = packet_form_ack(last_ack, 0);
                  sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                  unsigned short expected_block = 1;
                  char recv_buffer[PACKETSIZE];
                  int timeout_count = 0;

                  // Upload Loop: Receive DATA blocks sequentially and reply with ACKs
                  while (1) 
                  {
                        // Reset client_len before every recvfrom() to avoid address truncation
                        client_len = sizeof(client_addr);
                        int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&client_addr, &client_len);
                        
                        // --- TIMEOUT & ERROR HANDLING ---
                        if (bytes_received < 0) 
                        {
                              if (errno == EAGAIN || errno == EWOULDBLOCK) 
                              {
                                    timeout_count++;
                                    if (timeout_count >= TIMEOUT_LIMIT) 
                                    {
                                          printf("ERROR: Client timed out. Aborting transfer.\n");
                                          break;
                                    }
                                    // Retransmit the last ACK to prompt the client to send the next block
                                    printf("Timeout %d! Retransmitting ACK %d...\n", timeout_count, expected_block - 1);
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);
                                    continue; 
                              } 
                              else 
                              {
                                    // Hard socket error: exit loop safely before touching memory
                                    break; 
                              }
                        }

                        // Prevent integer underflow when computing (bytes_received - 4)
                        if (bytes_received < 4) continue; 

                        // Extract Opcode (bytes 0-1) and Block Number (bytes 2-3)
                        short rcv_opcode, block_num;
                        memcpy(&rcv_opcode, recv_buffer, 2);
                        memcpy(&block_num, recv_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        block_num = ntohs(block_num);

                        if (rcv_opcode == OPCODE_DATA) 
                        {
                              if (block_num == expected_block) 
                              {
                                    // Reset timeout only when the exact expected block arrives (ignores ghost packets)
                                    timeout_count = 0; 
                                    
                                    // Write payload (everything after the 4-byte TFTP header) to disk
                                    int data_len = bytes_received - 4;
                                    fwrite(recv_buffer + 4, 1, data_len, fp);
                                    printf("Received block %d (%d bytes)\n", block_num, data_len);

                                    // Build and send ACK for the block just written
                                    last_ack_len = packet_form_ack(last_ack, block_num);
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);

                                    // Fewer than 512 bytes indicates the final block of the file
                                    if (data_len < BLIMIT) 
                                    {
                                          printf("File upload complete.\n");
                                          break;
                                    }
                                    expected_block++;
                              } 
                              else if (block_num < expected_block) 
                              {
                                    // Client retransmitted an already-saved block (our ACK was lost); resend last ACK
                                    sendto(sockfd, last_ack, last_ack_len, 0, (struct sockaddr *)&client_addr, client_len);
                              }
                        }
                  }

                  // Close the uploaded file safely
                  if (fp != NULL) 
                  {
                        fclose(fp);
                  }

                  // Disable socket timeout so the server waits indefinitely for the next client
                  tv.tv_sec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }

            // =======================================================================
            // HANDLER 3: UNRECOGNIZED OPCODE (Error Code 4 - Illegal TFTP Operation)
            // =======================================================================
            else 
            {
                  char err_packet[PACKETSIZE];
                  int err_len = packet_form_error(err_packet, 4, "Illegal TFTP operation");
                  sendto(sockfd, err_packet, err_len, 0, (struct sockaddr *)&client_addr, client_len);
                  printf("Error: Received unknown opcode %d. Sent Error 4 to client.\n", opcode);
            }
      }

      return 0;
}