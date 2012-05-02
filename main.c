#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>

#include "cpacket.h"

volatile int run = 1;
const ssize_t fileCount =3;
const uint32_t blockRAMOffset=0x100;
const uint32_t lastValidBlockRAMaddr=0xC600;
FILE * debug_info[3];

void signal_handler(int sig)
{
  printf("Signal handler got signal %d\n",sig);
  if(sig == SIGINT)
    {      
      run = 0;
    }
}

double logN;
uint16_t lfsr[] = {0xACE1u,0xACE1u,0xACE1u};
//const double freq = 1000.0;
double freq;
double safePeriod;
double rconvert; //(1/frequency) converted to microseconds
double Compute_dt(int board)
 {
   //Compute next random number for this board
   uint32_t bit = ((lfsr[board] >> 0) ^ (lfsr[board] >> 2) ^ (lfsr[board] >> 3) ^ (lfsr[board] >> 5) ) & 1;
   lfsr[board] = (lfsr[board] >> 1) | (bit << 15);
   double dt = rconvert*(logN - log(lfsr[board]));
   return dt;
 }
uint32_t GetNextTime(uint32_t lastTime,int board)
{
  //double dt = 1E6/freq;
  double dt;  while( (dt=Compute_dt(board)) < safePeriod);
  return lastTime + dt;
}

typedef struct 
{
  ssize_t board;
  int sockFD;
  struct sockaddr_in boardAddr;
  cpacket1 ** packet1;
  cpacket2 ** packet2;
  uint32_t numberOfEvents;	  
  uint32_t lastTime;
} loopVars;

void * loop(void * arg)
{
  loopVars * vars = (loopVars *) arg;
  ssize_t board = vars->board;
  int sockFD = vars->sockFD;
  struct sockaddr_in boardAddr = vars->boardAddr;
  cpacket1 ** packet1 = vars->packet1;
  cpacket2 ** packet2 = vars->packet2;
  uint32_t numberOfEvents = vars->numberOfEvents;	  
  uint32_t lastTime = vars->lastTime;

  //Send halfway through the first loop
  uint32_t sendStartEvent = numberOfEvents >> 1;
  if(sendStartEvent > 50)
    sendStartEvent = 50;

  uint32_t blockRAMaddr = 0;
  uint32_t nextTime;
  uint32_t iEvent = 0;
  uint8_t upTo99 = 0;
  uint8_t write_parity = 0;
 
  int sendstart = 1;

  while(!run){usleep(1);};

  while(run)
    {
      //Update output time
      nextTime = GetNextTime(lastTime,board);
      packet1[iEvent][board].eventTimeStamp = htonl(nextTime);
      lastTime = nextTime;

      if(blockRAMaddr > lastValidBlockRAMaddr)
	blockRAMaddr = 0;
      //Update blockID;
      packet1[iEvent][board].memBlockAddress = htonl(blockRAMaddr);
      blockRAMaddr += blockRAMOffset;
      packet2[iEvent][board].memBlockAddress = htonl(blockRAMaddr);
      blockRAMaddr += blockRAMOffset;

      //Send packet1s      
      if(!SendPacket1(sockFD,
		      &(boardAddr), &(packet1[iEvent][board]), 
                      board, debug_info[board], write_parity))
	{
	  printf("Error sending to board %lu\n",board);
          break;
	}

      //Send packet2s
      if(!SendPacket2(sockFD,
		      &(boardAddr),
		      &(packet2[iEvent][board]), board))
	{
	  printf("Error sending to board %lu\n",board);
	  break;
	}	  
      iEvent++;
      if(iEvent >= numberOfEvents)
	iEvent = 0;

      upTo99 += 1;//debugging information
      if(upTo99 > 99)
      {
         upTo99 = 0;
         write_parity = !write_parity;
      }

      if(sendstart & (board == 1) && (iEvent == sendStartEvent))
	{
	  SendStartCommand(sockFD,&boardAddr);
	  sendstart = 0;
	}
    }
  return NULL;
}


int main(int argc, char ** argv)
{
  freq = 1000;
  if(argc < 4)
    {
      printf("usage: %s file1 file2 file3 [rate]",argv[0]);
    }
  if(argc > 4)
    {
      freq = atof(argv[4]);
    }
    
  rconvert = (1.0/freq) * 1E6;  //microseconds 
  safePeriod = 100;      //microseconds
  logN = log(0xFFFF);
  char * IPs[3];
  IPs[0] = "192.168.1.2";
  IPs[1] = "192.168.2.2";
  IPs[2] = "192.168.3.2";
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT,&sa,NULL);

  debug_info[0] = fopen("debug0.txt", "a");
  debug_info[1] = fopen("debug1.txt", "a");
  debug_info[2] = fopen("debug2.txt", "a");

  int errno;
  char * packetFile[fileCount];
  if(argc < (fileCount+1))
    {
      printf("%s file1 file2 file3\n",argv[0]);
      return -1;
    }
  else
    {
      int i = 0;
      for(i = 0; i<fileCount;i++)
	{
	  packetFile[i] = argv[1+i];
	}
    }


  //===============Load FAKE packets====================
  //find sizes of files and open them.
  struct stat fileStats[fileCount];
  int fileFD[fileCount];
  int iFile,iEvent;
  for(iFile = 0; iFile < fileCount;iFile++)
    {
      int statError = stat(packetFile[iFile],&(fileStats[iFile]));
      if(statError != 0)
	{
	  printf("ERROR: %s(file=%s)\n",strerror(errno),packetFile[iFile]);
	  return -1;
	}
      fileFD[iFile] = open(packetFile[iFile],O_RDONLY);
      if(fileFD[iFile] < 0)
	{
	  printf("ERROR: failed to open file %s\n",packetFile[iFile]);
	  return -1;
	}
    }
  //Check that the files are the same size (they always should be)
  assert((fileStats[0].st_size == fileStats[1].st_size) &&
	 (fileStats[0].st_size == fileStats[2].st_size));
  //Check that the files are an integer number of events in size
  assert((fileStats[0].st_size %(sizeof(cpacket1) + sizeof(cpacket2))) == 0);
  
  //Allocate the needed memory
  uint32_t numberOfEvents = fileStats[0].st_size/(sizeof(cpacket1) + sizeof(cpacket2));
  printf("Loaded %u events\n",numberOfEvents);
  cpacket1 ** packet1;
  cpacket2 ** packet2;
  packet1 = (cpacket1**) malloc(numberOfEvents*sizeof(cpacket1*));
  packet2 = (cpacket2**) malloc(numberOfEvents*sizeof(cpacket2*));
  if(packet1 == NULL)
    {
      printf("Error: Failed to allocate packet1[%d]\n",iFile);
      return -1;
    }
  if(packet2 == NULL)
    {
      printf("Eror: Failed to allocate packet2[%d]\n",iFile);
      return -1;
    }
  packet1[0] = malloc(numberOfEvents*sizeof(cpacket1)*fileCount);
  packet2[0] = malloc(numberOfEvents*sizeof(cpacket2)*fileCount);
  for(iEvent = 0; iEvent < numberOfEvents;iEvent++)
    {
      packet1[iEvent] = packet1[0] + iEvent*fileCount;
      packet2[iEvent] = packet2[0] + iEvent*fileCount;
    }


  for(iEvent = 0; iEvent < numberOfEvents;iEvent++)
    {
      for(iFile = 0; iFile < fileCount;iFile++)
	{
	  //Read packet 1
	  ssize_t readReturn = read(fileFD[iFile],
				    &(packet1[iEvent][iFile]),
				    sizeof(cpacket1));
	  if(readReturn != sizeof(cpacket1))
	    {
	      printf("ERROR: bad read on event %d's packet 1 in file %s\n",
		     iEvent,packetFile[iFile]);
	      return -1;
	    }
	  //Read packet 2
	  readReturn = read(fileFD[iFile],
			    &(packet2[iEvent][iFile]),
			    sizeof(cpacket2));
	  if(readReturn != sizeof(cpacket2))
	    {
	      printf("ERROR: bad read on event %d's packet 2 in file %s\n",
		     iEvent,packetFile[iFile]);
	      return -1;
	    }
	}
    }

  //===============Open Network Connections====================
  int sockFD[3];
  struct sockaddr_in * boardAddr;  
  boardAddr = malloc(fileCount*sizeof(struct sockaddr_in));
  bzero(boardAddr,sizeof(boardAddr));
  for(iFile = 0; iFile < fileCount;iFile++)
    {
      sockFD[iFile] = socket(AF_INET,SOCK_DGRAM,0);
      boardAddr[iFile].sin_family = AF_INET;
      boardAddr[iFile].sin_addr.s_addr=inet_addr(IPs[iFile]); //192.168.2.iFile
      boardAddr[iFile].sin_port = htons(791);
    }

  //===============Main loop====================
  uint32_t lastTime =1000000;
  sleep(1);
  SetPatienceMask(sockFD[0],&(boardAddr[0]));//alter time that vhdl can wait
  SetPatienceMask(sockFD[1],&(boardAddr[1]));
  SetPatienceMask(sockFD[2],&(boardAddr[2]));
  SendStartCommand(sockFD[1],&(boardAddr[1]));//Start-Stop-Start triggers restart
  SendStopCommand(sockFD[1],&(boardAddr[1]));
  sleep(1);
  loopVars var[3];
  for(iFile = 0; iFile < fileCount;iFile++)
    {      
      var[iFile].board = iFile;
      var[iFile].sockFD = sockFD[iFile];
      var[iFile].boardAddr = boardAddr[iFile];
      var[iFile].packet1 = packet1;
      var[iFile].packet2 = packet2;
      var[iFile].numberOfEvents = numberOfEvents;	  
      var[iFile].lastTime = lastTime;
    }
    
  //launch threads;
  pthread_t threadID[fileCount];  
  for(iFile = 0; iFile < fileCount;iFile++)
    {
      pthread_create(&threadID[iFile],
		     NULL,
		     loop,
		     &(var[iFile]));
    }  
  
  //start the run
  run = 1;
  //pthread join
  void * loopRet[3];
  SendReadCommand(sockFD[0],&(boardAddr[0]));
  for(iFile = 0; iFile < fileCount;iFile++)
    {
      pthread_join(threadID[iFile],&(loopRet[iFile]));
    }
  sleep(5);
  SendReadCommand(sockFD[0],&(boardAddr[0]));
  SendStopCommand(sockFD[1],&(boardAddr[1]));
  return 0;
}
