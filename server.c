/**
* @file server.c
* @author Corey Brantley, Shen Knoll, Harrison Sherwin
* @date  20 October 2024
* @brief  This program is a multithreaded server application that listens for incoming TCP
*         connections from clients, processes requests, and transfers MP3 files to the client.
*         It uses secure SSL/TLS connections with certificates generated using the openssl 
*         application. The server can handle multiple clients concurrently using threads, 
*         ensuring secure file transfers and file integrity verification.
*
*         The server supports the following client operations:
*          - Listing available MP3 files in the server directory.
*          - Searching for MP3 files based on a user-provided search term.
*          - Downloading an MP3 file and sending its SHA-256 hash to the client for verification.
*
*         The SSL/TLS connection ensures that communication between the client and the server
*         is encrypted and secure. To generate a self-signed certificate and private key that 
*         the server can use, at the command prompt type:
*
*         openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 365 -out cert.pem
*
*         This will create two files:
*          - 'key.pem': A private key.
*          - 'cert.pem': A self-signed certificate containing a public key.
*
*         These files are necessary for the server's SSL/TLS encryption. The client does not
*         need these files, as they are only used by the server to establish secure connections.
*
*         The server supports multiple clients concurrently by spawning a separate thread
*         for each connection. The server ensures data integrity by computing the SHA-256 
*         hash of each MP3 file before sending it to the client, allowing the client to 
*         verify the download.
*/

// Header libraries
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <pthread.h>

#include "CommunicationConstants.h"

// Constants to define buffer sizes, certificate file locations, and directory paths
#define BUFFER_SIZE       256
#define HASH_SIZE         SHA256_DIGEST_LENGTH
#define CERTIFICATE_FILE  "cert.pem"
#define KEY_FILE          "key.pem"
#define MP3_DIR           "./sample-mp3s"

// Function declarations
void list_files(SSL *ssl);
void search_files(SSL *ssl, const char *search_term);
void send_file_with_hash(SSL *ssl, const char *filename);
void *handle_client(void *client_socket);
void init_openssl();
void cleanup_openssl();
SSL_CTX* create_new_context();
void configure_context(SSL_CTX* ssl_ctx);
void handle_rpc_request(SSL *ssl);

/**
 * @brief Creates a TCP socket and binds it to the specified port.
 *        The server listens for incoming client connections on this socket.
 * 
 * @param port - The port number to bind the server to.
 * @return Socket descriptor to be used for communication.
 */
int create_socket(unsigned int port) {
    int s;
    struct sockaddr_in addr;
    
    // Set up the socket address structure for IPv4 and bind to the given port
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // Convert port number to network byte order
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all available interfaces

    // Create the socket using IPv4 (AF_INET) and TCP (SOCK_STREAM)
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket"); // Error if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified port and network interface
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind to socket");
        exit(EXIT_FAILURE);
    }

    // Set the socket to listen for incoming connections with a backlog of 5 clients
    if (listen(s, 5) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s; // Return the socket descriptor
}

/**
 * @brief Initialize the OpenSSL library, which will be used for creating
 *        secure SSL/TLS connections with clients.
 */
void init_openssl() {
    SSL_load_error_strings(); // Load SSL error strings for better debugging
    OpenSSL_add_ssl_algorithms(); // Initialize the SSL algorithms and ciphers
}

/**
 * @brief Cleanup OpenSSL resources, freeing any memory and unloading SSL algorithms.
 */
void cleanup_openssl() {
    EVP_cleanup(); // Free SSL resources used during the program execution
}

/**
 * @brief Create a new SSL context to manage the SSL settings for the server.
 *        This context will be used to create secure connections with clients.
 * 
 * @return The initialized SSL_CTX object.
 */
SSL_CTX* create_new_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    // Use the SSLv23 server method to provide SSL/TLS functionality
    method = SSLv23_server_method();
    ctx = SSL_CTX_new(method); // Create a new SSL context

    // If context creation fails, print error and exit
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx; // Return the created SSL context
}

/**
 * @brief Configure the SSL context with the server's certificate and private key.
 *        This ensures that SSL/TLS encryption is properly set up for the server.
 * 
 * @param ctx - The SSL_CTX object to configure.
 */
void configure_context(SSL_CTX* ctx) {
    SSL_CTX_set_ecdh_auto(ctx, 1); // Automatically select the best elliptic curve

    // Load the server's certificate for SSL/TLS
    if (SSL_CTX_use_certificate_file(ctx, CERTIFICATE_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Load the private key corresponding to the server's certificate
    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Handle each client connection in a separate thread.
 *        This function sets up SSL/TLS for the connection and processes client requests.
 * 
 * @param client_socket - The socket descriptor for the client connection.
 */
void *handle_client(void *client_socket) {
    int client = *(int*)client_socket;
    free(client_socket); // Free the dynamically allocated client socket

    // Create and configure SSL context for the new client connection
    SSL_CTX* ctx = create_new_context();
    configure_context(ctx);

    // Create new SSL object and bind it to the client socket
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client);

    // Perform the SSL handshake with the client
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr); // Log any SSL handshake errors
    } else {
        // Process the client's request (e.g., list files, search, download)
        handle_rpc_request(ssl);
    }

    // Cleanup the SSL connection and close the client socket
    SSL_free(ssl);
    close(client);
    SSL_CTX_free(ctx); // Free SSL context for this connection

    pthread_exit(NULL); // Exit the thread when done
}

/**
 * @brief Process and handle client RPC requests. Based on the request, it performs
 *        operations like listing available MP3 files, searching for files, or
 *        downloading a file with its hash.
 * 
 * @param ssl - The SSL object used for secure communication with the client.
 */
void handle_rpc_request(SSL *ssl) {
    char buffer[BUFFER_SIZE]; // Buffer to store client request
    char operation[BUFFER_SIZE]; // Buffer for the operation (LIST, SEARCH, etc.)
    char argument[BUFFER_SIZE]; // Buffer for any arguments (e.g., filename or search term)
    char errorMsg[BUFFER_SIZE]; // Buffer for error messages
    int scanned_items;

    // Read the client's request via SSL
    SSL_read(ssl, buffer, BUFFER_SIZE);

    // Parse the operation and arguments from the request
    scanned_items = sscanf(buffer, "%s %[^\n]", operation, argument);

    // Handle LIST operation (no arguments required)
    if (scanned_items == 1) {
        if (strcmp(operation, RPC_LIST_OPERATION) == 0) {
            list_files(ssl); // Send a list of available MP3 files to the client
        } else {
            // If operation is missing arguments, send an error to the client
            sprintf(errorMsg, "%s %d", ERROR_RPC_ERROR, RPC_ERROR_TOO_FEW_ARGS);
            SSL_write(ssl, errorMsg, sizeof(errorMsg));
        }
    } else if (scanned_items == 2) { // If operation has an argument (SEARCH or DOWNLOAD)
        if (strcmp(operation, RPC_SEARCH_OPERATION) == 0) {
            search_files(ssl, argument); // Search for files matching the search term
        } else if (strcmp(operation, RPC_DOWNLOAD_OPERATION) == 0) {
            send_file_with_hash(ssl, argument); // Send the requested file to the client
        } else {
            // If operation is invalid, send an error to the client
            sprintf(errorMsg, "%s %d", ERROR_RPC_ERROR, RPC_ERROR_BAD_OPERATION);
            SSL_write(ssl, errorMsg, sizeof(errorMsg));
        }
    } else {
        // If request is poorly formed, send an error response
        sprintf(errorMsg, "%s %d", ERROR_RPC_ERROR, RPC_ERROR_TOO_FEW_ARGS);
        SSL_write(ssl, errorMsg, sizeof(errorMsg));
    }
}

/**
 * @brief List all available MP3 files in the MP3 directory and send the list
 *        to the client over the secure SSL connection.
 * 
 * @param ssl - The SSL object used for secure communication.
 */
void list_files(SSL *ssl) {
    DIR *dir = opendir(MP3_DIR); // Open the MP3 directory
    struct dirent *entry;
    char fileName[BUFFER_SIZE];

    if (!dir) {
        perror("Unable to open mp3 directory"); // Error if directory cannot be opened
        return;
    }

    // Iterate through the directory and send each MP3 file to the client
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".mp3")) {
            snprintf(fileName, sizeof(fileName), "%s\n", entry->d_name);
            SSL_write(ssl, fileName, strlen(fileName));
        }
    }

    closedir(dir); // Close the directory when done
}

/**
 * @brief Search for MP3 files that match the provided search term and send the results
 *        to the client.
 * 
 * @param ssl - The SSL object used for secure communication.
 * @param search_term - The term to search for in the file names.
 */
void search_files(SSL *ssl, const char *search_term) {
    DIR *dir = opendir(MP3_DIR); // Open the MP3 directory
    struct dirent *entry;
    char fileName[BUFFER_SIZE];

    if (!dir) {
        perror("Unable to open mp3 directory");
        return;
    }

    // Iterate through the directory and send files that match the search term
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, search_term) && strstr(entry->d_name, ".mp3")) {
            snprintf(fileName, sizeof(fileName), "%s\n", entry->d_name);
            SSL_write(ssl, fileName, strlen(fileName));
        }
    }

    closedir(dir); // Close the directory when done
}

/**
 * @brief Send the requested MP3 file to the client along with its SHA-256 hash
 *        for integrity verification.
 * 
 * @param ssl - The SSL object used for secure communication.
 * @param filename - The name of the file to be sent to the client.
 */
void send_file_with_hash(SSL *ssl, const char *filename) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", MP3_DIR, filename); // Build the file path
    FILE *file = fopen(filepath, "rb"); // Open the file for reading in binary mode

    // If the file doesn't exist, send an error to the client
    if (!file) {
        char errorMsg[BUFFER_SIZE];
        snprintf(errorMsg, sizeof(errorMsg), "%s %d", ERROR_FILE_ERROR, errno);
        SSL_write(ssl, errorMsg, strlen(errorMsg));
        return;
    }

    unsigned char hash[HASH_SIZE]; // Buffer for the file's hash
    SHA256_CTX sha256;
    SHA256_Init(&sha256); // Initialize the SHA-256 context

    char buffer[BUFFER_SIZE];
    int bytes;
    // Read the file and send it in chunks, while calculating the hash
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        SSL_write(ssl, buffer, bytes); // Send the file chunk to the client
        SHA256_Update(&sha256, buffer, bytes); // Update the hash with the file chunk
    }

    // Finalize the SHA-256 hash and send it to the client
    SHA256_Final(hash, &sha256);
    SSL_write(ssl, hash, HASH_SIZE);

    fclose(file); // Close the file when done
}

/**
 * @brief Main server loop: initializes SSL, creates the socket, and handles
 *        incoming client connections by spawning a new thread for each client.
 */
int main(int argc, char **argv) {
    unsigned int port = (argc == 2) ? atoi(argv[1]) : DEFAULT_PORT; // Use port from args or default

    // Initialize the OpenSSL library
    init_openssl();
    SSL_CTX* ctx = create_new_context(); // Create SSL context for the server
    configure_context(ctx); // Load certificate and private key

    // Create the server socket and bind to the specified port
    int server_socket = create_socket(port);
    printf("Server is running on port %u\n", port);

    while (true) {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        int *client_socket = malloc(sizeof(int)); // Allocate memory for client socket

        // Accept incoming client connections
        *client_socket = accept(server_socket, (struct sockaddr*)&addr, &len);
        if (*client_socket < 0) {
            perror("Unable to accept connection");
            free(client_socket);
            continue;
        }

        // Spawn a new thread to handle each client connection
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_socket);
        pthread_detach(tid); // Automatically clean up the thread when it finishes
    }

    // Clean up server resources before shutting down
    close(server_socket);
    SSL_CTX_free(ctx); // Free the SSL context
    cleanup_openssl(); // Cleanup OpenSSL

    return 0;
}
