#pragma once

#include <thread>
#include <functional>
#include <memory>
#include <future>

class ServerThread {
public:
	ServerThread() = default;
	~ServerThread() {
		if (thread.joinable()) {
			thread.join();
		}
	}

	// Delete copy operations
	ServerThread(const ServerThread&) = delete;
	ServerThread& operator=(const ServerThread&) = delete;

	// Allow move operations
	ServerThread(ServerThread&&) noexcept = default;
	ServerThread& operator=(ServerThread&&) noexcept = default;

	template<typename Callable>
	void Create(Callable&& func) {
		thread = std::thread(std::forward<Callable>(func));
	}

	bool IsRunning() const {
		return thread.joinable();
	}

	void Join() {
		if (thread.joinable()) {
			thread.join();
		}
	}

private:
	std::thread thread;
};