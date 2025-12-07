#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define FTP_PORT 21
#define MAX_LENGTH 2048
#define MAX_RESPONSE 4096

// FTP response codes
#define FTP_READY 220
#define FTP_USER_OK 331
#define FTP_LOGIN_OK 230
#define FTP_PASSIVE 227
#define FTP_TRANSFER_START 150
#define FTP_TRANSFER_COMPLETE 226
#define FTP_FILE_OK 250

// URL structure to hold parsed FTP URL components
typedef struct {
    char user[256];
    char password[256];
    char host[256];
    char path[1024];
    char ip[16];
} URL;

// FTP response structure
typedef struct {
    int code;
    char message[MAX_RESPONSE];
} FTPResponse;

/**
 * URL parsing functions
 */
int parseURL(const char *url, URL *urlInfo);

/**
 * DNS and socket functions
 */
int getIPFromHost(const char *host, char *ip);
int createSocket(const char *ip, int port);

/**
 * FTP protocol functions
 */
int readFTPResponse(int sockfd, FTPResponse *response);
int sendFTPCommand(int sockfd, const char *command, const char *arg);
int ftpConnect(const char *ip, int port);
int ftpLogin(int sockfd, const char *user, const char *password);
int ftpPassive(int sockfd, char *ip, int *port);
int ftpRetrieve(int sockfd, const char *path);
int ftpQuit(int sockfd);

/**
 * File download functions
 */
int downloadFile(int sockfd, const char *filename);
int performDownload(const char *url);

#endif // DOWNLOAD_H
