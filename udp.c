/*############################################################################################### 
 *
 *           Author         : 
 *           Date           : 
 *           File           : udp.c©
 *           Descriptions   : Functions to bind udp sockets for server and clients.
 *           Objective      : Create a UDP socket and bind with given IP address and PORT number
 *           Usage          :
 *           Output         :
 *
###############################################################################################*/

#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tftp.h"

/* To create sockets and binding for server and clients*/

/* Create and bind a UDP socket (Used mainly by the server)*/
int create_bind_udp_socket(int port)
{
      int sockfd;
      struct sockaddr_in server_addr;

      //Create a UDP socket
      if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
      }

      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = INADDR_ANY;
      server_addr.sin_port = htons(port);

      // Bind the socket to the port
      if( bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
      {
            perror("Bind failed");
            close(sockfd);
            exit(EXIT_FAILURE);
      }

      return sockfd;
}