#pragma once

#include "amm_std.h"

#include "amm/Utility.h"

#include "amm/TopicNames.h"

#include "Net/Server.h"
#include "Net/Client.h"

#include <map>
#include <utility>
#include <tinyxml2.h>
#include <boost/process.hpp>

#include "bridge.h"

using namespace std;

class Manikin : ListenerInterface {

protected:
    const std::string moduleName = "AMM_TCP_Bridge";
    const std::string config_file = "config/tcp_bridge_ajams.xml";

    std::map <std::string, AMM::EventRecord> eventRecords;
    std::string manikin_id;

    std::map <std::string, std::map<std::string, double>> labNodes;

    std::vector <std::string> primaryServices = {
            "amm_module_manager",
            "amm_physiology_manager",
            "amm_sim_manager",
            "amm_tcp_bridge",
            "amm_rest_adapter",
            "simple_assessment",
            "amm_xapi_module",
            "amm_serial_bridge",
            "amm_sound",
            "ajams_services"
    };

    std::vector <std::string> secondaryServices = {
            "amm_module_manager",
            "amm_physiology_manager",
            "amm_sim_manager",
            "amm_tcp_bridge",
            "amm_rest_adapter",
            "simple_assessment",
            "amm_xapi_module",
            "amm_serial_bridge",
            "amm_sound",
            "ajams_services"
    };

public:
    Manikin(const std::string& mid, bool mode, std::string parentId);

	~Manikin();

    bool podMode = false;

  std::unique_ptr<AMM::DDSManager<Manikin>> mgr;


    static std::string ExtractServiceFromCommand(const std::string& in);

    void MakePrimary();
    void MakeSecondary();
    static bool isAuthorized();
    void sendConfig(Client *c, const std::string& scene, const std::string& clientType);
    void sendConfigToAll(const std::string& scene);

    void PublishSettings(std::string const &equipmentType);

    void HandleSettings(Client *c, std::string const &settingsVal);

    void HandleCapabilities(Client *c, std::string const &capabilityVal);

    void HandleStatus(Client *c, std::string const &statusVal);

    void DispatchRequest(Client *c, std::string const &request, std::string mid = std::string());

    void PublishOperationalDescription();

    void PublishConfiguration();

    void InitializeLabNodes();

	void SendEventRecord(const AMM::UUID &erID,
	                               const AMM::FMA_Location &location, const AMM::UUID &agentID, const std::string &type) const;

	void SendRenderModification(const AMM::UUID &erID,
	                                      const std::string &type, const std::string &payload) const;

	void SendPhysiologyModification(const AMM::UUID &erID,
	                                          const std::string &type, const std::string &payload) const;

	void SendAssessment(const AMM::UUID &erID) const;

	void SendCommand(const std::string &message) const;

	void SendModuleConfiguration(const std::string &name,
	                                       const std::string &config) const;

private:


    AMM::UUID m_uuid;

    Server* s{};

    std::string parentId;

    std::map <std::string, std::map<std::string, std::string>> equipmentSettings;

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


    std::map<std::string, double> nodeDataStorage;

    std::map<std::string, std::string> statusStorage = {{"STATUS",         "NOT RUNNING"},
                                                        {"TICK",           "0"},
                                                        {"TIME",           "0"},
                                                        {"SCENARIO",       ""},
                                                        {"STATE",          ""},
                                                        {"AIR_SUPPLY",     ""},
                                                        {"CLEAR_SUPPLY",   ""},
                                                        {"BLOOD_SUPPLY",   ""},
                                                        {"FLUIDICS_STATE", ""},
                                                        {"BATTERY1",       ""},
                                                        {"BATTERY2",       ""},
                                                        {"EXT_POWER",      ""},
                                                        {"IVARM_STATE",    ""}};

    bool isPaused = false;
	std::mutex m_mapmutex;                  // For clientTypeMap
	std::mutex m_clientMapMutex;            // For clientMap
	std::mutex m_topicMutex;                // For subscribedTopics and publishedTopics
	std::mutex m_labMutex;                  // For labNodes
	std::mutex m_eventRecordMutex;          // For eventRecords
	std::mutex m_equipmentSettingsMutex;    // For equipmentSettings
	std::mutex m_statusMutex;               // For currentStatus, currentScenario, currentState, and isPaused
	std::mutex gcMapMutex;

protected:

    /// Event listener for Logs.
    void onNewLog(AMM::Log &log, SampleInfo_t *info);

    /// Event listener for Module Configuration
    void onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info);

    /// Event listener for Status
    void onNewStatus(AMM::Status &status, SampleInfo_t *info);

    /// Event listener for Simulation Controller
    void onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info);

    /// Event listener for Assessment.
    void onNewAssessment(AMM::Assessment &assessment, SampleInfo_t *info);

    /// Event listener for Event Fragment.
    void onNewEventFragment(AMM::EventFragment &eventFrag, SampleInfo_t *info);

    /// Event listener for Event Record.
    void onNewEventRecord(AMM::EventRecord &eventRec, SampleInfo_t *info);

    /// Event listener for Fragment Amendment Request.
    void onNewFragmentAmendmentRequest(AMM::FragmentAmendmentRequest &ffar, SampleInfo_t *info);

    /// Event listener for Omitted Event.
    void onNewOmittedEvent(AMM::OmittedEvent &omittedEvent, SampleInfo_t *info);

    /// Event listener for Operational Description.
    void onNewOperationalDescription(AMM::OperationalDescription &opDescript, SampleInfo_t *info);

    /// Event listener for Render Modification.
    void onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info);

    /// Event listener for Physiology Modification.
    void onNewPhysiologyModification(AMM::PhysiologyModification &physMod, SampleInfo_t *info);

    /// Event listener for Command.
    void onNewCommand(AMM::Command &command, eprosima::fastrtps::SampleInfo_t *info);

	/// Event listener for physiology waves
    void onNewPhysiologyWaveform(AMM::PhysiologyWaveform &n, SampleInfo_t *info);

	/// Event listener for physiology values
    void onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info);


};


