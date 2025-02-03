#include "shared.h"

sockaddr_in find_server() {
    int listen_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (listen_fd < 0) {
        fatal_error("Failed To Create Listening Socket");
    }

    sockaddr_in listen_addr = { 0 };
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(BROADCAST_PORT);
    listen_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (sockaddr*)&listen_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Bind Broadcast Listening Socket");
    }

    timeval timeout = { 0 };
    timeout.tv_usec = 10000;

    if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        fatal_error("Failed To Put Listening Socket Into Non-Blocking Mode");
    }

    sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(sockaddr);
    char buffer[10]; // totally random size

    printf("Listening for server...\n");

    // oh no! this isn't nasa
    while (true) {
        int ret = recvfrom(listen_fd, buffer, sizeof(buffer), 0, 
            (sockaddr*)&server_addr, &server_addr_len);

        if (ret <= 0 && errno != EWOULDBLOCK) {
            fatal_error("Failed To Recieve Data On Listening Socket");
        }

        if (strncmp(buffer, MAGIC_TEXT, sizeof(MAGIC_TEXT)) == 0) {
            printf("Found Server %s:%hu\n", inet_ntoa(server_addr.sin_addr), SERVER_PORT);
            break;
        }
    }

    close(listen_fd);

    return server_addr;
}

int main(int argc, char** argv) {
    sockaddr_in server_addr = find_server();
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_family = AF_INET;

    int conn_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (conn_fd < 0) {
        fatal_error("Failed To Create Socket");
    } 

    if (connect(conn_fd, (sockaddr*)&server_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Connect To Server");
    }

    char input_buffer[1024]; 

    while (true) {
        printf("Enter Command --> ");
        fgets(input_buffer, sizeof(input_buffer), stdin);
        fflush(stdin);
        send(conn_fd, input_buffer, strlen(input_buffer), 0);
    }
}