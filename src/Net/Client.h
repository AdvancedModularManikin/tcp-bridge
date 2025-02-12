// Client.h
#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <algorithm>

#include <string>
#include <string_view>
#include <stdexcept>

class Client {
public:
	static constexpr size_t MAX_NAME_LENGTH = 40;

	// Constructor with proper initialization
	Client() : sock(-1), keepHistory(false) {}

	// Destructor to clean up resources
	~Client() {
		if (sock >= 0) {
			shutdown(sock, SHUT_RDWR);
			close(sock);
		}
	}

	// Delete copy constructor and assignment to prevent socket duplication
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// Allow moving
	Client(Client&& other) noexcept;
	Client& operator=(Client&& other) noexcept;

	// Setters with string_view for better performance
	void SetId(std::string_view id);
	void SetName(std::string_view name);
	void SetUUID(std::string_view uuid);
	void SetClientType(std::string_view clientType);
	void SetKeepHistory(bool historyflag);

	// Getters for encapsulation
	std::string_view GetId() const { return id; }
	std::string_view GetName() const { return name; }
	std::string_view GetUUID() const { return uuid; }
	std::string_view GetClientType() const { return clientType; }
	bool GetKeepHistory() const { return keepHistory; }
	int GetSocket() const { return sock; }

	// Socket setter with validation
	void SetSocket(int socketFd);

private:
	std::string id;
	std::string name;
	std::string uuid;
	std::string clientType;
	bool keepHistory;
	int sock;
};