#include <unordered_map>
#include "client.hpp"


class ConnectionManager {
private:
    enum Status {
        READ,
        WRITE,
        CLOSED
    };

    struct Connection {
        Status status;
    };

    std::unordered_map<int, Connection>;

public:
    ConnectionManager () {

    }


    ~ConnectionManager () {

    }
};