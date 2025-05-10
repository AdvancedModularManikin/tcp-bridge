#include "Client.h"

void Client::SetName(std::string &sname) {
    if (sname.size() > MAX_NAME_LENGTH) {
        sname.resize(MAX_NAME_LENGTH);
    }
    this->name = sname;
}

void Client::SetClientType(std::string &sclientType) {
    this->clientType = sclientType;
}

void Client::SetId(std::string sid) { this->id = std::move(sid); }
