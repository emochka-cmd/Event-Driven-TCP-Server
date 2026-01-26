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


int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0); // создает новый сокет и возвращает файловый дискриптор
    
    if (sock == -1) {
        std::cerr << "socket error" << strerror(errno) << "\n";
        exit(EXIT_FAILURE);
    }

    // настройка адреса сервера
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(8080); // т.к не системное 
    server_addr.sin_addr.s_addr = INADDR_ANY; // для сервера доступны все интерфейсы
    int bind_res =  bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)); // привязка сокета к его адресу и порту

    if (bind_res == -1) {
        std::cerr << "bind error" << strerror(errno) << "/n";
        close(sock);
        exit(EXIT_FAILURE);
    }

    // реализация listen
}