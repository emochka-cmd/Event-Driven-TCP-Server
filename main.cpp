#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> // sock_addrin
#include <arpa/inet.h> // inet_addr for sock
#include <unistd.h> // close sock


class Server {
private:
    int sock; 

    const int listen_count = 10;
    const int PORT = 8080;
    const int ADDR = INADDR_ANY;

    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    void bind_socket() {
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET; // IPv4
        server_addr.sin_port = htons(PORT); // use not sistem port
        server_addr.sin_addr.s_addr = ADDR; // any addr
        
        int bind_res = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (bind_res == -1) {
            std::cerr << "bind error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    int creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
        
        return sock_res;
    }

public:
    Server() : sock(creat_sock()){
        bind_socket(); 
        listening(); 
        std::cout << "Server created";
    } 

};
int main() {
    Server my_server;
}