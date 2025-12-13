#include "download.h"

 // Parse FTP URL in format: ftp://[<user>:<password>@]<host>/<url-path>

int parseURL(const char *url, URL *urlInfo) {
    // Init with defaults
    strcpy(urlInfo->user, "anonymous");
    strcpy(urlInfo->password, "anonymous@");
    memset(urlInfo->host, 0, sizeof(urlInfo->host));
    memset(urlInfo->path, 0, sizeof(urlInfo->path));
    
    // Check for ftp:// prefix
    if (strncmp(url, "ftp://", 6) != 0) {
        fprintf(stderr, "Error: URL must start with 'ftp://'\n");
        return -1;
    }
    
    const char *ptr = url + 6; // Skip ftp://
    char temp[1024];
    strcpy(temp, ptr);
    
    // Check for user:password@
    char *atSign = strchr(temp, '@');
    char *hostStart;
    
    if (atSign != NULL) {
        *atSign = '\0';
        hostStart = atSign + 1;
        
        // Parse user:password
        char *colon = strchr(temp, ':');
        if (colon != NULL) {
            *colon = '\0';
            strcpy(urlInfo->user, temp);
            strcpy(urlInfo->password, colon + 1);
        } else {
            strcpy(urlInfo->user, temp);
        }
    } else {
        hostStart = temp;
    }
    
    // Parse host/path
    char *slash = strchr(hostStart, '/');
    if (slash != NULL) {
        *slash = '\0';
        strcpy(urlInfo->host, hostStart);
        strcpy(urlInfo->path, slash + 1);
    } else {
        strcpy(urlInfo->host, hostStart);
        strcpy(urlInfo->path, "");
    }
    
    if (strlen(urlInfo->host) == 0) {
        fprintf(stderr, "Error: Invalid URL - no host specified\n");
        return -1;
    }
    
    printf("URL parsed successfully:\n");
    printf("  User: %s\n", urlInfo->user);
    printf("  Password: %s\n", urlInfo->password);
    printf("  Host: %s\n", urlInfo->host);
    printf("  Path: %s\n", urlInfo->path);
    
    return 0;
}


// Get IP address from hostname using DNS (based on getip.c)

int getIPFromHost(const char *host, char *ip) {
    struct hostent *h;
    
    printf("Resolving hostname '%s'\n", host);
    
    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname()");
        return -1;
    }
    
    strcpy(ip, inet_ntoa(*((struct in_addr *) h->h_addr)));
    printf("IP Address: %s\n", ip);
    
    return 0;
}

// Create a TCP socket and connect to server (based on clientTCP.c)
// Returns socket file descriptor on success

int createSocket(const char *ip, int port) {
    // Socket file descriptor
    int sockfd; 
    struct sockaddr_in server_addr;
    
    // Create TCP socket
    // IPv4, TCP
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }
    
    // Configure server address
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;               // IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip);    // Server IP
    server_addr.sin_port = htons(port);             // Server port
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}


//Handles both single line and multi line responses
 
int readFTPResponse(int sockfd, FTPResponse *response) {
    char byte;
    int i = 0;
    char line[MAX_LENGTH];
    int lineIndex = 0;
    char code[4] = {0};
    int gotCode = 0;
    int multiline = 0;
    
    memset(response->message, 0, MAX_RESPONSE);
    
    // Read data from socket one byte at a time
    while (1) {
        if (read(sockfd, &byte, 1) <= 0) {
            // Connection error or closed
            return -1;
        }
        
        if (i < MAX_RESPONSE - 1) {
            response->message[i++] = byte;
        }
        
        line[lineIndex++] = byte;
        
        // End of line
        if (byte == '\n') {
            line[lineIndex] = '\0';
            
            // Extract code from first line
            if (!gotCode && lineIndex >= 4) {
                strncpy(code, line, 3);
                code[3] = '\0';
                gotCode = 1;
                
                // Check if multi line response (code followed by '-')
                if (line[3] == '-') {
                    multiline = 1;
                }
            }
            
            // Check if this is final line
            if (gotCode) {
                if (!multiline) {
                    // Single line response complete
                    break;
                } else if (lineIndex >= 4 && strncmp(line, code, 3) == 0 && line[3] == ' ') {
                    // Multi line response complete (code followed by space)
                    break;
                }
            }
            
            lineIndex = 0;
        }
        
        if (lineIndex >= MAX_LENGTH - 1) {
            lineIndex = 0; // Prevent buffer overflow
        }
    }
    
    response->message[i] = '\0';
    
    // Extract response code
    sscanf(response->message, "%d", &response->code);
    
    printf("FTP Response: %s", response->message);
    
    return 0;
}


//  Send FTP command to server
//  Returns number of bytes written

int sendFTPCommand(int sockfd, const char *command, const char *arg) {
    char buffer[MAX_LENGTH];
    
    if (arg != NULL && strlen(arg) > 0) {
        snprintf(buffer, MAX_LENGTH, "%s %s\r\n", command, arg);
    } else {
        snprintf(buffer, MAX_LENGTH, "%s\r\n", command);
    }
    
    printf("Sending FTP command: %s", buffer);
    
    // Write data to the socket
    int bytes = write(sockfd, buffer, strlen(buffer));
    if (bytes < 0) {
        perror("write()");
        return -1;
    }
    
    return bytes;
}


//Connect to FTP server
// Returns socket file descriptor

int ftpConnect(const char *ip, int port) {
    FTPResponse response;
    
    printf("Connecting to %s:%d\n", ip, port);
    
    int sockfd = createSocket(ip, port);
    if (sockfd < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0 || response.code != FTP_READY) {
        fprintf(stderr, "Failed to connect to server\n");
        close(sockfd);
        return -1;
    }
    
    printf("Connected successfully\n");
    return sockfd;
}

 // Login to FTP server with username and password
 
int ftpLogin(int sockfd, const char *user, const char *password) {
    FTPResponse response;
    
    printf("Logging in as '%s'\n", user);
    
    // Send USER command
    if (sendFTPCommand(sockfd, "USER", user) < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0) {
        return -1;
    }
    
    // If 230, already logged in (no password needed)
    if (response.code == FTP_LOGIN_OK) {
        printf("Login successful\n");
        return 0;
    }
    
    // If 331, need password
    if (response.code != FTP_USER_OK) {
        fprintf(stderr, "Unexpected response to USER command: %d\n", response.code);
        return -1;
    }
    
    // Send PASS command
    if (sendFTPCommand(sockfd, "PASS", password) < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0) {
        return -1;
    }
    
    if (response.code != FTP_LOGIN_OK) {
        fprintf(stderr, "Login failed: %d\n", response.code);
        return -1;
    }
    
    printf("Login successful\n");
    return 0;
}


// Set binary transfer mode (TYPE I)

int ftpSetBinaryMode(int sockfd) {
    FTPResponse response;
    
    printf("Setting binary mode\n");
    
    // Send TYPE I command
    if (sendFTPCommand(sockfd, "TYPE", "I") < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0) {
        return -1;
    }
    
    if (response.code != FTP_COMMAND_OK) {
        fprintf(stderr, "Failed to set binary mode: %d\n", response.code);
        return -1;
    }
    
    printf("Binary mode enabled\n");
    return 0;
}


// Enter passive mode and get data connection info

int ftpPassive(int sockfd, char *ip, int *port) {
    FTPResponse response;
    
    printf("Entering passive mode\n");
    
    // Send PASV command
    if (sendFTPCommand(sockfd, "PASV", NULL) < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0) {
        return -1;
    }
    
    if (response.code != FTP_PASSIVE) {
        fprintf(stderr, "Failed to enter passive mode: %d\n", response.code);
        return -1;
    }
    
    // Parse PASV response: 227 Entering Passive Mode
    int ip1, ip2, ip3, ip4, port1, port2;
    char *start = strchr(response.message, '(');
    if (start == NULL) {
        fprintf(stderr, "Invalid PASV response format\n");
        return -1;
    }
    
    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6) {
        fprintf(stderr, "Failed to parse PASV response\n");
        return -1;
    }
    
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    *port = port1 * 256 + port2;
    
    printf("Passive mode: IP=%s, Port=%d\n", ip, *port);
    
    return 0;
}


// Send RETR command to retrieve file

int ftpRetrieve(int sockfd, const char *path) {
    FTPResponse response;
    
    printf("Requesting file '%s'\n", path);
    
    // Send RETR command
    if (sendFTPCommand(sockfd, "RETR", path) < 0) {
        return -1;
    }
    
    if (readFTPResponse(sockfd, &response) < 0) {
        return -1;
    }
    
    // Accept both 150 and 125 as valid transfer starting codes
    if (response.code != FTP_TRANSFER_START && response.code != FTP_TRANSFER_START_ALT) {
        fprintf(stderr, "Failed to start transfer: %d\n", response.code);
        return -1;
    }
    
    printf("Transfer started\n");
    return 0;
}


// Send QUIT command to close connection

int ftpQuit(int sockfd) {
    printf("Closing connection\n");
    sendFTPCommand(sockfd, "QUIT", NULL);
    close(sockfd);
    return 0;
}


int downloadFile(int sockfd, const char *filename) {
    FILE *file;
    char buffer[MAX_LENGTH];
    int bytes;
    int total = 0;
    
    // Extract filename from path
    const char *name = strrchr(filename, '/');
    if (name != NULL) {
        name++;
    } else {
        name = filename;
    }
    
    printf("Saving file as '%s'\n", name);
    
    file = fopen(name, "wb");
    if (file == NULL) {
        perror("fopen()");
        return -1;
    }
    
    // Read data from socket and write to file
    while ((bytes = read(sockfd, buffer, MAX_LENGTH)) > 0) {
        if (fwrite(buffer, 1, bytes, file) != (size_t)bytes) {
            perror("fwrite()");
            fclose(file);
            return -1;
        }
        total += bytes;
        printf("\rDownloaded: %d bytes", total);
        fflush(stdout);
    }
    
    printf("\nFile download complete: %d bytes\n", total);
    
    fclose(file);
    return 0;
}


// Main download function

int performDownload(const char *url) {
    URL urlInfo;
    int controlSocket = -1;
    int dataSocket = -1;
    char dataIP[16];
    int dataPort;
    FTPResponse response;
    
    printf("=== FTP Download Application ===\n");
    printf("URL: %s\n\n", url);
    
    printf("Phase 1: Parsing URL\n");
    if (parseURL(url, &urlInfo) < 0) {
        fprintf(stderr, "Failed at phase: URL parsing\n");
        return -1;
    }
    printf("\n");
    
    printf("Phase 2: DNS Resolution\n");
    if (getIPFromHost(urlInfo.host, urlInfo.ip) < 0) {
        fprintf(stderr, "Failed at phase: DNS resolution\n");
        return -1;
    }
    printf("\n");
    
    printf("Phase 3: Connecting to FTP server\n");
    controlSocket = ftpConnect(urlInfo.ip, FTP_PORT);
    if (controlSocket < 0) {
        fprintf(stderr, "Failed at phase: Connection\n");
        return -1;
    }
    printf("\n");
    
    printf("Phase 4: Login\n");
    if (ftpLogin(controlSocket, urlInfo.user, urlInfo.password) < 0) {
        fprintf(stderr, "Failed at phase: Login\n");
        ftpQuit(controlSocket);
        return -1;
    }
    printf("\n");
    
    printf("Phase 5: Setting binary mode\n");
    if (ftpSetBinaryMode(controlSocket) < 0) {
        fprintf(stderr, "Failed at phase: Binary mode\n");
        ftpQuit(controlSocket);
        return -1;
    }
    printf("\n");
    
    printf("Phase 6: Entering passive mode\n");
    if (ftpPassive(controlSocket, dataIP, &dataPort) < 0) {
        fprintf(stderr, "Failed at phase: Passive mode\n");
        ftpQuit(controlSocket);
        return -1;
    }
    printf("\n");
    
    printf("Phase 7: Opening data connection\n");
    dataSocket = createSocket(dataIP, dataPort);
    if (dataSocket < 0) {
        fprintf(stderr, "Failed at phase: Data connection\n");
        ftpQuit(controlSocket);
        return -1;
    }
    printf("Data connection established\n\n");
    
    // Step 8: Send RETR command
    printf("Phase 8: Requesting file\n");
    if (ftpRetrieve(controlSocket, urlInfo.path) < 0) {
        fprintf(stderr, "Failed at phase: File retrieval request\n");
        close(dataSocket);
        ftpQuit(controlSocket);
        return -1;
    }
    printf("\n");
    
    // Step 9: Download file
    printf("Phase 9: Downloading file\n");
    if (downloadFile(dataSocket, urlInfo.path) < 0) {
        fprintf(stderr, "Failed at phase: File download\n");
        close(dataSocket);
        ftpQuit(controlSocket);
        return -1;
    }
    printf("\n");
    
    // Close data connection
    close(dataSocket);
    
    // Read transfer complete message
    if (readFTPResponse(controlSocket, &response) < 0 || 
        response.code != FTP_TRANSFER_COMPLETE) {
        fprintf(stderr, "Warning: Unexpected response after transfer\n");
    }
    
    // Step 10: Quit
    printf("Phase 10: Closing connection\n");
    ftpQuit(controlSocket);
    
    printf("\n=== SUCCESS: File downloaded successfully ===\n");
    
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Incorrect number of argments. Try again.\n");
        exit(-1);
    }
    
    if (performDownload(argv[1]) < 0) {
        fprintf(stderr, "\n=== FAILURE: Download failed ===\n");
        exit(-1);
    }
    
    return 0;
}
