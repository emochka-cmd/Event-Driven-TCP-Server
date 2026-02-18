#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h> // sock_addrin
#include <arpa/inet.h> // inet_addr for sock
#include <unistd.h> // close sock
#include <sys/epoll.h> // epoll
#include <fcntl.h> // for non block


class Epoll {
private:
    int epoll_fd;
    const int server_socket;

    bool error = false;

    void create_epoll() {
        // запрос к ядру на размещение собитий
        epoll_fd = epoll_create1(0);
        
        if (epoll_fd == -1) {
            std::cerr << "epoll_create1: " << strerror(errno) << "\n";
            throw std::runtime_error("Failed to create epoll");
            error = true;
            return;
        } 

        std::cout << "Create epool" << "\n";
    }

    void epoll_registred() {
        struct epoll_event event;
        std::memset(&event, 0, sizeof(event));

        event.events = EPOLLIN; 
        event.data.fd = server_socket;
        
        int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event); // epoll поддерживает все описатели файлов, поддерживаемые poll(2)

        if (res == -1) {
            std::cerr << "Epoll reg server error: " << strerror(errno) << "\n";
            error = true;
            return;
        }

        else {
            std::cout << "Epool register" << "\n";
        }
    }

public:

    int get_fd () {
        return epoll_fd;
    }

    int modificate_client_fd(int client_fd, uint32_t events) {
        struct epoll_event event;

        event.events = events;
        event.data.fd = client_fd;

        int res = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event);

        if (res == -1) {
            std::cerr << "Epoll mod client error: " << strerror(errno) << "\n";
        }

        return res;
    }
    


    int register_client_fd(const int& client_fd) {
        struct epoll_event event;
        /*
            EPOLLIN - Ассоциированный файл доступен для операций read(2). 
            EPOLLOUT - Ассоциированный файл доступен для операций write(2). 
            EPOLLET - Устанавливает поведение Edge Triggered для ассоциированного описателя файлов
        */

        std::memset(&event, 0, sizeof(event));

        event.events = EPOLLIN; // далее добавить epollet
        event.data.fd = client_fd;
        
        int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

        if (res == -1) {
            std::cerr << "Epoll reg client error: " << strerror(errno) << "\n";
            close(client_fd);
            return -1;
        }
        
        else {
            std::cout << "Client " << client_fd << " add in epoll" << "\n";
            return res;
        }
    }


    explicit Epoll (int server_socket) : 
    server_socket(server_socket)
    {
        create_epoll();
        epoll_registred();

        if (error == true) {
            close(epoll_fd);
        }
    }

    ~Epoll () {
        if (epoll_fd != -1) {
            close(epoll_fd);
        }
    }
};