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
using namespace std::chrono;
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

std::unique_ptr<Server> s;

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

TPMS pod;

std::string ExtractManikinIDFromString(const std::string& in) {
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

void broadcastDisconnection(const ConnectionData& gc) {
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

void handleClientDisconnection(const std::shared_ptr<Client>& c) {
	std::string clientId(c->GetId());

	// Update game client status to DISCONNECTED
	ConnectionData gc = GetGameClient(clientId);
	gc.client_status = "DISCONNECTED";
	UpdateGameClient(clientId, gc);
	broadcastDisconnection(gc);

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	clientMap.erase(clientId);
	Server::RemoveClient(c);
}

void handleRegisterMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string registerVal = message.substr(registerPrefix.size());
	LOG_INFO << "Client " << clientId << " registered name: " << registerVal;

	// Parse client registration data
	auto parts = split(registerVal, ';');
	if (parts.size() >= 2) {
		auto gc = GetGameClient(clientId);
		gc.client_name = parts[0];
		gc.learner_name = parts[1];
		gc.client_status = "CONNECTED";
		UpdateGameClient(clientId, gc);
	} else {
		LOG_WARNING << "Malformed registration message: " << registerVal;
	}

	// Notify all clients of new registration
	std::ostringstream joinMessage;
	joinMessage << "CLIENT_JOINED=" << clientId << std::endl;
	Server::SendToAll(joinMessage.str());
}

void handleKickMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string kickId = message.substr(kickPrefix.size());
	LOG_INFO << "Client " << clientId << " requested kick of client ID: " << kickId;

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

void handleStatusMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string encodedStatus = message.substr(statusPrefix.size());
	std::string status;
	try {
		status = Utility::decode64(encodedStatus);
	} catch (const std::exception& e) {
		LOG_ERROR << "Error decoding base64 status message: " << e.what();
		return;
	}

	LOG_DEBUG << "Client " << clientId << " set status: " << status;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->HandleStatus(c, status);
}

void handleCapabilityMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string encodedCapabilities = message.substr(capabilityPrefix.size());
	std::string capabilities;
	std::ostringstream ack;

	try {
		capabilities = Utility::decode64(encodedCapabilities);
		LOG_INFO << "Client " << clientId << " sent capabilities.";

		auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
		if (tmgr) {
			tmgr->HandleCapabilities(c, capabilities);
		}

		ack << "CAPABILITIES_RECEIVED=" << clientId << std::endl;
		Server::SendToClient(c, ack.str());
	} catch (const std::exception& e) {
		LOG_ERROR << "Error decoding Base64 capabilities: " << e.what();
		ack << "ERROR_IN_CAPABILITIES_RECEIVED=" << clientId << std::endl;
		Server::SendToClient(c, ack.str());
	}
}

void handleSettingsMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string encodedSettings = message.substr(settingsPrefix.size());
	std::string settings;
	try {
		settings = Utility::decode64(encodedSettings);
	} catch (const std::exception& e) {
		LOG_ERROR << "Error decoding base64 settings: " << e.what();
		return;
	}

	LOG_INFO << "Client " << clientId << " sent settings: " << settings;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->HandleSettings(c, settings);
}

void handleRequestMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string request = message.substr(requestPrefix.size());
	LOG_INFO << "Client " << clientId << " sent request: " << request;
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->DispatchRequest(c, request);
}

void handleActionMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());
	std::string action = message.substr(actionPrefix.size());
	LOG_INFO << "Client " << clientId << " sent action: " << action;

	AMM::Command cmdInstance;
	cmdInstance.message(action);
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (tmgr) tmgr->mgr->WriteCommand(cmdInstance);
}

void handleKeepAliveMessage(const std::shared_ptr<Client>& c) {
	// LOG_TRACE << "Received KEEPALIVE from client " << std::string(c->GetId());
}

void parseKeyValuePairs(const std::string& message, std::map<std::string, std::string>& kvp) {
	std::vector<std::string> tokens;
	boost::split(tokens, message, boost::is_any_of(";"), boost::token_compress_on);

	for (const auto& token : tokens) {
		size_t sep_pos = token.find('=');
		if (sep_pos != std::string::npos) {
			std::string key = token.substr(0, sep_pos);
			std::string value = token.substr(sep_pos + 1);

			boost::trim(key);
			boost::trim(value);
			boost::to_lower(key);

			kvp[key] = value;
		} else {
			LOG_WARNING << "Malformed token in message: " << token;
		}
	}
}

void handleModificationMessage(const std::shared_ptr<Client>& c, const std::string& message, const std::string& topic) {
	std::string clientId(c->GetId());
	auto tmgr = pod.GetManikin(DEFAULT_MANIKIN_ID);
	if (!tmgr) {
		LOG_ERROR << "No manikin manager available for modification message.";
		return;
	}

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
		if (modPayload.empty()) {
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

void processClientMessage(const std::shared_ptr<Client>& c, const std::string& message) {
	std::string clientId(c->GetId());

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
		int firstBracket = message.find('[');
		int lastBracket = message.find(']');

		if (firstBracket != std::string::npos && lastBracket != std::string::npos) {
			std::string topic = message.substr(firstBracket + 1, lastBracket - firstBracket - 1);
			std::string content = message.substr(lastBracket + 1);
			handleModificationMessage(c, content, topic);
		} else {
			LOG_ERROR << "Malformed generic topic message from client " << clientId << ": " << message;
		}
	} else if (message.find(" Connected") == 0) {
		// Module connected message, ignore
	} else {
		LOG_ERROR << "Unknown or unsupported message from client " << clientId << ": " << message;
	}
}



void UdpDiscoveryThread(short port, bool enabled, std::string manikin_id) {
	if (enabled) {
		boost::asio::io_service io_service;
		UdpDiscoveryServer udps(io_service, port, std::move(manikin_id));
		LOG_INFO << "UDP Discovery listening on port " << port;
		io_service.run();
	} else {
		LOG_INFO << "UDP discovery service not started due to command line option.";
	}
}

int main(int argc, const char* argv[]) {
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

	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

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
		} catch (const std::exception& e) {
			LOG_ERROR << "Unable to initialize manikins in POD: " << e.what();
			return 1;
		}

		// Start UDP discovery in a separate thread
		std::thread discoveryThread(UdpDiscoveryThread, discoveryPort, discovery, manikinId);

		// Create and start the TCP server
		try {
			s = std::make_unique<Server>(bridgePort);
			LOG_INFO << "TCP Bridge listening on port " << bridgePort;
			s->AcceptAndDispatch();
		} catch (const std::exception& e) {
			LOG_ERROR << "Server error: " << e.what();
			return 1;
		}

		// Wait for discovery thread to finish
		if (discoveryThread.joinable()) {
			discoveryThread.join();
		}

		LOG_INFO << "TCP Bridge shutdown.";
		return 0;

	} catch (const std::exception& e) {
		LOG_ERROR << "Error: " << e.what();
		return 1;
	}
}