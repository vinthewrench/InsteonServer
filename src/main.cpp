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
 
void test(){
	vector<string> actions;
	
	actions.clear();
	DeviceID deviceID =  DeviceID("01.02.03");
	GroupID groupID =  GroupID("01.02.03");

	auto db = insteon.getDB();
	
	for(int i = 0; i < 4; i++){
		
		actionGroupID_t agID1;
		
		string name = "group_" + to_string(i);
		
		if(db->actionGroupCreate(&agID1, name)){
			if( db->actionGroupSetName(agID1, name + "_a")) {
								
				auto a1 = Action(deviceID, Action::ACTION_SET_LEVEL, 255);
				db->actionGroupAddAction(agID1, a1);
				db->actionGroupAddAction(agID1, Action(DeviceID("54.F5.2D"), Action::ACTION_BEEP));
				db->actionGroupAddAction(agID1, Action(deviceID, Action::ACTION_SET_LED_BRIGHTNESS, 120));
 				db->actionGroupAddAction(agID1,  Action(groupID, Action::ACTION_SET_LEVEL, 100));
				db->actionGroupAddAction(agID1, Action(22, 255));
				
				
				actionGroupID_t agID2;
				string subName = "subgroup_" + to_string(i);
				db->actionGroupCreate(&agID2, subName);
				db->actionGroupAddAction(agID1,  Action(agID2));
//		 		db->actionGroupDelete(agID1);
				
			}
		}

	}
	
	if(0){
		
		auto actionIDs = db->allActionGroupsIDs();
		for(auto agID : actionIDs ){
			
			insteon.executeActionGroup(agID,[=](bool didSucceed) {
				
				string name = db->actionGroupGetName(agID);
				auto actions = db->actionGroupGetActions(agID);
				
				printf("completed %x: %10s %ld\n", agID, name.c_str(), actions.size());
			});
			
		}
	}
// 	for(auto agID : actionIDs ){
//
//		string name = db->actionGroupGetName(agID);
//		auto actions = db->actionGroupGetActions(agID);
//
//		printf("%x: %10s %ld\n", agID, name.c_str(), actions.size());
//
//		for(auto ref :actions){
//			Action a1 = ref.get();
//			string str = a1.serialize();
//			printf("\t%s", str.c_str());
//
//		}
//
//		db->actionGroupDelete(agID);
//	}
}

// MARK: - MAIN
[[clang::no_destroy]]  InsteonMgr	insteon;
 
int main(int argc, const char * argv[]) {
	 
	
	printf("Open %s\n", SERIAL_DEVICE);

	START_INFO;

	insteon.begin(SERIAL_DEVICE,
						[=](bool didSucceed) {
		
		if(didSucceed){
			
 			insteon.syncPLM( [=](bool didSucceed) {
				insteon.validatePLM( [](bool didSucceed) {
					
// 				test();

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
