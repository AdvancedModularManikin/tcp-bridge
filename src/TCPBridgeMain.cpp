#include <algorithm>
#include <fstream>
#include <map>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "Net/Client.h"
#include "Net/Server.h"
#include "Net/UdpDiscoveryServer.h"

#include "amm_std.h"

#include "amm/BaseLogger.h"

#include "TPMS.h"

#include "tinyxml2.h"

#include "bridge.h"

using namespace std;
using namespace tinyxml2;
using namespace AMM;
using namespace std;
using namespace std::chrono;
using namespace eprosima::fastdds::dds;
using namespace eprosima::fastrtps::rtps;

Server *s;

short discoveryPort = 8888;
int bridgePort = 9015;

// Daemonize by default
int daemonize = 1;
int discovery = 1;

std::map <std::string, std::string> clientMap;
std::map <std::string, std::string> clientTypeMap;

std::map <std::string, std::vector<std::string>> subscribedTopics;
std::map <std::string, std::vector<std::string>> publishedTopics;

std::map <std::string, std::string> globalInboundBuffer;
const string DEFAULT_MANIKIN_ID = "manikin_1";
const string capabilityPrefix = "CAPABILITY=";
const string settingsPrefix = "SETTINGS=";
const string statusPrefix = "STATUS=";
const string configPrefix = "CONFIG=";
const string modulePrefix = "MODULE_NAME=";
const string registerPrefix = "REGISTER=";
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

bool closed = false;

std::string ExtractTypeFromRenderMod(std::string payload) {
    std::size_t pos = payload.find("type=");
    if (pos != std::string::npos) {
        std::string p1 = payload.substr(pos + 6);
        std::size_t pos2 = p1.find("\"");
        if (pos2 != std::string::npos) {
            std::string p2 = p1.substr(0, pos2);
            return p2;
        }
    }
    return {};
};

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
    return {};
}

std::string gen_random(const int len) {
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

// Override client handler code from Net Server
void *Server::HandleClient(void *args) {
    auto *c = (Client *) args;

    char buffer[8192 - 25];
    int index;
    ssize_t n;


    std::string uuid = gen_random(10);

    ServerThread::LockMutex(uuid);
    c->SetId(uuid);
    string defaultName = "Client " + c->id;
    c->SetName(defaultName);
    Server::clients.push_back(*c);
    clientMap[c->id] = uuid;
    LOG_DEBUG << "Adding client with id: " << c->id;
    ServerThread::UnlockMutex(uuid);

    while (true) {
        memset(buffer, 0, sizeof buffer);
        n = recv(c->sock, buffer, sizeof buffer, 0);

        // Client disconnected?
        if (n == 0) {
            LOG_INFO << c->name << " disconnected";
            shutdown(c->sock, 2);
            close(c->sock);

            // Remove client in Static clients <vector>
            ServerThread::LockMutex(uuid);
            index = Server::FindClientIndex(c);
            LOG_DEBUG << "Erasing user in position " << index
                      << " whose name id is: " << Server::clients[index].id;
            Server::clients.erase(Server::clients.begin() + index);
            ServerThread::UnlockMutex(uuid);

            // Remove from our client/UUID map
            LOG_DEBUG << "Erasing from client map";
            auto it = clientMap.find(c->id);
            clientMap.erase(it);
            LOG_DEBUG << "Done shutting down socket.";

            break;
        } else if (n < 0) {
            LOG_ERROR << "Error while receiving message from client: " << c->name;
        } else {
            std::string tempBuffer(buffer);
            globalInboundBuffer[c->id] += tempBuffer;
            if (!boost::algorithm::ends_with(globalInboundBuffer[c->id], "\n")) {
                continue;
            }
            vector <string> strings = Utility::explode("\n", globalInboundBuffer[c->id]);
            globalInboundBuffer[c->id].clear();

            for (auto str : strings) {
                boost::trim_right(str);
                if (!str.empty()) {
                    if (str.find("KEEPALIVE") != std::string::npos) {
                        break;
                    }
                    std::string requestManikin = ExtractManikinIDFromString(str);
                    auto tmgr = pod.GetManikin(requestManikin);
                    if (str.substr(0, modulePrefix.size()) == modulePrefix) {
                        std::string moduleName = str.substr(modulePrefix.size());

                        // Add the modules name to the static Client vector
                        ServerThread::LockMutex(uuid);
                        c->SetName(moduleName);
                        ServerThread::UnlockMutex(uuid);
                        LOG_DEBUG << "Client " << c->id
                                  << " module connected: " << moduleName;
                    } else if (str.substr(0, registerPrefix.size()) == registerPrefix) {
                        // Registering for data
                        std::string registerVal = str.substr(registerPrefix.size());
                        LOG_INFO << "Client " << c->id
                                 << " registered for: " << registerVal;
                    } else if (str.substr(0, statusPrefix.size()) == statusPrefix) {
                        // Client set their status (OPERATIONAL, etc)
                        std::string statusVal;
                        try {
                            statusVal = Utility::decode64(str.substr(statusPrefix.size()));
                        } catch (exception &e) {
                            LOG_ERROR << "Error decoding base64 string: " << e.what();
                            break;
                        }

                        LOG_DEBUG << "Client " << c->id << " sent status"; //  << statusVal;
                        tmgr->HandleStatus(c, statusVal);
                    } else if (str.substr(0, capabilityPrefix.size()) ==
                               capabilityPrefix) {
                        // Client sent their capabilities / announced
                        std::string capabilityVal;
                        try {
                            capabilityVal = Utility::decode64(str.substr(capabilityPrefix.size()));
                        } catch (exception &e) {
                            LOG_ERROR << "Error decoding base64 string: " << e.what();
                            break;
                        }
                        LOG_INFO << "Client " << c->id << " sent capabilities."; // << capabilityVal;
                        tmgr->HandleCapabilities(c, capabilityVal);
                    } else if (str.substr(0, settingsPrefix.size()) == settingsPrefix) {
                        std::string settingsVal;
                        try {
                            settingsVal = Utility::decode64(str.substr(settingsPrefix.size()));
                        } catch (exception &e) {
                            LOG_ERROR << "Error decoding base64 string: " << e.what();
                            break;
                        }
                        LOG_INFO << "Client " << c->id << " sent settings: " << settingsVal;
                        tmgr->HandleSettings(c, settingsVal);
                    } else if (str.substr(0, keepHistoryPrefix.size()) ==
                               keepHistoryPrefix) {
                        // Setting the KEEP_HISTORY flag
                        std::string keepHistory = str.substr(keepHistoryPrefix.size());
                        if (keepHistory == "TRUE") {
                            LOG_DEBUG << "Client " << c->id << " wants to keep history.";
                            c->SetKeepHistory(true);
                        } else {
                            LOG_DEBUG << "Client " << c->id
                                      << " does not want to keep history.";
                            c->SetKeepHistory(false);
                        }
                    } else if (str.substr(0, requestPrefix.size()) == requestPrefix) {
                        std::string request = str.substr(requestPrefix.size());
                        tmgr->DispatchRequest(c, request);
                    } else if (str.substr(0, actionPrefix.size()) == actionPrefix) {
                        // Sending action
                        std::string action = str.substr(actionPrefix.size());
                        LOG_INFO << "Client " << c->id
                                 << " posting action to AMM: " << action;
                        AMM::Command cmdInstance;
                        cmdInstance.message(action);
                        // tmgr->PublishCommand(cmdInstance);
                    } else if (!str.compare(0, genericTopicPrefix.size(), genericTopicPrefix)) {
                        std::string manikin_id, topic, message, modType, modLocation, modPayload, modLearner, modInfo;
                        unsigned first = str.find("[");
                        unsigned last = str.find("]");
                        topic = str.substr(first + 1, last - first - 1);
                        message = str.substr(last + 1);

                        if (topic == "KEEPALIVE") {
                            continue;
                        }

                        LOG_INFO << "[Received a message for topic " << topic << " with a payload of: " << message;

                        std::list <std::string> tokenList;
                        split(tokenList, message, boost::algorithm::is_any_of(";"), boost::token_compress_on);
                        std::map <std::string, std::string> kvp;

                        BOOST_FOREACH(std::string
                        token, tokenList) {
                            size_t sep_pos = token.find_first_of("=");
                            std::string key = token.substr(0, sep_pos);
                            std::string value = (sep_pos == std::string::npos ? "" : token.substr(
                                    sep_pos + 1,
                                    std::string::npos));
                            kvp[key] = value;
                            LOG_DEBUG << "\t" << key << " => " << kvp[key];
                        }

                        auto mid = kvp.find("mid");
                        if (mid != kvp.end()) {
                            manikin_id = mid->second;
                        } else {
                            manikin_id = DEFAULT_MANIKIN_ID;
                        }
                        LOG_TRACE << "Manikin id is " << manikin_id;

                        auto type = kvp.find("type");
                        if (type != kvp.end()) {
                            modType = type->second;
                        }

                        auto location = kvp.find("location");
                        if (location != kvp.end()) {
                            modLocation = location->second;
                        }

                        auto participant_id = kvp.find("participant_id");
                        if (participant_id != kvp.end()) {
                            modLearner = participant_id->second;
                        }

                        auto payload = kvp.find("payload");
                        if (payload != kvp.end()) {
                            modPayload = payload->second;
                        }

                        auto info = kvp.find("info");
                        if (info != kvp.end()) {
                            modInfo = info->second;
                        }

                        AMM::UUID erID;
                        auto eid = kvp.find("event_id");
                        if (eid != kvp.end()) {
                            LOG_INFO << " Event record came in with an event ID so let's use it.";
                            erID.id(eid->second);
                        } else {
                            erID.id(tmgr->mgr->GenerateUuidString());
                        }

                        if (topic == "AMM_Render_Modification") {

                            FMA_Location fma;
                            fma.name(modLocation);

                            AMM::UUID agentID;
                            agentID.id(modLearner);

                            if (modType.empty()) {
                                modType = ExtractTypeFromRenderMod(modPayload);
                            };

                            AMM::EventRecord er;
                            er.id(erID);
                            er.location(fma);
                            er.agent_id(agentID);
                            er.type(modType);
                            tmgr->mgr->WriteEventRecord(er);

                            AMM::RenderModification renderMod;
                            renderMod.event_id(erID);
                            renderMod.type(modType);
                            renderMod.data(modPayload);
                            tmgr->mgr->WriteRenderModification(renderMod);
                            LOG_INFO << "We sent a render mod of type " << renderMod.type();
                            LOG_INFO << "\tPayload was: " << renderMod.data();
                        } else if (topic == "AMM_Physiology_Modification") {

                            FMA_Location fma;
                            fma.name(modLocation);

                            AMM::UUID agentID;
                            agentID.id(modLearner);

                            AMM::EventRecord er;
                            er.id(erID);
                            er.location(fma);
                            er.agent_id(agentID);
                            er.type(modType);
                            tmgr->mgr->WriteEventRecord(er);

                            AMM::PhysiologyModification physMod;
                            physMod.event_id(erID);
                            physMod.type(modType);
                            physMod.data(modPayload);
                            tmgr->mgr->WritePhysiologyModification(physMod);
                        } else if (topic == "AMM_Assessment") {
                            FMA_Location fma;
                            fma.name(modLocation);
                            AMM::UUID agentID;
                            agentID.id(modLearner);
                            AMM::EventRecord er;
                            er.id(erID);
                            er.location(fma);
                            er.agent_id(agentID);
                            er.type(modType);
                            tmgr->mgr->WriteEventRecord(er);

                            AMM::Assessment assessment;
                            assessment.event_id(erID);
                            tmgr->mgr->WriteAssessment(assessment);
                        } else if (topic == "AMM_Command") {
                            AMM::Command cmdInstance;
                            cmdInstance.message(message);
                            tmgr->mgr->WriteCommand(cmdInstance);
                        } else if (topic == "AMM_ModuleConfiguration") {
                             AMM::ModuleConfiguration mc;
                             mc.name(modType);
                             mc.capabilities_configuration(modPayload);
                            tmgr->mgr->WriteModuleConfiguration(mc);
                        } else {
                                LOG_DEBUG << "Unknown topic: " << topic;
                            }
                        }
                    } else if (str.substr(0, keepAlivePrefix.size()) == keepAlivePrefix) {
                        // keepalive, ignore it
                    } else {
                        if (!boost::algorithm::ends_with(str, "Connected") && str.size() > 3) {
                            LOG_ERROR << "Client " << c->id << " unknown message:" << str;
                        }
                    }
                }
            }
        }

}

void UdpDiscoveryThread() {
    if (discovery) {
        boost::asio::io_service io_service;
        UdpDiscoveryServer udps(io_service, discoveryPort);
        LOG_INFO << "UDP Discovery listening on port " << discoveryPort;
        io_service.run();
    } else {
        LOG_INFO << "UDP discovery service not started due to command line option.";
    }
}

static void show_usage(const std::string &name) {
    std::cerr << "Usage: " << name << " <option(s)>"
              << "\nOptions:\n"
              << "\t-h,--help\t\tShow this help message\n"
              << std::endl;
}


int main(int argc, const char *argv[]) {
    static plog::ColorConsoleAppender <plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    LOG_INFO << "=== [AMM - TCP Bridge] ===";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            show_usage(argv[0]);
            return 0;
        }

        if (arg == "-d") {
            daemonize = 1;
        }

        if (arg == "-nodiscovery") {
            discovery = 0;
        }
    }

    try {
        pod.InitializeManikins();
    } catch (exception &e) {
        LOG_ERROR << "Unable to initialize manikins in POD: " << e.what();
    }

    std::thread t1(UdpDiscoveryThread);
    s = new Server(bridgePort);
    std::string action;


    s->AcceptAndDispatch();

    t1.join();

    LOG_INFO << "TCP Bridge shutdown.";
}
