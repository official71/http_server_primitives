#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>

#define _SOMAXCONN 1024
#define BUFF_SIZE 1024
int ssocket = -1;

/**
 ** @brief error exit
 **
 ** @param std::string reason, reason of exit
 ** @return void
 */
static void panic(std::string reason)
{
    perror(reason.c_str());
    if (ssocket)
        close(ssocket);
    exit(1);
}

/**
 ** @brief request handler, recv from socket, send response, close socket
 **
 ** @param int sock, client socket (non-blocking)
 ** @return void
 */
static void handler(int sock)
{
    if (sock <= 0)
        return;

    for (;;) {
        char buff[BUFF_SIZE];
        memset(buff, 0, sizeof(buff));
        int rc = recv(sock, buff, sizeof(buff), 0);
        if (rc < 0) {
            if (errno == EWOULDBLOCK)
                break;
            // std::cout << "Failed to recv: " << errno << std::endl;
            close(sock);
            return;
        } else if (rc == 0) {
            // std::cout "Client closed socket " << sock << std::endl;
            close(sock);
            return;
        } else if (rc < BUFF_SIZE) {
            /* in reality, for http requests, "\r\n\r\n" marks the end
             * of the request; for simplicity just compare the recved length
             */
            break;
        }
    }
    
    usleep(1000);

    std::string resp(1024, 'a');
    if (send(sock, resp.c_str(), resp.length(), 0) < 0)
        std::cout << "Failed to send on socket " << sock << std::endl;
    
    close(sock);
}

/**
 ** @brief set socket to be non-blocking
 **
 ** @param int sock
 ** @return void
 */
static void set_nonblocking(const int sock)
{
    int fl = fcntl(sock, F_GETFL);
    if (fl < 0)
        panic("fcntl failed to get flags");
    if (fcntl(sock, F_SETFL, fl | O_NONBLOCK) < 0)
        panic("fcntl failed to set flags");
}

/**
 ** @brief main loop of http server
 **
 ** @param int port, port that the server runs on
 ** @return void
 */
void run(int port)
{
    ssocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ssocket < 0)
        panic("Failed to create server socket");

    int reuse = 1;
    if (setsockopt(ssocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0)
        panic("Failed to set socket reusable");

    set_nonblocking(ssocket);

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);
    if (::bind(ssocket, (struct sockaddr*)&saddr, sizeof(saddr)) < 0)
        panic("Failed to bind server socket");

    if (listen(ssocket, _SOMAXCONN) < 0)
        panic("Failed to listen");

    fd_set allfds;
    FD_ZERO(&allfds);
    FD_SET(ssocket, &allfds);
    int maxsocket = ssocket;

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    std::cout << "Server created on socket: " << ssocket << std::endl;
    std::cout << "Allow maximum concurrent connections: " << _SOMAXCONN << std::endl;

    for (;;) {

        fd_set tmpfds;
        FD_ZERO(&tmpfds);
        memcpy(&tmpfds, &allfds, sizeof(allfds));

        int nr = select(maxsocket + 1, &tmpfds, NULL, NULL, &timeout);

        if (nr < 0)
            panic("Failed to select");

        if (nr == 0)
            continue;

        for (int sock = 0; sock <= maxsocket && nr > 0; ++sock) {
            if (!FD_ISSET(sock, &tmpfds))
                continue;
            nr--;

            if (sock == ssocket) {
                // std::cout << "server socket readable" << std::endl;
                int clnt;
                do {
                    clnt = accept(ssocket, NULL, NULL);
                    if (clnt < 0) {
                        if (errno != EWOULDBLOCK)
                            panic("Failed to accept");
                        break;
                    }
                    set_nonblocking(clnt);
                    FD_SET(clnt, &allfds);
                    if (clnt > maxsocket)
                        maxsocket = clnt;
                } while (clnt >= 0);
            } else {
                // std::cout << "client socket " << sock << " readable" << std::endl;
                /* client socket readable */
                std::thread t(handler, sock);
                if (t.joinable())
                    t.detach();

                FD_CLR(sock, &allfds);
                if (sock == maxsocket) {
                    while (!FD_ISSET(maxsocket, &allfds))
                        maxsocket--;
                }
            }
        }
    }

    close(ssocket);
}

int main(void) 
{
    run(8080);

    return 0;
}