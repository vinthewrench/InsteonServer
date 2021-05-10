//
//  main.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 4/9/21.
//

#include <iostream>

#include "ServerCmdQueue.hpp"
#include <regex>
#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <unistd.h>			//Needed for sleep

#include "Utils.hpp"

#include "TCPServer.hpp"
#include "Telnet/TelnetServerConnection.hpp"
#include "REST/RESTServerConnection.hpp"
#include "ServerCmdQueue.hpp"
#include "Telnet/CmdLineRegistry.hpp"

#include "ServerCmdValidators.hpp"
#include "ServerCommands.hpp"
 
#include "CommonIncludes.h"

#if __APPLE__
//  #define SERIAL_DEVICE "/dev/cu.usbserial-AK05Z8CQ"
//   #define SERIAL_DEVICE "/dev/cu.usbserial-AL02YD29"
 #define SERIAL_DEVICE "/dev/cu.usbserial-AK05ZAZ7"
#elif __arm__
#define SERIAL_DEVICE "/dev/ttyUSB0"
#else
#warning  NO CLUE

#endif


// MARK: - MAIN
[[clang::no_destroy]]  InsteonMgr	insteon;
 
int main(int argc, const char * argv[]) {
	 
	printf("Open %s\n", SERIAL_DEVICE);

	START_INFO;

	insteon.begin(SERIAL_DEVICE,
						[=](bool didSucceed) {
		
		if(didSucceed){
			
			insteon.syncPLM( [=](bool didSucceed) {
				
//				test1();
				
				insteon.validatePLM( [](bool didSucceed) {
//					insteon.getDB()->saveToCacheFile();
				});
			});
		};
	});
	
	// create the server command processor
	auto cmdQueue = ServerCmdQueue::shared();
	
	registerServerCommands();
	
	TCPServer telnet_server(cmdQueue);
	telnet_server.begin(2020, true, [=](){
		return new TelnetServerConnection();
	});
	
	TCPServer rest_server(cmdQueue);
	rest_server.begin(8080, false, [=](){
		return new RESTServerConnection();
	});
	
	// run the loop.
	try {
		
		while(true) {
			 	sleep(30);
		}
	}
		catch ( const InsteonException& e)  {
				printf("\tError %d %s\n\n", e.getErrorNumber(), e.what());
			return -1;
		}
		catch (std::invalid_argument& e)
		{
			printf("EXCEPTION: %s ",e.what() );
			 return -1;
		}

	return 0;
	
}
