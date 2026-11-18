/*############################################################################################### 
 *
 *           Author         : Kiruthik Kumaran K
 *           Date           : November 2026
 *           File           : client.c
 *           Description    : Client side of the TFTP (Trivial File Transfer Protocol)
 *           Objective      : Establish a UDP connection to a TFTP server to securely get (download) 
 *                            and put (upload) files with full timeout and error recovery.
 *           Usage          : ./client [optional: server_ip]
 *           Output         : Interactive TFTP command-line interface (tftp>)
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // Core socket functions (socket, recvfrom, sendto)
#include <netinet/in.h> // sockaddr_in structure for IPv4 addresses
#include <arpa/inet.h>  // inet_pton for IP string conversions
#include <string.h>     // string manipulation (strcmp, memcpy)
#include <unistd.h>     // close() for sockets
#include "tftp.h"       // Custom macros (OPCODEs, PACKETSIZE, TIMEOUT, etc.)
#include <errno.h>      // Error number tracking for timeouts (EAGAIN, EWOULDBLOCK)
#include <sys/time.h>   // timeval structure for setsockopt timeouts

int main(int argc, char *argv[])
{
      char server_ip[256] = {0}; // Stores the target server's IP address
      int connected = 0;         // State flag: 1 if connected, 0 if not
      int port = 3333;           // Standard TFTP port is 69, using 3333 for testing

      // Feature: Allow the user to connect immediately via command line args
      if(argc > 1)
      {
            strcpy(server_ip, argv[1]);
            connected = 1;
            printf("INFO: Connected to Server\n");
      }

      printf("Type help to get supported features\n");

      char input[256];
      char current_mode[16] = "octet"; // TFTP transfer mode: octet (binary) or netascii (text)
      
      // Main interactive shell loop
      while(1)
      {
            printf("tftp> ");
            
            // Read user input safely. Break if EOF (Ctrl+D) is detected.
            if(fgets(input, sizeof(input), stdin) == NULL) break;

            // Strip the trailing newline character (\n) added by fgets
            input[strcspn(input, "\n")] = 0;

            char cmd[32], filename[256];
            // Parse the input into a command and an argument (like "get" and "file.txt")
            int parsed = sscanf(input, "%31s %255s", cmd, filename);

            if(parsed < 1) continue; // User just pressed Enter without typing anything

            // =======================================================================
            // COMMAND: HELP
            // =======================================================================
            if(strcmp(cmd, "help") == 0)
            {
                  printf("connect <server-ip>   : Connect to server\n");
                  printf("get <file-name>       : Receive a file from server\n");
                  printf("put <file-name>       : Send a file to the server\n");
                  printf("mode <type>           : Set transfer mode ('octet' or 'netascii')\n");
                  printf("bye / quit            : Exit the applications\n");
            }

            // =======================================================================
            // COMMAND: CONNECT
            // =======================================================================
            else if(strcmp(cmd, "connect") == 0)
            {
                  if(parsed >= 2)
                  {
                        strcpy(server_ip, filename); // 'filename' variable holds the IP here
                        connected = 1;
                        printf("INFO: Connected to the server\n");
                  }
                  else
                  {
                        printf("Usage: connect <server-ip>\n");
                  }
            }

            // =======================================================================
            // COMMAND: GET (Download file from Server)
            // =======================================================================
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

                  // 1. Create a UDP socket (SOCK_DGRAM)
                  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

                  // 2. Configure the socket with a 1-second timeout (SO_RCVTIMEO)
                  // This allows recvfrom() to unblock if a packet is lost in the network.
                  struct timeval tv;
                  tv.tv_sec = TIMEOUT;
                  tv.tv_usec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  // 3. Setup the server's address structure
                  struct sockaddr_in server_addr;
                  memset(&server_addr, 0, sizeof(server_addr));
                  server_addr.sin_family = AF_INET;
                  server_addr.sin_port = htons(port);
                  inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

                  // 4. Form and send the Initial Read Request (RRQ)
                  char packet[PACKETSIZE];
                  int len = packet_form_rrq(packet, filename, current_mode);
                  sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  printf("Requested %s from server.\n", filename);

                  // 5. Open local file to write the incoming data
                  FILE *fp = fopen(filename, "wb");
                  if(fp == NULL)
                  {
                        perror("Failed to create output file");
                        close(sockfd);
                        continue;
                  }

                  // State tracking variables
                  unsigned short expected_block = 1;
                  char recv_buffer[PACKETSIZE];
                  int timeout_count = 0;
                  char last_ack_packet[4];
                  int last_ack_len = 0;
                  socklen_t server_len;

                  // 6. Download Loop (Receive DATA, Send ACK)
                  while(1)
                  {
                        // CRITICAL MEMORY FIX: Reset server_len to 16 bytes before every receive.
                        // If we don't do this, the OS shrinks the variable (Value-Result Argument),
                        // which corrupts the Port Number during our next sendto() call!
                        server_len = sizeof(server_addr);
                        int bytes_received = recvfrom(sockfd, recv_buffer, PACKETSIZE, 0, (struct sockaddr *)&server_addr, &server_len);

                        // --- TIMEOUT & ERROR HANDLING ---
                        if (bytes_received < 0) 
                        {
                              // EAGAIN means the 1-second timer expired without receiving data
                              if (errno == EAGAIN || errno == EWOULDBLOCK) 
                              {
                                    timeout_count++;
                                    if (timeout_count >= TIMEOUT_LIMIT) 
                                    {
                                          printf("ERROR: Connection timed out completely.\n");
                                          break;
                                    }
                                    printf("Timeout %d! Retransmitting last packet...\n", timeout_count);
                                    
                                    // If we haven't received block 1 yet, resend the RRQ.
                                    // Otherwise, resend our last ACK to nudge the server.
                                    if (expected_block == 1) {
                                          sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    } else {
                                          sendto(sockfd, last_ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    }
                                    continue; // Go back to waiting
                              } 
                              else 
                              {
                                    perror("recvfrom error");
                                    break;
                              }
                        }

                        // Ignore severely malformed packets (smaller than TFTP header size)
                        if (bytes_received < 4) continue; 

                        // Extract Opcode (Bytes 0-1) and Block Number (Bytes 2-3)
                        short opcode, block_num;
                        memcpy(&opcode, recv_buffer, 2);
                        memcpy(&block_num, recv_buffer + 2, 2);
                        
                        // Convert from Network Byte Order to Host Byte Order
                        opcode = ntohs(opcode);
                        block_num = ntohs(block_num);

                        // Catch server rejections (e.g., File Not Found)
                        if (opcode == OPCODE_ERROR) 
                        {
                              short err_code;
                              memcpy(&err_code, recv_buffer + 2, 2);
                              printf("Server Error %d: %s\n", ntohs(err_code), recv_buffer + 4);
                              remove(filename); // Delete the empty file we created
                              break;
                        }

                        // Process only if it's DATA and it's the exact block we are waiting for
                        // This prevents "Ghost Packets" from ruining the state machine
                        if (opcode == OPCODE_DATA && block_num == expected_block)
                        {
                              timeout_count = 0; // Success! Reset the timeout tracker
                              
                              // Write the file data (Total packet size minus the 4-byte header)
                              int data_len = bytes_received - 4;
                              fwrite(recv_buffer + 4, 1, data_len, fp);
                              printf("Received block %d (%d bytes)\n", block_num, data_len);

                              // Form and send the Acknowledgment (ACK)
                              last_ack_len = packet_form_ack(last_ack_packet, block_num);
                              
                              // CRITICAL FIX 2: Explicitly use sizeof(server_addr) here to guarantee
                              // the packet routes to the correct port, allowing instant transfer speeds.
                              sendto(sockfd, last_ack_packet, last_ack_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

                              // TFTP rule: If a data packet is less than 512 bytes, it is the final packet.
                              if (data_len < BLIMIT)
                              {
                                    printf("File transfer successful.\n");
                                    break;
                              }

                              expected_block++; // Move to the next block
                        }
                  }
                  
                  // Cleanup resources
                  if(fp != NULL) fclose(fp);
                  close(sockfd);
            }

            // =======================================================================
            // COMMAND: PUT (Upload file to Server)
            // =======================================================================
            else if(strcmp(cmd, "put") == 0)
            {
                  if(!connected)
                  {
                        printf("ERROR: Not connected to server\n");
                        continue;
                  }
                  if(parsed < 2)
                  {
                        printf("Usage: put <file-name>\n");
                        continue;
                  }

                  // 1. Create UDP socket and set 1-second timeout
                  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                  struct timeval tv;
                  tv.tv_sec = TIMEOUT;
                  tv.tv_usec = 0;
                  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                  // 2. Setup Server Address
                  struct sockaddr_in server_addr;
                  memset(&server_addr, 0, sizeof(server_addr));
                  server_addr.sin_family = AF_INET;
                  server_addr.sin_port = htons(port);
                  inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

                  // 3. Open local file for reading
                  FILE *fp = fopen(filename, "rb");
                  if(fp == NULL)
                  {
                        perror("Failed to open local file");
                        close(sockfd);
                        continue;
                  }

                  // 4. Form and send the Initial Write Request (WRQ)
                  char packet[PACKETSIZE];
                  int len = packet_form_wrq(packet, filename, current_mode);
                  sendto(sockfd, packet, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                  printf("Initiated upload for %s...\n", filename);

                  // State tracking variables
                  char ack_buffer[PACKETSIZE];
                  socklen_t server_len = sizeof(server_addr);
                  
                  unsigned short block_num = 1;
                  char file_data[BLIMIT];
                  int bytes_read;

                  int timeout_count = 0;
                  char last_packet[PACKETSIZE];
                  int last_packet_len = 0;

                  // Save the WRQ as the first "last_packet" so we can resend it 
                  // if the server's initial ACK 0 gets lost in the network.
                  memcpy(last_packet, packet, len);
                  last_packet_len = len;

                  // Track the exact ACK we expect to receive (Starts at 0 for WRQ)
                  unsigned short current_block = 0; 

                  // 5. Upload Loop (Receive ACK, Send DATA)
                  while(1)
                  {
                        // Reset address size to prevent port corruption
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
                                    
                                    // Packet was lost, resend the last packet we built
                                    sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    continue; 
                              } 
                              else 
                              {
                                    perror("recvfrom error");
                                    break;
                              }
                        }

                        if (bytes_received < 4) continue; // Ignore malformed packets

                        // Extract Opcode and Block Number
                        short rcv_opcode, ack_block;
                        memcpy(&rcv_opcode, ack_buffer, 2);
                        memcpy(&ack_block, ack_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        ack_block = ntohs(ack_block);

                        // Catch server rejections (e.g., Error 6: File Already Exists)
                        if (rcv_opcode == OPCODE_ERROR) 
                        {
                              short err_code;
                              memcpy(&err_code, ack_buffer + 2, 2);
                              printf("Server Error %d: %s\n", ntohs(err_code), ack_buffer + 4);
                              break; // Stop the upload cleanly without crashing
                        }

                        // Process only if it is the EXACT ACK we are waiting for
                        else if (rcv_opcode == OPCODE_ACK && ack_block == current_block)
                        {
                              timeout_count = 0; // Success! Reset the timeout tracker

                              // If we previously sent a block smaller than 512 bytes, and the 
                              // server just acknowledged it, the upload is officially finished.
                              if (current_block > 0 && bytes_read < BLIMIT) 
                              {
                                    printf("Upload successful.\n");
                                    break;
                              }

                              current_block++; // Move to the next block

                              // Read the next chunk of file data (up to 512 bytes)
                              bytes_read = fread(file_data, 1, BLIMIT, fp);
                              
                              // Form the DATA packet directly into the last_packet buffer
                              last_packet_len = packet_form_data(last_packet, current_block, file_data, bytes_read);
                              
                              // Send the DATA packet to the server
                              sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                              printf("Sent block %d (%d bytes)\n", current_block, bytes_read);
                        }
                        else 
                        {
                              // We received a ghost packet from an old crashed session.
                              // We ignore it entirely so the timeout counter doesn't break!
                              printf("DEBUG: Ignored stray packet (Opcode %d, Block %d)\n", rcv_opcode, ack_block);
                        }
                  }
                  
                  // Cleanup resources
                  if(fp != NULL) fclose(fp);
                  close(sockfd);
            }

            // =======================================================================
            // COMMAND: MODE
            // =======================================================================
            else if (strcmp(cmd, "mode") == 0) 
            {
                  // TFTP supports binary (octet) and ASCII text (netascii)
                  if (strcmp(filename, "octet") == 0 || strcmp(filename, "netascii") == 0) 
                  {
                        strcpy(current_mode, filename);
                        printf("Mode set to %s\n", current_mode);
                  } 
                  else 
                  {
                        printf("Invalid mode. Supported modes are 'octet' and 'netascii'.\n");
                  }
            }

            // =======================================================================
            // COMMAND: QUIT / BYE
            // =======================================================================
            else if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "bye") == 0)
            {
                  break; // Exit the while(1) loop
            }
            else
            {
                  printf("Invalid command\n");
            }
      }
      
      return 0;
}