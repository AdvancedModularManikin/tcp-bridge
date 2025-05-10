#pragma once

#include <iostream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>

#define MAX_NAME_LENGTH 40

class Client {
public:
    std::string id;
    std::string name;
    std::string uuid;
    std::string clientType;

    // Socket stuff
    int sock{};

    Client() {};

    void SetId(std::string id);

    void SetName(std::string &name);

    void SetClientType(std::string &clientType);

};

