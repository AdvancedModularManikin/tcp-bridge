// Client.cpp
#include "Client.h"

// Move constructor
Client::Client(Client&& other) noexcept
		: id(std::move(other.id))
		, name(std::move(other.name))
		, uuid(std::move(other.uuid))
		, clientType(std::move(other.clientType))
		, keepHistory(other.keepHistory)
		, sock(other.sock) {
	other.sock = -1; // Prevent double-close
}

// Move assignment operator
Client& Client::operator=(Client&& other) noexcept {
	if (this != &other) {
		// Clean up existing socket
		if (sock >= 0) {
			shutdown(sock, SHUT_RDWR);
			close(sock);
		}

		id = std::move(other.id);
		name = std::move(other.name);
		uuid = std::move(other.uuid);
		clientType = std::move(other.clientType);
		keepHistory = other.keepHistory;
		sock = other.sock;
		other.sock = -1; // Prevent double-close
	}
	return *this;
}

void Client::SetId(std::string_view newId) {
	if (newId.empty()) {
		throw std::invalid_argument("Client ID cannot be empty");
	}
	id = std::string(newId);
}

void Client::SetName(std::string_view newName) {
	if (newName.empty()) {
		throw std::invalid_argument("Client name cannot be empty");
	}
	name = std::string(newName.substr(0, MAX_NAME_LENGTH));
}

void Client::SetUUID(std::string_view newUuid) {
	if (newUuid.empty()) {
		throw std::invalid_argument("UUID cannot be empty");
	}
	uuid = std::string(newUuid);
}

void Client::SetClientType(std::string_view newClientType) {
	if (newClientType.empty()) {
		throw std::invalid_argument("Client type cannot be empty");
	}
	clientType = std::string(newClientType);
}

void Client::SetKeepHistory(bool historyflag) {
	keepHistory = historyflag;
}

void Client::SetSocket(int socketFd) {
	if (socketFd < 0) {
		throw std::invalid_argument("Invalid socket file descriptor");
	}

	// Clean up existing socket if any
	if (sock >= 0) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}

	sock = socketFd;
}