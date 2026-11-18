/*############################################################################################### 
 *
 *           Author         : Kiruthik Kumaran K
 *           Date           : November 2026
 *           File           : tftp.c
 *           Description    : TFTP (RFC 1350) packet serialization utilities
 *           Objective      : Construct properly formatted RRQ, WRQ, DATA, ACK, and ERROR
 *                            packets in Network Byte Order for UDP transmission.
 *           Usage          : Compiled and linked with both client.c and server.c
 *           Output         : Populated byte buffers ready for sendto()
 *
###############################################################################################*/

#include <string.h>     // memcpy, strcpy, strlen
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h> // htons() for Host-to-Network Byte Order conversion
#include "tftp.h"       // TFTP opcode macros and function prototypes

/*
 * TFTP Protocol Packet Builders (RFC 1350)
 * Shared by both client and server to serialize control and data packets.
 * All 2-byte integer fields (Opcodes, Block Numbers, Error Codes) are converted
 * to Network Byte Order (Big-Endian) using htons() before copying into the buffer.
 */

// =======================================================================
// RRQ (Read Request) Packet Builder
// Format: | Opcode (2B) | Filename (string) | 0 (1B) | Mode (string) | 0 (1B) |
// =======================================================================
int packet_form_rrq(char *buffer, const char *filename, const char *mode)
{
      int len = 0;

      // 1. Write 2-byte Opcode for RRQ (1) in Network Byte Order
      short opcode = htons(OPCODE_RRQ);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Append null-terminated filename string (+1 accounts for the '\0' byte)
      strcpy(buffer + len, filename);
      len += strlen(filename) + 1;

      // 3. Append null-terminated transfer mode ("octet" or "netascii")
      strcpy(buffer + len, mode);
      len += strlen(mode) + 1;

      return len; // Return total packet length in bytes
}

// =======================================================================
// DATA Packet Builder
// Format: | Opcode (2B) | Block # (2B) | Data (0 to 512 bytes) |
// =======================================================================
int packet_form_data(char *buffer, unsigned short block_num, const char *data, int data_len)
{
      int len = 0;

      // 1. Write 2-byte Opcode for DATA (3) in Network Byte Order
      short opcode = htons(OPCODE_DATA);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Write 2-byte sequential Block Number (1-indexed) in Network Byte Order
      short bnum = htons(block_num);
      memcpy(buffer + len, &bnum, 2);
      len += 2;

      // 3. Copy raw binary file payload immediately after the 4-byte header
      memcpy(buffer + len, data, data_len);
      len += data_len;

      return len; // Return total packet length (4-byte header + payload)
}

// =======================================================================
// ACK (Acknowledgment) Packet Builder
// Format: | Opcode (2B) | Block # (2B) |
// =======================================================================
int packet_form_ack(char *buffer, unsigned short block_num)
{
      int len = 0;

      // 1. Write 2-byte Opcode for ACK (4) in Network Byte Order
      short opcode = htons(OPCODE_ACK);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Write 2-byte Block Number being acknowledged (0 for WRQ, 1+ for DATA)
      short bnum = htons(block_num);
      memcpy(buffer + len, &bnum, 2);
      len += 2;
      
      return len; // ACK packets are always fixed at 4 bytes
}

// =======================================================================
// WRQ (Write Request) Packet Builder
// Format: | Opcode (2B) | Filename (string) | 0 (1B) | Mode (string) | 0 (1B) |
// =======================================================================
int packet_form_wrq(char *buffer, const char *filename, const char *mode)
{
      int len = 0;

      // 1. Write 2-byte Opcode for WRQ (2) in Network Byte Order
      short opcode = htons(OPCODE_WRQ);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Append null-terminated filename string
      strcpy(buffer + len, filename);
      len += strlen(filename) + 1;

      // 3. Append null-terminated transfer mode ("octet" or "netascii")
      strcpy(buffer + len, mode);
      len += strlen(mode) + 1;

      return len; // Return total packet length in bytes
}

// =======================================================================
// ERROR Packet Builder
// Format: | Opcode (2B) | ErrorCode (2B) | ErrMsg (string) | 0 (1B) |
// =======================================================================
int packet_form_error(char *buffer, unsigned short err_code, const char *err_msg)
{
      int len = 0;

      // 1. Write 2-byte Opcode for ERROR (5) in Network Byte Order
      short opcode = htons(OPCODE_ERROR);
      memcpy(buffer, &opcode, 2);
      len += 2;

      // 2. Write 2-byte RFC 1350 Error Code (e.g., 1 = Not Found, 6 = Already Exists)
      short ecode = htons(err_code);
      memcpy(buffer + len, &ecode, 2);
      len += 2;

      // 3. Append human-readable null-terminated error description
      strcpy(buffer + len, err_msg);
      len += strlen(err_msg) + 1;

      return len; // Return total packet length in bytes
}