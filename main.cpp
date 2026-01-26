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

    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY;
    
    const int listen_count = 10;


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
        server_addr.sin_family = family; // IPv4
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
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    
        return sock_res;
    }

public:
    Server() : sock(creat_sock()){
        bind_socket(); 
        listening(); 
        std::cout << "Server created" << "\n";
    } 

};


class Client {
private:
    int sock;

    int family = AF_INET; 
    const int PORT = 8080;
    const char* ADDR = "127.0.0.1";


    void connect_to_server() {
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = family; // IPv4
        server_addr.sin_port = htons(PORT); // use not sistem port

        if (inet_pton(family, ADDR, &server_addr.sin_addr) <= 0) { // give cp and copi in server_addr.sin_addr
            std::cerr << "invalid server addr" << strerror(errno) << "\n";
            close(sock);
            // errror!
        }

        int connect_result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (connect_result == -1) {
            std::cerr << "client connection error: " << strerror(errno) << "\n";
        }

        std::cout << "Client connect successful" << "\n";
    }

    int creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "client socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    
        return sock_res;
    }

public:
    Client() : sock(creat_sock()) {
        connect_to_server();
    }
};

int main() {
    Server my_server;
    Client my_client;
}