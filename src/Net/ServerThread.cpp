#include "ServerThread.h"

using namespace std;

ServerThread::ServerThread() = default;

int ServerThread::Create(void *Callback, void *args) {
    int tret = 0;

    tret = pthread_create(&this->tid, nullptr, (void *(*)(void *)) Callback, args);

    if (tret != 0) {
        cerr << "Error while creating threads." << endl;
        return tret;
    }

    // cout << "Thread successfully created." << endl;
    return 0;
}

int ServerThread::Join() {
    pthread_join(this->tid, nullptr);
    return 0;
}

