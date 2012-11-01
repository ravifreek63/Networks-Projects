#include "header.h"
#define PORT "3000"
#define BUFFER_LEN (1024*10) 
#define BACKLOG 500
#define CLIENT_TIMEOUT        30 

void *
get_in_addr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    {
      return &(((struct sockaddr_in *) sa)->sin_addr);
    }


  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int main (int argc, char*argv[]){

char serverPortNumber[6];
if (argc > 1)
  strcpy(serverPortNumber, argv[1]);
else
  strcpy(serverPortNumber, PORT);

if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
    {
      printf ("Unable to register SIG_IGN to SIGPIPE. Ending...");
      return -1;
    }

int receivedBytes;
fd_set readset;
char s[INET6_ADDRSTRLEN];
char buff[BUFFER_LEN], buf[BUFFER_LEN]; 	
int rv, serverFd, clientFd, retval;
struct addrinfo hints, *servinfo, *p;
socklen_t sin_size;
struct sockaddr_storage their_addr;
struct timeval timeout;
int sentBytes;

memset (&hints, 0, sizeof hints);
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;
hints.ai_flags = AI_PASSIVE;        // use my IP
int yes;

if ((rv = getaddrinfo (NULL, serverPortNumber, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return 1;
    }

for (p = servinfo; p != NULL; p = p->ai_next)
    {
      if ((serverFd = socket (p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
          perror ("server: socket");
          continue;
        }

      if (setsockopt (serverFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                      sizeof (int)) == -1)
        {
          perror ("setsockopt");
          exit (1);
        }

      if (bind (serverFd, p->ai_addr, p->ai_addrlen) == -1)
        {
          close (serverFd);
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
freeaddrinfo (servinfo);
if (listen (serverFd, BACKLOG) == -1)
    {
      perror ("listen");
      exit (1);
    }
printf ("server: waiting for connections...\n");

while (1)
    {                                // main accept() loop
      sin_size = sizeof (their_addr);
      clientFd =
        accept (serverFd, (struct sockaddr *) &their_addr, &sin_size);
      if (clientFd == -1)
        {
          perror ("accept");
          continue;
        }
    
    inet_ntop (their_addr.ss_family,
                 get_in_addr ((struct sockaddr *) &their_addr), s, sizeof s);
    printf ("server: got connection from %s\t File Desciptor = %d\n", s,
              clientFd);
    memset (&timeout, 0, sizeof (timeout));
    timeout.tv_sec = CLIENT_TIMEOUT;
    timeout.tv_usec = 0;
    FD_ZERO (&readset);
    FD_SET (clientFd, &readset);

    while (true){
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
	      fprintf (stderr, "Client not responding, no messsage received\n");
	      send (clientFd, "Server Error\r\n\r\n", 21, 0);
	      close (clientFd);        //  destroy the client connection after serving it
	      pthread_exit (0);
	    }


	  // Here the proxy accepts client's request
	  memset (buff, 0, BUFFER_LEN);
	  if ((receivedBytes = recv (clientFd, buff, BUFFER_LEN - 1, 0)) < 0)
	    {
	      send (clientFd, "Server Error\r\n\r\n", 21, 0);
	      close (clientFd);        //  destroy the client connection after serving it
	      perror ("Receive Error Server: ");
	      pthread_exit (0);
	    }
           else
	   {
		 buff[receivedBytes] = '\0';		 		 
		 ifstream myfile (buff, ifstream::binary);		 
		 char responseFromServer[chunkSize];
	
		 struct stat filestatus;
		 stat(buff, &filestatus );
		 int fileSize = filestatus.st_size;
		 int totalBytesRead=0;
		 int bytesToRead=0;
                 cout << "file size = " << fileSize << endl;
 		 if (myfile.is_open())
		  {
		    while (totalBytesRead < (fileSize-chunkSize))
		    {
		      memset(responseFromServer, 0, chunkSize);
		      myfile.read(responseFromServer, chunkSize);		      
	              sentBytes = send(clientFd, responseFromServer, chunkSize, 0);
		      totalBytesRead = totalBytesRead + chunkSize;
		    }
		    bytesToRead = fileSize - totalBytesRead;
		    if (bytesToRead > 0){
                      memset(responseFromServer, 0, chunkSize);
		      myfile.read(responseFromServer, bytesToRead);		      
	              sentBytes = send(clientFd, responseFromServer, bytesToRead, 0);
                    }
		    myfile.close();
		  }else{
		cout  << "Cannot open File" << endl;
		 }
		 close (clientFd);		 
		 break;
	   }
    }

}
return 0;
  
}
