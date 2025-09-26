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
#include <unordered_map>

// Constants for server configuration
const int DEFAULT_SERVER_BACKLOG = 30;
const int DEFAULT_UDP_SELECT_TIMEOUT_SEC = 1;
const int DEFAULT_SEND_SELECT_TIMEOUT_USEC = 100000; // 100ms
const int DEFAULT_THREAD_CLEANUP_INTERVAL_SEC = 5;
const int DEFAULT_ERROR_RETRY_DELAY_MS = 100;

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
	static bool IsClientConnected(Client* client);
	static void CleanupDisconnectedClient(Client* client);

	static std::vector<Client*> clients;
	static std::unordered_map<std::string, Client*> clientsById; // Fast lookup by ID
	static std::mutex clientsMutex;
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