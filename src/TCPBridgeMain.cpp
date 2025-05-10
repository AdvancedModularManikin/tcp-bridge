
#include "Net/Client.h"
#include "Net/Server.h"
#include "Net/UdpDiscoveryServer.h"

#include "amm_std.h"

#include "amm/BaseLogger.h"

#include "bridge.h"

#include "TPMS.h"

#include "tinyxml2.h"


using namespace std;
using namespace tinyxml2;
using namespace AMM;
using namespace std;
using namespace std::chrono;
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

Server *s;

std::map<std::string, std::string> clientMap;
std::map<std::string, std::string> clientTypeMap;

std::map<std::string, std::vector<std::string>> subscribedTopics;
std::map<std::string, std::vector<std::string>> publishedTopics;
std::map<std::string, ConnectionData> gameClientList;
std::map<std::string, std::string> globalInboundBuffer;

std::string DEFAULT_MANIKIN_ID = "manikin_1";
std::string CORE_ID;
std::string SESSION_PASSWORD;

const string capabilityPrefix = "CAPABILITY=";
const string settingsPrefix = "SETTINGS=";
const string statusPrefix = "STATUS=";
const string registerPrefix = "REGISTER=";
const string kickPrefix = "KICK=";
const string requestPrefix = "REQUEST=";
const string actionPrefix = "ACT=";
const string genericTopicPrefix = "[";
const string keepAlivePrefix = "[KEEPALIVE]";

std::mutex Server::clientsMutex;

TPMS pod;

void broadcastDisconnection(const ConnectionData &gc) {
	std::ostringstream message;
	message << "[SYS]UPDATE_CLIENT=";
	message << "client_id=" << gc.client_id << ";client_name=" << gc.client_name;
	message << ";learner_name=" << gc.learner_name << ";client_connection=" << gc.client_connection;
	message << ";client_type=" << gc.client_type << ";role=" << gc.role;
	message << ";client_status=" << gc.client_status << ";connect_time=" << gc.connect_time;

	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) {
		AMM::Command cmdInstance;
		cmdInstance.message(message.str());
		tmgr->mgr->WriteCommand(cmdInstance);
	} else {
		LOG_WARNING << "Cannot send disconnection update to manikin.";
	}
}


void handleClientDisconnection(Client *c) {
	if (!c) return;  // Safety check

	try {
		// Update game client status to DISCONNECTED
		ConnectionData gc = GetGameClient(c->id);
		gc.client_status = "DISCONNECTED";
		UpdateGameClient(c->id, gc);

		try {
			broadcastDisconnection(gc);
		} catch (const std::exception &e) {
			LOG_ERROR << "Error broadcasting disconnection: " << e.what();
			// Continue with cleanup regardless
		}

		{
			std::lock_guard<std::mutex> lock(Server::clientsMutex);
			clientMap.erase(c->id);
			globalInboundBuffer.erase(c->id);

			// Clean up topic subscriptions
			subscribedTopics.erase(c->id);
			publishedTopics.erase(c->id);
		}

		// Safe socket shutdown
		shutdown(c->sock, SHUT_RDWR);
		close(c->sock);

		// Remove from server's client list
		Server::RemoveClient(c);

		// Finally delete the client object
		delete c;

	} catch (const std::exception &e) {
		LOG_ERROR << "Exception in handleClientDisconnection: " << e.what();

		// Last-resort cleanup
		try {
			close(c->sock);
			delete c;
		} catch (...) {
			LOG_ERROR << "Fatal error during last-resort client cleanup";
		}
	}
}


// Handler for client registration
void handleRegisterMessage(Client *c, const std::string &message) {
	std::string registerVal = message.substr(registerPrefix.size());
	LOG_INFO << "Client " << c->id << " registered name: " << registerVal;

	// Parse client registration data
	auto parts = split(registerVal, ';');
	if (parts.size() >= 2) {
		auto gc = GetGameClient(c->id);
		gc.client_name = parts[0];
		gc.learner_name = parts[1];
		gc.client_status = "CONNECTED";
		UpdateGameClient(c->id, gc);
	} else {
		LOG_WARNING << "Malformed registration message: " << registerVal;
	}

	// Notify all clients of new registration
	std::ostringstream joinMessage;
	joinMessage << "CLIENT_JOINED=" << c->id << std::endl;
	Server::SendToAll(joinMessage.str());
}

// Handler for kicking a client
void handleKickMessage(Client *c, const std::string &message) {
	std::string kickId = message.substr(kickPrefix.size());
	LOG_INFO << "Client " << c->id << " requested kick of client ID: " << kickId;

	auto it = gameClientList.find(kickId);
	if (it != gameClientList.end()) {
		LOG_INFO << "Removing client: " << it->second.client_name;
		gameClientList.erase(it);
	} else {
		LOG_WARNING << "Attempted to kick non-existent client ID: " << kickId;
	}

	// Notify other modules of the kick action
	AMM::Command cmdInstance;
	cmdInstance.message("KICK_CLIENT=" + kickId);
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->mgr->WriteCommand(cmdInstance);
}

// Handler for setting client status
void handleStatusMessage(Client *c, const std::string &message) {
	std::string encodedStatus = message.substr(statusPrefix.size());
	std::string status;
	try {
		status = Utility::decode64(encodedStatus);
	} catch (std::exception &e) {
		LOG_ERROR << "Error decoding base64 status message: " << e.what();
		return;
	}

	LOG_DEBUG << "Client " << c->id << " set status: " << status;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->HandleStatus(c, status);
}

// Handler for client capabilities announcement
void handleCapabilityMessage(Client *c, const std::string &message) {
	std::string encodedCapabilities = message.substr(capabilityPrefix.size());
	std::string capabilities;
	std::ostringstream ack;

	try {
		capabilities = Utility::decode64(encodedCapabilities);
	} catch (std::exception &e) {
		LOG_ERROR << "Error decoding base64 capabilities: " << e.what();
		ack << "ERROR_IN_CAPABILITIES_RECEIVED=" << c->id << std::endl;
		Server::SendToClient(c, ack.str());
		return;
	}

	LOG_INFO << "Client " << c->id << " sent capabilities.";
	// LOG_DEBUG << "Client " << c->id << " sent capabilities: " << capabilities;

	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->HandleCapabilities(c, capabilities);

	// Send acknowledgment
	ack << "CAPABILITIES_RECEIVED=" << c->id << std::endl;
	Server::SendToClient(c, ack.str());
}

// Handler for client settings message
void handleSettingsMessage(Client *c, const std::string &message) {
	std::string encodedSettings = message.substr(settingsPrefix.size());
	std::string settings;
	try {
		settings = Utility::decode64(encodedSettings);
	} catch (std::exception &e) {
		LOG_ERROR << "Error decoding base64 settings: " << e.what();
		return;
	}

	LOG_INFO << "Client " << c->id << " sent settings: " << settings;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->HandleSettings(c, settings);
}

// Handler for client requests
void handleRequestMessage(Client *c, const std::string &message) {
	std::string request = message.substr(requestPrefix.size());
	LOG_INFO << "Client " << c->id << " sent request: " << request;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->DispatchRequest(c, request);
}

// Handler for client actions
void handleActionMessage(Client *c, const std::string &message) {
	std::string action = message.substr(actionPrefix.size());
	LOG_INFO << "Client " << c->id << " sent action: " << action;

	AMM::Command cmdInstance;
	cmdInstance.message(action);
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->mgr->WriteCommand(cmdInstance);
}

void parseKeyValuePairs(const std::string &message, std::map<std::string, std::string> &kvp) {
	// Split the message into tokens based on semicolons
	std::vector<std::string> tokens;
	boost::split(tokens, message, boost::is_any_of(";"), boost::token_compress_on);

	// Process each token to extract key-value pairs
	for (const auto &token: tokens) {
		size_t sep_pos = token.find('=');
		if (sep_pos != std::string::npos) {
			std::string key = token.substr(0, sep_pos);
			std::string value = token.substr(sep_pos + 1);

			// Trim whitespace and convert key to lowercase for consistency
			boost::trim(key);
			boost::trim(value);
			boost::to_lower(key);

			kvp[key] = value; // Insert into map
		} else {
			// Log a warning if the token is malformed
			LOG_WARNING << "Malformed token in message: " << token;
			std::string key = token.substr(0, sep_pos);
			std::string value = token.substr(sep_pos + 1);

			// Trim whitespace and convert key to lowercase for consistency
			boost::trim(key);
			boost::trim(value);
			boost::to_lower(key);
		}
	}
}

// Handler for physiological and render modifications
void handleModificationMessage(Client *c, const std::string &message, const std::string &topic) {
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (!tmgr) {
		LOG_ERROR << "No manikin manager available for modification message.";
		return;
	}

	// Command's don't need to be extracted
	if (topic == "AMM_Command") {
		LOG_INFO << "Sending command: " << message;
		return tmgr->SendCommand(message);
	}

	std::map<std::string, std::string> kvp;
	parseKeyValuePairs(message, kvp);

	AMM::UUID erID;
	erID.id(kvp["event_id"].empty() ? AMM::DDSManager<Manikin>::GenerateUuidString() : kvp["event_id"]);

	AMM::FMA_Location fma;
	fma.name(kvp["location"]);

	AMM::UUID agentID;
	agentID.id(kvp["participant_id"]);

	std::string modType = kvp["type"];

	std::string modPayload = kvp["payload"];

	if (modType.empty()) {
		modType = ExtractTypeFromRenderMod(modPayload);
	}

	if (topic == "AMM_Render_Modification") {
		tmgr->SendEventRecord(erID, fma, agentID, modType);
		if (modPayload.empty()) { // make a render mod payload
			std::ostringstream tPayload;
			tPayload << "<RenderModification type='" << modType << "'/>";
			tmgr->SendRenderModification(erID, modType, tPayload.str());
		} else {
			tmgr->SendRenderModification(erID, modType, modPayload);
		}
	} else if (topic == "AMM_Physiology_Modification") {
		tmgr->SendEventRecord(erID, fma, agentID, modType);
		tmgr->SendPhysiologyModification(erID, modType, modPayload);
	} else if (topic == "AMM_Assessment" || topic == "AMM_Performance_Assessment") {
		tmgr->SendEventRecord(erID, fma, agentID, modType);
		tmgr->SendAssessment(erID);
	} else if (topic == "AMM_Command") {
		tmgr->SendCommand(message);
	} else if (topic == "AMM_ModuleConfiguration") {
		tmgr->SendModuleConfiguration(modType, modPayload);
	} else {
		LOG_WARNING << "Unknown modification topic: " << topic;
	}
}

// Handler for "KEEPALIVE" messages - do nothing but log it for monitoring purposes
void handleKeepAliveMessage(Client *c) {
	// LOG_TRACE << "Received KEEPALIVE from client " << c->id;
}

void processClientMessage(Client *c, const std::string &message) {
	// Log and route the message based on its prefix/type
	if (message.find(keepAlivePrefix) == 0) {
		handleKeepAliveMessage(c);
	} else if (message.find(registerPrefix) == 0) {
		handleRegisterMessage(c, message);
	} else if (message.find(kickPrefix) == 0) {
		handleKickMessage(c, message);
	} else if (message.find(statusPrefix) == 0) {
		handleStatusMessage(c, message);
	} else if (message.find(capabilityPrefix) == 0) {
		handleCapabilityMessage(c, message);
	} else if (message.find(settingsPrefix) == 0) {
		handleSettingsMessage(c, message);
	} else if (message.find(requestPrefix) == 0) {
		handleRequestMessage(c, message);
	} else if (message.find(actionPrefix) == 0) {
		handleActionMessage(c, message);
	} else if (message.find(genericTopicPrefix) == 0) {
		// Parse the topic and message content for modification-type messages
		int firstBracket = message.find('[');
		int lastBracket = message.find(']');

		if (firstBracket != std::string::npos && lastBracket != std::string::npos) {
			std::string topic = message.substr(firstBracket + 1, lastBracket - firstBracket - 1);
			std::string content = message.substr(lastBracket + 1);
			handleModificationMessage(c, content, topic);
		} else {
			LOG_ERROR << "Malformed generic topic message from client " << c->id << ": " << message;
		}
	} else if (message.find(" Connected") != std::string::npos) {
		LOG_INFO << "Module connection message: " << message;
	} else {
		// Log an unknown or unsupported message type
		LOG_ERROR << "Unknown or unsupported message from client " << c->id << ": " << message;
	}
}

void *Server::HandleClient(void *args) {
	auto *c = static_cast<Client *>(args);
	if (!c) return nullptr;

	try {
		char buffer[8192 - 25];
		ssize_t n;
		std::string uuid = gen_random(10);

		// Create a scope for better resource management
		{
			// Mutex management and client setup
			CreateClient(c, uuid);

			clientMap[c->id] = uuid;

			// Initialize game client data
			auto gc = GetGameClient(c->id);
			gc.client_id = c->id;
			gc.client_connection = "TCP";
			gc.connect_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			UpdateGameClient(c->id, gc);

			// Set socket to non-blocking only - don't use SO_RCVTIMEO/SO_SNDTIMEO
			int flags = fcntl(c->sock, F_GETFL, 0);
			fcntl(c->sock, F_SETFL, flags | O_NONBLOCK);

			// Ensure TCP keepalive is enabled to detect dead peers
			int keepalive = 1;
			int keepidle = 60; // Start probing after 60 seconds of inactivity
			int keepintvl = 10; // Send probes every 10 seconds
			int keepcnt = 6;   // Consider connection dead after 6 failed probes

			setsockopt(c->sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

			// These might not be available on all platforms
#ifdef TCP_KEEPIDLE
			setsockopt(c->sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
#endif

#ifdef TCP_KEEPINTVL
			setsockopt(c->sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
#endif

#ifdef TCP_KEEPCNT
			setsockopt(c->sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
#endif

			// Variables for detecting client inactivity
			auto lastActivity = std::chrono::steady_clock::now();
			const auto maxInactivityDuration = std::chrono::minutes(10); // 10 minutes timeout

			bool clientActive = true;
			while (clientActive) {
				memset(buffer, 0, sizeof(buffer));

				// Use select() to wait for data with timeout
				fd_set readfds;
				FD_ZERO(&readfds);
				FD_SET(c->sock, &readfds);

				// Set timeout for select - shorter timeout allows us to check connection health
				struct timeval timeout;
				timeout.tv_sec = 30;  // 30 second timeout
				timeout.tv_usec = 0;

				int activity = select(c->sock + 1, &readfds, NULL, NULL, &timeout);

				auto now = std::chrono::steady_clock::now();
				auto inactivityDuration = now - lastActivity;

				// Check if client has been inactive for too long
				if (inactivityDuration > maxInactivityDuration) {
					LOG_WARNING << "Client " << c->id << " inactive for too long, disconnecting";
					clientActive = false;
					break;
				}

				if (activity < 0) {
					if (errno == EINTR) {
						// Interrupted, just continue
						continue;
					}

					LOG_ERROR << "Select error for client " << c->id << ": " << strerror(errno);
					clientActive = false;
					break;
				}

				// No data available within timeout
				if (activity == 0) {
					// Send keepalive message to check connection
					std::string keepaliveMsg = "[KEEPALIVE]\n";
					if (send(c->sock, keepaliveMsg.c_str(), keepaliveMsg.length(), 0) < 0) {
						if (errno != EAGAIN && errno != EWOULDBLOCK) {
							LOG_ERROR << "Error sending keepalive to client " << c->id << ": " << strerror(errno);
							clientActive = false;
							break;
						}
					}
					continue;
				}

				// Read data if available
				if (FD_ISSET(c->sock, &readfds)) {
					lastActivity = now; // Update last activity time

					// Use non-blocking recv - it will not block since select indicated data is ready
					n = recv(c->sock, buffer, sizeof(buffer), 0);

					// Check if client disconnected
					if (n == 0) {
						LOG_INFO << c->name << " disconnected";
						clientActive = false;
						break;
					} else if (n < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							// No data available - this shouldn't happen after select(), but just in case
							continue;
						}
						LOG_ERROR << "Error while receiving message from client: " << c->name << ": "
						          << strerror(errno);
						clientActive = false;
						break;
					}

					// Process received message
					std::string tempBuffer(buffer);
					globalInboundBuffer[c->id] += tempBuffer;

					if (!boost::algorithm::ends_with(globalInboundBuffer[c->id], "\n")) {
						continue;
					}

					auto messages = Utility::explode("\n", globalInboundBuffer[c->id]);
					globalInboundBuffer[c->id].clear();

					for (auto &message: messages) {
						boost::trim(message);
						if (message.empty()) continue;

						try {
							processClientMessage(c, message);
							lastActivity = std::chrono::steady_clock::now(); // Update activity time after successful processing
						} catch (std::exception &e) {
							LOG_ERROR << "Exception while processing client message: " << e.what();
							// Continue processing other messages despite error
						}
					}
				}
			}
		}

		// Clean up resources when we exit the loop
		handleClientDisconnection(c);

	} catch (const std::exception &e) {
		LOG_ERROR << "Exception in HandleClient: " << e.what();

		// Ensure client is disconnected and cleaned up
		try {
			handleClientDisconnection(c);
		} catch (const std::exception &cleanup_e) {
			LOG_ERROR << "Exception during cleanup: " << cleanup_e.what();

			// Last-resort cleanup
			try {
				std::lock_guard<std::mutex> lock(Server::clientsMutex);
				clientMap.erase(c->id);
				globalInboundBuffer.erase(c->id);
				subscribedTopics.erase(c->id);
				publishedTopics.erase(c->id);

				Server::RemoveClient(c);
				close(c->sock);
				delete c;
			} catch (...) {
				LOG_ERROR << "Fatal error during client cleanup for client " << c->id;
			}
		}
	} catch (...) {
		LOG_ERROR << "Unknown exception in HandleClient";

		// Try to clean up even in case of unknown exception
		try {
			handleClientDisconnection(c);
		} catch (...) {
			// Last-resort cleanup
			try {
				close(c->sock);
				delete c;
			} catch (...) {
				LOG_ERROR << "Fatal error during client cleanup";
			}
		}
	}

	return nullptr;
}


void UdpDiscoveryThread(short port, bool enabled, std::string manikin_id) {
	if (enabled) {
		boost::asio::io_service io_service;
		UdpDiscoveryServer udps(io_service, port, manikin_id);
		LOG_INFO << "UDP Discovery listening on port " << port;
		io_service.run();
	} else {
		LOG_INFO << "UDP discovery service not started due to command line option.";
	}
}

int main(int argc, const char *argv[]) {
	static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
	plog::init(plog::verbose, &consoleAppender);

	short discoveryPort = 8888;
	int bridgePort = 9015;
	bool podMode = true;
	bool discovery = true;
	int manikinCount = 1;
	std::string coreId;
	std::string manikinId = DEFAULT_MANIKIN_ID;

	namespace po = boost::program_options;

	po::variables_map vm;
	po::options_description desc("Allowed options");
	desc.add_options()
			("help,h", "print usage message")
			("discovery", po::value(&discovery)->default_value(true), "UDP autodiscovery")
			("discovery_port,dp", po::value(&discoveryPort)->default_value(8888), "Autodiscovery port")
			("server_port,sp", po::value(&bridgePort)->default_value(9015), "Bridge port")
			("pod_mode", po::value(&podMode)->default_value(false), "POD mode")
			("manikin_id", po::value(&manikinId)->default_value("manikin_1"), "Manikin ID")
			("manikins", po::value(&manikinCount)->default_value(1))
			("core_id", po::value(&coreId)->default_value("AMM_000"), "Core ID");


	// This isn't set to enforce it, but there are two modes of operation
	//  POD mode, which will register 1-3 manikins and act as the instructor bridge
	//  Manikin mod, which will register as a single manikin and act as the standard TCP bridge


	// TWo example run commands:
	// ./amm_tcp_bridge -dp=8889 -sp=9016 -pod_mode=true manikins=4
	//      Will launch in pod mode with 4 manikins
	//
	// ./amm_tcp_bridge -dp=8888 -sp=9015 -pod_mode=false manikin_id=manikin_2
	//      Will launch in manikin mode with a single manikin using profile manikin_2

	// parse arguments and save them in the variable map (vm)
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	// Check if there are enough args or if --help is given
	if (vm.count("help")) {
		std::cerr << desc << "\n";
		return 1;
	}

	DEFAULT_MANIKIN_ID = manikinId;
	CORE_ID = coreId;

	LOG_INFO << "=== [AMM - TCP Bridge] ===";
	try {
		pod.SetID(manikinId);
		pod.SetMode(podMode);
		if (podMode) {
			pod.InitializeManikins(manikinCount);
		} else {
			pod.InitializeManikin(manikinId);
		}

	} catch (exception &e) {
		LOG_ERROR << "Unable to initialize manikins in POD: " << e.what();
	}

	std::thread t1(UdpDiscoveryThread, discoveryPort, discovery, manikinId);
	s = new Server(bridgePort);
	std::string action;

	LOG_INFO << "TCP Bridge listening on port " << bridgePort;

	s->AcceptAndDispatch();

	t1.join();

	LOG_INFO << "TCP Bridge shutdown.";
}
