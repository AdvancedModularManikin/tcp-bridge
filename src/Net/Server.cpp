#include "Server.h"

// Static members
std::vector<Client *> Server::clients;

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
		// Get socket info without global lock - individual socket operations are thread-safe
		int clientSocket = client->sock;
		std::string clientId = client->id;

		// Validate socket is still valid
		if (clientSocket <= 0) {
			LOG_WARNING << "Invalid socket for client " << clientId;
			return;
		}

		// Use select with much shorter timeout to check if socket is writable
		fd_set writefds;
		FD_ZERO(&writefds);
		FD_SET(clientSocket, &writefds);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;  // 100ms timeout instead of 1 second

		int selectResult = select(clientSocket + 1, NULL, &writefds, NULL, &timeout);

		if (selectResult < 0) {
			if (errno == EBADF) {
				LOG_INFO << "Bad socket descriptor for client " << clientId << " - cleaning up disconnected client";
				CleanupDisconnectedClient(client);
			} else {
				LOG_ERROR << "Select error before sending to client " << clientId << ": " << strerror(errno);
			}
			return;
		}

		if (selectResult == 0) {
			// Timeout on select - socket not ready for writing, likely disconnected
			LOG_INFO << "Socket timeout for client " << clientId << " - cleaning up potentially disconnected client";
			CleanupDisconnectedClient(client);
			return;
		}

		// Socket is ready for writing
		if (FD_ISSET(clientSocket, &writefds)) {
			// Use MSG_DONTWAIT for non-blocking send as additional safety
			ssize_t sent = send(clientSocket, message.c_str(), message.length(), MSG_DONTWAIT);
			if (sent < 0) {
				if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
					LOG_INFO << "Client " << clientId << " disconnected (send failed: " << strerror(errno) << ") - cleaning up";
					CleanupDisconnectedClient(client);
				} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
					LOG_DEBUG << "Send would block for client " << clientId << " - skipping message";
				} else {
					LOG_ERROR << "Error sending to client " << clientId << ": " << strerror(errno);
				}
			} else if (sent < static_cast<ssize_t>(message.length())) {
				LOG_WARNING << "Incomplete send to client " << clientId << ": sent " << sent << " of "
				            << message.length() << " bytes";
			} else {
			  // LOG_TRACE << "Successfully sent " << sent << " bytes to client " << clientId;
			}
		}
	} catch (const std::exception &e) {
		LOG_ERROR << "Exception sending to client: " << e.what();
	}
}

void Server::SendToAll(const std::string &message) {
	// Get a snapshot of all clients to avoid holding lock during sends
	std::vector<Client*> clientSnapshot;
	std::vector<Client*> disconnectedClients;

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		for (auto &client: clients) {
			if (client) {
				if (IsClientConnected(client)) {
					clientSnapshot.push_back(client);
				} else {
					disconnectedClients.push_back(client);
				}
			}
		}
	}

	// Clean up any disconnected clients found during the check
	for (auto* client : disconnectedClients) {
		if (client) {
			LOG_INFO << "Removing disconnected client found during SendToAll: " << client->id;
			CleanupDisconnectedClient(client);
		}
	}

	// Send to each connected client without holding the global lock
	// Each SendToClient call is now non-blocking and won't affect other clients
	for (auto* client : clientSnapshot) {
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

// Check if a client is still connected by testing the socket
bool Server::IsClientConnected(Client *client) {
	if (!client || client->sock <= 0) {
		return false;
	}

	// Use a quick non-blocking recv to check if the socket is still valid
	char buffer[1];
	ssize_t result = recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT | MSG_PEEK);
	
	if (result == 0) {
		// recv returned 0, which means the connection is closed
		return false;
	} else if (result < 0) {
		// Check the error code
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// No data available, but connection is still alive
			return true;
		} else if (errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
			// Connection is broken
			return false;
		}
		// Other errors - assume connected for safety
		return true;
	}
	
	// recv returned > 0, connection is alive (put data back with MSG_PEEK)
	return true;
}

// Clean up a disconnected client immediately
void Server::CleanupDisconnectedClient(Client *client) {
	if (!client) return;
	
	LOG_INFO << "Removing disconnected client from active list: " << client->id;
	
	try {
		// Mark the socket as invalid to prevent future send attempts
		if (client->sock > 0) {
			client->sock = -1; // Mark as closed to prevent further send attempts
		}
		
		// Remove from the client list - this prevents future SendToAll from trying this client
		// The actual comprehensive cleanup will happen in the client thread when it detects disconnection
		RemoveClient(client);
		
	} catch (const std::exception &e) {
		LOG_ERROR << "Exception during client cleanup: " << e.what();
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
