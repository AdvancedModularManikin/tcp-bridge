#ifndef SERVERTHREAD_H
#define SERVERTHREAD_H

#include <pthread.h>
#include <iostream>
#include <atomic>
#include <memory>
#include <functional>
#include <cstring> // For strerror

class ServerThread {
public:
	ServerThread();
	~ServerThread();

	// Modified to return the thread ID
	pthread_t Create(void *Callback, void *args);
	int Join();
	bool IsCompleted() const { return completed; }
	pthread_t GetThreadId() const { return tid; }

	// Add a static method to manage thread cleanup
	static void* ThreadCleanupWrapper(void* data);

private:
	pthread_t tid;
	std::atomic<bool> completed{false};

	// Structure to hold thread data
	struct ThreadData {
		ServerThread* thread;
		void* callback;
		void* args;
	};
};

#endif // SERVERTHREAD_H