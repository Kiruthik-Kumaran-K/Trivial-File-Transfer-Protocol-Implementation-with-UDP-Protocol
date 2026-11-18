/*############################################################################################### 
 *
 *           Author         : Kiruthik Kumaran k
 *           Date           : November 2026
 *           File           : udp.c
 *           Description    : UDP socket initialization and port-binding utilities
 *           Objective      : Create an IPv4 UDP socket and bind it to a specified local port
 *                            so the TFTP server can listen for incoming datagrams.
 *           Usage          : Compiled and linked with server.c (via udp.h)
 *           Output         : Valid bound socket file descriptor, or process exit on failure
 *
###############################################################################################*/

#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY, htons()
#include <stdio.h>      // perror() for system error reporting
#include <errno.h>      // System error codes
#include <stdlib.h>     // exit(), EXIT_FAILURE
#include <string.h>     // memset()
#include <sys/socket.h> // socket(), bind(), AF_INET, SOCK_DGRAM
#include <arpa/inet.h>  // Internet address manipulation utilities
#include <unistd.h>     // close() for releasing socket descriptors on error
#include "tftp.h"       // Shared TFTP definitions

// =======================================================================
// UDP Socket Creation & Binding (Primarily used by the TFTP Server)
// =======================================================================
int create_bind_udp_socket(int port)
{
      int sockfd;
      struct sockaddr_in server_addr;

      // 1. Create an IPv4 (AF_INET), connectionless UDP (SOCK_DGRAM) socket
      if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
      }

      // 2. Zero-initialize the address structure to clear padding bytes (sin_zero)
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;                // IPv4 address family
      server_addr.sin_addr.s_addr = INADDR_ANY;        // Listen on all available network interfaces (0.0.0.0)
      server_addr.sin_port = htons(port);              // Convert port to Network Byte Order (Big-Endian)

      // 3. Bind the socket to the configured IP and port so clients can reach the server
      if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
      {
            perror("Bind failed");
            close(sockfd); // Release the socket descriptor before terminating
            exit(EXIT_FAILURE);
      }

      // Return the ready-to-use bound socket file descriptor
      return sockfd;
}