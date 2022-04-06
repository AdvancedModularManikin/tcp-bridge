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


//#include "AMM/Utility.h"

#include "tinyxml2.h"

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

std::string currentScenario = "NONE";
std::string currentState = "NONE";
std::string currentStatus = "NOT RUNNING";
bool isPaused = false;

bool closed = false;

std::map <std::string, std::vector<std::string>> subscribedTopics;
std::map <std::string, std::vector<std::string>> publishedTopics;

std::map <std::string, std::map<std::string, double>> labNodes;
std::map <std::string, std::map<std::string, std::string>> equipmentSettings;
std::map <std::string, std::string> clientMap;
std::map <std::string, std::string> clientTypeMap;


void InitializeLabNodes() {
    //
    labNodes["ALL"]["Substance_Sodium"] = 0.0f;
    labNodes["ALL"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["ALL"]["Substance_Glucose_Concentration"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_BloodUreaNitrogen_Concentration"] = 0.0f;
    labNodes["ALL"]["Substance_Creatinine_Concentration"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_WhiteBloodCell_Count"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_RedBloodCell_Count"] = 0.0f;
    labNodes["ALL"]["Substance_Hemoglobin_Concentration"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_Hemaocrit"] = 0.0f;
    labNodes["ALL"]["CompleteBloodCount_Platelet"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_BloodPH"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_Arterial_CarbonDioxide_Pressure"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_Arterial_Oxygen_Pressure"] = 0.0f;
    labNodes["ALL"]["Substance_Bicarbonate"] = 0.0f;
    labNodes["ALL"]["Substance_BaseExcess"] = 0.0f;
    labNodes["ALL"]["Substance_Lactate_Concentration_mmol"] = 0.0f;
    labNodes["ALL"]["BloodChemistry_CarbonMonoxide_Saturation"] = 0.0f;
    labNodes["ALL"]["Anion_Gap"] = 0.0f;
    labNodes["ALL"]["Substance_Ionized_Calcium"] = 0.0f;

    labNodes["POCT"]["Substance_Sodium"] = 0.0f;
    labNodes["POCT"]["MetabolicPanel_Potassium"] = 0.0f;
    labNodes["POCT"]["MetabolicPanel_Chloride"] = 0.0f;
    labNodes["POCT"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["POCT"]["Substance_Glucose_Concentration"] = 0.0f;
    labNodes["POCT"]["BloodChemistry_BloodUreaNitrogen_Concentration"] = 0.0f;
    labNodes["POCT"]["Substance_Creatinine_Concentration"] = 0.0f;
    labNodes["POCT"]["Anion_Gap"] = 0.0f;
    labNodes["POCT"]["Substance_Ionized_Calcium"] = 0.0f;

    labNodes["Hematology"]["BloodChemistry_Hemaocrit"] = 0.0f;
    labNodes["Hematology"]["Substance_Hemoglobin_Concentration"] = 0.0f;

    labNodes["ABG"]["BloodChemistry_BloodPH"] = 0.0f;
    labNodes["ABG"]["BloodChemistry_Arterial_CarbonDioxide_Pressure"] = 0.0f;
    labNodes["ABG"]["BloodChemistry_Arterial_Oxygen_Pressure"] = 0.0f;
    labNodes["ABG"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["ABG"]["Substance_Bicarbonate"] = 0.0f;
    labNodes["ABG"]["Substance_BaseExcess"] = 0.0f;
    labNodes["ABG"]["BloodChemistry_Oxygen_Saturation"] = 0.0f;
    labNodes["ABG"]["Substance_Lactate_Concentration_mmol"] = 0.0f;
    labNodes["ABG"]["BloodChemistry_CarbonMonoxide_Saturation"] = 0.0f;

    labNodes["VBG"]["BloodChemistry_BloodPH"] = 0.0f;
    labNodes["VBG"]["BloodChemistry_Arterial_CarbonDioxide_Pressure"] = 0.0f;
    labNodes["VBG"]["BloodChemistry_Arterial_Oxygen_Pressure"] = 0.0f;
    labNodes["VBG"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["VBG"]["Substance_Bicarbonate"] = 0.0f;
    labNodes["VBG"]["Substance_BaseExcess"] = 0.0f;
    labNodes["VBG"]["BloodChemistry_VenousCarbonDioxidePressure"] = 0.0f;
    labNodes["VBG"]["BloodChemistry_VenousOxygenPressure"] = 0.0f;
    labNodes["VBG"]["Substance_Lactate_Concentration_mmol"] = 0.0f;
    labNodes["VBG"]["BloodChemistry_CarbonMonoxide_Saturation"] = 0.0f;

    labNodes["BMP"]["Substance_Sodium"] = 0.0f;
    labNodes["BMP"]["MetabolicPanel_Potassium"] = 0.0f;
    labNodes["BMP"]["MetabolicPanel_Chloride"] = 0.0f;
    labNodes["BMP"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["BMP"]["Substance_Glucose_Concentration"] = 0.0f;
    labNodes["BMP"]["BloodChemistry_BloodUreaNitrogen_Concentration"] = 0.0f;
    labNodes["BMP"]["Substance_Creatinine_Concentration"] = 0.0f;
    labNodes["BMP"]["Anion_Gap"] = 0.0f;
    labNodes["BMP"]["Substance_Ionized_Calcium"] = 0.0f;

    labNodes["CBC"]["BloodChemistry_WhiteBloodCell_Count"] = 0.0f;
    labNodes["CBC"]["BloodChemistry_RedBloodCell_Count"] = 0.0f;
    labNodes["CBC"]["Substance_Hemoglobin_Concentration"] = 0.0f;
    labNodes["CBC"]["BloodChemistry_Hemaocrit"] = 0.0f;
    labNodes["CBC"]["CompleteBloodCount_Platelet"] = 0.0f;

    labNodes["CMP"]["Substance_Albumin_Concentration"] = 0.0f;
    labNodes["CMP"]["BloodChemistry_BloodUreaNitrogen_Concentration"] = 0.0f;
    labNodes["CMP"]["Substance_Calcium_Concentration"] = 0.0f;
    labNodes["CMP"]["MetabolicPanel_Chloride"] = 0.0f;
    labNodes["CMP"]["MetabolicPanel_CarbonDioxide"] = 0.0f;
    labNodes["CMP"]["Substance_Creatinine_Concentration"] = 0.0f;
    labNodes["CMP"]["Substance_Glucose_Concentration"] = 0.0f;
    labNodes["CMP"]["MetabolicPanel_Potassium"] = 0.0f;
    labNodes["CMP"]["Substance_Sodium"] = 0.0f;
    labNodes["CMP"]["MetabolicPanel_Bilirubin"] = 0.0f;
    labNodes["CMP"]["MetabolicPanel_Protein"] = 0.0f;
}


const std::string moduleName = "AMM_TCP_Bridge";
//const std::string configFile = "config/tcp_bridge_amm.xml";
//AMM::DDSManager <TCPBridgeListener> *tmgr = new AMM::DDSManager<TCPBridgeListener>(configFile);
AMM::UUID m_uuid;

/**
 * FastRTPS/DDS Listener for subscriptions
 */
class TCPBridgeListener {
public:
    std::map <std::string, AMM::EventRecord> eventRecords;
    AMM::DDSManager <TCPBridgeListener> *mgr;
    std::string manikin_id = "manikin_1";

    void setManager(AMM::DDSManager <TCPBridgeListener> *tmgr) {
        mgr = tmgr;
    }

    void setManikinID(std::string mid) {
        manikin_id = mid;
    }

    std::string getManikinID() {
        return manikin_id;
    }

    void onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info) {
        LOG_TRACE << "Module Configuration recieved for " << mc.name();

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::string clientType = clientTypeMap[it->first];
            if (clientType.find(mc.name()) != std::string::npos) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    std::string encodedConfigContent = Utility::encode64(mc.capabilities_configuration());
                    std::ostringstream encodedConfig;
                    encodedConfig << configPrefix << encodedConfigContent << ";mid=" << manikin_id << std::endl;
                    Server::SendToClient(c, encodedConfig.str());
                }
            }
            ++it;
        }
    }

    /// Event handler for incoming Physiology Waveform data.
    void onNewPhysiologyWaveform(AMM::PhysiologyWaveform &n, SampleInfo_t *info) {
        std::string hfname = "HF_" + n.name();
        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];
            if (std::find(subV.begin(), subV.end(), hfname) != subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    std::ostringstream messageOut;
                    messageOut << n.name() << "=" << n.value() << ";mid=" << manikin_id << "|" << std::endl;
                    string stringOut = messageOut.str();
                    Server::SendToClient(c, messageOut.str());
                }
            }
            ++it;
        }
    }

    void onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info) {
        // Drop values into the lab sheets
        for (auto &outer_map_pair : labNodes) {
            if (labNodes[outer_map_pair.first].find(n.name()) !=
                labNodes[outer_map_pair.first].end()) {
                labNodes[outer_map_pair.first][n.name()] = n.value();
            }
        }

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];

            if (std::find(subV.begin(), subV.end(), n.name()) != subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    std::ostringstream messageOut;
                    messageOut << n.name() << "=" << n.value() << ";mid=" << manikin_id << "|" << std::endl;
                    Server::SendToClient(c, messageOut.str());
                }
            }
            ++it;
        }
    }

    void onNewPhysiologyModification(AMM::PhysiologyModification &pm, SampleInfo_t *info) {
        std::string location;
        std::string practitioner;

        if (eventRecords.count(pm.event_id().id()) > 0) {
            AMM::EventRecord er = eventRecords[pm.event_id().id()];
            location = er.location().name();
            practitioner = er.agent_id().id();
        }

        std::ostringstream messageOut;
        messageOut << "[AMM_Physiology_Modification]"
                   << "id=" << pm.id().id() << ";"
                   << "mid=" << manikin_id << ";"
                   << "event_id=" << pm.event_id().id() << ";"
                   << "type=" << pm.type() << ";"
                   << "location=" << location << ";"
                   << "participant_id=" << practitioner << ";"
                   << "payload=" << pm.data()
                   << std::endl;
        string stringOut = messageOut.str();

        LOG_DEBUG << "Received a phys mod via DDS, republishing to TCP clients: " << stringOut;

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];

            if (std::find(subV.begin(), subV.end(), pm.type()) != subV.end() ||
                std::find(subV.begin(), subV.end(), "AMM_Physiology_Modification") !=
                subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    Server::SendToClient(c, stringOut);
                }
            }
            ++it;
        }
    }

    void onNewEventRecord(AMM::EventRecord &er, SampleInfo_t *info) {
        std::string location;
        std::string practitioner;
        std::string eType;
        std::string eData;
        std::string pType;

        LOG_DEBUG << "Received an event record of type " << er.type()
                  << " on DDS bus, so we're storing it in a simple map.";
        eventRecords[er.id().id()] = er;
        location = er.location().name();
        practitioner = er.agent_id().id();
        eType = er.type();
        eData = er.data();
        pType = AMM::Utility::EEventAgentTypeStr(er.agent_type());

        std::ostringstream messageOut;

        messageOut << "[AMM_EventRecord]"
                   << "id=" << er.id().id() << ";"
                   << "mid=" << manikin_id << ";"
                   << "type=" << eType << ";"
                   << "location=" << location << ";"
                   << "participant_id=" << practitioner << ";"
                   << "participant_type=" << pType << ";"
                   << "data=" << eData << ";"
                   << std::endl;
        string stringOut = messageOut.str();

        LOG_DEBUG << "Received an EventRecord via DDS, republishing to TCP clients: " << stringOut;

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];
            if (std::find(subV.begin(), subV.end(), "AMM_EventRecord") != subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    Server::SendToClient(c, stringOut);
                }
            }
            ++it;
        }
    }

    void onNewAssessment(AMM::Assessment &a, eprosima::fastrtps::SampleInfo_t *info) {
        std::string location;
        std::string practitioner;
        std::string eType;

        LOG_INFO << "Assessment received on DDS bus";
        if (eventRecords.count(a.event_id().id()) > 0) {
            AMM::EventRecord er = eventRecords[a.event_id().id()];
            location = er.location().name();
            practitioner = er.agent_id().id();
            eType = er.type();
        }

        std::ostringstream messageOut;

        messageOut << "[AMM_Assessment]"
                   << "id=" << a.id().id() << ";"
                   << "mid=" << manikin_id << ";"
                   << "event_id=" << a.event_id().id() << ";"
                   << "type=" << eType << ";"
                   << "location=" << location << ";"
                   << "participant_id=" << practitioner << ";"
                   << "value=" << AMM::Utility::EAssessmentValueStr(a.value()) << ";"
                   << "comment=" << a.comment()
                   << std::endl;
        string stringOut = messageOut.str();

        LOG_DEBUG << "Received an assessment via DDS, republishing to TCP clients: " << stringOut;

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];
            if (std::find(subV.begin(), subV.end(), "AMM_Assessment") != subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    Server::SendToClient(c, stringOut);
                }
            }
            ++it;
        }
    }

    void onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info) {
        std::string location;
        std::string practitioner;

        // LOG_INFO << "Render mod received on DDS bus";
        if (eventRecords.count(rendMod.event_id().id()) > 0) {
            AMM::EventRecord er = eventRecords[rendMod.event_id().id()];
            location = er.location().name();
            practitioner = er.agent_id().id();
        }

        std::ostringstream messageOut;
        std::string rendModPayload;
        std::string rendModType;
        if (rendMod.data().empty()) {
            rendModPayload = "<RenderModification type='" + rendMod.type() + "'/>";
            rendModType = "";
        } else {
            rendModPayload = rendMod.data();
            rendModType = rendMod.type();
        }
        messageOut << "[AMM_Render_Modification]"
                   << "id=" << rendMod.id().id() << ";"
                   << "mid=" << manikin_id << ";"
                   << "event_id=" << rendMod.event_id().id() << ";"
                   << "type=" << rendModType << ";"
                   << "location=" << location << ";"
                   << "participant_id=" << practitioner << ";"
                   // << "payload=" << rendMod.data()
                   << "payload=" << rendModPayload
                   << std::endl;
        string stringOut = messageOut.str();

        if (rendModPayload.find("START_OF") == std::string::npos) {
            LOG_DEBUG << "Received a render mod via DDS, republishing to TCP clients: " << stringOut;
        }

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];
            if (std::find(subV.begin(), subV.end(), rendMod.type()) != subV.end() ||
                std::find(subV.begin(), subV.end(), "AMM_Render_Modification") !=
                subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    Server::SendToClient(c, stringOut);
                }
            }
            ++it;
        }
    }

    void onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info) {
        bool doWriteTopic = false;

        switch (simControl.type()) {
            case AMM::ControlType::RUN: {
                currentStatus = "RUNNING";
                isPaused = false;
                LOG_INFO << "Message recieved; Run sim.";
                std::ostringstream tmsg;
                tmsg << "ACT=START_SIM" << ";mid=" << manikin_id << std::endl;
                s->SendToAll(tmsg.str());
                break;
            }

            case AMM::ControlType::HALT: {
                if (isPaused) {
                    currentStatus = "PAUSED";
                } else {
                    currentStatus = "NOT RUNNING";
                }
                LOG_INFO << "Message recieved; Halt sim";
                std::ostringstream tmsg;
                tmsg << "ACT=PAUSE_SIM" << ";mid=" << manikin_id << std::endl;
                s->SendToAll(tmsg.str());
                break;
            }

            case AMM::ControlType::RESET: {
                currentStatus = "NOT RUNNING";
                isPaused = false;
                LOG_INFO << "Message recieved; Reset sim";
                std::ostringstream tmsg;
                tmsg << "ACT=RESET_SIM" << ";mid=" << manikin_id << std::endl;
                s->SendToAll(tmsg.str());
                break;
            }

            case AMM::ControlType::SAVE: {
                LOG_INFO << "Message recieved; Save sim";
                //SaveSimulation(doWriteTopic);
                break;
            }
        }
    }

    void onNewOperationalDescription(AMM::OperationalDescription &opD, SampleInfo_t *info) {
        LOG_INFO << "Operational description for module " << opD.name() << " / model " << opD.model();

        // [AMM_OperationalDescription]name=;description=;manufacturer=;model=;serial_number=;module_id=;module_version=;configuration_version=;AMM_version=;capabilities_configuration=(BASE64 ENCODED STRING - URLSAFE)

        std::ostringstream messageOut;
        std::string capabilities = Utility::encode64(opD.capabilities_schema());

        messageOut << "[AMM_OperationalDescription]"
                   << "name=" << opD.name() << ";"
                   << "mid=" << manikin_id << ";"
                   << "description=" << opD.description() << ";"
                   << "manufacturer=" << opD.manufacturer() << ";"
                   << "model=" << opD.model() << ";"
                   << "serial_number=" << opD.serial_number() << ";"
                   << "module_id=" << opD.module_id().id() << ";"
                   << "module_version=" << opD.module_version() << ";"
                   << "configuration_version=" << opD.configuration_version() << ";"
                   << "AMM_version=" << opD.AMM_version() << ";"
                   << "capabilities_configuration=" << capabilities
                   << std::endl;
        string stringOut = messageOut.str();

        LOG_DEBUG << "Received an Operational Description via DDS, republishing to TCP clients: " << stringOut;

        auto it = clientMap.begin();
        while (it != clientMap.end()) {
            std::string cid = it->first;
            std::vector <std::string> subV = subscribedTopics[cid];
            if (std::find(subV.begin(), subV.end(), "AMM_OperationalDescription") != subV.end()) {
                Client *c = Server::GetClientByIndex(cid);
                if (c) {
                    Server::SendToClient(c, stringOut);
                }
            }
            ++it;
        }
    }

    void onNewCommand(AMM::Command &c, eprosima::fastrtps::SampleInfo_t *info) {
        LOG_INFO << "** Message came in on manikin " << manikin_id << ": " << c.message();
        if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
            std::string value = c.message().substr(sysPrefix.size());
            if (value.find("START_SIM") != std::string::npos) {
                currentStatus = "RUNNING";
                isPaused = false;
                AMM::SimulationControl simControl;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                simControl.timestamp(ms);
                simControl.type(AMM::ControlType::RUN);
                mgr->WriteSimulationControl(simControl);
                std::string tmsg = "ACT=START_SIM;mid=" + manikin_id;
                s->SendToAll(tmsg);
            } else if (value.find("STOP_SIM") != std::string::npos) {
                currentStatus = "NOT RUNNING";
                isPaused = false;
                AMM::SimulationControl simControl;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                simControl.timestamp(ms);
                simControl.type(AMM::ControlType::HALT);
                mgr->WriteSimulationControl(simControl);
                std::string tmsg = "ACT=STOP_SIM;mid=" + manikin_id;
                s->SendToAll(tmsg);
            } else if (value.find("PAUSE_SIM") != std::string::npos) {
                currentStatus = "PAUSED";
                isPaused = true;
                AMM::SimulationControl simControl;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                simControl.timestamp(ms);
                simControl.type(AMM::ControlType::HALT);
                mgr->WriteSimulationControl(simControl);
                std::string tmsg = "ACT=PAUSE_SIM;mid=" + manikin_id;
                s->SendToAll(tmsg);
            } else if (value.find("RESET_SIM") != std::string::npos) {
                currentStatus = "NOT RUNNING";
                isPaused = false;
                std::string tmsg = "ACT=RESET_SIM;mid=" + manikin_id;
                s->SendToAll(tmsg);
                AMM::SimulationControl simControl;
                auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                simControl.timestamp(ms);
                simControl.type(AMM::ControlType::RESET);
                mgr->WriteSimulationControl(simControl);
                InitializeLabNodes();
            } else if (value.find("END_SIMULATION") != std::string::npos) {
                currentStatus = "NOT RUNNING";
                isPaused = true;
                std::string tmsg = "ACT=END_SIMULATION_SIM;mid=" + manikin_id;
                s->SendToAll(tmsg);
            }
            else if (!value.compare(0, loadScenarioPrefix.size(), loadScenarioPrefix)) {
                // currentScenario = value.substr(loadScenarioPrefix.size());
                // sendConfigToAll(currentScenario);
                // std::ostringstream messageOut;
                // messageOut << "ACT" << "=" << c.message() << std::endl;
                // s->SendToAll(messageOut.str());
            } else if (!value.compare(0, loadPrefix.size(), loadPrefix)) {
                // currentState = value.substr(loadStatePrefix.size());
                // std::ostringstream messageOut;
                // messageOut << "ACT" << "=" << c.message() << std::endl;
                // s->SendToAll(messageOut.str());
            } else {
                std::ostringstream messageOut;
                messageOut << "ACT" << "=" << c.message() << ";mid=" << manikin_id << std::endl;
                LOG_INFO << "Sending unknown system message: " << messageOut.str();
                s->SendToAll(messageOut.str());
            }
        } else {
            std::ostringstream messageOut;
            messageOut << "ACT"
                       << "=" << c.message() << ";mid=" << manikin_id << std::endl;
            LOG_INFO << "Sending unknown message: " << messageOut.str();
            s->SendToAll(messageOut.str());
        }
    }
};


void PublishSettings(AMM::DDSManager <TCPBridgeListener> *tmgr, std::string const &equipmentType) {
    std::ostringstream payload;
    LOG_INFO << "Publishing equipment " << equipmentType << " settings";
    for (auto &inner_map_pair : equipmentSettings[equipmentType]) {
        payload << inner_map_pair.first << "=" << inner_map_pair.second
                << std::endl;
        LOG_DEBUG << "\t" << inner_map_pair.first << ": " << inner_map_pair.second;
    }

    AMM::InstrumentData i;
    i.instrument(equipmentType);
    i.payload(payload.str());
    tmgr->WriteInstrumentData(i);
}

void HandleSettings(AMM::DDSManager <TCPBridgeListener> *tmgr, Client *c, std::string const &settingsVal) {
    XMLDocument doc(false);
    doc.Parse(settingsVal.c_str());
    tinyxml2::XMLNode *root =
            doc.FirstChildElement("AMMModuleConfiguration");
    tinyxml2::XMLElement *module = root->FirstChildElement("module");
    tinyxml2::XMLElement *caps =
            module->FirstChildElement("capabilities");
    if (caps) {
        for (tinyxml2::XMLNode *node =
                caps->FirstChildElement("capability");
             node; node = node->NextSibling()) {
            tinyxml2::XMLElement *cap = node->ToElement();
            std::string capabilityName = cap->Attribute("name");
            tinyxml2::XMLElement *configEl =
                    cap->FirstChildElement("configuration");
            if (configEl) {
                for (tinyxml2::XMLNode *settingNode =
                        configEl->FirstChildElement("setting");
                     settingNode; settingNode = settingNode->NextSibling()) {
                    tinyxml2::XMLElement *setting = settingNode->ToElement();
                    std::string settingName = setting->Attribute("name");
                    std::string settingValue = setting->Attribute("value");
                    equipmentSettings[capabilityName][settingName] =
                            settingValue;
                }
            }
            PublishSettings(tmgr, capabilityName);
        }
    }
}

void HandleCapabilities(AMM::DDSManager <TCPBridgeListener> *tmgr, Client *c, std::string const &capabilityVal) {
    XMLDocument doc(false);
    doc.Parse(capabilityVal.c_str());

    tinyxml2::XMLNode *root = doc.FirstChildElement("AMMModuleConfiguration");
    tinyxml2::XMLElement *module = root->FirstChildElement("module")->ToElement();
    const char *name = module->Attribute("name");
    const char *manufacturer = module->Attribute("manufacturer");
    const char *model = module->Attribute("model");
    const char *serial = module->Attribute("serial_number");
    const char *module_version = module->Attribute("module_version");

    std::string nodeName(name);
    std::string nodeManufacturer(manufacturer);
    std::string nodeModel(model);
    std::string serialNumber(serial);
    std::string moduleVersion(module_version);

    AMM::OperationalDescription od;
    od.name(nodeName);
    od.model(nodeModel);
    od.manufacturer(nodeManufacturer);
    od.serial_number(serialNumber);
    od.module_id(m_uuid);
    od.module_version(moduleVersion);
    // const std::string capabilities = AMM::Utility::read_file_to_string("config/tcp_bridge_capabilities.xml");
    od.capabilities_schema(capabilityVal);
    od.description();
    tmgr->WriteOperationalDescription(od);

    // Set the client's type
    ServerThread::LockMutex(c->id);
    c->SetClientType(nodeName);
    clientTypeMap[c->id] = nodeName;
    ServerThread::UnlockMutex(c->id);

    subscribedTopics[c->id].clear();
    publishedTopics[c->id].clear();

    tinyxml2::XMLElement *caps =
            module->FirstChildElement("capabilities");
    if (caps) {
        for (tinyxml2::XMLNode *node = caps->FirstChildElement("capability"); node; node = node->NextSibling()) {
            tinyxml2::XMLElement *cap = node->ToElement();
            std::string capabilityName = cap->Attribute("name");
            tinyxml2::XMLElement *starting_settings =
                    cap->FirstChildElement("starting_settings");
            if (starting_settings) {
                for (tinyxml2::XMLNode *settingNode =
                        starting_settings->FirstChildElement("setting");
                     settingNode; settingNode = settingNode->NextSibling()) {
                    tinyxml2::XMLElement *setting = settingNode->ToElement();
                    std::string settingName = setting->Attribute("name");
                    std::string settingValue = setting->Attribute("value");
                    equipmentSettings[capabilityName][settingName] =
                            settingValue;
                }
                PublishSettings(tmgr, capabilityName);
            }

            tinyxml2::XMLNode *subs =
                    node->FirstChildElement("subscribed_topics");
            if (subs) {
                for (tinyxml2::XMLNode *sub =
                        subs->FirstChildElement("topic");
                     sub; sub = sub->NextSibling()) {
                    tinyxml2::XMLElement *s = sub->ToElement();
                    std::string subTopicName = s->Attribute("name");

                    if (s->Attribute("nodepath")) {
                        std::string subNodePath = s->Attribute("nodepath");
                        if (subTopicName == "AMM_HighFrequencyNode_Data") {
                            subTopicName = "HF_" + subNodePath;
                        } else {
                            subTopicName = subNodePath;
                        }
                    }
                    Utility::add_once(subscribedTopics[c->id], subTopicName);
                    LOG_DEBUG << "[" << capabilityName << "][" << c->id
                              << "] Subscribing to " << subTopicName;
                }
            }

            // Store published topics for this capability
            tinyxml2::XMLNode *pubs =
                    node->FirstChildElement("published_topics");
            if (pubs) {
                for (tinyxml2::XMLNode *pub =
                        pubs->FirstChildElement("topic");
                     pub; pub = pub->NextSibling()) {
                    tinyxml2::XMLElement *p = pub->ToElement();
                    std::string pubTopicName = p->Attribute("name");
                    Utility::add_once(publishedTopics[c->id], pubTopicName);
                    LOG_DEBUG << "[" << capabilityName << "][" << c->id
                              << "] Publishing " << pubTopicName;
                }
            }
        }
    }
}

void HandleStatus(AMM::DDSManager <TCPBridgeListener> *tmgr, Client *c, std::string const &statusVal) {
    XMLDocument doc(false);
    doc.Parse(statusVal.c_str());

    tinyxml2::XMLNode *root = doc.FirstChildElement("AMMModuleStatus");
    tinyxml2::XMLElement *module =
            root->FirstChildElement("module")->ToElement();
    const char *name = module->Attribute("name");
    std::string nodeName(name);

    std::size_t found = statusVal.find(haltingString);
    AMM::Status s;
    s.module_id(m_uuid);
    s.capability(nodeName);
    if (found != std::string::npos) {
        s.value(AMM::StatusValue::INOPERATIVE);
    } else {
        s.value(AMM::StatusValue::OPERATIONAL);
    }
    tmgr->WriteStatus(s);
}

void DispatchRequest(AMM::DDSManager <TCPBridgeListener> *tmgr, Client *c, std::string const &request,
                     std::string mid = std::string()) {
    if (boost::starts_with(request, "STATUS")) {
        LOG_DEBUG << "STATUS request";
        std::ostringstream messageOut;
        messageOut << "STATUS" << "=" << currentStatus << "|";
        messageOut << "SCENARIO" << "=" << currentScenario << "|";
        messageOut << "STATE" << "=" << currentState << "|";
        Server::SendToClient(c, messageOut.str());
    } else if (boost::starts_with(request, "LABS")) {
        LOG_DEBUG << "LABS request: " << request;
        const auto equals_idx = request.find_first_of(';');
        if (std::string::npos != equals_idx) {
            auto str = request.substr(equals_idx + 1);
            LOG_DEBUG << "Return lab values for " << str;
            auto it = labNodes[str].begin();
            while (it != labNodes[str].end()) {
                std::ostringstream messageOut;
                messageOut << it->first << "=" << it->second << ":" << str << ";mid=" << mid << "|";
                Server::SendToClient(c, messageOut.str());
                ++it;
            }
        } else {
            LOG_DEBUG << "No specific labs requested, return all values.";
            auto it = labNodes["ALL"].begin();
            while (it != labNodes["ALL"].end()) {
                std::ostringstream messageOut;
                messageOut << it->first << "=" << it->second << ";mid=" << mid << "|";
                Server::SendToClient(c, messageOut.str());
                ++it;
            }
        }
    }
}

class TPMS_POD {
public:
    AMM::DDSManager <TCPBridgeListener> *mgr1 = new AMM::DDSManager<TCPBridgeListener>("config/tcp_bridge_ajams.xml",
                                                                                       "manikin_1");

    AMM::DDSManager <TCPBridgeListener> *GetManikin1() {
        return mgr1;
    }

    void InitializeManikins() {
        LOG_INFO << "Initializing DDS managers for each manikin...";
        InitializeManager(mgr1, "manikin_1");
    }

    void InitializeManager(AMM::DDSManager <TCPBridgeListener> *tmgr, std::string manikin_id) {
        LOG_INFO << "\tinitializing " << manikin_id;
        TCPBridgeListener tl;

        tl.setManager(tmgr);
        tl.setManikinID(manikin_id);

        tmgr->InitializeCommand();
        tmgr->InitializeInstrumentData();
        tmgr->InitializeSimulationControl();
        tmgr->InitializePhysiologyModification();
        tmgr->InitializeRenderModification();
        tmgr->InitializeAssessment();
        tmgr->InitializePhysiologyValue();
        tmgr->InitializePhysiologyWaveform();
        tmgr->InitializeEventRecord();
        tmgr->InitializeOperationalDescription();
        tmgr->InitializeModuleConfiguration();
        tmgr->InitializeStatus();

        tmgr->CreateOperationalDescriptionPublisher();
        tmgr->CreateModuleConfigurationPublisher();
        tmgr->CreateStatusPublisher();
        tmgr->CreateEventRecordPublisher();
        tmgr->CreatePhysiologyValueSubscriber(&tl, &TCPBridgeListener::onNewPhysiologyValue);
        tmgr->CreatePhysiologyWaveformSubscriber(&tl, &TCPBridgeListener::onNewPhysiologyWaveform);
        tmgr->CreateCommandSubscriber(&tl, &TCPBridgeListener::onNewCommand);
        tmgr->CreateSimulationControlSubscriber(&tl, &TCPBridgeListener::onNewSimulationControl);
        tmgr->CreateAssessmentSubscriber(&tl, &TCPBridgeListener::onNewAssessment);
        tmgr->CreateRenderModificationSubscriber(&tl, &TCPBridgeListener::onNewRenderModification);
        tmgr->CreatePhysiologyModificationSubscriber(&tl, &TCPBridgeListener::onNewPhysiologyModification);
        tmgr->CreateEventRecordSubscriber(&tl, &TCPBridgeListener::onNewEventRecord);
        tmgr->CreateOperationalDescriptionSubscriber(&tl, &TCPBridgeListener::onNewOperationalDescription);
        tmgr->CreateModuleConfigurationSubscriber(&tl, &TCPBridgeListener::onNewModuleConfiguration);
        tmgr->CreateRenderModificationPublisher();
        tmgr->CreatePhysiologyModificationPublisher();
        tmgr->CreateSimulationControlPublisher();
        tmgr->CreateCommandPublisher();
        tmgr->CreateInstrumentDataPublisher();
        tmgr->CreateAssessmentPublisher();
        m_uuid.id(tmgr->GenerateUuidString());
    }

};

TPMS_POD pod;

std::string ExtractTypeFromRenderMod(std::string payload) {
    std::size_t pos = payload.find("type=");
    if (pos != std::string::npos) {
        std::string p1 = payload.substr(pos+6);
        std::size_t pos2 = p1.find("\"");
        if (pos2 != std::string::npos) {
            std::string p2 = p1.substr(0,pos2);
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

// Override client handler code from Net Server
void *Server::HandleClient(void *args) {
    auto *c = (Client *) args;

    char buffer[8192 - 25];
    int index;
    ssize_t n;

    AMM::DDSManager <TCPBridgeListener> *tmgr = pod.GetManikin1();

    std::string uuid = tmgr->GenerateUuidString();

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

                        LOG_DEBUG << "Client " << c->id << " sent status: " << statusVal;
                        HandleStatus(tmgr, c, statusVal);
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
                        LOG_INFO << "Client " << c->id
                                 << " sent capabilities: " << capabilityVal;
                        HandleCapabilities(tmgr, c, capabilityVal);
                    } else if (str.substr(0, settingsPrefix.size()) == settingsPrefix) {
                        std::string settingsVal;
                        try {
                            settingsVal = Utility::decode64(str.substr(settingsPrefix.size()));
                        } catch (exception &e) {
                            LOG_ERROR << "Error decoding base64 string: " << e.what();
                            break;
                        }
                        LOG_INFO << "Client " << c->id << " sent settings: " << settingsVal;
                        HandleSettings(tmgr, c, settingsVal);
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
                        DispatchRequest(tmgr, c, request);
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
                            erID.id(tmgr->GenerateUuidString());
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
                            tmgr->WriteEventRecord(er);

                            AMM::RenderModification renderMod;
                            renderMod.event_id(erID);
                            renderMod.type(modType);
                            renderMod.data(modPayload);
                            tmgr->WriteRenderModification(renderMod);
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
                            tmgr->WriteEventRecord(er);

                            AMM::PhysiologyModification physMod;
                            physMod.event_id(erID);
                            physMod.type(modType);
                            physMod.data(modPayload);
                            tmgr->WritePhysiologyModification(physMod);
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
                            tmgr->WriteEventRecord(er);

                            AMM::Assessment assessment;
                            assessment.event_id(erID);
                            tmgr->WriteAssessment(assessment);
                        } else if (topic == "AMM_Command") {
                            AMM::Command cmdInstance;
                            cmdInstance.message(message);
                            tmgr->WriteCommand(cmdInstance);
                        } else {
                            LOG_DEBUG << "Unknown topic: " << topic;
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

    return nullptr;
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

void PublishOperationalDescription(AMM::DDSManager <TCPBridgeListener> *tmgr) {
    AMM::OperationalDescription od;
    od.name(moduleName);
    od.model("TCP Bridge");
    od.manufacturer("Vcom3D");
    od.serial_number("1.0.0");
    od.module_id(m_uuid);
    od.module_version("1.0.0");
    const std::string capabilities = AMM::Utility::read_file_to_string("config/tcp_bridge_capabilities.xml");
    od.capabilities_schema(capabilities);
    od.description();
    tmgr->WriteOperationalDescription(od);
}

void PublishConfiguration(AMM::DDSManager <TCPBridgeListener> *tmgr) {
    AMM::ModuleConfiguration mc;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    mc.timestamp(ms);
    mc.module_id(m_uuid);
    mc.name(moduleName);
    const std::string configuration = AMM::Utility::read_file_to_string("config/tcp_bridge_configuration.xml");
    mc.capabilities_configuration(configuration);
    tmgr->WriteModuleConfiguration(mc);
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


    InitializeLabNodes();

    pod.InitializeManikins();

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    std::thread t1(UdpDiscoveryThread);
    s = new Server(bridgePort);
    std::string action;


    s->AcceptAndDispatch();

    t1.join();

    LOG_INFO << "TCP Bridge shutdown.";
}
