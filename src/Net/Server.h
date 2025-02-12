// Server.h
#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <string_view>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <system_error>

#include "amm/BaseLogger.h"
#include "Client.h"
#include "ServerThread.h"

class Server {
public:
	explicit Server(int port);
	~Server();

	// Delete copy and move operations
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;
	Server(Server&&) = delete;
	Server& operator=(Server&&) = delete;

	void AcceptAndDispatch();

	static void SendToAll(const std::string& message);
	static void SendToClient(const std::shared_ptr<Client>& client, const std::string& message);
	static std::shared_ptr<Client> GetClientByIndex(const std::string& id);
	static void RemoveClient(const std::shared_ptr<Client>& client);

	static std::mutex& GetClientsMutex() { return clientsMutex; }

private:
	static void HandleClient(const std::shared_ptr<Client>& client);
	static void ListClients();
	void setNonBlocking(int sockfd);
	static void sendLargeMessage(int sockfd, const std::string& message, size_t chunkSize = 8192);
	void cleanup();

	int serverSock;
	struct sockaddr_in serverAddr{}, clientAddr{};
	std::atomic<bool> m_runThread{true};

	static std::mutex clientsMutex;
	static std::vector<std::shared_ptr<Client>> clients;

	std::vector<std::unique_ptr<ServerThread>> clientThreads;
	std::mutex threadsMutex;

	static constexpr int MAX_LISTEN_BACKLOG = 30;
	static constexpr size_t BUFFER_SIZE = 8192;
};