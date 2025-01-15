#include "Server.h"

// Static members
std::vector<Client> Server::clients;

// Constructor
Server::Server(int port) {
	int yes = 1;
	m_runThread = true;

	// Initialize the server socket
	serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSock < 0) {
		throw std::runtime_error("Failed to create socket");
	}

	// Configure the server address
	memset(&serverAddr, 0, sizeof(sockaddr_in));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);

	// Set socket options
	setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	setsockopt(serverSock, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int));

	// Bind the socket
	if (bind(serverSock, (struct sockaddr *) &serverAddr, sizeof(sockaddr_in)) < 0) {
		throw std::runtime_error("Failed to bind socket");
	}

	// Start listening
	if (listen(serverSock, 30) < 0) {
		throw std::runtime_error("Failed to listen on socket");
	}

	LOG_INFO << "Server initialized on port " << port;
}

// Accept new connections and dispatch them to threads
void Server::AcceptAndDispatch() {
	socklen_t cliSize = sizeof(sockaddr_in);

	while (m_runThread) {
		auto *client = new Client();
		client->sock = accept(serverSock, (struct sockaddr *) &clientAddr, &cliSize);
		if (client->sock >= 0) {
			setNonBlocking(client->sock); // Set the client socket to non-blocking
		}

		if (client->sock < 0) {
			LOG_ERROR << "Error on accept: " << strerror(errno);
			delete client;

			if (errno == EINTR) {
				continue; // Retry on interrupt
			} else {
				break; // Exit on fatal error
			}
		}

		// Handle new client connection
		try {
			auto *clientThread = new ServerThread();
			clientThread->Create((void *) Server::HandleClient, client);
		} catch (...) {
			LOG_ERROR << "Failed to create thread for client";
			close(client->sock);
			delete client;
		}
	}

	close(serverSock);
	LOG_INFO << "Server stopped accepting connections.";
}

// Send a message to all clients
void Server::SendToAll(const std::string &message) {
	std::vector<Client> tempClients;

	// Step 1: Copy the clients list while holding the lock
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		tempClients = clients; // Copy clients list
	}

	std::vector<std::string> disconnectedClients;

	// Step 2: Send the message to each client
	for (auto &client: tempClients) {
		try {
			sendLargeMessage(client.sock, message); // Use the sendLargeMessage function
		} catch (const std::exception &e) {
			LOG_ERROR << "Error sending to client " << client.id << ": " << e.what();
			disconnectedClients.push_back(client.id); // Mark client for removal
		}
	}

	// Step 3: Remove disconnected clients while holding the lock
	if (!disconnectedClients.empty()) {
		std::lock_guard<std::mutex> lock(clientsMutex);

		for (const auto &id: disconnectedClients) {
			auto it = std::remove_if(clients.begin(), clients.end(),
			                         [&id](const Client &c) { return c.id == id; });
			if (it != clients.end()) {
				LOG_INFO << "Removing disconnected client: " << id;
				shutdown(it->sock, SHUT_RDWR);
				close(it->sock);
				clients.erase(it);
			}
		}
	}
}

// Overloaded version to send a C-string
void Server::SendToAll(char *message) {
	SendToAll(std::string(message));
}

// Send a message to a specific client
void Server::SendToClient(Client *client, const std::string &message) {
	if (!client) {
		LOG_WARNING << "Attempted to send to a null client.";
		return;
	}

	try {
		sendLargeMessage(client->sock, message); // Use the sendLargeMessage function
	} catch (const std::exception &e) {
		LOG_ERROR << "Error sending to client " << client->id << ": " << e.what();

		// Handle client disconnection
		{
			std::lock_guard<std::mutex> lock(clientsMutex);

			// Find the client in the list and remove it
			auto it = std::remove_if(clients.begin(), clients.end(),
			                         [&client](const Client &c) { return c.id == client->id; });
			if (it != clients.end()) {
				LOG_INFO << "Removing disconnected client: " << client->id;
				shutdown(client->sock, SHUT_RDWR);
				close(client->sock);
				clients.erase(it);
			} else {
				LOG_WARNING << "Client not found for removal: " << client->id;
			}
		}
	}
}

void Server::sendLargeMessage(int sockfd, const std::string &message, size_t chunkSize) {
	size_t totalSize = message.size();
	size_t offset = 0;

	while (offset < totalSize) {
		size_t bytesToSend = std::min(chunkSize, totalSize - offset);
		ssize_t sent = send(sockfd, message.c_str() + offset, bytesToSend, 0);

		if (sent < 0) {
			perror("send error");
			throw std::runtime_error("Failed to send message.");
		}

		offset += sent;
	}
}

// List all connected clients
void Server::ListClients() {

	for (const auto &client: clients) {
		LOG_TRACE << "|" << client.name << "|" << client.clientType << endl;
	}
}

// Remove a client from the list and clean up resources
void Server::RemoveClient(Client *client) {

	int index = FindClientIndex(client);
	if (index != -1) {
		clients.erase(clients.begin() + index);
		LOG_INFO << "Client removed: " << client->id;
	} else {
		LOG_ERROR << "Client not found for removal: " << client->id;
	}

	shutdown(client->sock, SHUT_RDWR);
	close(client->sock);
}

// Find the index of a client in the list
int Server::FindClientIndex(Client *client) {


	for (size_t i = 0; i < clients.size(); ++i) {
		if (clients[i].id == client->id) {
			return static_cast<int>(i);
		}
	}
	LOG_ERROR << "Client ID not found: " << client->id;
	return -1;
}

// Get a client by ID
Client *Server::GetClientByIndex(const std::string &id) {

	for (auto &client: clients) {
		if (client.id == id) {
			return &client;
		}
	}
	return nullptr;
}


void Server::setNonBlocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl F_GETFL");
		return;
	}
	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl F_SETFL");
	}
}