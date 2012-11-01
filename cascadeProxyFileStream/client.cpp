/*
 Connects a client to a server with a given hostname, server port.
*/

#include "header.h"

#define BUFFER_LEN (1024*10) 
#define SERVER_TIMEOUT  2              // Timeout for server response, seconds.
#define SERVER_TIMEOUT_INIT  30        // Timeout for server response, seconds.
char fileToBeDownloaded[50];
char hostname[50];
char serverPortNumberString[6];
void *
get_in_addr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    {
      return &(((struct sockaddr_in *) sa)->sin_addr);
    }


  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}


int readNecho (int sockfd, char *buf)
{
  string response;
  bool once_not_over = true;
  bool running = true;
  ofstream outFile;
  outFile.open("outputSample.txt",ofstream::binary);  
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
      memset (buf, 0, BUFFER_LEN * sizeof (buf[0]));
      int bytesRead;
      if ((bytesRead = read (sockfd, buf, chunkSize)) <= 0)
        {
          printf ("End reading.\n");
          break;
        }
      
      responseBuffer = string (buf, bytesRead);
      response += responseBuffer;
      if (outFile.is_open()){
      	outFile.write(buf, bytesRead);      	
      }else{
       cout << "Error - outfile closed" << endl;
      }
    }
    outFile.close();
 
  close (sockfd);
  return 1;
}

int clientStub (){
  string message = fileToBeDownloaded;
  cout << "I am inside ClientStub." << endl;
  int serverFd, numbytes;
  char buf[BUFFER_LEN], serverPortStr[10];

  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM; //The socket-connection is of type TCP 

  // Resolving the address of the server - DNS Resolution
  cout << serverPortNumberString << endl;
  if ((rv = getaddrinfo ((const char*)hostname, serverPortNumberString, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return -1;
    }
  cout << "After resolving host name" << endl;

//   loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      cout << "Looping to resolve host name." << endl;
      if ((serverFd = socket (p->ai_family, p->ai_socktype,
                               p->ai_protocol)) == -1)
        {
          perror ("Client: socket error");
          continue;
        }
      int x = fcntl (serverFd, F_GETFL, 0);	        // Get socket flags
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

  cout << "Above inet_top." << endl;
  inet_ntop (p->ai_family, get_in_addr ((struct sockaddr *) p->ai_addr),
             s, sizeof s);
  printf ("Client: connecting to %s\t on socket %d\n", s, serverFd);
  freeaddrinfo (servinfo);        // all done with this structure


  int numbytesSent =
    send (serverFd, message.c_str (), message.length (), 0);
  if (numbytesSent == -1)
    {
      perror ("Send: ");
      printf ("Error In send\n");
      close (serverFd);
      pthread_exit (0);
    }

  //  Receive HTTP response from the web server
  cout << "Request Sent To The Server : Request Length " << numbytesSent
    << " : socket FD " << serverFd << endl;

  return readNecho (serverFd, buf);        //  reads all the available data from the socket
}

int main (int argc, char *argv[]){
  memset (fileToBeDownloaded, 0, 50);
  memset (hostname, 0, 50);
  memset(serverPortNumberString, 0, 6);
  ifstream inFile(CONFIG_FILE);
  if (inFile.is_open()){
  	inFile.getline (fileToBeDownloaded,50);
	inFile.getline(hostname, 50, ' ');
        inFile.getline(serverPortNumberString, 50);
 	
  }
  inFile.close();
  return (clientStub());  
}
