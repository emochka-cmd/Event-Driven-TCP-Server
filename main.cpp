#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unordered_map>
#include <sys/uio.h>
#include <netinet/in.h> // sock_addrin
#include <arpa/inet.h> // inet_addr for sock
#include <unistd.h> // close sock
#include <sys/epoll.h> // epoll
#include <fcntl.h> // for non block

class Server {
private:
    enum SERVER_STATUS {
        // need realize all
        ACTIVE,
        STARTING,
        STOPPED,
        REBOOTING,
        FAILED,
        MAINTENANCE
    };

    int sock; 
    int epoll_fd;
    SERVER_STATUS server_status; 

    enum STATUS {
        // NEW, // нужен ли?
        CONNECTED, // принят в accept, дальше в READING/CLOSED
        READING, // вызывает recv, далее в PROCESING/CLOSSED/ERROR
        PROCESSING, // данные полученны WRITING/READING/CLOSSED
        WRITING, // все отправленно READING/CLOSED/ERROR
        CLOSED,
        ERROR // CLOSED
    };

    enum ReadState {
        READ_LEN, // ждем длины
        READ_BODY // ждем сообщения
     };
    struct Client {
        STATUS status;
        uint16_t expected_len = 0; // ожидаемая длина сообщения
        uint16_t received_len = 0; // полученная длина сообщения
        ReadState read_state;
        std::string buffer;
    };
    

    std::unordered_map<int, Client> client_accepting; // первое значение - fd, второе status

    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY; // возможно заменить на "127.0.0.1"  
    
    const int listen_count = 10;
    const int buffer_size = 1024;
    const int max_events = 20;

    void creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            server_status = FAILED;
            exit(EXIT_FAILURE);
        }
        
        sock = sock_res;
        std::cout << "Socket is creating " << "\n";
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
            exit(0);
        }
        
        std::cout << "Socket in non-blocked mode" << "\n";
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
            server_status = FAILED;
            exit(EXIT_FAILURE);
        }
    }

    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            server_status = FAILED;
            exit(EXIT_FAILURE);
        }

        else {
            std::cout << "Start listening" << "\n";
        }
    }

    void create_epoll() {
        // запрос к ядру на размещение собитий
        epoll_fd = epoll_create1(0);
        
        if (epoll_fd == -1) {
            std::cerr << "Create_epool error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        } 

        else {
            register_server_fd();
            std::cout << "Create epool" << "\n";
        }
    }

    void register_server_fd() {
        struct epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.events = EPOLLIN; // more info in wiki
        event.data.fd = sock;
        // в данном случает с EPOLL_CTL_ADD добавляет целевой описатель fd в epoll
        int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event); // epoll поддерживает все описатели файлов, поддерживаемые poll(2)

        if (res == -1) {
            std::cerr << "Epoll reg server error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }

        else {
            std::cout << "Epool register" << "\n";
        }
    }

    void register_client_fd(const int& client_fd) {
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
            exit(EXIT_FAILURE);
            close(client_fd);
        }
        
        else {
            std::cout << "Client " << client_fd << " add in epoll" << "\n";
        }
    }

    void read_from_client(const int& fd) {
       Client& client = client_accepting[fd];

       while (true) {
            if (client.read_state == READ_LEN) {
                // чтение длины сообщения
                uint16_t net_len;
                ssize_t n = recv(fd, &net_len, sizeof(net_len), 0);

                if (n == 0) {
                    client.status = CLOSED;
                    return;
                }

                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }  
                    std::cerr << "Read len error: " << strerror(errno) << "\n";
                    client.status = ERROR;
                    return;
                }

                if (n != sizeof(net_len)) {
                    std::cerr << "Partial read of length\n";
                    client.status = ERROR;
                    return;
                }

                client.expected_len = ntohs(net_len);
                client.buffer.clear();
                client.buffer.reserve(client.expected_len);
                client.received_len = 0;
                client.read_state = READ_BODY;
            }

            if (client.read_state == READ_BODY) {
                char tmp[1024];
                size_t to_read = std::min(
                sizeof(tmp),
                static_cast<size_t>(client.expected_len - client.received_len));

                ssize_t n = recv(fd, &tmp, to_read, 0);
                
                if (n == 0) {
                    client.status = CLOSED;
                    return;
                }

                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }

                    client.status = ERROR;
                    return;
                }

                client.buffer.append(tmp, n);
                client.received_len += n;
                
                if (client.expected_len == client.received_len) {
                    client.status = PROCESSING;

                    std::cout << "Message from fd " << fd << ": [" << client.buffer << "]\n";

                    //к следующему сообщению
                    client.read_state = READ_LEN;
                    return;
                }
            }
        }
    }

    void run_server() {
        server_status = ACTIVE;

        while (server_status == ACTIVE) {
            struct epoll_event event[max_events];

            int num_events = epoll_wait(epoll_fd, event, max_events, -1);

            if (num_events == -1) {
                if (errno == EINTR) { // сигнал прервал системный вызов
                    continue;
                }
                std::cerr << "Epoll wait error: " << strerror(errno) << "\n";
                break; 
            }


            for (int i = 0; i < num_events; i++) {
                int fd = event[i].data.fd;
                uint32_t ev = event[i].events; 

                if (fd == sock) {
                    client_accept();
                }

                else if (ev & EPOLLIN) { // ready for read
                    
                    if (client_accepting.find(fd) != client_accepting.end()){
                        switch (client_accepting[fd].status) {

                            case CONNECTED:
                                read_from_client(fd);
                                std::cout << "client " << fd << " buffer: " << client_accepting[fd].buffer << " status: " << client_accepting[fd].status;
                                break;
                            
                            case READING:
                                break;

                            case PROCESSING:
                                break;

                            case WRITING:
                                break;

                            case ERROR:
                                break;

                            case CLOSED:
                                close(fd);
                                break;

                        }
                        
                    }

                    else {
                        std::cerr << "Client " << fd << " not find in client map";
                        close(fd);
                    }
                }

            }
        }
    }

    void client_accept() {
        struct sockaddr_in client_addr;
        
        socklen_t len_client_addr= sizeof(client_addr);
        int client_fd = accept(sock, (struct sockaddr*)&client_addr, &len_client_addr);

        if (client_fd == -1) {
            std::cerr << "client accept error: " << strerror(errno) << "\n";
        } 

        else {
            int flags = fcntl(client_fd, F_GETFL, 0);
            
            if (flags == -1) {
                std::cerr << "Error getting client socket flags: " << strerror(errno) << "\n";
                close(client_fd);
                return;
            }
        
            if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                std::cerr << "Error setting client non-blocking: " << strerror(errno) << "\n";
                close(client_fd);
                return;
            }

            // регистрация клиента
            client_accepting[client_fd].status = CONNECTED;
            client_accepting[client_fd].buffer = "";
            client_accepting[client_fd].read_state = READ_LEN;

            register_client_fd(client_fd); 
            std::cout << "Client--non-blocked accept, res: " << client_fd <<"\n";    
        }
    
    }

    void start_server() {
        server_status = STARTING;

        creat_sock();
        non_blocked_sock_mod();
        bind_socket();
        listening();
        create_epoll();
        
        //run_server();
    }


public:

    void get_server_status() {
          switch (server_status) {
            case ACTIVE:
                std::cout << "Server is ACTIVE and running normally" << std::endl;
                break;
            case STARTING:
                std::cout << "Server is STARTING up" << std::endl;
                break;
            case STOPPED:
                std::cout << "Server is STOPPED" << std::endl;
                break;
            case REBOOTING:
                std::cout << "Server is REBOOTING" << std::endl;
                break;
            case FAILED:
                std::cout << "Server has FAILED" << std::endl;
                break;
            case MAINTENANCE:
                std::cout << "Server is in MAINTENANCE mode" << std::endl;
                break;
            default:
                std::cout << "Unknown server status" << std::endl;
                break;
        }
    }
    
    std::string get_server_status_string() {
    switch (server_status) {
        case ACTIVE:       return "ACTIVE";
        case STARTING:     return "STARTING";
        case STOPPED:      return "STOPPED";
        case REBOOTING:    return "REBOOTING";
        case FAILED:       return "FAILED";
        case MAINTENANCE:  return "MAINTENANCE";
        default:           return "UNKNOWN";
    }
}

    void for_debag() {
        for (auto& c : client_accepting) {
            std::cout << "fd: " << c.first << " status: " << c.second.status << " BUFFER: " << c.second.buffer << "\n";
        }
    }

    void serv() {
        run_server();
    }

    Server () {
        start_server();
    }

};


class Client_Server {
private:
    int sock;

    int family = AF_INET; 
    const int PORT = 8080;
    const char* ADDR = "127.0.0.1";


    int creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "client socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    
        return sock_res;
    }

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

        else {
        std::cout << "Client connect SUCCSESS" << "\n";
        }

    }

    void send_message(const std::string& message) {
        // [16 bit - len][n bit - data]
        const uint16_t MAX_LENGHT = 65535;
        
        if (message.length() > MAX_LENGHT) {
            std::cerr << "Message from user " << sock << " to big. Max - " << MAX_LENGHT
        << " bytes";
            return;
        }
        
        uint16_t len_message = static_cast<uint16_t>((message.length())); 
        uint16_t net_len = htons(len_message);
        // создание буферов буфера для отправки
        struct iovec iov[2];

        // первый для длины
        iov[0].iov_base = &net_len;
        iov[0].iov_len = sizeof(net_len);

        // второй для сообщения
        iov[1].iov_base = const_cast<char*>(message.data());
        iov[1].iov_len = message.length();

        // изменение счетчиков
        size_t total_to_send = sizeof(net_len) + message.length();
        size_t already_send = 0;


        // отправка
        while (already_send < total_to_send) {
            ssize_t curr_sent = writev(sock, iov, 2);

            if (curr_sent > 0) {
                already_send += curr_sent;
                
                // обновление iov
                while (curr_sent > 0) {
                    if (iov[0].iov_len > 0) {
                        if (static_cast<size_t>(curr_sent) >= iov[0].iov_len) {
                            curr_sent -= iov[0].iov_len;
                            iov[0].iov_len = 0;
                            iov[0].iov_base = nullptr;
                        }

                        else {
                            iov[0].iov_len -= curr_sent;
                            iov[0].iov_base = static_cast<char*>(iov[0].iov_base) + curr_sent; 
                            curr_sent = 0;
                        }
                    }

                    else if (iov[1].iov_len > 0) {
                        if (static_cast<size_t>(curr_sent) >= iov[1].iov_len) {
                            curr_sent -= iov[1].iov_len;
                            iov[1].iov_len = 0;
                            iov[1].iov_base = nullptr;
                        }

                        else {
                            iov[1].iov_len -= curr_sent;
                            iov[1].iov_base = static_cast<char*>(iov[1].iov_base) + curr_sent; 
                            curr_sent = 0;
                        }
                    }
                }
                
                continue;
            }

            if (curr_sent == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // в будующем переделать для производительности
                    usleep(1000);
                    continue;
                }
            }

            std::cerr << "Send error: " << strerror(errno) << "\n";
            return; 
        }
        std::cout << "Send message SUCCESS (" << already_send << " bytes)\n";
    }


public:
    Client_Server() : sock(creat_sock()) {
        connect_to_server();
    }

    void create_message(const std::string& message){
        send_message(message);
    }
};


int main() {
    Server my_server;

    Client_Server my_client1, my_client2, my_client3;

    my_client1.create_message("fffffasfddasdaswds");
    my_client2.create_message("ffff");
    my_client3.create_message("fffffasfddasdaswdsfesf");

    // for debaging
    my_server.serv();
    
    //my_server.for_debag();

    
}