#############################
# CMake - TCP Bridge Module - root/src
#############################

set(
        TCP_BRIDGE_MODULE_SOURCES
        TCPBridgeMain.cpp
        Net/Client.cpp Net/Client.h
        Net/Server.cpp Net/Server.h
        Net/ServerThread.cpp Net/ServerThread.h
        Net/UdpDiscoveryServer.cpp Net/UdpDiscoveryServer.h
)

add_executable(amm_tcp_bridge ${TCP_BRIDGE_MODULE_SOURCES})

target_link_libraries(
   amm_tcp_bridge
        PUBLIC amm_std
        PUBLIC Boost::system
        PUBLIC Boost::thread
	tinyxml2
)

install(TARGETS amm_tcp_bridge RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
