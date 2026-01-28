#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unordered_map>
#include <netinet/in.h> // sock_addrin
#include <arpa/inet.h> // inet_addr for sock
#include <unistd.h> // close sock
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

    struct Client {
        STATUS status;
        std::string buffer;
    };

    std::unordered_map<int, Client> client_accepting; // первое значение - fd, второе status

    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY; // возможно заменить на "127.0.0.1"  
    
    const int listen_count = 10;
    const int buffer_size = 1024;


    int creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            server_status = FAILED;
        }
        
        std::cout << "Socket is creating." << "\n";
        return sock_res;
    }

    void non_blocked_sock_mod() {
        int flags = fcntl(sock, F_GETFL, 0);
        
        if (flags == -1) {
            std::cerr << "Error getting socket flags: " << strerror(errno) << "\n";
            exit(0); 
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
        }
    }

    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            server_status = FAILED;
        }
    }

    void start_server() {
        server_status = STARTING;

        sock = creat_sock();
        non_blocked_sock_mod();
        bind_socket();
        listening();
    }


    void run_server() {
        server_status = ACTIVE;

        while (server_status == ACTIVE) {
            client_accept();
            for (auto& client : client_accepting) {
                switch (client.second.status) {
                    //case (NEW):
                    
                    case (CONNECTED): // прием сообщения
                        read_from_client(client.first);
                        break;

                     case (READING):
                        std::cout << "need make reading" << "\n";
                        break;

                    case (PROCESSING):
                        std::cout << "need make processing" << "\n";
                        break;

                    case (WRITING):
                        std::cout << "need make writing" << "\n";
                        break;

                    case (ERROR): // возможно добавление обработки в будующем
                        client_accepting[client.first].status = CLOSED; 
                        break;

                    case (CLOSED): // удаление
                        client_accepting.erase(client.first);
                        break;

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
            client_accepting[client_fd] = {CONNECTED, ""};
            std::cout << "Client accept, res: " << client_fd <<"\n";    
        }
    
    }

    void read_from_client(const int& fd) {
        char buffer[buffer_size];
        
        ssize_t reed_res = recv(fd, buffer, sizeof(buffer) - 1, 0);

        if (reed_res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            client_accepting[fd].status = ERROR;
            std::cerr << "Read client " << fd << " " << strerror(errno) << "\n";
            
            return;
        }

        if (reed_res == 0) {
            client_accepting[fd].status = CLOSED;
            std::cerr << "Client " << fd << " closed connection" << "\n"; 
            return;
        }

        buffer[reed_res] = '\0'; 
        client_accepting[fd].buffer += buffer;
        
        std::cout << "Client " << fd << " send " << reed_res << " bytes" << "\n";

        if (client_accepting[fd].buffer.find('\n') != std::string::npos) {
            // проверка, полное ли сообщение
            client_accepting[fd].status = PROCESSING;
        }
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
        usleep(666);
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
        std::cout << "Client connect SUCCSESS." << "\n";
        }

    }

    void send_message(const std::string& message) {
        size_t alredy_send = 0, need_send = message.length();
        const char* data = message.c_str();

        while (alredy_send <= need_send) {
            size_t send_res = send(sock, data + alredy_send, need_send - alredy_send, 0); // last - flags, more in man send
            
            if (send_res == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    usleep(1000);
                    continue;
                }

                std::cerr << "Send Error: " << strerror(errno) << "\n";
            }
            
            if (send_res == 0) {
                std::cerr << "connection closed\n";
                return;
            }

            alredy_send += send_res;
            need_send -= send_res;
        }
        
        std::cout << "Send message SUCCSESS" << "\n";
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

    Client_Server my_client;

    my_client.create_message("fffffasfddasdaswds");

    // for debaging
    my_server.serv();

    my_server.for_debag();
}