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

    sockaddr_in server_addr;

    if (argc >= 2) {
        if (inet_pton(AF_INET, argv[1], &(server_addr.sin_addr)) != 1) {
            printf("Provided Address Argument Not Valid\n");
            server_addr = find_server();
        }
    } else {
        server_addr = find_server();
    }

    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_family = AF_INET;

    int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock_fd < 0) {
        fatal_error("Failed To Create Socket");
    } 

    if (connect(sock_fd, (sockaddr*)&server_addr, sizeof(sockaddr)) < 0) {
        fatal_error("Failed To Connect To Server");
    }

    char send_buffer[1024] = { }; 
    char recv_buffer[1024] = { }; 

    while (true) {
        printf("<-- ");
        fgets(send_buffer, LENGTHOF(send_buffer), stdin);

        size_t len = strcspn(send_buffer, "\n");

        send_buffer[len] = '\0';
        fflush(stdin);

        if (len != 0) {
            int ret = send(sock_fd, send_buffer, strlen(send_buffer), 0);
            ret = recv(sock_fd, recv_buffer, LENGTHOF(recv_buffer), 0);

            if (ret <= 0) {
                // stuff
            }

            printf("--> %.*s\n", ret, recv_buffer);
        }
    }

exit:
    close(sock_fd);
}