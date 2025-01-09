#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "amm/BaseLogger.h"

#include "Client.h"
#include "ServerThread.h"

class Server {
public:
	explicit Server(int port);

	// Core server operations
	void AcceptAndDispatch();
	static void* HandleClient(void* args);

	// Client operations
	static void SendToAll(const std::string& message);
	static void SendToClient(Client* client, const std::string& message);
	static Client* GetClientByIndex(const std::string& id);
	static int FindClientIndex(Client* client);
	static void RemoveClient(Client* client);

	static std::mutex clientsMutex;

private:
	// Utility and internal operations
	static void ListClients();
	static void SendToAll(char* message);

	// Member variables
	int serverSock;
	struct sockaddr_in serverAddr, clientAddr;
	bool m_runThread;

	// Static shared resources
	static std::vector<Client> clients;

	static void CreateClient(Client *c, string &uuid);
};
