#include "ServerThread.h"

ServerThread::ServerThread() = default;

ServerThread::~ServerThread() {
	// If thread is still running when object is destroyed, detach it
	if (!completed) {
		pthread_detach(tid);
		std::cerr << "Warning: ServerThread destroyed while thread still running. Thread detached." << std::endl;
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
	// Create thread data structure
	auto threadData = new ThreadData{this, Callback, args};

	int tret = pthread_create(&this->tid, nullptr, ThreadCleanupWrapper, threadData);

	if (tret != 0) {
		std::cerr << "Error while creating thread: " << strerror(tret) << std::endl;
		delete threadData;
		return 0; // Return 0 as an invalid thread ID
	}

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