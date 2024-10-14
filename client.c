// So far this is just a copy of my code from Assignment 4.

/**
* @file client.c
* @author Harrison Sherwin
* @date  28 September 2024
 * @brief This program is a small client application that establishes a secure
          TCP connection to a server and simply exchanges messages. It uses a
          SSL/TLS connection using X509 certificates generated with the openssl
          application.

          The purpose is to demonstrate how to establish and use secure
          communication channels between a client and server using public key
          cryptography.

          Some of the code and descriptions can be found in "Network Security
          with OpenSSL", O'Reilly Media, 2002.
*/

// Header libraries
#include <netdb.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "CommunicationConstants.h"
#include "playaudio.h"

// Global statics
#define DEFAULT_HOST        "localhost"
#define MAX_HOSTNAME_LENGTH 256
#define BUFFER_SIZE         256
#define DEFAULT_DOWNLOAD_LOCATION "downloaded-mp3s"
#define LIST_MP3S 1
#define SEARCH_MP3S 2
#define DOWNLOAD_MP3 3
#define PLAY_MP3 4
#define STOP_MP3 5
#define QUIT_PROGRAM 0
#define MAX_FILES 50
#define MAX_RETRIES 3


struct SSL_Connection
{
  const SSL_METHOD* method;
  SSL_CTX* ssl_ctx;
  SSL* ssl;
  int sockfd;
  char remote_host[MAX_HOSTNAME_LENGTH];
  unsigned int port;
  int connected;
};


void requestAvailableDownloads(struct SSL_Connection *ssl_connection);
void searchAvailableDownloads(struct SSL_Connection *ssl_connection);
int downloadMP3(struct SSL_Connection *ssl_connection);
int playMP3(char *fileName, pthread_t *ptid);
int promptUser();
int chooseFromDownloadedMP3s(char *fileChoice);
void printDownloadedChoices(char *fileNames[MAX_FILES], int fileCount);
int stopMP3(pthread_t *ptid);

int *stopPlaying;
pthread_mutex_t mutexPlaying;

/**
* @brief This function does the basic necessary housekeeping to establish a secure TCP
*        connection to the server specified by 'hostname'.
*/
int create_socket(char* hostname, unsigned int port) {
  int                sockfd;
  struct hostent*    host;
  struct sockaddr_in dest_addr;

  host = gethostbyname(hostname);
  if (host == NULL) {
    fprintf(stderr, "Client: Cannot resolve hostname %s\n",  hostname);
    exit(EXIT_FAILURE);
  }

  // Create a socket (endpoint) for network communication.  The socket()
  // call returns a socket descriptor, which works exactly like a file
  // descriptor for file system operations we worked with in CS431
  //
  // Sockets are by default blocking, so the server will block while reading
  // from or writing to a socket. For most applications this is acceptable.
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Server: Unable to create socket: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // First we set up a network socket. An IP socket address is a combination
  // of an IP interface address plus a 16-bit port number. The struct field
  // sin_family is *always* set to AF_INET. Anything else returns an error.
  // The TCP port is stored in sin_port, but needs to be converted to the
  // format on the host machine to network byte order, which is why htons()
  // is called. The s_addr field is the network address of the remote host
  // specified on the command line. The earlier call to gethostbyname()
  // retrieves the IP address for the given hostname.
  dest_addr.sin_family=AF_INET;
  dest_addr.sin_port=htons(port);
  dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);

  // Now we connect to the remote host.  We pass the connect() system call the
  // socket descriptor, the address of the remote host, and the size in bytes
  // of the remote host's address
  if (connect(sockfd, (struct sockaddr *) &dest_addr,
	      sizeof(struct sockaddr)) <0) {
    fprintf(stderr, "Client: Cannot connect to host %s [%s] on port %d: %s\n",
	    hostname, inet_ntoa(dest_addr.sin_addr), port, strerror(errno));
    exit(EXIT_FAILURE);
  }

  return sockfd;
}

void initialize_connection(struct SSL_Connection *ssl_connection) {
  // Initialize OpenSSL ciphers and digests
  OpenSSL_add_all_algorithms();
  printf("\n");
  // SSL_library_init() registers the available SSL/TLS ciphers and digests.
  if(SSL_library_init() < 0) {
    fprintf(stderr, "Client: Could not initialize the OpenSSL library!\n");
    exit(EXIT_FAILURE);
  }

  // Use the SSL/TLS method for clients
  ssl_connection->method = SSLv23_client_method();

  // Create new context instance
  ssl_connection->ssl_ctx = SSL_CTX_new(ssl_connection->method);
  if (ssl_connection->ssl_ctx == NULL) {
    fprintf(stderr, "Unable to create a new SSL context structure.\n");
    exit(EXIT_FAILURE);
  }

  // This disables SSLv2, which means only SSLv3 and TLSv1 are available
  // to be negotiated between client and server
  SSL_CTX_set_options(ssl_connection->ssl_ctx, SSL_OP_NO_SSLv2);

  // Create a new SSL connection state object
  ssl_connection->ssl = SSL_new(ssl_connection->ssl_ctx);

  // Create the underlying TCP socket connection to the remote host
  ssl_connection->sockfd = create_socket(ssl_connection->remote_host, ssl_connection->port);
  if(ssl_connection->sockfd != 0)
    fprintf(stderr, "Client: Established TCP connection to '%s' on port %u\n", ssl_connection->remote_host, ssl_connection->port);
  else {
    fprintf(stderr, "Client: Could not establish TCP connection to %s on port %u\n", ssl_connection->remote_host, ssl_connection->port);
    exit(EXIT_FAILURE);
  }

  // Bind the SSL object to the network socket descriptor. The socket descriptor
  // will be used by OpenSSL to communicate with a server. This function should
  // only be called once the TCP connection is established, i.e., after
  // create_socket()
  SSL_set_fd(ssl_connection->ssl, ssl_connection->sockfd);

  // Initiates an SSL session over the existing socket connection. SSL_connect()
  // will return 1 if successful.
  if (SSL_connect(ssl_connection->ssl) == 1)
    printf("Client: Established SSL/TLS session to '%s' on port %u\n", ssl_connection->remote_host, ssl_connection->port);
  else {
    fprintf(stderr, "Client: Could not establish SSL session to '%s' on port %u\n", ssl_connection->remote_host, ssl_connection->port);
    exit(EXIT_FAILURE);
  }
  printf("\n\n");
  ssl_connection->connected = 1;
}

// Deallocate memory for the SSL data structures and close the socket
void close_ssl_connection(struct SSL_Connection *ssl_connection) {
    SSL_free(ssl_connection->ssl);
    SSL_CTX_free(ssl_connection->ssl_ctx);
    close(ssl_connection->sockfd);
    ssl_connection->connected = -1;
    printf("Client: Terminated SSL/TLS connection with server '%s'\n",
	    ssl_connection->remote_host);
}

/**
* @brief The sequence of steps required to establish a secure SSL/TLS connection is:
*
*        1.  Initialize the SSL algorithms
*        2.  Create and configure an SSL context object
*        3.  Create an SSL session object
*        4.  Create a new network socket in the traditional way
*        5.  Bind the SSL object to the network socket descriptor
*        6.  Establish an SSL session on top of the network connection
*
* Once these steps are completed successfully, use the functions SSL_read() and
* SSL_write() to read from/write to the socket, but using the SSL object rather
* then the socket descriptor.  Once the session is complete, free the memory
* allocated to the SSL object and close the socket descriptor.
*/
int main(int argc, char** argv) {
  char              buffer[BUFFER_SIZE];
  char              fileName[BUFFER_SIZE];
  char              operation[5];
  char              serverError[11];
  int               serverErrno;
  char*             temp_ptr;
  int               writefd = 0;
  int               rcount;
  int               wcount;
  int               rtotal = 0;
  int               wtotal = 0;
  struct            SSL_Connection ssl_connection = {0};
  int               userChoice;
  char*             fileChoice = malloc(BUFFER_SIZE);
  int               continuePrompting = 1;
  int               stopFlag = -1;
  int               downloadTries;
  pthread_t         ptid;

  stopPlaying = &stopFlag;
  pthread_mutex_init(&mutexPlaying, NULL);

  ssl_connection.connected = -1;

  if (argc != 2) {
    fprintf(stderr, "Client: Usage: ssl-client <server name>:<port>\n");
    exit(EXIT_FAILURE);
  } else {
    // Search for ':' in the argument to see if port is specified
    temp_ptr = strchr(argv[1], ':');
    if (temp_ptr == NULL) {    // Hostname only. Use default port
      strncpy(ssl_connection.remote_host, argv[1], MAX_HOSTNAME_LENGTH);
      ssl_connection.port = DEFAULT_PORT;
    } else {
      // Argument is formatted as <hostname>:<port>. Need to separate
      // First, split out the hostname from port, delineated with a colon
      // remote_host will have the <hostname> substring
      strncpy(ssl_connection.remote_host, strtok(argv[1], ":"), MAX_HOSTNAME_LENGTH);
      // Port number will be the substring after the ':'. At this point
      // temp is a pointer to the array element containing the ':'
      ssl_connection.port = (unsigned int) atoi(temp_ptr+sizeof(char));
    }
  }

  while (continuePrompting > 0) {
    userChoice = promptUser();
    switch (userChoice)
    {
    case LIST_MP3S:
      requestAvailableDownloads(&ssl_connection);
      break;
    case SEARCH_MP3S:
      searchAvailableDownloads(&ssl_connection);
      break;
    case DOWNLOAD_MP3:
      downloadTries = 1;
      while (downloadMP3(&ssl_connection) != EXIT_SUCCESS && downloadTries <= MAX_RETRIES) {
        printf("DOWNLOAD FAILED RETRYING -- Try %d of %d\n", downloadTries, MAX_RETRIES);
      }
      break;
    case PLAY_MP3:
      if (stopFlag == 0) { stopMP3(&ptid); }

      if (chooseFromDownloadedMP3s(fileChoice) == EXIT_SUCCESS) {
        playMP3(fileChoice, &ptid);
      }
      break;
    case STOP_MP3:
      if (stopFlag == 0) { stopMP3(&ptid); }
      break;
    case QUIT_PROGRAM:
      continuePrompting = -1;
      break;
    default:
      printf("Invalid Choice");
      break;
    }
  }

  if (ssl_connection.connected == 1) {
    close_ssl_connection(&ssl_connection);
  }

  return EXIT_SUCCESS;
}

void *thread_playMP3(void *arg) {

  char* fileName = (char *)arg;
  playAudio(arg, stopPlaying);
  // error check playaudio
  pthread_exit(NULL);
}

int stopMP3(pthread_t *ptid) {
    // Signal the playback thread to stop
    pthread_mutex_lock(&mutexPlaying);
    *stopPlaying = -1;
    pthread_mutex_unlock(&mutexPlaying);

    // Wait for the thread to exit gracefully
    pthread_join(*ptid, NULL);

    printf("Audio stopped.\n");
    return EXIT_SUCCESS;
}

// Play MP3 file
int playMP3(char *fileName, pthread_t *ptid)  {
  int result;

  pthread_mutex_lock(&mutexPlaying);
  *stopPlaying = 0;
  pthread_mutex_unlock(&mutexPlaying);
  result = pthread_create(ptid, NULL, &thread_playMP3, fileName);
  if (result != 0) {
        fprintf(stderr, "Error creating playback thread: %s\n", strerror(result));
        return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int promptUser() {
  int choice;
  char buffer[BUFFER_SIZE];
  // Print the menu options for the user
  printf("\nPlease choose from the following options:\n");
  printf("%d. List available MP3s to download\n", LIST_MP3S);
  printf("%d. Search MP3s to download\n", SEARCH_MP3S);
  printf("%d. Download MP3\n", DOWNLOAD_MP3);
  printf("%d. Play MP3\n", PLAY_MP3);
  printf("%d. Stop MP3\n\n", STOP_MP3);
  printf("%d. Stop Program\n", QUIT_PROGRAM);

  // Optionally, prompt the user for input (not part of the original request)
  
  printf("Enter your choice (1-5) or 0 to stop: ");
  bzero(buffer, BUFFER_SIZE);
  fgets(buffer, BUFFER_SIZE-1, stdin);
  // Remove trailing newline character
  buffer[strlen(buffer)-1] = '\0';

  sscanf(buffer, "%d", &choice);
  printf("YOUR CHOICE WAS: %d\n\n", choice);
  return choice;
}

void printDownloadedChoices(char *fileNames[MAX_FILES], int fileCount) {
  printf("Please Choose From List of Downloaded MP3\n");
  for (int i = 0; i < fileCount; i++) {
    printf("%d. %s\n", i+1, fileNames[i]);
  }
}
int chooseFromDownloadedMP3s(char *fileChoice) {
  DIR *dp;
  struct dirent *ep;
  char downloadLocation[BUFFER_SIZE];
  char chosenFile[BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  char *fileNames[MAX_FILES];  // Array to hold file names
  int validChoice = -1;
  int fileCount = 0;
  int userChoice;
  char menuCommand;
  
  sprintf(downloadLocation, "./%s/", DEFAULT_DOWNLOAD_LOCATION); 
  dp = opendir(downloadLocation);
  if (dp != NULL) {
    while ((ep = readdir (dp)) != NULL) {
      if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) {
        continue; 
      } 
      // Add the file name to the array if there's space left
      if (fileCount < MAX_FILES) {
        fileNames[fileCount] = malloc(strlen(ep->d_name) + 1);  // Allocate space for the file name
        if (fileNames[fileCount] == NULL) {
            perror("Memory allocation failed");
            closedir(dp);
            exit(EXIT_FAILURE);
        }
        if (strstr(ep->d_name, ".mp3") == NULL) { continue; } // Skip anything without mp3 extension
        strcpy(fileNames[fileCount], ep->d_name);  // Copy the file name to the array
        fileCount++;
      } else {
        printf("Maximum file limit reached, cannot store more file names.\n");
        break;
      }
          
    }
    (void) closedir (dp);
    
  } else {
    perror ("Couldn't open the directory");
    exit(EXIT_FAILURE);
  }

  printDownloadedChoices(fileNames, fileCount);

  while (validChoice < 0) {
    printf("Type the name or corresponding number (1-%d) of the MP3 you'd like to play\n", fileCount);
    printf("-- Type \"?\" to list downloaded songs or \"q\" to quit\n");
    printf("-> ");
    
    bzero(buffer, BUFFER_SIZE);
    fgets(buffer, BUFFER_SIZE-1, stdin);

    if (sscanf(buffer, "%d", &userChoice) > 0) {
      if (userChoice > fileCount || userChoice < 1) { // user choose a number outside of what is available
        printf("Invalid choice, please choose a number between 1 and %d\n", fileCount);
        continue;
      }
      sprintf(chosenFile, "%s", fileNames[userChoice - 1]);
      validChoice = 1;
    } else if (sscanf(buffer, "%c", &menuCommand) > 0) {
      switch (tolower(menuCommand))
      {
      case '?':
        printDownloadedChoices(fileNames, fileCount);
        break;
      
      case 'q':
        return EXIT_FAILURE;

      default:
        printf("Invalid choice, Type \"?\" to list downloaded songs or \"q\" to quit\n");
        break;
      }
    }
    else {
      sscanf(buffer, "%s", chosenFile);

      if (strstr(chosenFile, ".mp3") == NULL) {
        strcat(chosenFile, ".mp3");
      }

      for (int i = 0; i < fileCount; i++) {
        if (strcmp(chosenFile, fileNames[i]) == 0) {
          validChoice = 1;
          break;
        }
        if (i == fileCount - 1) {
          printf("Invalid choice, file name unknown\n");
        }
      }
    }
  }

  sprintf(fileChoice, "%s/%s", DEFAULT_DOWNLOAD_LOCATION, chosenFile);
  return EXIT_SUCCESS;
}

void requestAvailableDownloads(struct SSL_Connection *ssl_connection) {
  int wcount;
  char request[BUFFER_SIZE];

  initialize_connection(ssl_connection);

  sprintf(request, "%s", RPC_LIST_OPERATION);

  wcount = SSL_write(ssl_connection->ssl, request, strlen(request));
  if (wcount < 0) {
    fprintf(stderr, "Client: Could not write message to socket: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  } else {
    printf("Client: Successfully sent message \"%s\" to %s on port %u\n",
    RPC_LIST_OPERATION, ssl_connection->remote_host, ssl_connection->port);
  }

  close_ssl_connection(ssl_connection);
}

void searchAvailableDownloads(struct SSL_Connection *ssl_connection) {
  int wcount;
  char requestMessage[BUFFER_SIZE];
  char searchTerm [] = "Sprouts";
  initialize_connection(ssl_connection);
  sprintf(requestMessage, "%s %s", RPC_SEARCH_OPERATION, searchTerm);
  wcount = SSL_write(ssl_connection->ssl, requestMessage, strlen(requestMessage));
  if (wcount < 0) {
    fprintf(stderr, "Client: Could not write message to socket: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  } else {
    printf("Client: Successfully sent message \"%s\" to %s on port %u\n",
    requestMessage, ssl_connection->remote_host, ssl_connection->port);
  }
  close_ssl_connection(ssl_connection);
}

int downloadMP3(struct SSL_Connection *ssl_connection) {
  char fileName[BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  char request[BUFFER_SIZE];
  char downloadLocation[BUFFER_SIZE];
  char serverError[BUFFER_SIZE];
  int writefd = -1;
  int wcount;
  int rcount;
  int serverErrno;

  initialize_connection(ssl_connection);

  // Read input
  printf("Client: Please enter the name of the mp3 you want to download: ");
  bzero(buffer, BUFFER_SIZE);
  fgets(buffer, BUFFER_SIZE-1, stdin);

  // Remove trailing newline character
  // buffer[strlen(buffer)-1] = '\0';
  
  // Pull everything written by user as filename. Filename could include spaces. 
  sscanf(buffer, "%s", fileName);

  // Build the request
  sprintf(request, "%s %s", RPC_DOWNLOAD_OPERATION, fileName);
  // Build the download location
  sprintf(downloadLocation, "%s/%s", DEFAULT_DOWNLOAD_LOCATION, fileName);

  // Write to server
  wcount = SSL_write(ssl_connection->ssl, request, strlen(request));
  if (wcount < 0) {
    fprintf(stderr, "Client: Could not write message to socket: %s\n",
	    strerror(errno));
    exit(EXIT_FAILURE);
  } else {
    printf("Client: Successfully sent message \"%s\" to %s on port %u\n",
	   request, ssl_connection->remote_host, ssl_connection->port);
  }

  bzero(buffer, BUFFER_SIZE);

  
  // Recieve from server
  while ((rcount = SSL_read(ssl_connection->ssl, buffer, BUFFER_SIZE)) > 0 ) {
    buffer[rcount] = '\0';

    sscanf(buffer, "%10s %d", serverError, &serverErrno);
    if (strcmp(serverError, ERROR_FILE_ERROR) == 0) {
      fprintf(stderr, "Client: Server encountered file error: %s\n", strerror(serverErrno));
      exit(EXIT_FAILURE);
    } else if (strcmp(serverError, ERROR_RPC_ERROR) == 0) {
      if (serverErrno == RPC_ERROR_BAD_OPERATION) {
        fprintf(stderr, "Client: Server encountered error -- 'Bad Operation'\n");
      } else if (serverErrno == RPC_ERROR_TOO_FEW_ARGS) {
        fprintf(stderr, "Client: Server encountered error -- 'Too few arguments'\n");
      } else if (serverErrno == RPC_ERROR_TOO_MANY_ARGS) {
        fprintf(stderr, "Client: Server encountered error -- 'Too many arguments'\n");
      }
      
      exit(EXIT_FAILURE);

    } else if(writefd < 1) { // if a file hasn't been opened for writing
      // Open file for writing
      writefd = open(downloadLocation, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // Open for writing, create if it doesn't exist, overwrite if allowed to
      if (writefd < 0) {
        fprintf(stderr, "Client: Could not open file \"%s\" for writing: %s\n", downloadLocation, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }

    wcount = write(writefd, buffer, rcount);
    if (wcount < 0) {
      fprintf(stderr, "Client: Error while writing to file \"%s\": %s\n", fileName, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if (rcount < 0) {
        fprintf(stderr, "Error reading from server: %s\n", strerror(errno));
        return EXIT_FAILURE;
  } else
  {
    printf("Client: Succesfully downloaded file to: %s\n", downloadLocation);
  }

  close_ssl_connection(ssl_connection);
  return EXIT_SUCCESS;
}