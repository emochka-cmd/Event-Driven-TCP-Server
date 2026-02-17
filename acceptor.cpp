#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h> 
#include <fcntl.h>


class Acceptor {
private:
    int sock = -1;

    const int family; 
    const int PORT;
    const char* ADDR;

    const int listen_count;



    void creat_sock() {
        int sock_res = socket(family, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
        
        sock = sock_res;
        std::cout << "server socket is creating..." << "\n";
    }


    void non_blocked_sock_mod() {
        /*
        F_GETFL - читает флаги файлового дескриптора
        F_SETFL - устанавливает часть флагов. В Linux данная команда может изменять только флаги O_APPEND, O_NONBLOCK, O_ASYNC и O_DIRECT. 
        */


        int flags = fcntl(sock, F_GETFL, 0);
        
        if (flags == -1) {
            std::cerr << "Error getting socket flags: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }

        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "Error setting non-blocking mode: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
        
        std::cout << "Sever socket in non-blocked mode" << "\n";
    }


    void bind_socket() {
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = family; 
        server_addr.sin_port = htons(PORT);
        server_addr.sin_addr.s_addr = inet_addr(ADDR); 

        int bind_res = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (bind_res == -1) {
            std::cerr << "bind error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }
    }


    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }

            std::cout << "Start listening" << "\n";
    }


public:


    int client_accept() {
        /* в случае ошибки - возвращаеться -1*/
        struct sockaddr_in client_addr;
        
        socklen_t len_client_addr= sizeof(client_addr);
        int client_fd = accept(sock, (struct sockaddr*)&client_addr, &len_client_addr);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -1;
            }
            std::cerr << "client accept error: " << strerror(errno) << "\n";
            return -1;
        } 

        int flags = fcntl(client_fd, F_GETFL, 0);
            
        if (flags == -1) {
            std::cerr << "Error getting client socket flags: " << strerror(errno) << "\n";
            close(client_fd);
            return - 1;
        }
        
        if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "Error setting client non-blocking: " << strerror(errno) << "\n";
            close(client_fd);
            return -1;
        }  

        std::cout << "Client non-blocking accept\n";
        return client_fd;
    
    }


    explicit Acceptor (
        int family = AF_INET,
        int PORT = 8080, 
        const char* ADDR = "127.0.0.1", 
        int listen_count = 10
    ) :

    family(family), PORT(PORT), ADDR(ADDR), listen_count(listen_count)
        
    {
        creat_sock();
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