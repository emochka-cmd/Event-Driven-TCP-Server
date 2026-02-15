#include <cstdlib>
#include <cstring>
#include <netinet/in.h> // sock_addrin
#include <arpa/inet.h> // inet_addr for sock
#include <sys/socket.h>
#include <unistd.h> // close sock
#include <cerrno>
#include <iostream>
#include <fcntl.h> 

class Acceptor {
private:
    int sock;
    
    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY;

    const int listen_count = 10; 

    void creat_socket() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
        
        sock = sock_res;
        std::cout << "Socket is creating " << "\n";

    }

    void bind_socket() {
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = family;
        server_addr.sin_port = htons(PORT); 
        server_addr.sin_addr.s_addr =htonl(ADDR); 
        
        int bind_res = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (bind_res == -1) {
            std::cerr << "bind error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }

    }

    void non_blocked_sock_mod() {
        int flags = fcntl(sock, F_GETFL, 0);
        
        if (flags == -1) {
            std::cerr << "Error getting socket flags: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }

        int res = fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        if (res == -1) {
            std::cerr << "Error setting non-blocking mode: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
        
        std::cout << "Socket in non-blocked mode" << "\n";
    }

    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }

        else {
            std::cout << "Start listening" << "\n";
        }
    }

public:
    int client_accept() {
        struct sockaddr_in client_addr;
        socklen_t len_client_addr = sizeof(client_addr);
        
        int client_fd = accept(sock, (struct sockaddr*)&client_addr, &len_client_addr);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -1;
            }

            std::cerr << "accept error: " << strerror(errno) << "\n";
            return -1;
        }

        // Установить неблокирующий флаг
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags == -1) {
            std::cerr << "error getting client socket flags: " << strerror(errno) << "\n";
            close(client_fd);
            return -1;
        }

        if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "Error setting client non-blocking: " << strerror(errno) << "\n";
            close(client_fd);
            return -1;
        }

        return client_fd;
    }


    /*
    в будущем изменить:
    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY;

    const int listen_count = 10; 
    */
    
    Acceptor() {
        creat_socket();
        non_blocked_sock_mod();
        bind_socket();
        
        listening();
    }

    ~Acceptor () {
        if (sock != -1) {
            close(sock);
        }
    }
};