#include "cpacket.h"

volatile extern int run;
static volatile int32_t last_time[3];
char line_buffer[3][255] = {"", "", ""};
uint32_t success_count[3] = {0, 0, 0};
volatile uint32_t total_sent[3] = {0, 0, 0};

int SendPacket1(int sockFD,struct sockaddr_in * addr,cpacket1 * packet, const uint8_t board, FILE * debug_info, uint8_t write_parity)
{
   uint8_t reply[100];
   uint8_t waddress;
   int32_t time;//, time_p, time_f;
   bzero(reply,sizeof(reply));
   while(1)
   {
      if(run)
      {	  //Send packet
	  sendto(sockFD,packet,sizeof(cpacket1),0,
		 (struct sockaddr *)addr,sizeof(struct sockaddr));      
	  recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
          total_sent[board] += 1;      
      }
      else
      {
         fprintf(debug_info, "End of Run\n");
         fclose(debug_info);
         return 0;
      }      
      //      usleep(1);
      if(reply[3] == 0x24)
      {
         success_count[board] += 1;
	 break;
      }

      //debugging information
      waddress = (ntohl(packet->memBlockAddress)) >> 9;
      time = (((uint32_t)reply[4]&3)<<24)|(((uint32_t)reply[5])<<16)|
         (((uint32_t)reply[6])<<8)|((uint32_t)reply[7]);
      //time_p = (((uint32_t)reply[8]&3)<<24)|(((uint32_t)reply[9])<<16)|
      //   (((uint32_t)reply[10])<<8)|((uint32_t)reply[11]);
      //time_f = (((uint32_t)reply[12]&3)<<24)|(((uint32_t)reply[13])<<16)|
      //   (((uint32_t)reply[14])<<8)|((uint32_t)reply[15]);
      if(reply[4]&0x04)
         time |= 0xFC000000;
      //if(reply[8]&0x04)
      //    time_p |= 0xFC000000;
      //if(reply[12]&0x04)
      //   time_f |= 0xFC000000;
      
      //      /*Dan 2012-04-27
      if(((last_time[board]<=0)&&(time>0))||((last_time[board]>0)&&(time<=0))||
         (!(reply[4]>>7))||(((waddress&0x7)<<4) != (reply[4]&0x70)))
      {
         fprintf(debug_info, line_buffer[board]);
         fflush(debug_info);
	 printf("yikes %d\n", board);
	 run = 0;
      }
      //      */

      sprintf(line_buffer[board], "board: %d, conf: %d, W: %d%d%d%d%d%d%d, P: %d, S: %d, R: %d%d%d, P: %d, time: %d, last time: %d, packet: %d\n", board,
          reply[4]>>7,!!(waddress&0x40),!!(waddress&0x20),!!(waddress&0x10),
          !!(waddress&0x08),!!(waddress&0x04),!!(waddress&0x02),
          !!(waddress&0x01),write_parity,success_count[board], 
          !!(reply[4]&0x40),!!(reply[4]&0x20),!!(reply[4]&0x10),
          !!(reply[4]&0x08),time,last_time[board],total_sent[board]<<1);

      if(((last_time[board]<=0)&&(time>0))||((last_time[board]>0)&&(time<=0))||
            (!(reply[4]>>7))||(((waddress&0x7)<<4) != (reply[4]&0x70)))
      {
         fprintf(debug_info, line_buffer[board]);
         fprintf(debug_info, "\n");
	 fflush(debug_info);
      }
      last_time[board] = time;
      success_count[board] = 0;
   }
   return 1;
}

int SendPacket2(int sockFD,struct sockaddr_in * addr,cpacket2 * packet, uint8_t board)
{
  uint8_t reply[100];
  bzero(reply,sizeof(reply));
  do
    {
      if(run)
	{	  //Send packet
	  sendto(sockFD,packet,sizeof(cpacket2),0,
		 (struct sockaddr *)addr,sizeof(struct sockaddr));      
	  recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
          total_sent[board] += 1;    
	}
      else
	return 0;      
      //      usleep(1);
      //    }while(run);
    } while((reply[3]) != 0x24);
  return 1;
}

int SendStartCommand(int sockFD,struct sockaddr_in * addr)
{
  uint32_t command[3];
  uint8_t reply[100];
  command[0] = htonl(0x00000120);//write one value 
                                 //0x20 is write command
                                 //0x******XX is the write size in bytes
  command[1] = htonl(0xFFFFFFFF);//start run register??
  command[2] = htonl(0x00000539);//non-zero value turns output on
  //Send command
  sendto(sockFD,command,sizeof(command),0,
	 (struct sockaddr *)addr,sizeof(struct sockaddr));
  //Wait for response.
  recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
  printf("Sending start\n");
  //use response at some point to return somethign smarter than "1"
  return 1;
}

int SendStopCommand(int sockFD, struct sockaddr_in * addr)
{
  uint32_t command[3];
  uint8_t reply[100];
  command[0] = htonl(0x00000120);//write one value 
                                 //0x20 is write command
                                 //0x******XX is the write size in bytes
  command[1] = htonl(0xFFFFFFFF);//start run register??
  command[2] = htonl(0x00000000);//zero value turns output off
  //Send command
  sendto(sockFD,command,sizeof(command),0,
	 (struct sockaddr *)addr,sizeof(struct sockaddr));
  //Wait for response.
  recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
  //use response at some point to return something smarter than "1"
  printf("Sending stop\n");
  return 1;
}

int SendReadCommand(int sockFD, struct sockaddr_in * addr)
{
   uint32_t command[2];
   uint8_t reply[100];
   command[0] = htonl(0x00000118);//read one value
   command[1] = htonl(0x00000539);//from any register
   //Send command
   sendto(sockFD,command,sizeof(command),0,
         (struct sockaddr *)addr,sizeof(struct sockaddr));
   //Wait for response
   recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
   printf("check 0x0000011C: %x\n", ntohl(*((uint32_t*)(&reply[0]))));
   printf("fail count: %d\n", ntohl(*((uint32_t*)(&reply[4]))));
   return 1;
}

int SetPatienceMask(int sockFD, struct sockaddr_in * addr)
{
   uint32_t command[3];
   uint8_t reply[100];
   command[0] = htonl(0x00000120);//write one value
   command[1] = htonl(0x7FFFFFFF);//patience register
   command[2] = htonl(0xF0000000);//time that vhdl is willing to wait 
      //to send an event. 0 is complete patience, 0xFC000000 is the default
      //value of 1 minute, 0xFFFFFFFF is complete impatience
   //Send command
   sendto(sockFD,command,sizeof(command),0,
	 (struct sockaddr *)addr,sizeof(struct sockaddr));
   //Wait for response.
   recvfrom(sockFD,reply,sizeof(reply),0,NULL,NULL);
   printf("You have altered my recklessness.\n");
   return 1;
}
