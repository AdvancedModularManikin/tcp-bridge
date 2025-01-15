
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
const string configPrefix = "CONFIG=";
const string modulePrefix = "MODULE_NAME=";
const string registerPrefix = "REGISTER=";
const string kickPrefix = "KICK=";
const string requestPrefix = "REQUEST=";
const string keepHistoryPrefix = "KEEP_HISTORY=";
const string actionPrefix = "ACT=";
const string genericTopicPrefix = "[";
const string keepAlivePrefix = "[KEEPALIVE]";
const string loadScenarioPrefix = "LOAD_SCENARIO:";
const string loadStatePrefix = "LOAD_STATE:";
const string haltingString = "HALTING_ERROR";
const string sysPrefix = "[SYS]";
const string actPrefix = "[ACT]";
const string loadPrefix = "LOAD_STATE:";

std::mutex Server::clientsMutex;

TPMS pod;

std::string ExtractManikinIDFromString(std::string in) {
	std::size_t pos = in.find("mid=");
	if (pos != std::string::npos) {
		std::string mid1 = in.substr(pos + 4);
		std::size_t pos1 = mid1.find(";");
		if (pos1 != std::string::npos) {
			std::string mid2 = mid1.substr(0, pos1);
			return mid2;
		}
		return mid1;
	}
	return DEFAULT_MANIKIN_ID;
}


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
	// Update game client status to DISCONNECTED
	ConnectionData gc = GetGameClient(c->id);
	gc.client_status = "DISCONNECTED";
	UpdateGameClient(c->id, gc);
	broadcastDisconnection(gc);

	std::lock_guard<std::mutex> lock(Server::clientsMutex);
	clientMap.erase(c->id);

	Server::RemoveClient(c);
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

	// Broadcast action as an AMM Command
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
	LOG_TRACE << "Received KEEPALIVE from client " << c->id;
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
	} else if (message.find("Module Connected") == 0) {
		// Module connected message, ignore
	} else {
		// Log an unknown or unsupported message type
		LOG_ERROR << "Unknown or unsupported message from client " << c->id << ": " << message;
	}
}

void *Server::HandleClient(void *args) {
	auto *client = static_cast<Client *>(args);
	if (!client) return nullptr;

	std::vector<char> buffer(8192); // Buffer for incoming data
	ssize_t bytesRead;

	std::string uuid = gen_random(10); // Generate a random UUID
	CreateClient(client, uuid);

	clientMap[client->id] = uuid; // Associate client ID with UUID

	auto gc = GetGameClient(client->id);
	gc.client_id = client->id;
	gc.client_connection = "TCP";
	gc.connect_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	UpdateGameClient(client->id, gc);

	while (true) {
		// Attempt to receive data
		bytesRead = recv(client->sock, buffer.data(), buffer.size(), 0);

		if (bytesRead > 0) {
			std::string message(buffer.data(), bytesRead);
			globalInboundBuffer[client->id] += message;

			// Process complete messages
			if (boost::algorithm::ends_with(globalInboundBuffer[client->id], "\n")) {
				auto messages = Utility::explode("\n", globalInboundBuffer[client->id]);
				globalInboundBuffer[client->id].clear();

				for (const auto &msg: messages) {
					if (msg.empty()) continue;
					processClientMessage(client, msg);
				}
			}
		} else if (bytesRead == 0) {
			// Client disconnected
			LOG_INFO << client->name << " disconnected.";
			handleClientDisconnection(client);
			break;
		} else {
			// Error handling
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No data available, continue
				continue;
			} else {
				LOG_ERROR << "Error receiving data from client " << client->name << ": " << strerror(errno);
				handleClientDisconnection(client);
				break;
			}
		}
	}

	return nullptr;
}

void Server::CreateClient(Client *c, string &uuid) {
	std::lock_guard<std::mutex> lock(clientsMutex);
	c->SetId(uuid);
	string defaultName = "Client " + c->id;
	c->SetName(defaultName);
	clients.push_back(*c);
	LOG_DEBUG << "Adding client with id: " << c->id;
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
