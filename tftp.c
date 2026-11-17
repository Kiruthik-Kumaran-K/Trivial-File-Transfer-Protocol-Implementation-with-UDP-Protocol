/*############################################################################################### 
 *
 *           Author         : 
 *           Date           : 
 *           File           : tftp.c©
 *           Descriptions   : function to create/extract packets before/after sending/receving
 *           Objective      : Packet creation and extraction
 *           Usage          :
 *           Output         :
 *
###############################################################################################*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include "tftp.h"
#include "file.h"

/* tftp protocol implementation 
 * Both server and client must use this file for sen/receive data
 */

 // Formats an RRQ packet buffer
 // Opcode (2 Bytes) | Filename | 0 | Mode | 0
 int packet_form_rrq(char *buffer, const char *filename, const char *mode)
 {
      int len = 0;

      // 1. Add Opcode for RRQ (1)
      short opcode = htons(OPCODE_RRQ);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Add Filename + Null terminator
      strcpy(buffer + len, filename);
      len += strlen(filename) + 1;

      // 3. Add Mode ('netascii" or "octet") + Null terminator
      strcpy(buffer + len, mode);
      len += strlen(mode) + 1;

      return len; // Return total packet size
 }

// Formats a DATA packets
 int packet_form_data(char *buffer, unsigned short block_num, const char *data, int data_len)
 {
      int len = 0;

      // 1. Add Opcode for DATA (3)
      short opcode = htons(OPCODE_DATA);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Add Block NUmber
      short bnum = htons(block_num);
      memcpy(buffer + len, &bnum, 2);
      len += 2;

      // 3. Add the actual file data
      memcpy(buffer + len, data, data_len);
      len += data_len;

      return len;// Returns the total size of the packet to send
 }

 //Formats an ACK packet
 // Opcode (2 Bytes) | Block Number (2 bytes)
 int packet_form_ack(char *buffer, unsigned short block_num)
 {
      int len = 0;

      // 1. Add Opcode for ACK (4)
      short opcode = htons(OPCODE_ACK);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Add Block Number being Acknowledged
      short bnum = htons(block_num);
      memcpy(buffer + len, &bnum, 2);
      len += 2;
      
      return len; // ACK packets are always exactly 4 bytes lon
 }

 //Formats a WRQ packet buffer
 //Opcode (2) | Filename | 0 | Mode | 0
 int packet_form_wrq(char *buffer, const char *filename, const char *mode)
 {
      int len = 0;

      // Add Opcode for WRQ (2)
      short opcode = htons(OPCODE_WRQ);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // Add Filename + Null terminator
      strcpy(buffer + len, filename);
      len += strlen(filename) + 1;

      //Add Mode + Null terminator
      strcpy(buffer + len, mode);
      len += strlen(mode) + 1;

      return len;
 }

 //Formats an ERROR packet
 // Opcode (2 Bytes) | Error Code (2 Bytes) | ErrMSG } 0
 int packet_form_error(char *buffer, unsigned short err_code, const char *err_msg)
 {
      int len = 0;

      // Add Opcode for ERROR (5)
      short opcode = htons(OPCODE_ERROR);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // Add Error Code
      short ecode = htons(err_code);
      memcpy(buffer + len, &ecode, 2);
      len += 2;

      // Add Error message + Null terminator
      strcpy(buffer + len, err_msg);
      len += strlen(err_msg) + 1;

      return len;
 }