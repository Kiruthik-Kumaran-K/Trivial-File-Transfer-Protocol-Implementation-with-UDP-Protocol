int timeout_count = 0;
                  char last_packet[PACKETSIZE];
                  int last_packet_len = len;
                  
                  // Save the WRQ as the first "last_packet"
                  memcpy(last_packet, packet, len); 

                  char ack_buffer[PACKETSIZE];
                  socklen_t server_len = sizeof(server_addr);
                  
                  // Track the exact ACK we expect to receive (starts at 0 for WRQ)
                  unsigned short current_block = 0; 

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
                                    printf("Timeout %d! Retransmitting last packet...\n", timeout_count);
                                    sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                                    continue; 
                              } 
                              else 
                              {
                                    perror("recvfrom error");
                                    break;
                              }
                        }

                        timeout_count = 0; // Reset on success
                        if (bytes_received < 4) continue; // Ignore malformed packets

                        // Extract Opcode and Block Number from ACK
                        short rcv_opcode, ack_block;
                        memcpy(&rcv_opcode, ack_buffer, 2);
                        memcpy(&ack_block, ack_buffer + 2, 2);
                        rcv_opcode = ntohs(rcv_opcode);
                        ack_block = ntohs(ack_block);

                        printf("DEBUG Client: Received Opcode %d | ACK Block %d\n", rcv_opcode, ack_block);

                        // If it is an ACK and it matches the block we are currently waiting for
                        if (rcv_opcode == OPCODE_ACK && ack_block == current_block)
                        {
                              // If we just acknowledged a block that was less than 512 bytes, we are officially done!
                              if (current_block > 0 && bytes_read < BLIMIT) 
                              {
                                    printf("Upload successful.\n");
                                    break;
                              }

                              // Move to the next block
                              current_block++; 
                              
                              // Read the next chunk of the file
                              bytes_read = fread(file_data, 1, BLIMIT, fp);

                              // Form the new DATA packet directly into our last_packet buffer
                              last_packet_len = packet_form_data(last_packet, current_block, file_data, bytes_read);

                              // Send the new DATA packet
                              sendto(sockfd, last_packet, last_packet_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                              printf("Sent block %d (%d bytes)\n", current_block, bytes_read);
                        }
                  }
                  
                  if (fp != NULL) fclose(fp);
            }