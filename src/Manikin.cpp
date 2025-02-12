#include "Manikin.h"

using namespace AMM;
namespace bp = boost::process;

Manikin::Manikin(const std::string &mid, bool pm, std::string pid) :
		parentId(std::move(pid)),
		podMode(pm),
		manikin_id(mid) {
	LOG_INFO << "Initializing manikin manager and listener for " << mid;
	if (podMode) {
		LOG_INFO << "\tCurrently in POD/TPMS mode.";
	}

	mgr = std::make_unique<DDSManager<Manikin>>(config_file, manikin_id);

	mgr->InitializeCommand();
	mgr->InitializeInstrumentData();
	mgr->InitializeSimulationControl();
	mgr->InitializePhysiologyModification();
	mgr->InitializeRenderModification();
	mgr->InitializeAssessment();
	mgr->InitializePhysiologyValue();
	mgr->InitializePhysiologyWaveform();
	mgr->InitializeEventRecord();
	mgr->InitializeOperationalDescription();
	mgr->InitializeModuleConfiguration();
	mgr->InitializeStatus();
	mgr->InitializeOmittedEvent();

	mgr->CreatePhysiologyValueSubscriber(this, &Manikin::onNewPhysiologyValue);
	mgr->CreatePhysiologyWaveformSubscriber(this, &Manikin::onNewPhysiologyWaveform);
	mgr->CreateCommandSubscriber(this, &Manikin::onNewCommand);
	mgr->CreateSimulationControlSubscriber(this, &Manikin::onNewSimulationControl);
	mgr->CreateAssessmentSubscriber(this, &Manikin::onNewAssessment);
	mgr->CreateRenderModificationSubscriber(this, &Manikin::onNewRenderModification);
	mgr->CreatePhysiologyModificationSubscriber(this, &Manikin::onNewPhysiologyModification);
	mgr->CreateEventRecordSubscriber(this, &Manikin::onNewEventRecord);
	mgr->CreateOmittedEventSubscriber(this, &Manikin::onNewOmittedEvent);
	mgr->CreateOperationalDescriptionSubscriber(this, &Manikin::onNewOperationalDescription);
	mgr->CreateModuleConfigurationSubscriber(this, &Manikin::onNewModuleConfiguration);
	mgr->CreateStatusSubscriber(this, &Manikin::onNewStatus);

	mgr->CreateOperationalDescriptionPublisher();
	mgr->CreateModuleConfigurationPublisher();
	mgr->CreateStatusPublisher();
	mgr->CreateEventRecordPublisher();
	mgr->CreateRenderModificationPublisher();
	mgr->CreatePhysiologyModificationPublisher();
	mgr->CreateSimulationControlPublisher();
	mgr->CreateCommandPublisher();
	mgr->CreateInstrumentDataPublisher();
	mgr->CreateAssessmentPublisher();

	m_uuid.id(AMM::DDSManager<Manikin>::GenerateUuidString());

	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

Manikin::~Manikin() {
	mgr->Shutdown();
}

void Manikin::sendConfig(const std::shared_ptr<Client> &c, const std::string &scene, const std::string &clientType) {
	std::string clientId(c->GetId());
	std::ostringstream static_filename;
	static_filename << "static/module_configuration_static/" << scene << "_"
	                << clientType << "_configuration.xml";

	LOG_DEBUG << "Sending " << static_filename.str() << " to " << clientId;
	std::ifstream ifs(static_filename.str());

	if (ifs.fail()) {
		LOG_WARNING << "Static configuration file for client type " << clientType
		            << " to load scenario " << scene << " does not exist";
		return;
	}

	std::string configContent((std::istreambuf_iterator<char>(ifs)),
	                          (std::istreambuf_iterator<char>()));
	std::string encodedConfigContent = Utility::encode64(configContent);
	std::string encodedConfig = configPrefix + encodedConfigContent + "\n";

	Server::SendToClient(c, encodedConfig);
}

void Manikin::sendConfigToAll(const std::string &scene) {
	LOG_DEBUG << "Sending config to all for scene " << scene;

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::string clientType = clientTypeMap[cid];
		auto client = Server::GetClientByIndex(cid);
		if (client) {
			LOG_DEBUG << "Sending data to client " << cid
			          << ", type " << clientType
			          << " for scene " << scene;
			sendConfig(client, scene, clientType);
		}
		++it;
	}
}

void Manikin::MakePrimary() {
	LOG_INFO << "Making " << parentId << " into the primary.";
	// bp::system("supervisorctl start amm_startup");
	// bp::system("supervisorctl start amm_tpms_bridge");
}

void Manikin::MakeSecondary() {
	LOG_INFO << "Making " << parentId << " into a secondary.";
	// bp::system("supervisorctl start amm_startup");
	// bp::system("supervisorctl stop amm_tpms_bridge");
}

std::string Manikin::ExtractType(const std::string &in) {
	std::size_t pos = in.find("type=");
	if (pos != std::string::npos) {
		std::string mid1 = in.substr(pos + 5);
		std::size_t pos1 = mid1.find(';');
		if (pos1 != std::string::npos) {
			std::string mid2 = mid1.substr(0, pos1);
			return mid2;
		}
		return mid1;
	}
	return {};
}

std::string Manikin::ExtractServiceFromCommand(const std::string &in) {
	std::size_t pos = in.find("service=");
	if (pos != std::string::npos) {
		std::string mid1 = in.substr(pos + 8);
		std::size_t pos1 = mid1.find(';');
		if (pos1 != std::string::npos) {
			std::string mid2 = mid1.substr(0, pos1);
			return mid2;
		}
		return mid1;
	}
	return {};
}

void Manikin::HandleCapabilities(const std::shared_ptr<Client> &c, const std::string &capabilityVal) {
	std::string clientId(c->GetId());

	tinyxml2::XMLDocument doc(false);
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
	od.module_version(moduleVersion);
	od.capabilities_schema(capabilityVal);
	od.description();

	mgr->WriteOperationalDescription(od);

	// Set the client's type
	c->SetClientType(nodeName);

	std::lock_guard<std::mutex> lock(m_mapmutex);
	try {
		clientTypeMap[clientId] = nodeName;
	} catch (const std::exception &e) {
		LOG_ERROR << "Unable to insert into clientTypeMap: " << e.what();
	}

	subscribedTopics[clientId].clear();
	publishedTopics[clientId].clear();

	ConnectionData gc = GetGameClient(clientId);
	gc.client_type = nodeName;
	UpdateGameClient(clientId, gc);
	tinyxml2::XMLElement *caps = module->FirstChildElement("capabilities");
	if (caps) {
		for (tinyxml2::XMLNode *node = caps->FirstChildElement("capability");
		     node;
		     node = node->NextSibling()) {
			tinyxml2::XMLElement *cap = node->ToElement();
			std::string capabilityName = cap->Attribute("name");
			tinyxml2::XMLElement *starting_settings = cap->FirstChildElement("starting_settings");

			if (starting_settings) {
				for (tinyxml2::XMLNode *settingNode = starting_settings->FirstChildElement("setting");
				     settingNode;
				     settingNode = settingNode->NextSibling()) {
					tinyxml2::XMLElement *setting = settingNode->ToElement();
					std::string settingName = setting->Attribute("name");
					std::string settingValue = setting->Attribute("value");
					equipmentSettings[capabilityName][settingName] = settingValue;
				}
				PublishSettings(capabilityName);
			}

			tinyxml2::XMLNode *subs = node->FirstChildElement("subscribed_topics");
			if (subs) {
				for (tinyxml2::XMLNode *sub = subs->FirstChildElement("topic");
				     sub;
				     sub = sub->NextSibling()) {
					tinyxml2::XMLElement *sE = sub->ToElement();
					std::string subTopicName = sE->Attribute("name");

					if (sE->Attribute("nodepath")) {
						std::string subNodePath = sE->Attribute("nodepath");
						if (subTopicName == "AMM_HighFrequencyNode_Data") {
							subTopicName = "HF_" + subNodePath;
						} else {
							subTopicName = subNodePath;
						}
					}
					std::lock_guard<std::mutex> topicLock(m_topicmutex);
					Utility::add_once(subscribedTopics[clientId], subTopicName);
					LOG_TRACE << "[" << capabilityName << "][" << clientId
					          << "] Subscribing to " << subTopicName;
				}
			}

			tinyxml2::XMLNode *pubs = node->FirstChildElement("published_topics");
			if (pubs) {
				for (tinyxml2::XMLNode *pub = pubs->FirstChildElement("topic");
				     pub;
				     pub = pub->NextSibling()) {
					tinyxml2::XMLElement *p = pub->ToElement();
					std::string pubTopicName = p->Attribute("name");
					std::lock_guard<std::mutex> topicLock(m_topicmutex);
					Utility::add_once(publishedTopics[clientId], pubTopicName);
					LOG_TRACE << "[" << capabilityName << "][" << clientId
					          << "] Publishing " << pubTopicName;
				}
			}
		}
	}
}

void Manikin::HandleSettings(const std::shared_ptr<Client> &c, const std::string &settingsVal) {
	std::string clientId(c->GetId());

	tinyxml2::XMLDocument doc(false);
	doc.Parse(settingsVal.c_str());
	tinyxml2::XMLNode *root = doc.FirstChildElement("AMMModuleConfiguration");
	tinyxml2::XMLElement *module = root->FirstChildElement("module");
	tinyxml2::XMLElement *caps = module->FirstChildElement("capabilities");

	if (caps) {
		for (tinyxml2::XMLNode *node = caps->FirstChildElement("capability");
		     node;
		     node = node->NextSibling()) {
			tinyxml2::XMLElement *cap = node->ToElement();
			std::string capabilityName = cap->Attribute("name");
			tinyxml2::XMLElement *configEl = cap->FirstChildElement("configuration");

			if (configEl) {
				for (tinyxml2::XMLNode *settingNode = configEl->FirstChildElement("setting");
				     settingNode;
				     settingNode = settingNode->NextSibling()) {
					tinyxml2::XMLElement *setting = settingNode->ToElement();
					std::string settingName = setting->Attribute("name");
					std::string settingValue = setting->Attribute("value");
					equipmentSettings[capabilityName][settingName] = settingValue;
				}
			}
			PublishSettings(capabilityName);
		}
	}
}

void Manikin::HandleStatus(const std::shared_ptr<Client> &c, const std::string &statusVal) {
	std::string clientId(c->GetId());

	tinyxml2::XMLDocument doc(false);
	doc.Parse(statusVal.c_str());

	tinyxml2::XMLNode *root = doc.FirstChildElement("AMMModuleStatus");
	tinyxml2::XMLElement *module = root->FirstChildElement("module")->ToElement();
	const char *name = module->Attribute("name");
	std::string nodeName(name);

	std::size_t found = statusVal.find(haltingString);
	AMM::Status status;
	status.module_id(m_uuid);
	status.capability(nodeName);
	if (found != std::string::npos) {
		status.value(AMM::StatusValue::INOPERATIVE);
	} else {
		status.value(AMM::StatusValue::OPERATIONAL);
	}
	mgr->WriteStatus(status);
}

void Manikin::DispatchRequest(const std::shared_ptr<Client> &c, const std::string &request, std::string mid) {
	std::string clientId(c->GetId());

	if (boost::starts_with(request, "STATUS")) {
		std::ostringstream messageOut;
		messageOut << "STATUS=" << currentStatus << "|"
		           << "SCENARIO=" << currentScenario << "|"
		           << "STATE=" << currentState << "|";

		Server::SendToClient(c, messageOut.str());
	} else if (boost::starts_with(request, "CLIENTS")) {
		LOG_DEBUG << "Client table request";

		std::ostringstream messageOut;
		messageOut << "client_id,client_name,learner_name,client_connection,"
		           << "client_type,role,client_status,connect_time\n";

		for (const auto &client: gameClientList) {
			const ConnectionData &clientData = client.second;
			messageOut << clientData.client_id << ","
			           << clientData.client_name << ","
			           << clientData.learner_name << ","
			           << clientData.client_connection << ","
			           << clientData.client_type << ","
			           << clientData.role << ","
			           << clientData.client_status << ","
			           << clientData.connect_time << "\n";
		}

		Server::SendToClient(c, messageOut.str());
	} else if (boost::starts_with(request, "LABS")) {
		LOG_DEBUG << "LABS request: " << request;

		const auto delimiterIdx = request.find_first_of(';');
		const std::string labCategory = (delimiterIdx != std::string::npos)
		                                ? request.substr(delimiterIdx + 1)
		                                : "ALL";

		LOG_DEBUG << "Return lab values for: " << labCategory;

		const auto &labValues = labNodes[labCategory];
		if (labValues.empty()) {
			LOG_WARNING << "No lab values found for category: " << labCategory;
			return;
		}

		for (const auto &lab: labValues) {
			std::ostringstream messageOut;
			messageOut << lab.first << "=" << lab.second << ";mid=" << mid << "|";
			Server::SendToClient(c, messageOut.str());
		}
	} else {
		LOG_WARNING << "Unknown request type: " << request;
	}
}

void Manikin::onNewStatus(AMM::Status &st, SampleInfo_t *info) {
	std::ostringstream statusValue;
	statusValue << AMM::Utility::EStatusValueStr(st.value());

	LOG_DEBUG << "[" << st.module_id().id() << "][" << st.module_name() << "]["
	          << st.capability() << "] Status = " << statusValue.str()
	          << " (" << st.value() << ")";

	std::string sStatus = statusValue.str();
	std::string sData = st.message();

	std::ostringstream messageOut;
	messageOut << "[AMM_Status]"
	           << "mid=" << manikin_id << ";"
	           << "capability=" << st.capability() << ";"
	           << "status_code=" << sStatus << ";"
	           << "status=" << st.value() << ";"
	           << "data=" << sData
	           << std::endl;
	std::string stringOut = messageOut.str();

	LOG_TRACE << " Sending status message to clients: " << messageOut.str();

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];

		if (std::find(subV.begin(), subV.end(), "AMM_Status") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewModuleConfiguration(AMM::ModuleConfiguration &mc, SampleInfo_t *info) {
	LOG_DEBUG << "Received module config from manikin " << manikin_id << " for " << mc.name();

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::string clientType;
		auto pos = clientTypeMap.find(cid);
		if (pos == clientTypeMap.end()) {
			//handle the error
			LOG_WARNING << "Client type not found for client ID: " << cid;
			++it;
			continue;
		}

		clientType = pos->second;
		if (clientType.find(mc.name()) != std::string::npos || mc.name() == "metadata") {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				std::string capConfig = mc.capabilities_configuration().to_string();
				std::string encodedConfigContent = Utility::encode64(capConfig);
				std::ostringstream encodedConfig;
				encodedConfig << configPrefix << encodedConfigContent << ";mid=" << manikin_id << std::endl;
				Server::SendToClient(client, encodedConfig.str());
			}
		}
		++it;
	}
}

void Manikin::onNewPhysiologyWaveform(AMM::PhysiologyWaveform &n, SampleInfo_t *info) {
	std::string hfname = "HF_" + n.name();
	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());

	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), hfname) != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				std::ostringstream messageOut;
				if (podMode) {
					messageOut << n.name() << "=" << n.value() << ";mid=" << manikin_id << "|" << std::endl;
				} else {
					messageOut << n.name() << "=" << n.value() << "|" << std::endl;
				}
				Server::SendToClient(client, messageOut.str());
			}
		}
		++it;
	}
}

void Manikin::onNewPhysiologyValue(AMM::PhysiologyValue &n, SampleInfo_t *info) {
	// Drop values into the lab sheets
	for (auto &outer_map_pair: labNodes) {
		if (labNodes[outer_map_pair.first].find(n.name()) !=
		    labNodes[outer_map_pair.first].end()) {
			labNodes[outer_map_pair.first][n.name()] = n.value();
		}
	}

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];

		if (std::find(subV.begin(), subV.end(), n.name()) != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				std::ostringstream messageOut;
				if (podMode) {
					messageOut << n.name() << "=" << n.value() << ";mid=" << manikin_id << "|" << std::endl;
				} else {
					messageOut << n.name() << "=" << n.value() << "|" << std::endl;
				}
				Server::SendToClient(client, messageOut.str());
			}
		}
		++it;
	}
}

void Manikin::onNewPhysiologyModification(AMM::PhysiologyModification &pm, SampleInfo_t *info) {
	LOG_DEBUG << "Received a phys mod from manikin " << manikin_id;
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
	std::string stringOut = messageOut.str();

	LOG_DEBUG << "Received a phys mod via DDS, republishing to TCP clients: " << stringOut;

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];

		if (std::find(subV.begin(), subV.end(), pm.type()) != subV.end() ||
		    std::find(subV.begin(), subV.end(), "AMM_Physiology_Modification") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewRenderModification(AMM::RenderModification &rendMod, SampleInfo_t *info) {
	std::string location;
	std::string practitioner;

	if (eventRecords.count(rendMod.event_id().id()) > 0) {
		AMM::EventRecord er = eventRecords[rendMod.event_id().id()];
		location = er.location().name();
		practitioner = er.agent_id().id();
	}

	std::ostringstream messageOut;
	std::string rendModPayload;
	std::string rendModType;

	if (rendMod.data() == "") {
		rendModPayload = "<RenderModification type='" + rendMod.type() + "'/>";
		rendModType = "";
	} else {
		rendModPayload = rendMod.data();
		rendModType = "";
	}

	messageOut << "[AMM_Render_Modification]"
	           << "id=" << rendMod.id().id() << ";"
	           << "mid=" << manikin_id << ";"
	           << "event_id=" << rendMod.event_id().id() << ";"
	           << "type=" << rendModType << ";"
	           << "location=" << location << ";"
	           << "participant_id=" << practitioner << ";"
	           << "payload=" << rendModPayload
	           << std::endl;

	std::string stringOut = messageOut.str();

	if (rendModPayload.find("START_OF") == std::string::npos) {
		LOG_INFO << "Render mod Message came in on manikin " << manikin_id
		         << ", republishing to TCP: " << stringOut;
	}

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), rendMod.type()) != subV.end() ||
		    std::find(subV.begin(), subV.end(), "AMM_Render_Modification") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewSimulationControl(AMM::SimulationControl &simControl, SampleInfo_t *info) {
	LOG_INFO << "Simulation control Message came in on manikin " << manikin_id;

	switch (simControl.type()) {
		case AMM::ControlType::RUN: {
			currentStatus = "RUNNING";
			isPaused = false;
			LOG_INFO << "\tMessage received; Run sim.";
			std::ostringstream tmsg;
			tmsg << "[SYS]START_SIM" << ";mid=" << manikin_id << std::endl;
			Server::SendToAll(tmsg.str());
			break;
		}
		case AMM::ControlType::HALT: {
			currentStatus = isPaused ? "PAUSED" : "NOT RUNNING";
			LOG_INFO << "\tMessage received; Halt sim";
			std::ostringstream tmsg;
			tmsg << "[SYS]PAUSE_SIM" << ";mid=" << manikin_id << std::endl;
			Server::SendToAll(tmsg.str());
			break;
		}
		case AMM::ControlType::RESET: {
			currentStatus = "NOT RUNNING";
			isPaused = false;
			LOG_INFO << "\tMessage received; Reset sim";
			std::ostringstream tmsg;
			tmsg << "[SYS]RESET_SIM" << ";mid=" << manikin_id << std::endl;
			Server::SendToAll(tmsg.str());
			break;
		}
		case AMM::ControlType::SAVE: {
			LOG_INFO << "\tMessage received; Save sim";
			break;
		}
	}
}

void Manikin::PublishSettings(const std::string &equipmentType) {
	std::ostringstream payload;
	LOG_INFO << "Publishing equipment " << equipmentType << " settings";

	for (auto &inner_map_pair: equipmentSettings[equipmentType]) {
		payload << inner_map_pair.first << "=" << inner_map_pair.second << std::endl;
		LOG_DEBUG << "\t" << inner_map_pair.first << ": " << inner_map_pair.second;
	}

	AMM::InstrumentData i;
	i.instrument(equipmentType);
	i.payload(payload.str());
	mgr->WriteInstrumentData(i);
}

void Manikin::PublishOperationalDescription() {
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
	mgr->WriteOperationalDescription(od);
}

void Manikin::PublishConfiguration() {
	AMM::ModuleConfiguration mc;
	auto ms = duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	mc.timestamp(ms);
	mc.module_id(m_uuid);
	mc.name(moduleName);
	const std::string configuration = AMM::Utility::read_file_to_string("config/tcp_bridge_configuration.xml");
	mc.capabilities_configuration(configuration);
	mgr->WriteModuleConfiguration(mc);
}

bool Manikin::isAuthorized() {
	std::ifstream infile("/tmp/disabled");
	return !infile.good();
}

void Manikin::SendEventRecord(const AMM::UUID &erID, const AMM::FMA_Location &location,
                              const AMM::UUID &agentID, const std::string &type) const {
	AMM::EventRecord er;
	er.id(erID);
	er.location(location);
	er.agent_id(agentID);
	er.type(type);
	mgr->WriteEventRecord(er);
}

void Manikin::SendRenderModification(const AMM::UUID &erID, const std::string &type,
                                     const std::string &payload) const {
	AMM::RenderModification renderMod;

	if (!type.empty() && payload.empty()) {
		std::ostringstream tpayload;
		tpayload << "<RenderModification type='" << type << "'/>";
		renderMod.data(tpayload.str());
	} else {
		renderMod.data(payload);
	}

	renderMod.event_id(erID);
	renderMod.type(type);
	mgr->WriteRenderModification(renderMod);
}

void Manikin::SendPhysiologyModification(const AMM::UUID &erID, const std::string &type,
                                         const std::string &payload) const {
	AMM::PhysiologyModification physMod;
	physMod.event_id(erID);
	physMod.type(type);
	physMod.data(payload);
	mgr->WritePhysiologyModification(physMod);
}

void Manikin::SendAssessment(const AMM::UUID &erID) const {
	AMM::Assessment assessment;
	assessment.event_id(erID);
	mgr->WriteAssessment(assessment);
}

void Manikin::SendCommand(const std::string &message) const {
	AMM::Command cmdInstance;
	cmdInstance.message(message);
	mgr->WriteCommand(cmdInstance);
}

void Manikin::SendModuleConfiguration(const std::string &name, const std::string &config) const {
	AMM::ModuleConfiguration mc;
	mc.name(name);
	mc.capabilities_configuration(config);
	mgr->WriteModuleConfiguration(mc);
}

void Manikin::InitializeLabNodes() {
	// Initialize lab nodes with modern map syntax
	labNodes = {
			{"ALL",        {
					               {"Substance_Sodium",                    0.0f},
					               {"MetabolicPanel_CarbonDioxide",                   0.0f},
					               {"Substance_Glucose_Concentration",         0.0f},
					               {"BloodChemistry_BloodUreaNitrogen_Concentration", 0.0f},
					               {"Substance_Creatinine_Concentration", 0.0f},
					               {"BloodChemistry_WhiteBloodCell_Count",            0.0f},
					               {"BloodChemistry_RedBloodCell_Count",          0.0f},
					               {"Substance_Hemoglobin_Concentration",   0.0f},
					               {"BloodChemistry_Hemaocrit",                 0.0f},
					               {"CompleteBloodCount_Platelet",              0.0f},
					               {"BloodChemistry_BloodPH", 0.0f},
					               {"BloodChemistry_Arterial_CarbonDioxide_Pressure", 0.0f},
					               {"BloodChemistry_Arterial_Oxygen_Pressure", 0.0f},
					               {"Substance_Bicarbonate", 0.0f},
					               {"Substance_BaseExcess", 0.0f},
					               {"Substance_Lactate_Concentration_mmol", 0.0f},
					               {"BloodChemistry_CarbonMonoxide_Saturation", 0.0f},
					               {"Anion_Gap", 0.0f},
					               {"Substance_Ionized_Calcium", 0.0f}
			               }},
			{"POCT",       {
					               {"Substance_Sodium",                    0.0f},
					               {"MetabolicPanel_Potassium",                       0.0f},
					               {"MetabolicPanel_Chloride",                 0.0f},
					               {"MetabolicPanel_CarbonDioxide",                   0.0f},
					               {"Substance_Glucose_Concentration",    0.0f},
					               {"BloodChemistry_BloodUreaNitrogen_Concentration", 0.0f},
					               {"Substance_Creatinine_Concentration",         0.0f},
					               {"Anion_Gap",                            0.0f},
					               {"Substance_Ionized_Calcium",                0.0f}
			               }},
			{"Hematology", {
					               {"BloodChemistry_Hemaocrit",            0.0f},
					               {"Substance_Hemoglobin_Concentration",             0.0f}
			               }},
			{"ABG",        {
					               {"BloodChemistry_BloodPH",              0.0f},
					               {"BloodChemistry_Arterial_CarbonDioxide_Pressure", 0.0f},
					               {"BloodChemistry_Arterial_Oxygen_Pressure", 0.0f},
					               {"MetabolicPanel_CarbonDioxide",                   0.0f},
					               {"Substance_Bicarbonate",              0.0f},
					               {"Substance_BaseExcess",                           0.0f},
					               {"BloodChemistry_Oxygen_Saturation",           0.0f},
					               {"Substance_Lactate_Concentration_mmol", 0.0f},
					               {"BloodChemistry_CarbonMonoxide_Saturation", 0.0f}
			               }},
			{"VBG",        {
					               {"BloodChemistry_BloodPH",              0.0f},
					               {"BloodChemistry_Arterial_CarbonDioxide_Pressure", 0.0f},
					               {"BloodChemistry_Arterial_Oxygen_Pressure", 0.0f},
					               {"MetabolicPanel_CarbonDioxide",                   0.0f},
					               {"Substance_Bicarbonate",              0.0f},
					               {"Substance_BaseExcess",                           0.0f},
					               {"BloodChemistry_VenousCarbonDioxidePressure", 0.0f},
					               {"BloodChemistry_VenousOxygenPressure",  0.0f},
					               {"Substance_Lactate_Concentration_mmol",     0.0f},
					               {"BloodChemistry_CarbonMonoxide_Saturation", 0.0f}
			               }},
			{"BMP",        {
					               {"Substance_Sodium",                    0.0f},
					               {"MetabolicPanel_Potassium",                       0.0f},
					               {"MetabolicPanel_Chloride",                 0.0f},
					               {"MetabolicPanel_CarbonDioxide",                   0.0f},
					               {"Substance_Glucose_Concentration",    0.0f},
					               {"BloodChemistry_BloodUreaNitrogen_Concentration", 0.0f},
					               {"Substance_Creatinine_Concentration",         0.0f},
					               {"Anion_Gap",                            0.0f},
					               {"Substance_Ionized_Calcium",                0.0f}
			               }},
			{"CBC",        {
					               {"BloodChemistry_WhiteBloodCell_Count", 0.0f},
					               {"BloodChemistry_RedBloodCell_Count",              0.0f},
					               {"Substance_Hemoglobin_Concentration",      0.0f},
					               {"BloodChemistry_Hemaocrit",                       0.0f},
					               {"CompleteBloodCount_Platelet",        0.0f}
			               }},
			{"CMP",        {
					               {"Substance_Albumin_Concentration",     0.0f},
					               {"BloodChemistry_BloodUreaNitrogen_Concentration", 0.0f},
					               {"Substance_Calcium_Concentration",         0.0f},
					               {"MetabolicPanel_Chloride",                        0.0f},
					               {"MetabolicPanel_CarbonDioxide",       0.0f},
					               {"Substance_Creatinine_Concentration",             0.0f},
					               {"Substance_Glucose_Concentration",            0.0f},
					               {"MetabolicPanel_Potassium",             0.0f},
					               {"Substance_Sodium",                         0.0f},
					               {"MetabolicPanel_Bilirubin",                 0.0f},
					               {"MetabolicPanel_Protein", 0.0f}
			               }}
	};
}

void Manikin::onNewAssessment(AMM::Assessment &a, eprosima::fastrtps::SampleInfo_t *info) {
	std::string location;
	std::string practitioner;
	std::string eType;

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
	std::string stringOut = messageOut.str();

	LOG_DEBUG << "Received an assessment via DDS, republishing to TCP clients: " << stringOut;

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), "AMM_Assessment") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewEventRecord(AMM::EventRecord &er, SampleInfo_t *info) {
	std::string location;
	std::string practitioner;
	std::string eType;
	std::string eData;
	std::string pType;

	LOG_DEBUG << "Received an event record of type " << er.type()
	          << " from manikin " << manikin_id;
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
	std::string stringOut = messageOut.str();

	LOG_DEBUG << "Received an EventRecord via DDS, republishing to TCP clients: " << stringOut;

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), "AMM_EventRecord") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewOmittedEvent(AMM::OmittedEvent &oe, SampleInfo_t *info) {
	std::string location;
	std::string practitioner;
	std::string eType;
	std::string eData;
	std::string pType;

	// Make a mock event record
	AMM::EventRecord er;
	er.id(oe.id());
	er.location(oe.location());
	er.agent_id(oe.agent_id());
	er.type(oe.type());
	er.timestamp(oe.timestamp());
	er.agent_type(oe.agent_type());
	er.data(oe.data());

	LOG_DEBUG << "Received an omitted event record of type " << er.type() << " from manikin " << manikin_id;
	eventRecords[er.id().id()] = er;
	location = er.location().name();
	practitioner = er.agent_id().id();
	eType = er.type();
	eData = er.data();
	pType = AMM::Utility::EEventAgentTypeStr(er.agent_type());

	std::ostringstream messageOut;
	messageOut << "[AMM_OmittedEvent]"
	           << "id=" << er.id().id() << ";"
	           << "mid=" << manikin_id << ";"
	           << "type=" << eType << ";"
	           << "location=" << location << ";"
	           << "participant_id=" << practitioner << ";"
	           << "participant_type=" << pType << ";"
	           << "data=" << eData << ";"
	           << std::endl;
	std::string stringOut = messageOut.str();

	LOG_DEBUG << "Received an EventRecord via DDS, republishing to TCP clients: " << stringOut;

	std::lock_guard<std::mutex> lock(Server::GetClientsMutex());
	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), "AMM_EventRecord") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewOperationalDescription(AMM::OperationalDescription &opD, SampleInfo_t *info) {
	LOG_INFO << "Operational Description came in on manikin " << manikin_id << " (" << opD.name() << ")";

	std::ostringstream messageOut;
	std::string capSchema = opD.capabilities_schema().to_string();
	std::string capabilities = Utility::encode64(capSchema);

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
	std::string stringOut = messageOut.str();

	auto it = clientMap.begin();
	while (it != clientMap.end()) {
		std::string cid = it->first;
		std::vector<std::string> subV = subscribedTopics[cid];
		if (std::find(subV.begin(), subV.end(), "AMM_OperationalDescription") != subV.end()) {
			auto client = Server::GetClientByIndex(cid);
			if (client) {
				Server::SendToClient(client, stringOut);
			}
		}
		++it;
	}
}

void Manikin::onNewCommand(AMM::Command &c, eprosima::fastrtps::SampleInfo_t *info) {
	LOG_INFO << "Command Message came in on manikin " << manikin_id << ": " << c.message();

	if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
		std::string value = c.message().substr(sysPrefix.size());
		std::string mid = ExtractIDFromString(value);

		if (value.find("START_SIM") != std::string::npos) {
			currentStatus = "RUNNING";
			isPaused = false;
			AMM::SimulationControl simControl;
			auto ms = duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			simControl.timestamp(ms);
			simControl.type(AMM::ControlType::RUN);
			mgr->WriteSimulationControl(simControl);
			std::string tmsg = "ACT=START_SIM;mid=" + manikin_id;
			Server::SendToAll(tmsg);
		} else if (value.find("STOP_SIM") != std::string::npos) {
			currentStatus = "NOT RUNNING";
			isPaused = false;
			AMM::SimulationControl simControl;
			auto ms = duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			simControl.timestamp(ms);
			simControl.type(AMM::ControlType::HALT);
			mgr->WriteSimulationControl(simControl);
			std::string tmsg = "ACT=STOP_SIM;mid=" + manikin_id;
			Server::SendToAll(tmsg);
		} else if (value.find("PAUSE_SIM") != std::string::npos) {
			currentStatus = "PAUSED";
			isPaused = true;
			AMM::SimulationControl simControl;
			auto ms = duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			simControl.timestamp(ms);
			simControl.type(AMM::ControlType::HALT);
			mgr->WriteSimulationControl(simControl);
			std::string tmsg = "ACT=PAUSE_SIM;mid=" + manikin_id;
			Server::SendToAll(tmsg);
		} else if (value.find("RESET_SIM") != std::string::npos) {
			currentStatus = "NOT RUNNING";
			isPaused = false;
			std::string tmsg = "ACT=RESET_SIM;mid=" + manikin_id;
			Server::SendToAll(tmsg);
			AMM::SimulationControl simControl;
			auto ms = duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			simControl.timestamp(ms);
			simControl.type(AMM::ControlType::RESET);
			mgr->WriteSimulationControl(simControl);
			InitializeLabNodes();
		} else if (value.find("RESTART_SERVICE") != std::string::npos) {
			if (mid == parentId || !podMode) {
				std::string service = ExtractServiceFromCommand(value);
				LOG_INFO << "Command to restart service " << service;
				if (service.find("all") != std::string::npos) {
					LOG_INFO << "Restarting all services, which is like assigning primary.";
					if (podMode && mid == parentId) {
						MakePrimary();
					} else if (podMode) {
						MakeSecondary();
					} else {
						std::string command = "supervisorctl restart " + service;
						int result = bp::system(command);
					}
				} else {
					LOG_INFO << "Restarting single service.";
					std::string command = "supervisorctl restart " + service;
					int result = bp::system(command);
				}
			} else {
				LOG_TRACE << "Got a restart command that's not for us.";
			}
		} else if (value.find("START_SERVICE") != std::string::npos) {
			if (mid == parentId) {
				std::string service = ExtractServiceFromCommand(value);
				LOG_INFO << "Command to start service " << service;
				if (service.find("all") != std::string::npos) {
					LOG_INFO << "Restarting all services, which is like assigning primary.";
					if (mid == parentId) {
						// we're the primary
						MakePrimary();
					} else {
						// we're a secondary
						MakeSecondary();
					}
				} else {
					std::string command = "supervisorctl start " + service;
					int result = bp::system(command);
				}
			}
		} else if (value.find("STOP_SERVICE") != std::string::npos) {
			if (mid == parentId) {
				std::string service = ExtractServiceFromCommand(value);
				LOG_INFO << "Command to stop service " << service;
				std::string command = "supervisorctl stop " + service;
				int result = bp::system(command);
			}
		} else if (value.find("DISABLE_REMOTE") != std::string::npos) {
			LOG_INFO << "Request to disable Remote / RTC";
			std::string command = "supervisorctl stop amm_rtc_bridge";
			int result = bp::system(command);
			LOG_INFO << "Service stop: " << result;
			if (result == 0) {
				std::ostringstream tmsg;
				tmsg << "REMOTE=DISABLED" << std::endl;
				Server::SendToAll(tmsg.str());
			}
		} else if (value.find("SET_PRIMARY") != std::string::npos) {
			if (mid == parentId) {
				MakePrimary();
			} else {
				MakeSecondary();
			}
		} else if (value.find("END_SIMULATION") != std::string::npos) {
			currentStatus = "NOT RUNNING";
			isPaused = true;
			AMM::SimulationControl simControl;
			auto ms = duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count();
			simControl.timestamp(ms);
			simControl.type(AMM::ControlType::HALT);
			mgr->WriteSimulationControl(simControl);
			std::string tmsg = "ACT=END_SIMULATION_SIM;mid=" + manikin_id;
			Server::SendToAll(tmsg);
		} else if (value.find("ENABLE_REMOTE") != std::string::npos) {
			std::string remoteData = value.substr(sizeof("ENABLE_REMOTE"));
			LOG_INFO << "Enabling remote with options:" << remoteData;
			std::list<std::string> tokenList;
			split(tokenList, remoteData, boost::algorithm::is_any_of(";"), boost::token_compress_on);
			std::map<std::string, std::string> kvp;

			for (const auto &token: tokenList) {
				size_t sep_pos = token.find_first_of('=');
				std::string kvp_key = token.substr(0, sep_pos);
				boost::algorithm::to_lower(kvp_key);
				std::string kvp_value = (sep_pos == std::string::npos ? "" : token.substr(
						sep_pos + 1,
						std::string::npos));
				kvp[kvp_key] = kvp_value;
				LOG_DEBUG << "\t" << kvp_key << " => " << kvp[kvp_key];
			}

			if (kvp.find("password") != kvp.end()) {
				SESSION_PASSWORD = kvp["password"];
				LOG_INFO << "Enabling remote with password " << SESSION_PASSWORD;
				WritePassword(SESSION_PASSWORD);
			} else {
				LOG_WARNING << "No password set, we can't do anything with this.";
				return;
			}

			std::ostringstream tmsg;
			if (!isAuthorized()) {
				LOG_WARNING << "Core not authorized for REMOTE.";
				tmsg << "REMOTE=REJECTED" << std::endl;
				std::string command = "supervisorctl stop amm_rtc_bridge";
				int result = bp::system(command);
				LOG_INFO << "Service stop: " << result;
			} else {
				LOG_INFO << "Request to enable Remote / RTC";
				std::string command = "supervisorctl restart amm_rtc_bridge";
				int result = bp::system(command);
				LOG_INFO << "Service start: " << result;
				if (result == 0) {
					tmsg << "REMOTE=ENABLED" << std::endl;
				} else {
					tmsg << "REMOTE=DISABLED" << std::endl;
				}
			}
			Server::SendToAll(tmsg.str());
		} else if (value.find("UPDATE_CLIENT") != std::string::npos) {
			std::string clientData = value.substr(sizeof("UPDATE_CLIENT"));
			LOG_DEBUG << "Updating client with client data:" << clientData;
			std::list<std::string> tokenList;
			split(tokenList, clientData, boost::algorithm::is_any_of(";"), boost::token_compress_on);
			std::map<std::string, std::string> kvp;

			for (const auto &token: tokenList) {
				size_t sep_pos = token.find_first_of('=');
				std::string kvp_key = token.substr(0, sep_pos);
				boost::algorithm::to_lower(kvp_key);
				std::string kvp_value = (sep_pos == std::string::npos ? "" : token.substr(
						sep_pos + 1,
						std::string::npos));
				kvp[kvp_key] = kvp_value;
				LOG_TRACE << "\t" << kvp_key << " => " << kvp[kvp_key];
			}

			std::string client_id;
			if (kvp.find("client_id") != kvp.end()) {
				client_id = kvp["client_id"];
			} else {
				LOG_WARNING << "No client ID found, we can't do anything with this.";
				return;
			}

			ConnectionData gc = GetGameClient(client_id);
			gc.client_id = client_id;
			if (kvp.find("client_name") != kvp.end()) {
				gc.client_name = kvp["client_name"];
			}
			if (kvp.find("learner_name") != kvp.end()) {
				gc.learner_name = kvp["learner_name"];
			}
			if (kvp.find("client_connection") != kvp.end()) {
				gc.client_connection = kvp["client_connection"];
			}
			if (kvp.find("client_type") != kvp.end()) {
				gc.client_type = kvp["client_type"];
			}
			if (kvp.find("role") != kvp.end()) {
				gc.role = kvp["role"];
			}
			if (kvp.find("connect_time") != kvp.end()) {
				gc.connect_time = std::stoi(kvp["connect_time"]);
			}
			if (kvp.find("client_status") != kvp.end()) {
				gc.client_status = kvp["client_status"];
			}

			UpdateGameClient(client_id, gc);
			std::ostringstream messageOut;
			messageOut << "ACT" << "=" << c.message() << ";mid=" << manikin_id << std::endl;
			Server::SendToAll(messageOut.str());
		} else if (value.find("KICK") != std::string::npos) {
			std::string kickC = value.substr(sizeof("KICK"));
			LOG_INFO << "Got kick via DDS bus command.";
			for (auto it = gameClientList.cbegin(); it != gameClientList.cend();) {
				if (it->first == kickC) {
					LOG_INFO << "Found client, we're removing: " << it->second.client_name;
					it = gameClientList.erase(it);
				} else {
					++it;
				}
			}
		} else if (!value.compare(0, loadScenarioPrefix.size(), loadScenarioPrefix)) {
			currentScenario = value.substr(loadScenarioPrefix.size());
			LOG_DEBUG << "Setting scenario: " << currentScenario;
			sendConfigToAll(currentScenario);
			std::ostringstream messageOut;
			messageOut << "ACT" << "=" << c.message() << ";mid=" << manikin_id << std::endl;
			LOG_DEBUG << "Sending " << messageOut.str() << " to all TCP clients.";
			Server::SendToAll(messageOut.str());
		} else if (!value.compare(0, loadPrefix.size(), loadPrefix)) {
			currentState = value.substr(loadStatePrefix.size());
			LOG_DEBUG << "Current state is " << loadStatePrefix;
			std::ostringstream messageOut;
			messageOut << "ACT" << "=" << c.message() << ";mid=" << manikin_id << std::endl;
			LOG_DEBUG << "Sending " << messageOut.str() << " to all TCP clients.";
			Server::SendToAll(messageOut.str());
		} else {
			std::ostringstream messageOut;
			messageOut << "ACT" << "=" << c.message() << ";mid=" << manikin_id << std::endl;
			LOG_WARNING << "Sending unknown message: " << messageOut.str();
			Server::SendToAll(messageOut.str());
		}
	}
}
