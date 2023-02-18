//
// Created by Braden Nicholson on 2/15/23.
//

#ifndef RADAR_SOCKET_H
#define RADAR_SOCKET_H

#include "vector"

using std::vector;


class Socket {

public:
    Socket();

    esp_err_t broadcast(const char *str, uint64_t time);
    void configureUDP();
private:
    bool connected = false;

    int clientFd;

    esp_err_t handleClient(int fd, const char *buffer, int len);
};


#endif //RADAR_SOCKET_H
