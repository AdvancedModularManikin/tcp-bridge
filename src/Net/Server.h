#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "Client.h"
#include "ServerThread.h"

using namespace std;

class Server {

private:
	int serverSock;
    struct sockaddr_in serverAddr, clientAddr;

public:
    explicit Server(int port);

    void AcceptAndDispatch();

    static void *HandleClient(void *args);

    static void SendToAll(const std::string &message);

    static void SendToClient(Client *c, const std::string &message);

    static Client *GetClientByIndex(std::string id);

	static int FindClientIndex(Client *c);

	static vector<Client> clients;
private:
    static void ListClients();

    static void SendToAll(char *message);

protected:
    bool m_runThread;
};

