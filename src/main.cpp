//
//  main.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 4/9/21.
//

#include <iostream>

#include "ServerCmdQueue.hpp"
#include "InsteonAPISecretMgr.hpp"

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

#include "Action.hpp"

#if __APPLE__
//  #define SERIAL_DEVICE "/dev/cu.usbserial-AK05Z8CQ"
//   #define SERIAL_DEVICE "/dev/cu.usbserial-AL02YD29"
#define SERIAL_DEVICE "/dev/cu.usbserial-AK05ZAZ7"
#elif __arm__
#define SERIAL_DEVICE "/dev/ttyUSB0"
#else
#warning  NO CLUE

#endif

void linkKP(){
	DeviceID deviceID =  DeviceID("33.4F.F6");
	auto keypad = InsteonKeypadDevice(deviceID );
	
	START_VERBOSE;
	
	// link up the keypad button to groups.
	vector<pair<uint8_t, uint8_t> > pairs =
	{ {2, 0xA2},{3,0xA3},{4,0xA4},{5,0xA5},{6,0xA6},{7,0xA7},{8,0xA8},   };
	
	
	insteon.linkKeyPadButtonsToGroups(deviceID, pairs, [=](bool didSucceed) {
		
		insteon.addResponderToDevice(deviceID, 0x02,  [=](bool didSucceed) {
			insteon.addResponderToDevice(deviceID, 0x03,  [=](bool didSucceed) {
				insteon.addResponderToDevice(deviceID, 0x04,  [=](bool didSucceed) {
					insteon.addResponderToDevice(deviceID, 0x05,  [=](bool didSucceed) {
						insteon.addResponderToDevice(deviceID, 0x06,  [=](bool didSucceed) {
							insteon.addResponderToDevice(deviceID, 0x07,  [=](bool didSucceed) {
								insteon.addResponderToDevice(deviceID, 0x08,  [=](bool didSucceed) {
									
									printf("success");
									
									
								});
							});
						});
					});
				});
			});
		});
	});
}

void test(){
	DeviceID deviceID =  DeviceID("33.4F.F6");
	
	//	InsteonKeypadDevice(deviceID).setNonToggleMask(00, [=]( bool didSucceed) {
	//	});
	//
	
	InsteonKeypadDevice(deviceID).getKeypadLEDState( [=](uint8_t mask, bool didSucceed) {
		
		if(didSucceed){
			mask = ~mask;
			InsteonKeypadDevice(deviceID).setKeypadLEDState(mask, [=](bool didSucceed) {
				
				test();
			});
			
		}
		else {
			test();
		}
		
		
	});
	
	
}

// MARK: - MAIN
[[clang::no_destroy]]  InsteonMgr	insteon;

int main(int argc, const char * argv[]) {
	
	START_INFO;
	
// load the database cachefile
	insteon.loadCacheFile();

//set up the api secrets
	InsteonAPISecretMgr apiSecrets(insteon.getDB());
//	apiSecrets.apiSecretCreate("foobar","12345" );
	
	// create the server command processor
	auto cmdQueue = new ServerCmdQueue(&apiSecrets);
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
	while(true) {
		auto state = insteon.currentState();
		bool noPLM = state == InsteonMgr::STATE_NO_PLM || state == InsteonMgr::STATE_SETUP;
		
		try{
			if(noPLM){
				
				insteon.begin("",
								  [=](bool didSucceed) {
					
					if(didSucceed){
						insteon.syncPLM( [=](bool didSucceed) {
							if(didSucceed){
								insteon.validatePLM( [](bool didSucceed) {
								});
								
							}
						});
					}
					
				});
			}
		}
		catch ( const InsteonException& e)  {
			printf("\tError %d %s\n\n", e.getErrorNumber(), e.what());
			noPLM = true;
		}
		
		if(noPLM)
			sleep(5);
		else
			sleep(30);
	}
	
	return 0;
	
}
