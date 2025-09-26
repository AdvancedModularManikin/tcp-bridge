#include "ServerThread.h"

ServerThread::ServerThread() = default;

ServerThread::~ServerThread() {
	// If thread is still running when object is destroyed, detach it
	if (!completed && tid != 0) {
		// Try to join first with a short timeout, then detach as last resort
		struct timespec timeout;
		timeout.tv_sec = 1;  // 1 second timeout
		timeout.tv_nsec = 0;
		
		int result = pthread_timedjoin_np(tid, nullptr, &timeout);
		if (result == ETIMEDOUT) {
			// Thread didn't finish in time, detach it
			pthread_detach(tid);
			std::cerr << "Warning: ServerThread destroyed while thread still running. Thread detached." << std::endl;
		} else if (result == 0) {
			// Thread joined successfully
			completed = true;
		}
		// If pthread_timedjoin_np is not available, fall back to detach
		#ifndef _GNU_SOURCE
		pthread_detach(tid);
		std::cerr << "Warning: ServerThread destroyed while thread still running. Thread detached." << std::endl;
		#endif
	}
}

void* ServerThread::ThreadCleanupWrapper(void* data) {
	// Extract the thread data
	std::unique_ptr<ThreadData> threadData(static_cast<ThreadData*>(data));
	ServerThread* self = threadData->thread;
	void* callback = threadData->callback;
	void* args = threadData->args;

	// Call the original callback
	void* result = ((void*(*)(void*))callback)(args);

	// Mark as completed
	self->completed = true;

	return result;
}

pthread_t ServerThread::Create(void *Callback, void *args) {
	// Create thread data structure with exception safety
	std::unique_ptr<ThreadData> threadData;
	try {
		threadData = std::make_unique<ThreadData>(ThreadData{this, Callback, args});
	} catch (const std::exception& e) {
		std::cerr << "Error allocating thread data: " << e.what() << std::endl;
		return 0;
	}

	int tret = pthread_create(&this->tid, nullptr, ThreadCleanupWrapper, threadData.get());

	if (tret != 0) {
		std::cerr << "Error while creating thread: " << strerror(tret) << std::endl;
		return 0; // threadData automatically cleaned up by unique_ptr
	}

	// Release ownership since thread will manage it now
	threadData.release();
	return this->tid;
}

int ServerThread::Join() {
	if (!completed && tid != 0) {
		void* status;
		int result = pthread_join(tid, &status);
		if (result == 0) {
			completed = true;
		}
		return result;
	}
	return 0;
}