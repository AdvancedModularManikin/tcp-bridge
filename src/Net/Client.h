#pragma once

#include <iostream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <unistd.h>  // For close()

#define MAX_NAME_LENGTH 40

class Client {
public:
    std::string id;
    std::string name;
    std::string uuid;
    std::string clientType;

    // Socket stuff
    int sock{};

    Client() = default;

    ~Client() {
        // Close socket if still open to prevent resource leak
        if (sock > 0) {
            close(sock);
            sock = -1;
        }
    }

    // Disable copy to prevent double-close of socket
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Enable move semantics
    Client(Client&& other) noexcept
        : id(std::move(other.id))
        , name(std::move(other.name))
        , uuid(std::move(other.uuid))
        , clientType(std::move(other.clientType))
        , sock(other.sock)
    {
        other.sock = -1;  // Transfer ownership
    }

    Client& operator=(Client&& other) noexcept {
        if (this != &other) {
            // Close existing socket
            if (sock > 0) {
                close(sock);
            }

            // Move data
            id = std::move(other.id);
            name = std::move(other.name);
            uuid = std::move(other.uuid);
            clientType = std::move(other.clientType);
            sock = other.sock;
            other.sock = -1;
        }
        return *this;
    }

    void SetId(std::string id);

    void SetName(const std::string &name);

    void SetClientType(const std::string &clientType);

};

