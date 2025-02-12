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

class Manikin : ListenerInterface {
protected:
	const std::string moduleName = "AMM_TCP_Bridge";
	const std::string config_file = "config/tcp_bridge_ajams.xml";

	std::map<std::string, AMM::EventRecord> eventRecords;
	std::string manikin_id;

	std::mutex m_mapmutex;
	std::mutex m_topicmutex;

	std::map<std::string, std::map<std::string, double>> labNodes;

	std::vector<std::string> primaryServices = {
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

	std::vector<std::string> secondaryServices = {
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
	Manikin(const std::string& mid, bool mode, std::string pid);
	~Manikin();

	using ClientPtr = std::shared_ptr<Client>;
	using EventRecordMap = std::map<std::string, AMM::EventRecord>;
	using LabNodeMap = std::map<std::string, std::map<std::string, double>>;
	using EquipmentSettingsMap = std::map<std::string, std::map<std::string, std::string>>;

	bool podMode = false;
	std::unique_ptr<AMM::DDSManager<Manikin>> mgr;

	static std::string ExtractServiceFromCommand(const std::string& in);
	std::string ExtractType(const std::string& in);
	void MakePrimary();
	void MakeSecondary();
	static bool isAuthorized();

	// Updated method signatures to use shared_ptr
	void sendConfig(const std::shared_ptr<Client>& c, const std::string& scene, const std::string& clientType);
	void sendConfigToAll(const std::string& scene);
	void HandleSettings(const std::shared_ptr<Client>& c, const std::string& settingsVal);
	void HandleCapabilities(const std::shared_ptr<Client>& c, const std::string& capabilityVal);
	void HandleStatus(const std::shared_ptr<Client>& c, const std::string& statusVal);
	void DispatchRequest(const std::shared_ptr<Client>& c, const std::string& request, std::string mid = std::string());

	void ParseCapabilities(tinyxml2::XMLElement* node);
	void PublishSettings(const std::string& equipmentType);
	void PublishOperationalDescription();
	void PublishConfiguration();
	void InitializeLabNodes();

	void SendEventRecord(const AMM::UUID& erID, const AMM::FMA_Location& location,
	                     const AMM::UUID& agentID, const std::string& type) const;
	void SendRenderModification(const AMM::UUID& erID, const std::string& type,
	                            const std::string& payload) const;
	void SendPhysiologyModification(const AMM::UUID& erID, const std::string& type,
	                                const std::string& payload) const;
	void SendAssessment(const AMM::UUID& erID) const;
	void SendCommand(const std::string& message) const;
	void SendModuleConfiguration(const std::string& name, const std::string& config) const;

private:
	AMM::UUID m_uuid;
	Server* s{};
	std::string parentId;
	std::map<std::string, std::map<std::string, std::string>> equipmentSettings;

	// Constants
	const std::string capabilityPrefix = "CAPABILITY=";
	const std::string settingsPrefix = "SETTINGS=";
	const std::string statusPrefix = "STATUS=";
	const std::string configPrefix = "CONFIG=";
	const std::string modulePrefix = "MODULE_NAME=";
	const std::string registerPrefix = "REGISTER=";
	const std::string requestPrefix = "REQUEST=";
	const std::string keepHistoryPrefix = "KEEP_HISTORY=";
	const std::string actionPrefix = "ACT=";
	const std::string genericTopicPrefix = "[";
	const std::string keepAlivePrefix = "[KEEPALIVE]";
	const std::string loadScenarioPrefix = "LOAD_SCENARIO:";
	const std::string loadStatePrefix = "LOAD_STATE:";
	const std::string haltingString = "HALTING_ERROR";
	const std::string sysPrefix = "[SYS]";
	const std::string actPrefix = "[ACT]";
	const std::string loadPrefix = "LOAD_STATE:";

	std::string currentScenario = "NONE";
	std::string currentState = "NONE";
	std::string currentStatus = "NOT RUNNING";

	std::map<std::string, double> nodeDataStorage;
	std::map<std::string, std::string> statusStorage = {
			{"STATUS", "NOT RUNNING"},
			{"TICK", "0"},
			{"TIME", "0"},
			{"SCENARIO", ""},
			{"STATE", ""},
			{"AIR_SUPPLY", ""},
			{"CLEAR_SUPPLY", ""},
			{"BLOOD_SUPPLY", ""},
			{"FLUIDICS_STATE", ""},
			{"BATTERY1", ""},
			{"BATTERY2", ""},
			{"EXT_POWER", ""},
			{"IVARM_STATE", ""}
	};

	bool isPaused = false;

protected:
	// Event listeners
	void onNewLog(AMM::Log& log, SampleInfo_t* info);
	void onNewModuleConfiguration(AMM::ModuleConfiguration& mc, SampleInfo_t* info);
	void onNewStatus(AMM::Status& status, SampleInfo_t* info);
	void onNewSimulationControl(AMM::SimulationControl& simControl, SampleInfo_t* info);
	void onNewAssessment(AMM::Assessment& assessment, SampleInfo_t* info);
	void onNewEventFragment(AMM::EventFragment& eventFrag, SampleInfo_t* info);
	void onNewEventRecord(AMM::EventRecord& eventRec, SampleInfo_t* info);
	void onNewFragmentAmendmentRequest(AMM::FragmentAmendmentRequest& ffar, SampleInfo_t* info);
	void onNewOmittedEvent(AMM::OmittedEvent& omittedEvent, SampleInfo_t* info);
	void onNewOperationalDescription(AMM::OperationalDescription& opDescript, SampleInfo_t* info);
	void onNewRenderModification(AMM::RenderModification& rendMod, SampleInfo_t* info);
	void onNewPhysiologyModification(AMM::PhysiologyModification& physMod, SampleInfo_t* info);
	void onNewCommand(AMM::Command& command, eprosima::fastrtps::SampleInfo_t* info);
	void onNewPhysiologyWaveform(AMM::PhysiologyWaveform& n, SampleInfo_t* info);
	void onNewPhysiologyValue(AMM::PhysiologyValue& n, SampleInfo_t* info);
};