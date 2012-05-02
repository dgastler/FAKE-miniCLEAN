#ifndef __CPACKET__
#define __CPACKET__
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>

//Note!  all of this is stored in network byte order, so if you want to modify ANYTHING, first call ntohl() to get the word and then htonl() to return it to the cpacket!!!!

typedef struct
{
  uint32_t writeCommand;   //lowest 8 bits should be 0x20 for write
                           //bits 8 and above are write size
                           //i.e. 0x100 (256 time samples)
  uint32_t memBlockAddress;//location in the FPGA memory where this 
                           //event will be saved. 
  uint32_t eventTimeStamp; //based on a 1Mhz clock
  uint32_t promptBitTwoPattern[5]; //The MSB patterns for the prompt region
  uint32_t promptBitOnePattern[5];   //The MidSB patterns for the prompt region
  uint32_t bitZeroPattern[245];      //the lsb pattern for 5+240 clock ticks
}  cpacket1;
typedef struct
{
  uint32_t writeCommand;   //lowest 8 bits should be 0x20 for write
                           //bits 8 and above are write size
                           //i.e. 0x100 (256 time samples)
  uint32_t memBlockAddress;//location in the FPGA memory where this 
                           //event will be sent. 
  uint32_t bitZeroPattern[256];  //the last 256 lsb patterns for this board
} cpacket2;

int SendPacket1(int sockFD,struct sockaddr_in * addr,cpacket1 * packet, uint8_t board, FILE * debug_info, uint8_t write_parity);
int SendPacket2(int sockFD,struct sockaddr_in * addr,cpacket2 * packet, uint8_t board);
int SendStartCommand(int sockFD,struct sockaddr_in * addr);
int SendStopCommand (int sockFD,struct sockaddr_in * addr);
int SendReadCommand(int sockFD, struct sockaddr_in * addr);
int SetPatienceMask(int sockFD, struct sockaddr_in * addr);
#endif
