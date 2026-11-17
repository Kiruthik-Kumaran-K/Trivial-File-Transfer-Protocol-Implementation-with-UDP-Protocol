#ifndef TFTP
#define TFTP

#define BLIMIT      512 /*Maximum amount of file contents that can be buffered*/
#define PACKETSIZE  516 /*Total size of TFTP packet*/

typedef unsigned short opcode_t; /*TFTP Operation Code*/
#define OPCODE_RRQ   1
#define IS_RRQ(op)   ((op) == OPCODE_RRQ) /*Evaluates as true or false*/
#define OPCODE_WRQ   2
#define IS_WRQ(op)   ((op) == OPCODE_WRQ)
#define OPCODE_DATA  3
#define IS_DATA(op)  ((op) == OPCODE_DATA)
#define OPCODE_ACK   4
#define IS_ACK(op)   ((op) == OPCODE_ACK)
#define OPCODE_ERROR 5
#define IS_ERROR(op) ((op) == OPCODE_ERROR)

typedef unsigned short ecode_t; /*TFTP Error code*/
#define ECODE_NONE  8
#define ECODE_0     0
#define IS_ECODE_0(ec)  ((ec) == ECODE_0)
#define ESTRING_0   "Not defined, see error message(if any)."
#define ECODE_1     1
#define IS_ECODE_1(ec)  ((ec) == ECODE_1)
#define ESTRING_1   "File not found."
#define ECODE_2     2
#define IS_ECODE_2(ec)  ((ec) == ECODE_2)
#define ESTRING_2   "Access violation."
#define ECODE_3     3
#define IS_ECODE_3(ec)  ((ec) == ECODE_3)
#define ESTRING_3   "Disk full or allocation exceeded."
#define ECODE_4     4
#define IS_ECODE_4(ec)  ((ec) == ECODE_4)
#define ESTRING_4   "Illegal TFTP operation."
#define ECODE_5     5
#define IS_ECODE_5(ec)  ((ec) == ECODE_5)
#define ESTRING_5   "Unknown transfer ID."
#define ECODE_6     6
#define IS_ECODE_6(ec)  ((ec) == ECODE_6)
#define ECODE_7     7
#define IS_ECODE_7(ec)  ((ec) == ECODE_7)
#define ESTRING_7   "No such user."

/*Transfer operation modes*/
#define MODE_NETASCII "netascii" 
#define MODE_OCTET    "octet"
#define MODE_MAIL     "mail"

#define TIMEOUT         1 /*Socket receive timeout*/
#define TIMEOUT_LIMIT   10


int packet_form_rrq(char *buffer, const char *filename, const char *mode);
int packet_form_data(char *buffer, unsigned short block_num, const char *data, int data_len);
int packet_form_ack(char *buffer, unsigned short block_num);
int packet_form_wrq(char *buffer, const char *filename, const char *mode);
int packet_form_error(char *buffer, unsigned short err_code, const char *err_msg);

#endif
