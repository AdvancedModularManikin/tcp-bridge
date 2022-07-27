#pragma once

#ifndef BRIDGE_H
#define BRIDGE_H

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

#include <boost/program_options.hpp>

#include <map>
#include <vector>
#include <string>

extern std::map <std::string, std::string> clientMap;
extern std::map <std::string, std::string> clientTypeMap;

extern std::map <std::string, std::vector<std::string>> subscribedTopics;
extern std::map <std::string, std::vector<std::string>> publishedTopics;


struct ConnectionData {
    std::string client_id;
    std::string client_name;
    std::string learner_name;
    std::string client_connection;
    std::string client_type;
    std::string role;
    int connect_time;
};

extern std::map <std::string, ConnectionData> gameClientList;

ConnectionData GetGameClient(std::string id);

void UpdateGameClient(std::string id, ConnectionData cData);
std::vector<std::string> split (const std::string &s, char delim);
std::string ExtractTypeFromRenderMod(std::string payload);
std::string ExtractIDFromString(std::string in);
//std::string ExtractManikinIDFromString(std::string in);
std::string gen_random(const int len);

#endif