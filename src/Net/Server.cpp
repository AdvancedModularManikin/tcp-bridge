#include "Server.h"

// Static members
vector<Client> Server::clients;

// Constructor
Server::Server(int port) {
	int yes = 1;
	m_runThread = true;

	// Initialize the server socket
	serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSock < 0) {
		throw runtime_error("Failed to create socket");
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
	if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(sockaddr_in)) < 0) {
		throw runtime_error("Failed to bind socket");
	}

	// Start listening
	if (listen(serverSock, 30) < 0) {
		throw runtime_error("Failed to listen on socket");
	}

	LOG_INFO << "Server initialized on port " << port;
}

// Accept new connections and dispatch them to threads
void Server::AcceptAndDispatch() {
	socklen_t cliSize = sizeof(sockaddr_in);

	while (m_runThread) {
		auto* client = new Client();
		client->sock = accept(serverSock, (struct sockaddr*)&clientAddr, &cliSize);

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
			auto* clientThread = new ServerThread();
			clientThread->Create((void*)Server::HandleClient, client);
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
void Server::SendToAll(const std::string& message) {
//	std::lock_guard<std::mutex> lock(clientsMutex);

	for (auto& client : clients) {
		if (send(client.sock, message.c_str(), message.length(), 0) < 0) {
			LOG_ERROR << "Error sending to client " << client.id << ": " << strerror(errno);
		}
	}
}

// Overloaded version to send a C-string
void Server::SendToAll(char* message) {
	SendToAll(std::string(message));
}

// Send a message to a specific client
void Server::SendToClient(Client* client, const std::string& message) {
	// std::lock_guard<std::mutex> lock(clientsMutex);

	if (send(client->sock, message.c_str(), message.length(), 0) < 0) {
		LOG_ERROR << "Error sending to client " << client->id << ": " << strerror(errno);
	}
}

// List all connected clients
void Server::ListClients() {
	std::lock_guard<std::mutex> lock(clientsMutex);

	for (const auto& client : clients) {
		LOG_TRACE << "|" << client.name << "|" << client.clientType << endl;
	}
}

// Remove a client from the list and clean up resources
void Server::RemoveClient(Client* client) {
	std::lock_guard<std::mutex> lock(clientsMutex);

	int index = FindClientIndex(client);
	if (index != -1) {
		clients.erase(clients.begin() + index);
		LOG_INFO << "Client removed: " << client->id;
	} else {
		LOG_ERROR << "Client not found for removal: " << client->id;
	}

	// shutdown(client->sock, SHUT_RDWR);
	close(client->sock);
}

// Find the index of a client in the list
int Server::FindClientIndex(Client* client) {
	std::lock_guard<std::mutex> lock(clientsMutex);

	for (size_t i = 0; i < clients.size(); ++i) {
		if (clients[i].id == client->id) {
			return static_cast<int>(i);
		}
	}
	LOG_ERROR << "Client ID not found: " << client->id;
	return -1;
}

// Get a client by ID
Client* Server::GetClientByIndex(const std::string& id) {
	std::lock_guard<std::mutex> lock(clientsMutex);

	for (auto& client : clients) {
		if (client.id == id) {
			return &client;
		}
	}
	return nullptr;
}