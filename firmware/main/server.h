//
// Created by Braden Nicholson on 2/6/23.
//

#ifndef RADAR_SERVER_H
#define RADAR_SERVER_H
#include <esp_event.h>
#include <esp_log.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include <esp_http_server.h>
#include <cJSON.h>
#include <esp_timer.h>
static httpd_handle_t hd{};
static int fd = -1;

static void setSession(httpd_req_t *ses) {
    hd = ses->handle;
    fd = httpd_req_to_sockfd(ses);
}

class Server{


public:

    Server();

private:

    httpd_handle_t server{};

};


#endif //RADAR_SERVER_H
