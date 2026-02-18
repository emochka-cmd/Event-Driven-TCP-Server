#include <cstdint>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>
#include "client.hpp"


#include "acceptor.hpp"
#include "epoll.hpp"
#include "threadpool.hpp"

class ConnectionManager {
private:
    enum class State : uint8_t {
        READING_LEN,
        READING_MSG,
        WRITING,
        CLOSED
    };

    struct Connection {
        int fd;
        State state;
        uint16_t expected_len;        // ожидаемая длина
        std::vector<char> read_buffer;// буфер для накопления
        std::string write_buffer; // буфер для отправки

        Connection(int fd) : fd(fd), state(State::READING_LEN), expected_len(0) {}
    };

    Acceptor& acceptor_;
    Epoll& epoll_;
    ThreadPool& threadpool_;
    std::unordered_map<int, Connection> connections_;


    void update_events(int fd, uint32_t add, uint32_t remove) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        //  в будещем изменить для add и remove
        uint32_t new_events = EPOLLIN;

        if (it->second.state == State::WRITING || !it->second.write_buffer.empty()) {
            new_events |= EPOLLOUT;
        }

        epoll_.modificate_client_fd(fd, new_events);
    }
    

    void close_connection(int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;

        std::cout << "Closing connection " << fd << "\n";
        close(fd);
        connections_.erase(it);
    }


    
public:


    ConnectionManager () {

    }

    ~ConnectionManager () {

    }
};