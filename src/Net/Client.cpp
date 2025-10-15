#include "Client.h"

void Client::SetName(const std::string &sname) {
    // Don't modify input parameter - truncate on assignment if needed
    if (sname.size() > MAX_NAME_LENGTH) {
        this->name = sname.substr(0, MAX_NAME_LENGTH);
    } else {
        this->name = sname;
    }
}

void Client::SetClientType(const std::string &sclientType) {
    this->clientType = sclientType;
}

void Client::SetId(std::string sid) { this->id = std::move(sid); }
