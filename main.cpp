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

enum STATUS {
    NEW, //
    CONNECTED, // принят в accept, дальше в READING/CLOSED
    READING, // вызывает recv, далее в PROCESING/CLOSSED/ERROR
    PROCESSING, // данные полученны WRITING/READING/CLOSSED
    WRITING, // все отправленно READING/CLOSED/ERROR
    CLOSED,
    ERROR // CLOSED
};

std::unordered_map<int, STATUS> client_accepting; // первое значение - fd, второе концепт, пока бред

class Server {
private:
    int sock; 



    


    const int family = AF_INET; 
    const int PORT = 8080;
    const int ADDR = INADDR_ANY; // возможно заменить на "127.0.0.1"  
    
    const int listen_count = 10;


    int creat_sock() {
        int sock_res = socket(AF_INET, SOCK_STREAM, 0); 
        
        if (sock_res == -1) {
            std::cerr << "server socket error: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    
        return sock_res;
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

    void listening() {
        int listen_res = listen(sock, listen_count);

        if (listen_res == -1) {
            std::cerr << "lesten error: " << strerror(errno) << "\n";
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    void client_accept() {
        struct sockaddr_in client_addr;
        
        socklen_t len_client_addr= sizeof(client_addr);
        int accept_res = accept(sock, (struct sockaddr*)&client_addr, &len_client_addr);
        
        if (accept_res == -1) {
            std::cerr << "client accept error: " << strerror(errno) << "\n";
        } 

        else {

            std::cout << "Client accept, res: " << accept_res <<"\n";
             
        }
        
    }

    void get_message() {
        for (const auto& curr_client : client_accepting) {
            
            if (curr_client.second == CONNECTED) {

                char buffer[4096];
                std::memset(&buffer, 0, sizeof(buffer));

                int recv_res = recv(curr_client.first, buffer, sizeof(buffer) - 1, 0);

                if (recv_res > 0) {
                    buffer[recv_res] = '\0';

                    std::string message(buffer);
                    std::cout << "get message: " << message << "\n";
                }

                if (recv_res == -1) {
                    std::cerr << "Error in get_message" << strerror(errno) << "\n"; 
                }

                client_accepting[curr_client.first] = READING;
            }

        }
    }


public:
    void start() {
        sock = creat_sock();
        bind_socket();
        listening();
    }

    void accept_client() {
        client_accept();
    }

    void message_get(){
        get_message();
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

        std::cout << "Client connect SUCCSESS." << "\n";
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
    my_server.start();

    sleep(1);

    Client_Server my_client;

    my_server.accept_client();

    my_client.create_message("fffffasfddasdaswds");

    my_server.message_get();
}