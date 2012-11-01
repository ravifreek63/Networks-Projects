#ifndef __HEADER_H__
#define __HEADER_H__

#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <list>
int chunkSize = 2;

using namespace std;

struct args {
  int clientfd;
  FILE *logfilefd;
  char clientsAddr[INET6_ADDRSTRLEN];
};

struct readWriteArgs {
  int *readCounter;
  int *writeCounter;
  char *buf; 
  int clientFd;
  int serverFd;
  int *readFlag;
  int *writeFlag;
  sem_t *readWriteSemaphore;
  int *writeCount;
};

void * echoResponse (void * readWriteArgument);

#define CONFIG_FILE "PROXY.IN"
#endif

