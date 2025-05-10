#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>

#include "amm/BaseLogger.h"

#include "Client.h"
#include "ServerThread.h"

class Server {
public:
	Server(int port);
	~Server(); // Add this line to declare the destructor

	void AcceptAndDispatch();
	static void* HandleClient(void*);

	static void SendToAll(std::string const& message);
	static void SendToAll(char* message);
	static void SendToClient(Client* client, std::string const& message);

	static void ListClients();
	static void RemoveClient(Client* client);
	static int FindClientIndex(Client* client);
	static Client* GetClientByIndex(std::string const& id);
	static void CreateClient(Client* c, std::string& uuid);

	static std::vector<Client*> clients;
	static std::mutex clientsMutex;
	static std::mutex sendMutex;
	bool m_runThread;

private:
	int serverSock;
	struct sockaddr_in serverAddr;
	struct sockaddr_in clientAddr;

	std::vector<std::unique_ptr<ServerThread>> clientThreads;
	std::mutex threadsMutex;
	void CleanupCompletedThreads();

	// Add a thread monitor method
	void MonitorThreads();
	std::atomic<bool> monitorRunning{false};
	std::unique_ptr<std::thread> monitorThread;
};

#endif // SERVER_H