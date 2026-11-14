#include "bridge.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <random>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <tinyxml2.h>
#include "amm/BaseLogger.h"

struct RenderModMapping {
    std::string from;
    std::string fromState;  // empty = match any state
    std::string to;
    std::string toState;    // empty = leave state unchanged
};

static std::vector<RenderModMapping> renderModMappings;

// Prefers a mapping that matches both type and state, then a type-only mapping.
static const RenderModMapping *FindRenderModMapping(const std::string &type, const std::string &state) {
    const RenderModMapping *typeOnly = nullptr;
    for (const auto &m : renderModMappings) {
        if (m.from != type) continue;
        if (m.fromState.empty()) {
            if (!typeOnly) typeOnly = &m;
        } else if (m.fromState == state) {
            return &m;
        }
    }
    return typeOnly;
}

void LoadRenderModMappings(const std::string &path) {
    renderModMappings.clear();
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) {
        LOG_WARNING << "No render modification mappings loaded from " << path << ": " << doc.ErrorStr();
        return;
    }
    tinyxml2::XMLElement *root = doc.FirstChildElement("RenderModificationMappings");
    if (!root) {
        LOG_ERROR << "Missing RenderModificationMappings root element in " << path;
        return;
    }
    for (tinyxml2::XMLElement *m = root->FirstChildElement("Map"); m; m = m->NextSiblingElement("Map")) {
        const char *from = m->Attribute("from");
        const char *to = m->Attribute("to");
        if (!from || !to || !*from || !*to) {
            LOG_WARNING << "Skipping render modification mapping with missing from/to in " << path;
            continue;
        }
        const char *fromState = m->Attribute("fromState");
        const char *toState = m->Attribute("toState");
        renderModMappings.push_back({from, fromState ? fromState : "", to, toState ? toState : ""});
        LOG_INFO << "Render modification mapping: " << from
                 << (fromState ? std::string(" [state=") + fromState + "]" : std::string())
                 << " -> " << to
                 << (toState ? std::string(" [state=") + toState + "]" : std::string());
    }
}

std::string MapRenderModType(const std::string &type) {
    const RenderModMapping *m = FindRenderModMapping(type, "");
    return m ? m->to : type;
}

// Rewrites an element's attribute names to lowercase (clients may send e.g. State="on").
static void LowercaseAttributeNames(tinyxml2::XMLElement *el) {
    std::vector<std::pair<std::string, std::string>> origAttrs;
    for (const tinyxml2::XMLAttribute *a = el->FirstAttribute(); a; a = a->Next()) {
        origAttrs.emplace_back(a->Name(), a->Value());
    }
    for (const auto &kv : origAttrs) el->DeleteAttribute(kv.first.c_str());
    for (const auto &kv : origAttrs) {
        std::string name = kv.first;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        el->SetAttribute(name.c_str(), kv.second.c_str());
    }
}

std::string MapRenderModPayload(const std::string &payload, std::string *mappedType) {
    if (renderModMappings.empty()) return payload;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(payload.c_str()) != tinyxml2::XML_SUCCESS) return payload;
    tinyxml2::XMLElement *root = doc.FirstChildElement("RenderModification");
    if (!root) return payload;
    LowercaseAttributeNames(root);
    const char *typeAttr = root->Attribute("type");
    if (!typeAttr) return payload;
    // Copy: the attribute pointers are invalidated by SetAttribute below.
    const std::string type = typeAttr;
    const char *stateAttr = root->Attribute("state");
    const std::string state = stateAttr ? stateAttr : "";
    const RenderModMapping *m = FindRenderModMapping(type, state);
    LOG_DEBUG << "Render modification received: type=" << type << " state=" << (stateAttr ? state : "<none>");
    if (!m) return payload;
    root->SetAttribute("type", m->to.c_str());
    if (!m->toState.empty()) root->SetAttribute("state", m->toState.c_str());
    if (mappedType) *mappedType = m->to;
    tinyxml2::XMLPrinter printer(nullptr, true);
    doc.Print(&printer);
    LOG_INFO << "Render modification mapped: type=" << type << " state=" << (stateAttr ? state : "<none>")
             << " -> " << printer.CStr();
    return printer.CStr();
}

bool BuildVentilationPhysMod(const std::string &renderModPayload, std::string &physModPayload) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(renderModPayload.c_str()) != tinyxml2::XML_SUCCESS) return false;
    tinyxml2::XMLElement *root = doc.FirstChildElement("RenderModification");
    if (!root) return false;
    LowercaseAttributeNames(root);
    const char *type = root->Attribute("type");
    const char *rateAttr = root->Attribute("rate");
    if (!type || !rateAttr || std::string(type) != "Intubation") return false;

    // Only emit a validated number so the value can't inject XML.
    double rate = 0;
    try {
        size_t used = 0;
        rate = std::stod(rateAttr, &used);
        if (used != std::string(rateAttr).size()) throw std::invalid_argument("trailing characters");
    } catch (const std::exception &) {
        LOG_WARNING << "Intubation render modification has non-numeric rate: " << rateAttr;
        return false;
    }

    std::ostringstream out;
    out << "<PhysiologyModification type=\"Ventilation\">"
        << "<State>" << (rate == 0 ? "Off" : "On") << "</State>"
        << "<Rate value=\"" << rate << "\" unit=\"1/min\"/>"
        << "<InspiratoryExpiratoryRatio>0.5</InspiratoryExpiratoryRatio>"
        << "<PositiveEndExpiredPressure value=\"5\" unit=\"cmH2O\"/>"
        << "<VentilatorPressure value=\"20\" unit=\"cmH2O\"/>"
        << "<OxygenFraction>0.21</OxygenFraction>"
        << "</PhysiologyModification>";
    physModPayload = out.str();
    return true;
}

std::mutex gcMapMutex;

ConnectionData GetGameClient(std::string id) {
	std::lock_guard<std::mutex> lock(gcMapMutex);
	auto i = gameClientList.find(id);
	if (i == gameClientList.end()) {
		return {};
	}
	else {
		return i->second;
	}
}

void UpdateGameClient(std::string id, ConnectionData cData) {
	std::lock_guard<std::mutex> lock(gcMapMutex);
	gameClientList.insert_or_assign(id, cData);
	// PublishClientDataUpdate(cData);
}

std::vector<std::string> split (const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

std::string ExtractTypeFromRenderMod(std::string payload) {
    std::size_t pos = payload.find("type=");
    if (pos != std::string::npos) {
        std::string p1 = payload.substr(pos + 6);
        std::size_t pos2 = p1.find('\"');
        if (pos2 != std::string::npos) {
            std::string p2 = p1.substr(0, pos2);
            return p2;
        }
    }
    return {};
}



std::string gen_random(const int len) {
    // Validate input length
    if (len <= 0) {
        throw std::invalid_argument("Random string length must be positive");
    }
    if (len > 1000) {  // Reasonable upper limit
        throw std::invalid_argument("Random string length too large");
    }
    
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    
    // Thread-safe random number generation
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string tmp_s;
    tmp_s.reserve(len);

    try {
        for (int i = 0; i < len; ++i) {
            tmp_s += alphanum[dis(gen)];
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to generate random string: " + std::string(e.what()));
    }

    return tmp_s;
}


std::string ExtractIDFromString(std::string in) {
    std::size_t pos = in.find("mid=");
    if (pos != std::string::npos) {
        std::string mid1 = in.substr(pos + 4);
        std::size_t pos1 = mid1.find(';');
        if (pos1 != std::string::npos) {
            std::string mid2 = mid1.substr(0, pos1);
            return mid2;
        }
        return mid1;
    }
    return {};
}

void WritePassword(std::string str) {
    // Create a secure temporary file using process ID
    std::string filename = "/tmp/.amm_sess_" + std::to_string(getpid()) + ".hash";
    
    std::ofstream outfile(filename, std::ofstream::binary | std::ios::out);
    if (!outfile) {
        throw std::runtime_error("Failed to create password file: " + filename);
    }
    
    // Set restrictive permissions (owner read/write only)
    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR) != 0) {
        outfile.close();
        unlink(filename.c_str());  // Clean up on failure
        throw std::runtime_error("Failed to set password file permissions");
    }
    
    outfile << str;
    if (!outfile.good()) {
        outfile.close();
        unlink(filename.c_str());  // Clean up on failure
        throw std::runtime_error("Failed to write password file");
    }
    
    outfile.close();
}

std::string ReadPassword() {
    // Read the session file matching our process ID
    std::string pidStr = std::to_string(getpid());
    std::string filename = "/tmp/.amm_sess_" + pidStr + ".hash";
    
    std::ifstream infile(filename);
    if (!infile) {
        throw std::runtime_error("Password file not found or inaccessible: " + filename);
    }
    
    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string result = buffer.str();
    
    if (buffer.fail() && !buffer.eof()) {
        throw std::runtime_error("Failed to read password file");
    }
    
    return result;
}