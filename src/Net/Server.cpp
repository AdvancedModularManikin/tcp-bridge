#include "Server.h"

// Static member initialization
std::vector<std::shared_ptr<Client>> Server::clients;
std::mutex Server::clientsMutex;

Server::Server(int port) : m_runThread(true), serverSock(-1) {
	int yes = 1;

	try {
		// Initialize the server socket
		serverSock = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSock < 0) {
			throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
		}

		// Configure the server address
		memset(&serverAddr, 0, sizeof(sockaddr_in));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(port);

		// Set socket options
		if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
			throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
		}

		if (setsockopt(serverSock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0) {
			throw std::runtime_error("Failed to set TCP_NODELAY: " + std::string(strerror(errno)));
		}

		// Bind the socket
		if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(sockaddr_in)) < 0) {
			throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
		}

		// Start listening
		if (listen(serverSock, MAX_LISTEN_BACKLOG) < 0) {
			throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
		}

		LOG_INFO << "Server initialized on port " << port;
	}
	catch (const std::exception& e) {
		cleanup();
		throw;
	}
}

Server::~Server() {
	cleanup();
}

void Server::cleanup() {
	m_runThread = false;

	if (serverSock >= 0) {
		shutdown(serverSock, SHUT_RDWR);
		close(serverSock);
		serverSock = -1;
	}

	std::lock_guard<std::mutex> lock(threadsMutex);
	for (auto& thread : clientThreads) {
		if (thread && thread->IsRunning()) {
			thread->Join();
		}
	}
	clientThreads.clear();
}

void Server::AcceptAndDispatch() {
	socklen_t cliSize = sizeof(sockaddr_in);

	while (m_runThread) {
		auto client = std::make_shared<Client>();
		int newSocket = accept(serverSock, (struct sockaddr*)&clientAddr, &cliSize);

		if (newSocket >= 0) {
			setNonBlocking(newSocket);
			try {
				client->SetSocket(newSocket);

				auto thread = std::make_unique<ServerThread>();
				thread->Create([client]() {
					HandleClient(client);
				});

				{
					std::lock_guard<std::mutex> lock(threadsMutex);
					clientThreads.push_back(std::move(thread));

					// Cleanup finished threads
					clientThreads.erase(
							std::remove_if(clientThreads.begin(), clientThreads.end(),
							               [](const auto& t) { return !t || !t->IsRunning(); }),
							clientThreads.end()
					);
				}
			}
			catch (const std::exception& e) {
				LOG_ERROR << "Failed to handle new client: " << e.what();
				close(newSocket);
			}
		}
		else if (errno != EINTR) {
			LOG_ERROR << "Error accepting connection: " << strerror(errno);
			break;
		}
	}

	cleanup();
	LOG_INFO << "Server stopped accepting connections.";
}

void Server::HandleClient(const std::shared_ptr<Client>& client) {
	if (!client) {
		LOG_ERROR << "Null client passed to HandleClient";
		return;
	}

	std::vector<char> buffer(BUFFER_SIZE);
	std::string messageBuffer;

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		clients.push_back(client);
	}

	while (true) {
		ssize_t bytesRead = recv(client->GetSocket(), buffer.data(), buffer.size(), 0);

		if (bytesRead > 0) {
			messageBuffer.append(buffer.data(), bytesRead);

			size_t pos;
			while ((pos = messageBuffer.find('\n')) != std::string::npos) {
				std::string message = messageBuffer.substr(0, pos);
				messageBuffer.erase(0, pos + 1);

				if (!message.empty()) {
					LOG_DEBUG << "Received message from client " << client->GetId() << ": " << message;
					// Process message here
				}
			}
		}
		else if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
			break;
		}
	}

	RemoveClient(client);
}

void Server::SendToAll(const std::string& message) {
	std::vector<std::shared_ptr<Client>> tempClients;

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		tempClients = clients;
	}

	for (const auto& client : tempClients) {
		try {
			SendToClient(client, message);
		}
		catch (const std::exception& e) {
			LOG_ERROR << "Error broadcasting to client " << client->GetId() << ": " << e.what();
		}
	}
}

void Server::SendToClient(const std::shared_ptr<Client>& client, const std::string& message) {
	if (!client) {
		LOG_WARNING << "Attempted to send to a null client.";
		return;
	}

	try {
		sendLargeMessage(client->GetSocket(), message);
	}
	catch (const std::exception& e) {
		LOG_ERROR << "Error sending to client " << client->GetId() << ": " << e.what();
		RemoveClient(client);
	}
}

void Server::RemoveClient(const std::shared_ptr<Client>& client) {
	if (!client) return;

	std::lock_guard<std::mutex> lock(clientsMutex);
	auto it = std::remove_if(clients.begin(), clients.end(),
	                         [&client](const std::shared_ptr<Client>& c) {
		                         return c->GetId() == client->GetId();
	                         });

	if (it != clients.end()) {
		clients.erase(it, clients.end());
		LOG_INFO << "Client removed: " << client->GetId();
	}
}

std::shared_ptr<Client> Server::GetClientByIndex(const std::string& id) {
	std::lock_guard<std::mutex> lock(clientsMutex);
	auto it = std::find_if(clients.begin(), clients.end(),
	                       [&id](const std::shared_ptr<Client>& c) {
		                       return c->GetId() == id;
	                       });

	return (it != clients.end()) ? *it : nullptr;
}

void Server::setNonBlocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		throw std::runtime_error("Failed to get socket flags: " + std::string(strerror(errno)));
	}

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		throw std::runtime_error("Failed to set socket non-blocking: " + std::string(strerror(errno)));
	}
}

void Server::sendLargeMessage(int sockfd, const std::string& message, size_t chunkSize) {
	size_t totalSize = message.size();
	size_t offset = 0;

	while (offset < totalSize) {
		size_t bytesToSend = std::min(chunkSize, totalSize - offset);
		ssize_t sent = send(sockfd, message.c_str() + offset, bytesToSend, 0);

		if (sent < 0) {
			throw std::runtime_error("Failed to send message: " + std::string(strerror(errno)));
		}

		offset += sent;
	}
}

void Server::ListClients() {
	std::lock_guard<std::mutex> lock(clientsMutex);
	for (const auto& client : clients) {
		LOG_TRACE << "|" << client->GetName() << "|" << client->GetClientType();
	}
}