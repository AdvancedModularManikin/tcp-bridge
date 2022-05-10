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
        Manikin.cpp Manikin.h TPMS.cpp TPMS.h bridge.h)

add_executable(amm_tcp_bridge ${TCP_BRIDGE_MODULE_SOURCES})

target_link_libraries(
   amm_tcp_bridge
        PUBLIC amm_std
        PUBLIC boost_system
        PUBLIC boost_thread
		pthread
		boost_program_options
	tinyxml2
)

install(TARGETS amm_tcp_bridge RUNTIME DESTINATION bin)
install(DIRECTORY ../config DESTINATION bin)
