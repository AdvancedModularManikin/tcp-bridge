#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

using namespace std;

class ServerThread {
public:
    pthread_t tid;

private:

public:
    ServerThread();

    int Create(void *Callback, void *args);

    int Join();

};
