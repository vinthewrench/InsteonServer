# export CC=/usr/local/clang_9.0.0/bin/clang
# export CXX=/usr/local/clang_9.0.0/bin/clang++

cmake_minimum_required(VERSION 3.0)

project(InsteonMgr)      

set(SOURCES
	src/main.cpp
	src/ServerCmdValidators.cpp
	src/ServerCommands.cpp
	src/main.cpp 
	InsteonMgr/Action.cpp
	InsteonMgr/DeviceID.cpp
	InsteonMgr/DeviceInfo.cpp
	InsteonMgr/Event.cpp
	InsteonMgr/InsteonALDB.cpp
	InsteonMgr/InsteonAPISecretMgr.cpp
	InsteonMgr/InsteonCmdQueue.cpp
	InsteonMgr/InsteonDB.cpp
	InsteonMgr/InsteonDevice.cpp
	InsteonMgr/InsteonDeviceEventMgr.cpp
	InsteonMgr/InsteonLinking.cpp
	InsteonMgr/InsteonMgr.cpp
	InsteonMgr/InsteonPLM.cpp
	InsteonMgr/InsteonParser.cpp
	InsteonMgr/InsteonValidator.cpp
	Submodule/FooServer/src/LogMgr.cpp
	InsteonMgr/NotificationCenter.cpp
	InsteonMgr/PLMStream.cpp
	InsteonMgr/ScheduleMgr.cpp
	Submodule/FooServer/src/TimeStamp.cpp
	InsteonMgr/sunset.cpp
	InsteonMgr/xxhash.c
	InsteonMgr/sleep.c
	Submodule/FooServer/src/RESTutils.cpp
	Submodule/FooServer/src/ServerCmdQueue.cpp
	Submodule/FooServer/src/TCPClientInfo.cpp
	Submodule/FooServer/src/TCPServer.cpp
	Submodule/FooServer/src/REST/http_parser.c
	Submodule/FooServer/src/REST/REST_URL.cpp
	Submodule/FooServer/src/REST/RESTServerConnection.cpp
 	Submodule/FooServer/src/REST/yuarel.c
 	Submodule/FooServer/src/Telnet/CmdLineBuffer.cpp
 	Submodule/FooServer/src/Telnet/CmdLineBuffer.cpp
 	Submodule/FooServer/src/Telnet/CmdLineHelp.cpp
 	Submodule/FooServer/src/Telnet/CmdLineMgr.cpp
 	Submodule/FooServer/src/Telnet/CmdLineRegistry.cpp
 	Submodule/FooServer/src/Telnet/TelnetServerConnection.cpp
 	Submodule/FooServer/src/Telnet/libtelnet.c
 	Submodule/FooServer/src/hash-library/sha256.cpp
 	)

add_executable(InsteonMgr ${SOURCES})


set_target_properties(InsteonMgr PROPERTIES
            CXX_STANDARD 17
            CXX_EXTENSIONS OFF
            )
            
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(InsteonMgr PRIVATE Threads::Threads)

target_include_directories(InsteonMgr
	PRIVATE 
	src
	InsteonMgr  
	Submodule/FooServer/src
	Submodule/FooServer/src/hash-library 
	Submodule/FooServer/src/REST
	Submodule/FooServer/src/Telnet
	)

set(CMAKE_BINARY_DIR "bin")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_BINARY_DIR}
)
add_dependencies(InsteonMgr copy_assets)
