#include "Server.h"

// Static members
std::vector<Client *> Server::clients;
std::mutex Server::sendMutex;

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

	// Add non-blocking socket option
	fcntl(serverSock, F_SETFL, O_NONBLOCK);

	// Bind the socket
	if (bind(serverSock, (struct sockaddr *) &serverAddr, sizeof(sockaddr_in)) < 0) {
		throw std::runtime_error("Failed to bind socket");
	}

	// Start listening
	if (listen(serverSock, 30) < 0) {
		throw std::runtime_error("Failed to listen on socket");
	}

	// Start the thread monitor
	monitorRunning = true;
	monitorThread = std::make_unique<std::thread>(&Server::MonitorThreads, this);

	LOG_INFO << "Server initialized on port " << port;
}

// Add a destructor or cleanup method to stop the monitor thread
Server::~Server() {
	// Signal the monitor thread to stop and wait for it
	monitorRunning = false;
	if (monitorThread && monitorThread->joinable()) {
		monitorThread->join();
	}

	// Cleanup remaining threads
	CleanupCompletedThreads();
}

// Add the monitor thread implementation
void Server::MonitorThreads() {
	while (monitorRunning) {
		// Sleep to avoid high CPU usage
		std::this_thread::sleep_for(std::chrono::seconds(5));

		// Check and clean up completed threads
		CleanupCompletedThreads();
	}
}

void Server::CleanupCompletedThreads() {
	std::lock_guard<std::mutex> lock(threadsMutex);

	auto it = clientThreads.begin();
	while (it != clientThreads.end()) {
		if ((*it)->IsCompleted()) {
			// Thread is completed, we can safely remove it
			it = clientThreads.erase(it);
		} else {
			++it;
		}
	}
}

void Server::AcceptAndDispatch() {
	socklen_t cliSize = sizeof(sockaddr_in);

	while (m_runThread) {
		auto *client = new Client();

		// Set the accept() to use a timeout with select()
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(serverSock, &readfds);

		struct timeval timeout;
		timeout.tv_sec = 1;  // 1 second timeout
		timeout.tv_usec = 0;

		int activity = select(serverSock + 1, &readfds, NULL, NULL, &timeout);

		if (activity < 0) {
			if (errno != EINTR) {
				LOG_ERROR << "Select error on server socket: " << strerror(errno);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			delete client;
			continue;
		}

		if (activity == 0) {
			// Timeout, no new connections
			delete client;
			continue;
		}

		// Accept new connection
		client->sock = accept(serverSock, (struct sockaddr *) &clientAddr, &cliSize);

		if (client->sock < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No new connection available
				delete client;
				continue;
			}

			LOG_ERROR << "Error on accept: " << strerror(errno);
			delete client;

			if (errno == EINTR) {
				continue; // Retry on interrupt
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue; // Continue instead of breaking to avoid service interruption
			}
		}

		// Handle new client connection
		try {
			auto clientThread = std::make_unique<ServerThread>();
			pthread_t threadId = clientThread->Create((void *) Server::HandleClient, client);

			if (threadId == 0) {
				throw std::runtime_error("Failed to create thread");
			}

			// Store thread in our container
			{
				std::lock_guard<std::mutex> lock(threadsMutex);
				clientThreads.push_back(std::move(clientThread));
			}

		} catch (std::exception &e) {
			LOG_ERROR << "Failed to create thread for client: " << e.what();
			close(client->sock);
			delete client;
		}
	}

	// Final cleanup
	close(serverSock);
	LOG_INFO << "Server stopped accepting connections.";
}


// Send a message to a specific client
void Server::SendToClient(Client *client, const std::string &message) {
	if (!client) return;

	try {
		// Use a local copy of the client socket to minimize lock time
		int clientSocket;
		std::string clientId;

		{
			// Short lock just to access the socket
			std::lock_guard<std::mutex> lock(sendMutex);
			clientSocket = client->sock;
			clientId = client->id;
		}

		// Use select to check if the socket is writable
		fd_set writefds;
		FD_ZERO(&writefds);
		FD_SET(clientSocket, &writefds);

		struct timeval timeout;
		timeout.tv_sec = 1;  // 1 second timeout for sending
		timeout.tv_usec = 0;

		int selectResult = select(clientSocket + 1, NULL, &writefds, NULL, &timeout);

		if (selectResult < 0) {
			LOG_ERROR << "Select error before sending to client " << clientId << ": " << strerror(errno);
			return;
		}

		if (selectResult == 0) {
			// Timeout on select - socket not ready for writing
			LOG_ERROR << "Timeout waiting for socket to be ready for writing, client " << clientId;
			return;
		}

		// Socket is ready for writing
		if (FD_ISSET(clientSocket, &writefds)) {
			ssize_t sent = send(clientSocket, message.c_str(), message.length(), 0);
			if (sent < 0) {
				LOG_ERROR << "Error sending to client " << clientId << ": " << strerror(errno);
			} else if (sent < static_cast<ssize_t>(message.length())) {
				LOG_WARNING << "Incomplete send to client " << clientId << ": sent " << sent << " of "
				            << message.length() << " bytes";
			}
		}
	} catch (const std::exception &e) {
		LOG_ERROR << "Exception sending to client: " << e.what();
	}
}

void Server::SendToAll(const std::string &message) {
	// Get a copy of all current client IDs
	std::vector<std::string> clientIds;

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		for (auto &client: clients) {
			if (client) {
				clientIds.push_back(client->id);
			}
		}
	}

	// Now send to each client by ID without holding the global lock
	for (const auto &id: clientIds) {
		Client *client = nullptr;

		// Get the client pointer with a brief lock
		{
			std::lock_guard<std::mutex> lock(clientsMutex);
			client = GetClientByIndex(id);
		}

		// Send the message if client still exists
		if (client) {
			SendToClient(client, message);
		}
	}
}

void Server::SendToAll(char *message) {
	SendToAll(std::string(message));
}

Client *Server::GetClientByIndex(const std::string &id) {
	// This method expects the caller to have already acquired clientsMutex
	for (auto &client: clients) {
		if (client && client->id == id) {
			return client;
		}
	}
	return nullptr;
}

// List all connected clients
void Server::ListClients() {
	std::lock_guard<std::mutex> lock(clientsMutex);

	for (const auto &client: clients) {
		LOG_TRACE << "|" << client->name << "|" << client->clientType << std::endl;
	}
}

// Remove a client from the list and clean up resources
void Server::RemoveClient(Client *client) {
	std::lock_guard<std::mutex> lock(clientsMutex);

	int index = FindClientIndex(client);
	if (index != -1) {
		// Don't delete the client here, it will be deleted by the caller
		clients.erase(clients.begin() + index);
		LOG_INFO << "Client removed: " << client->id;
	} else {
		LOG_ERROR << "Client not found for removal: " << client->id;
	}

}

// Find the index of a client in the list
int Server::FindClientIndex(Client *client) {
	// No need to lock here, caller should already have the lock
	for (size_t i = 0; i < clients.size(); ++i) {
		if (clients[i]->id == client->id) {
			return static_cast<int>(i);
		}
	}
	LOG_ERROR << "Client ID not found: " << client->id;
	return -1;
}


void Server::CreateClient(Client *c, std::string &uuid) {
	std::lock_guard<std::mutex> lock(clientsMutex);
	c->SetId(uuid);
	std::string defaultName = "Client " + c->id;
	c->SetName(defaultName);
	clients.push_back(c);  // Store the pointer directly, not a copy
	LOG_DEBUG << "Adding client with id: " << c->id;
}
