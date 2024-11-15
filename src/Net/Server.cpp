#include "Server.h"

using namespace std;

vector<Client> Server::clients;

Server::Server(int port) {

    // Initialize static mutex from ServerThread
    ServerThread::InitMutex();

    // For setsock opt (REUSEADDR)
    int yes = 1;
    m_runThread = true;

    // Init serverSock and start listen()'ing
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverAddr, 0, sizeof(sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    setsockopt(serverSock, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int));



    if (bind(serverSock, (struct sockaddr *) &serverAddr, sizeof(sockaddr_in)) < 0)
        cerr << "Failed to bind";

    listen(serverSock, 30);
}

/*void Server::AcceptAndDispatch() {

    Client *c;
    ServerThread *t;

    socklen_t cliSize = sizeof(sockaddr_in);

    while (m_runThread) {

        c = new Client();
        t = new ServerThread();

        // Blocks here;
        c->sock = accept(serverSock, (struct sockaddr *) &clientAddr, &cliSize);
        if (c->sock < 0) {
            cerr << "Error on accept";
        } else {
            t->Create((void *) Server::HandleClient, c);
        }
    }
}*/

void Server::AcceptAndDispatch() {
	socklen_t cliSize = sizeof(sockaddr_in);

	while (m_runThread) {
		auto* c = new Client();
		auto* t = new ServerThread();

		// Attempt to accept a new connection
		c->sock = accept(serverSock, (struct sockaddr*)&clientAddr, &cliSize);
		if (c->sock < 0) {
			cerr << "Error on accept: " << strerror(errno) << endl;

			// Clean up resources in case of accept failure
			delete c;
			delete t;

			if (errno == EINTR) {
				continue;  // Interrupted by signal, retry accept
			} else {
				break;  // Fatal error, exit the loop
			}
		} else {
			try {
				// Create a new thread for handling the client
				t->Create((void*)Server::HandleClient, c);
			} catch (const std::exception& e) {
				cerr << "Failed to create thread: " << e.what() << endl;

				// Clean up resources if thread creation fails
				close(c->sock);
				delete c;
				delete t;
			} catch (...) {
				cerr << "Unknown error occurred during thread creation." << endl;

				// Clean up resources for unknown exceptions
				close(c->sock);
				delete c;
				delete t;
			}
		}
	}

	// Ensure server socket is closed when stopping
	close(serverSock);
}

void Server::SendToAll(const std::string &message) {
    ssize_t n;

    ServerThread::LockMutex("'SendToAll()'");

    for (auto &client : clients) {
        n = send(client.sock, message.c_str(), strlen(message.c_str()), 0);
        // cout << n << " bytes sent." << endl;
    }

    // Release the lock
    ServerThread::UnlockMutex("'SendToAll()'");
}

void Server::SendToAll(char *message) {
    ssize_t n;

    // Acquire the lock
    ServerThread::LockMutex("'SendToAll()'");

    for (auto &client : clients) {
        n = send(client.sock, message, strlen(message), 0);
        // cout << n << " bytes sent." << endl;
    }

    // Release the lock
    ServerThread::UnlockMutex("'SendToAll()'");
}

void Server::SendToClient(Client *c, const std::string &message) {
    ssize_t n;
    ServerThread::LockMutex("'SendToClient()'");

    // cout << " Sending message to [" << c->name << "](" << c->id << "): " <<
    // message << endl;
    n = send(c->sock, message.c_str(), strlen(message.c_str()), 0);
    // cout << n << " bytes sent." << endl;
    ServerThread::UnlockMutex("'SendToClient()'");
}

void Server::ListClients() {
    for (auto &client : clients) {
        cout << "|" << client.name << "|" << client.clientType << endl;

    }
}

/*
  Should be called when vector<Client> clients is locked!
*/
int Server::FindClientIndex(Client *c) {
    for (size_t i = 0; i < clients.size(); i++) {
        if ((Server::clients[i].id) == c->id)
            return (int) i;
    }
    cerr << "Client id not found." << endl;
    return -1;
}

Client *Server::GetClientByIndex(std::string id) {
    for (size_t i = 0; i < clients.size(); i++) {
        if ((Server::clients[i].id) == id)
            return &Server::clients[i];
    }
    return nullptr;
}

