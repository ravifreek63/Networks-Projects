#include "header.h"

#define PORT "5000"                // the port users will be connecting to.
#define BUFFER_LEN (1024*10)        // max size of a browser request.
#define BACKLOG 500                // how many pending connections queue will hold.
#define MAXLINE 1024                // array size containing uri info.
#define uDELAY  0                // microsecond delay for the select call.
#define CLIENT_TIMEOUT        30        // Timeout for client response, seconds.
#define SERVER_TIMEOUT  2        // Timeout for server response, seconds.
#define SERVER_TIMEOUT_INIT  30        // Timeout for server response, seconds.
#define PROXY_SLEEPS (500000000L/(1024*1024*10))        // Proxy sleeps for this time after dispatching a thread.
#define MAX_OUTSTANDING 7        // Limits the maximum no. of outstanding client connection with proxy server.


int serverPort;
char serverPortNumberString[6];
char hostname[50];

int num_connections, num_disconnections;
void * get_in_addr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    {
      return &(((struct sockaddr_in *) sa)->sin_addr);
    }


  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}
/*
Function that establishes connection between the client and the server. It maintains synchronization between the two worker threads.
*/

int readNecho (int sockfd, char *buf, int clientFd, string clientaddr)
{
  cout << "I am inside readNecho." << endl;
  cout << "sockfd: " << sockfd << endl;
  cout << "clientFd: " << clientFd << endl;
  bool once_not_over = true;
  bool running = true;
  double elapsed_time_total = 0.0;
  int readPosition=0;
  int writeCounter=0;
  int readCounter=0;
  int readFlag=1;
  int writeFlag=1;
  int writeCount=0;
  sem_t readWriteSemaphore;
  sem_init(&readWriteSemaphore, 0, 1);
  
  pthread_t thread_id_readSend;
  struct readWriteArgs readWriteArguments;
  readWriteArguments.buf = buf;
  readWriteArguments.clientFd = clientFd;
  readWriteArguments.readCounter = &(readCounter);
  readWriteArguments.writeCounter = &(writeCounter); 
  readWriteArguments.readFlag=&(readFlag);
  readWriteArguments.writeFlag=&(writeFlag);
  readWriteArguments.readWriteSemaphore = &(readWriteSemaphore);
  readWriteArguments.writeCount = &(writeCount);
  

  pthread_create (&thread_id_readSend, NULL, echoResponse, &(readWriteArguments)); 
 
  memset (buf, 0, BUFFER_LEN * sizeof (buf[0]));
  
  while (running)                // loop for select 
    {
      struct timeval tv;
      fd_set readfds;
      if (once_not_over)
        {
          tv.tv_sec = SERVER_TIMEOUT_INIT;
          once_not_over = false;
        }
      else
        {
          tv.tv_sec = SERVER_TIMEOUT;
        }
      tv.tv_usec = 0;
      string responseBuffer = "";
      int offset = 0, numbytes = 0, retval;
      FD_ZERO (&readfds);
      FD_SET (sockfd, &readfds);
      retval = select ((sockfd + 1) * 500, &readfds, NULL, NULL, &tv);
      if (retval < 0)
        {
          fprintf (stderr, "Error In select < 0\n");
          perror ("select: read&echo: ");
          continue;
        }                        
      
      int bytesRead;
      sem_wait(&(readWriteSemaphore));
      readPosition = (writeCounter * chunkSize)  % (BUFFER_LEN); 
      if ((bytesRead = read (sockfd, buf + readPosition, chunkSize)) <=0) 
        {
	  sem_post(&(readWriteSemaphore));
          break;
        }
      else{
           writeCounter++;
           writeCount = writeCount + bytesRead;
       }
     sem_post(&(readWriteSemaphore));
  }
     
  readFlag = 0;
  while(writeFlag);
  shutdown (clientFd, SHUT_RDWR);
  close (sockfd);
  return 1;
}

/*
Reads data from the common buffer and sends the response back to the client.
*/
void * echoResponse (void * readWriteArgument){
 int positionToRead;
 char *sharedBuffer = ((struct readWriteArgs *) readWriteArgument)->buf;
 int clientFd = ((struct readWriteArgs *) readWriteArgument)->clientFd;
 int numBytesSent;
 int *readFlag = ((struct readWriteArgs *) readWriteArgument)->readFlag;
 int *writeFlag = ((struct readWriteArgs *) readWriteArgument)->writeFlag;
 sem_t *addrReadWriteSemaphore = ((struct readWriteArgs *) readWriteArgument)->readWriteSemaphore;
 int *readCounter = ((struct readWriteArgs *) readWriteArgument)->readCounter;
 int *writeCounter = ((struct readWriteArgs *) readWriteArgument)->writeCounter;
 int *writeCount = ((struct readWriteArgs *) readWriteArgument)->writeCount;
 int bytesToSend=0;
 while(true){

 	sem_wait(addrReadWriteSemaphore);
	
 	while (*writeCounter > *readCounter){
	  positionToRead = (*readCounter * chunkSize) % (BUFFER_LEN);
          bytesToSend = *writeCount - (*readCounter * chunkSize);
          if (bytesToSend > chunkSize)
		bytesToSend = chunkSize;
          
          numBytesSent = send(clientFd, sharedBuffer+positionToRead, bytesToSend, 0);
	  if (numBytesSent < 0){

		perror ("Error in Send");
		break;
		}
          else  {			
	  		*readCounter = *readCounter +1;
		}
	
 	}
 	sem_post(addrReadWriteSemaphore); 
	if(*readFlag == 0 && (*writeCounter == *readCounter)){
                *writeFlag = 0;
		break;
        }

}
}
/*
 Function that connects the proxy server to the next server 
*/

int ProxyAsClient (char *hostname, int serverPort, string proxyReq, int clientFd, string clientaddr)
{

  int serverFd, numbytes;
  char buf[BUFFER_LEN], serverPortStr[10];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  sprintf (serverPortStr, "%d", serverPort);
  if ((rv = getaddrinfo (hostname, serverPortNumberString, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return -1;
    }
    // loop through all the results to find the number of results obtained
    int desired_maximum = 0;

  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      cout << "Looping to resolve host name." << endl;
      if ((serverFd = socket (p->ai_family, p->ai_socktype,
                               p->ai_protocol)) == -1)
        {
          perror ("Proxy client: socket");
          continue;
        }
      int x = fcntl (serverFd, F_GETFL, 0);        // Get socket flags
      fcntl (serverFd, F_SETFL, x | O_NONBLOCK);        // Add non-blocking flag

      fd_set myset;
      struct timeval tv;
      int valopt;
      socklen_t lon;
      int res;
      bool has_timed_out = false;
      // Connecting on a non-blocking socket
      if (res = connect (serverFd, p->ai_addr, p->ai_addrlen) < 0)
        {
          if (errno == EINPROGRESS)
            {
              fprintf (stderr, "EINPROGRESS in connect() - selecting\n");
              do
                {
                  tv.tv_sec = 300;
                  tv.tv_usec = 0;
                  FD_ZERO (&myset);
                  FD_SET (serverFd, &myset);
                  res = select (serverFd + 1, NULL, &myset, NULL, &tv);
                  if (res < 0 && errno != EINTR)
                    {
                      fprintf (stderr, "Error connecting %d - %s\n", errno,
                               strerror (errno));
                      exit (0);
                    }
                  else if (res > 0)
                    {
                      // Socket selected for write 
                      lon = sizeof (int);
                      if (getsockopt
                          (serverFd, SOL_SOCKET, SO_ERROR,
                           (void *) (&valopt), &lon) < 0)
                        {
                          fprintf (stderr, "Error in getsockopt() %d - %s\n",
                                   errno, strerror (errno));
                          exit (0);
                        }
                      // Check the value returned... 
                      if (valopt)
                        {
                          fprintf (stderr,
                                   "Error in delayed connection() %d - %s\n",
                                   valopt, strerror (valopt));
                          exit (0);
                        }
                      break;
                    }
                  else
                    {
                      fprintf (stderr, "Timeout in select() - Cancelling!\n");
                      close (serverFd);
                      has_timed_out = true;
                      break;
                    }
                }
              while (1);
            }
          else
            {
              fprintf (stderr, "Error connecting %d - %s\n", errno,
                       strerror (errno));
              close (serverFd);
              perror ("Proxy client: connect");
              continue;
            }
        }
      if (has_timed_out) {
        continue;
      }
      break;
    }


  if (p == NULL)
    {
      fprintf (stderr, "Proxy client: failed to connect\n");
      return -2;
    }
  
  inet_ntop (p->ai_family, get_in_addr ((struct sockaddr *) p->ai_addr),
             s, sizeof s);
  printf ("Proxy client: connecting to %s\t on socket %d\n", s, serverFd);

  freeaddrinfo (servinfo);        // all done with this structure
// Sending the request to the server - contains the name of the file that has been requested by the client.
  int numbytesSent = send (serverFd, proxyReq.c_str (), proxyReq.length (), 0);
  if (numbytesSent == -1)
    {
      perror ("Send: ");
      printf ("Error In send\n");
      send (clientFd, "Server Error\r\n\r\n", 21, 0);
      close (clientFd);
      close (serverFd);
      pthread_exit (0);
    }

  //  Receive HTTP response from the web server
  cout << "Request Sent To The Web-Server : Request Length " << numbytesSent  << " : socket FD " << serverFd << endl;
  return readNecho (serverFd, buf, clientFd, clientaddr);        //  reads all the available data from the socket
}


void sigchld_handler (int s)
{
  while (waitpid (-1, NULL, WNOHANG) > 0);
}

/*
 Function that reads data from the server. It spawns a new thread that sends data to the client or the next proxy server.
*/
void * WorksAsProxy (void *argStruct)
{
  // Copy the arguments in thread's local variables
  int clientFd = ((struct args *) (argStruct))->clientfd;
  FILE *log_fd = ((struct args *) (argStruct))->logfilefd;
  string clientaddr = string (((struct args *) (argStruct))->clientsAddr);
  char buff[BUFFER_LEN], buf[BUFFER_LEN];        // buff stores the browser's request
  int receivedBytes;
  fd_set readset;
  int retval;
  struct timeval timeout;
  // Set up for select, to read initial client request. 
  memset (&timeout, 0, sizeof (timeout));
  timeout.tv_sec = CLIENT_TIMEOUT;
  timeout.tv_usec = 0;
  FD_ZERO (&readset);
  FD_SET (clientFd, &readset);

  if ((retval =
       select (clientFd * 100 + 1, &readset, NULL, NULL, &timeout)) < 0)
    {
      fprintf (stderr, "Error on select: %s\n", strerror (errno));
      send (clientFd, "Server Error\r\n\r\n", 21, 0);
      close (clientFd);        //  destroy the client connection after serving it
      pthread_exit (0);
    }
  else if (retval == 0)
    {
      fprintf (stderr, "Client not responding, disconnecting\n");
      send (clientFd, "HTTP/1.0 404 Server Error\r\n\r\n", 29, 0);
      close (clientFd);        //  destroy the client connection after serving it
      pthread_exit (0);
    }


  // Here the proxy accepts client's request
  memset (buff, 0, BUFFER_LEN);
  if ((receivedBytes = recv (clientFd, buff, BUFFER_LEN - 1, 0)) < 0)
    {
      send (clientFd, "Server Error\r\n\r\n", 29, 0);
      close (clientFd);        //  destroy the client connection after serving it
      perror ("Recv_WorkAsProxy: ");
      pthread_exit (0);
    }
  else
    {
      buff[receivedBytes] = '\0';
      cout << "Proxy Server Received Request From Client On Socket : " <<  clientFd << endl;
      int responseSize;
      string clientMessage = buff;
      if ((responseSize = ProxyAsClient (hostname, serverPort, clientMessage, clientFd, clientaddr)) < 0){
		              send (clientFd, "Server Error\r\n\r\n", 21, 0);
		              close (clientFd);
		              printf ("Error In ProxyAsClient\n");		              
		              pthread_exit (0);
     }
     }   
          
  close (clientFd);                //  destroy the client connection after serving it
  pthread_exit (0);
}


int
sleepnano ()
{
  struct timespec tim, tim2;
  tim.tv_sec = 0;
  tim.tv_nsec = PROXY_SLEEPS;


  if (nanosleep (&tim, &tim2) < 0)
    {
      printf ("Nano sleep system call failed \n");
      return -1;
    }


  printf ("Nano sleep successfull \n");


  return 0;
}

// Main function
int main (int argc, char *argv[])
{
  char proxyPortNumber[6];
  memset(hostname, 0, 50);
  if (argc > 1) {
  	strcpy(proxyPortNumber, argv[1]);
  }
  if (argc > 2){
      strcpy(hostname, argv[2]);
  }
  if (argc >3 ){
  	strcpy(serverPortNumberString, argv[3]);
  }
  
  num_connections = 0;
  num_disconnections = 0;
  string logString;                //  Contains a proxy log entry


  //  register signal handler to ignore SIGPIPE
  if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
    {
      printf ("Unable to register SIG_IGN to SIGPIPE. Ending...");
      return -1;
    }

  int proxyFd, clientFd;        // listen on sock_fd, new connection on clientFd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;        // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;        

  if ((rv = getaddrinfo (NULL, proxyPortNumber, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return 1;
    }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      if ((proxyFd = socket (p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
          perror ("server: socket");
          continue;
        }


      if (setsockopt (proxyFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                      sizeof (int)) == -1)
        {
          perror ("setsockopt");
          exit (1);
        }

      if (bind (proxyFd, p->ai_addr, p->ai_addrlen) == -1)
        {
          close (proxyFd);
          perror ("server: bind");
          continue;
        }
      break;
    }

  if (p == NULL)
    {
      fprintf (stderr, "server: failed to bind\n");
      return 2;
    }

  freeaddrinfo (servinfo);        // all done with this structure

  if (listen (proxyFd, BACKLOG) == -1)
    {
      perror ("listen");
      exit (1);
    }
  printf ("server: waiting for connections...\n");


  while (1)
    {                                // main accept() loop
      sin_size = sizeof (their_addr);
      clientFd =
        accept (proxyFd, (struct sockaddr *) &their_addr, &sin_size);
      if (clientFd == -1)
        {
          perror ("accept");
          continue;
        }

      inet_ntop (their_addr.ss_family,
                 get_in_addr ((struct sockaddr *) &their_addr), s, sizeof s);
      printf ("server: got connection from %s\t File Desciptor = %d\n", s,
              clientFd);


      struct args threadArgs;
      threadArgs.clientfd = clientFd;
      strcpy (threadArgs.clientsAddr, s);

       if (1)
        {
          pthread_t thread_id;
          if (pthread_create // Thread created, where the proxy server receives data from the next server
              (&thread_id, NULL, WorksAsProxy, (void *) &(threadArgs)) != 0)
            {
              perror ("Server: pthread_create");
              send (clientFd, "Server Error\r\n\r\n", 21, 0);
              close (clientFd);
            }
        }
      sleepnano ();
    }
  return 0;
}
