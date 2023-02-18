//
// Created by Braden Nicholson on 2/15/23.
//
#include <sys/socket.h>
#include <lwip/netdb.h>
#include "socket.h"


#define MAXLINE 1024
struct sockaddr_in serverAddr, clientAddr;

void Socket::configureUDP() {

    int sockFd = 0;

    if ((sockFd = socket(AF_INET, SOCK_STREAM, AI_PASSIVE)) < 0) {
        printf("Socket failed\n");
        return;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4567);

    if (bind(sockFd, (const struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        printf("Bind failed\n");
        return;
    }

    if (listen(sockFd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    int cfd = -1;

    if ((cfd = accept(sockFd, (struct sockaddr *) &serverAddr,
                           (socklen_t *) &serverAddr))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char buffer[MAXLINE];
    printf("Setup UDP done, Listening...\n");
    while (1) {

        int n = recv(cfd, buffer, MAXLINE, 0);
        if (n < 0) {
            connected = false;
            break;
        }
        buffer[n] = '\0';

        handleClient(cfd, buffer, n);
    }

    close(cfd);
    shutdown(sockFd, SHUT_RDWR);



}

Socket::Socket() {

}

esp_err_t Socket::broadcast(const char *str, uint64_t time) {
    if (!connected) return ESP_OK;
    int n = send(clientFd, str, MSG_WAITALL, 0);
    if (n < 0) {
        connected = false;
    }
    return 0;
}

esp_err_t Socket::handleClient(int fd, const char *buffer, int len) {
    clientFd = fd;
    connected = true;
    printf("Got message: %s\n", buffer);
    return 0;
}