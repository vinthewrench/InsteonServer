//
//  ServerCommands.cpp
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
#include "date.h"

#include "TCPServer.hpp"
#include "Telnet/TelnetServerConnection.hpp"
#include "REST/RESTServerConnection.hpp"
#include "ServerCmdQueue.hpp"
#include "Telnet/CmdLineRegistry.hpp"

#include "ServerCmdValidators.hpp"
#include "ServerCommands.hpp"
#include "CommonIncludes.h"
#include "InsteonDevice.hpp"

#include <string>
#include <sstream>
#include <iomanip>

template <typename T> inline std::string int_to_hex(T val, size_t width=sizeof(T)*2)
{
	 std::stringstream ss;
	 ss << std::setfill('0') << std::setw(width) << std::hex << (val|0);
	 return ss.str();
}

// MARK: - SERVER FUNCTIONS


constexpr string_view FOO_VERSION_STRING		= "1.0.0";

constexpr string_view JSON_CMD_SETLEVEL 		= "set";
constexpr string_view JSON_CMD_SHOWDEVICE 	= "show";
constexpr string_view JSON_CMD_LIST			 	= "list";
constexpr string_view JSON_CMD_DATE			 	= "date";
constexpr string_view JSON_CMD_PLM			 	= "plm";
constexpr string_view JSON_CMD_STATUS			= "status";
constexpr string_view JSON_CMD_BEEP				= "beep";
constexpr string_view JSON_CMD_BACKLIGHT		= "backlight";

constexpr string_view JSON_CMD_VERSION			= "version";
constexpr string_view JSON_CMD_GROUP			= "group";
constexpr string_view JSON_ARG_DEVICEID 		= "deviceID";
constexpr string_view JSON_ARG_GROUPID 		= "groupID";

constexpr string_view JSON_ARG_LEVEL 			= "level";
constexpr string_view JSON_ARG_FUNCTION		= "function";
constexpr string_view JSON_ARG_DATE				= "date";
constexpr string_view JSON_ARG_VERSION			= "version";
constexpr string_view JSON_ARG_TIMESTAMP		= "timestamp";
constexpr string_view JSON_ARG_DEVICEIDS 		= "deviceIDs";
constexpr string_view JSON_ARG_GROUPIDS 		= "groupIDs";

constexpr string_view JSON_ARG_DEVICEINFO 	= "deviceInfo";
constexpr string_view JSON_ARG_INFO			 	= "info";
constexpr string_view JSON_ARG_DETAILS 		= "details";
constexpr string_view JSON_ARG_STATE			= "state";
constexpr string_view JSON_ARG_STATESTR		= "stateString";
constexpr string_view JSON_ARG_ETAG				= "ETag";
constexpr string_view JSON_ARG_FORCE			= "force";
constexpr string_view JSON_ARG_NAME					= "name";
constexpr string_view JSON_ARG_DESCRIPTION		= "description";
 
constexpr string_view JSON_ARG_DATABASE		= "database";
constexpr string_view JSON_VAL_ALL				= "all";
constexpr string_view JSON_VAL_VALID			= "valid";
constexpr string_view JSON_VAL_DETAILS			= "details";
constexpr string_view JSON_VAL_CHANGED			= "changed";

static void ListDevicesCommand( ServerCmdQueue* cmdQueue,
									 json request,  // entire request
									 TCPClientInfo cInfo,
									 ServerCmdQueue::cmdCallback_t completion){

	using namespace rest;
	using namespace date;
	using namespace std::chrono;

	json reply;
	
	ServerCmdArgValidator v;
	string listFunc;
	long eTag;

//	JSON_CMD_LIST DATABASE
	if(v.getStringFromJSON(JSON_ARG_DATABASE, request, listFunc)){
		
//		printf("LIST DATABASE %s \n", listFunc.c_str());
		
		auto state = insteon.currentState();
		if(state == InsteonMgr::STATE_READY || state == InsteonMgr::STATE_VALIDATING) {
			
			bool showDetails = false;
			bool onlyChanges = false;
			auto db = insteon.getDB();
		
			vector<DeviceID> deviceIDs;

			if(listFunc == JSON_VAL_ALL) {
				deviceIDs = db->allDevices();
			}
			else if(listFunc == JSON_VAL_VALID) {
				deviceIDs = db->validDevices();
			}
			else if(listFunc == JSON_VAL_DETAILS) {
				deviceIDs = db->allDevices();
				showDetails = true;
			}
			else if(listFunc == JSON_VAL_CHANGED) {
				if(v.getLongIntFromJSON(JSON_ARG_ETAG, request, eTag)){
				deviceIDs = db->devicesUpdateSinceEtag(eTag);
				onlyChanges = true;
				}
				else {
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Missing eTag" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return;
				}
			}

			if(showDetails){
				json devicesEntries;
				for(DeviceID deviceID :deviceIDs) {
					insteon_dbEntry_t info;
					if( db->getDeviceInfo(deviceID,  &info)) {
						
						string strDeviceID = deviceID.string();
		
						json entry;
						entry[string(JSON_VAL_VALID)] = info.isValidated;
						entry[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
						system_clock::time_point tp = system_clock::from_time_t (info.lastUpdated);
						auto rfc399 =  date::format("%FT%TZ", tp);
						entry["lastUpdated"] =  rfc399;

						devicesEntries[strDeviceID] = entry;
					}
					
					reply[string(JSON_ARG_DETAILS)] = devicesEntries;
 				}
 
			}
			else {
				vector<string> deviceList;

				for(DeviceID deviceID :deviceIDs) {
					deviceList.push_back(deviceID.string());
				}
				reply[string(JSON_ARG_DEVICEIDS)] = deviceList;

			}
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return;
			
		}
		else {
			makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
			(completion) (reply, STATUS_UNAVAILABLE);
			return;
		}
	}
//		if(v.getStringFromJSON(JSON_VAL_CHANGED, request, listFunc)){
//	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "List what?" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}

static void SetBacklightLevelCommand( ServerCmdQueue* cmdQueue,
									 json request,  // entire request
									 TCPClientInfo cInfo,
									 ServerCmdQueue::cmdCallback_t completion){
	using namespace rest;
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	DeviceLevelArgValidator vLevel;
	
	string deviceID;
	int level;
	
	if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, request, deviceID)){
		if(vLevel.getvalueFromJSON(JSON_ARG_LEVEL, request, level)){
			
			bool queued = insteon.setLEDBrightness(deviceID, level,
														[=](bool didSucceed){
				
				json reply;
				reply[string(JSON_ARG_DEVICEID)] = deviceID;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
				(completion) (reply, STATUS_UNAVAILABLE);
			}
		}
		else {
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for level" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for deviceID" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}

static void SetLevelCommand( ServerCmdQueue* cmdQueue,
									 json request,  // entire request
									 TCPClientInfo cInfo,
									 ServerCmdQueue::cmdCallback_t completion){
	using namespace rest;
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	DeviceLevelArgValidator vLevel;
	
	string deviceID;
	int level;
	
	if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, request, deviceID)){
		if(vLevel.getvalueFromJSON(JSON_ARG_LEVEL, request, level)){
			
			bool queued = insteon.setOnLevel(deviceID, level,
														[=](eTag_t eTag, bool didSucceed){
				
				json reply;
				reply[string(JSON_ARG_DEVICEID)] = deviceID;
				reply[string(JSON_ARG_ETAG)] = eTag;
				reply[string(JSON_ARG_LEVEL)]  = level;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
				(completion) (reply, STATUS_UNAVAILABLE);
			}
		}
		else {
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for level" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for deviceID" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}
	 
 

static void ShowDeviceCommand( ServerCmdQueue* cmdQueue,
									 json request,  // entire request
										TCPClientInfo cInfo,
										ServerCmdQueue::cmdCallback_t completion){
	using namespace rest;
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	ServerCmdArgValidator v;
	
	string strdeviceID;
	
	bool forceLookup = false;
	bool showDetails = false;
	
	//	JSON_ARG_FORCE
	if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, request, strdeviceID)){
		
		DeviceID deviceID = DeviceID(strdeviceID);
		
		// get optional forceLookup
		v.getBoolFromJSON(JSON_ARG_FORCE, request, forceLookup);
	
		// get optional details
		v.getBoolFromJSON(JSON_ARG_DETAILS, request, showDetails);
 
		bool queued = insteon.getOnLevel(strdeviceID, forceLookup,
													[=](uint8_t level, eTag_t eTag, bool didSucceed){
			
			auto db = insteon.getDB();
			
			insteon_dbEntry_t info;
		
			json reply;
			reply[string(JSON_ARG_DEVICEID)] 	= strdeviceID;
			reply[string(JSON_ARG_ETAG)] 		= eTag;
			reply[string(JSON_ARG_LEVEL)]  		= level;
	 
			if(showDetails){
				reply[string(JSON_ARG_NAME)] = deviceID.nameString();
				if(db->getDeviceInfo(deviceID,  &info)) {
					
					reply[string(JSON_VAL_VALID)] = info.isValidated;
					reply[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
					reply[string(JSON_ARG_DESCRIPTION)] = info.deviceInfo.descriptionString();
				}
			}
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
		});
		
		if(!queued) {
	 		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
			(completion) (reply, STATUS_UNAVAILABLE);
		}
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for deviceID" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}


static void ShowDateCommand(ServerCmdQueue* cmdQueue,
								  json request,  // entire request
									 TCPClientInfo cInfo,
								  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	string plmFunc;
	
	using namespace date;
	using namespace std::chrono;

	printf("DATE request \n" );
	
	const auto tp = time_point_cast<seconds>(system_clock::now());
	auto rfc399 =  date::format("%FT%TZ", tp);
	
	reply["date"] =  rfc399;
 
	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

static void PlmFunctionsCommand(ServerCmdQueue* cmdQueue,
											json request,  // entire request
										  TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	ServerCmdArgValidator v;
	string plmFunc;
	
	if(v.getStringFromJSON(JSON_ARG_FUNCTION, request, plmFunc)){
		
		printf("PLM %s \n", plmFunc.c_str());
		
		auto state = insteon.currentState();
		
		if(plmFunc == JSON_ARG_INFO) {
			if(state == InsteonMgr::STATE_READY || state == InsteonMgr::STATE_VALIDATING) {
				DeviceID deviceID;
				DeviceInfo deviceInfo;
				json reply;
				
				if(insteon.plmInfo(&deviceID, &deviceInfo)){
					
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
					reply[string(JSON_ARG_DEVICEINFO)] = deviceInfo.string();
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
					return;
					
				}
				
			}
		}
		
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "invalid request" );;
		(completion) (reply, STATUS_BAD_REQUEST);
		return;
		
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "No argument for function" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}

static void GroupFunctionsCommand(ServerCmdQueue* cmdQueue,
											json request,  // entire request
											 TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	
		auto dump = request.dump(4);
		dump = replaceAll(dump, "\n","\r\n" );
		cout << dump << "\n";

	ServerCmdArgValidator v;
	DeviceIDArgValidator vDeviceID;

	string groupFunc;
	auto db = insteon.getDB();

	if(v.getStringFromJSON(JSON_ARG_FUNCTION, request, groupFunc)){
	
		if(groupFunc == JSON_CMD_LIST) {
			json groupsList;
			
			auto groupIDs = db->allGroups();
			for(auto groupID : groupIDs){
				
				json entry;
				entry[string(JSON_ARG_NAME)] =  db->groupGetName(groupID);
				vector<DeviceID> deviceIDs = db->groupGetDevices(groupID	);
				vector<string> deviceList;
				for(DeviceID deviceID :deviceIDs) {
					deviceList.push_back(deviceID.string());
				}
				entry[string(JSON_ARG_DEVICEIDS)] = deviceList;
				groupsList[to_string(groupID)] = entry;
			}
			reply[string(JSON_ARG_GROUPIDS)] = groupsList;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return;
		}
		else if(groupFunc == JSON_ARG_DETAILS) {
	
		}
		else {
			
		}
		
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "invalid request" );;
		(completion) (reply, STATUS_BAD_REQUEST);
		return;

//		printf("Group %s \n", groupFunc.c_str());
//
//		string key = string(JSON_ARG_DEVICEIDS);
//
//		if( request.contains(key)
//			&& request.at(key).is_array()){
//
//			auto deviceIDs = request.at(key);
//
//			for(auto val : deviceIDs){
//				if(val.is_string()){
//					string deviceID = val;
//
//					if(vDeviceID.validateArg(deviceID)) {
//						printf("add: %s\n",deviceID.c_str());
//					}
//					else {
//						// flag error
//					}
//				}
//
//			}
//		}
//
//		makeStatusJSON(reply,STATUS_OK);
//		(completion) (reply, STATUS_OK);
//		return;
		
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "No argument for function" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}


static void VersionCommand(ServerCmdQueue* cmdQueue,
											json request,  // entire request
										TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	reply[string(JSON_ARG_VERSION)] =   FOO_VERSION_STRING;
	reply[string(JSON_ARG_TIMESTAMP)]	=  string(__DATE__) + " " + string(__TIME__);
	
	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

static void StatusCommand(ServerCmdQueue* cmdQueue,
											json request,  // entire request
										TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;

	auto state = insteon.currentState();
	string stateStr = insteon.currentStateString();

	reply[string(JSON_ARG_STATE)] =   state;
	reply[string(JSON_ARG_STATESTR)] =   stateStr;
	 
	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}
 
static void BeepCommand(ServerCmdQueue* cmdQueue,
											json request,  // entire request
										TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	string deviceID;
	
	if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, request, deviceID)){
		
		InsteonDevice(deviceID).beep([=](bool didSucceed){
 			json reply;

			reply[string(JSON_ARG_DEVICEID)] = deviceID;

 			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			});
		return;
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Bad argument for deviceID" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
}

 
// MARK: - COMMAND LINE FUNCTIONS

static bool SETcmdHandler( stringvector 		line,
								  CmdLineMgr* 		mgr,
								  boolCallback_t 	cb){
	
	using namespace rest;
	string errorStr;
	string command = line[0];
	string deviceID;
	string onlevel;
	 
	uint8_t dimLevel = 0;

	DeviceIDArgValidator v1;
 
	if(	line.size() > 1)
		deviceID = line[1];
	
	if(	line.size() > 2)
		onlevel = line[2];
	
	if(	deviceID.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(command == "dim" && onlevel.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects level.";
	}
	else if(!v1.validateArg(deviceID)){
		errorStr =  "\x1B[36;1;4m"  + deviceID + "\x1B[0m is not a valid DeviceID.";
	}
	else if(!InsteonDevice::stringToLevel(string(onlevel), &dimLevel)){
 		errorStr =  "\x1B[36;1;4m"  + onlevel + "\x1B[0m is not a valid device level.";
	}
	else {
		
		json request;
		request[kREST_command] =  JSON_CMD_SETLEVEL ;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		
		if(command == "turn-on") {
			request[string(JSON_ARG_LEVEL)] = 255;
		}
		else if(command == "turn-off") {
			request[string(JSON_ARG_LEVEL)] = 0;
		}
		else if(command == "dim"){
 			request[string(JSON_ARG_LEVEL)] =  dimLevel;
		}
		
//		auto dump = request.dump(4);
//		dump = replaceAll(dump, "\n","\r\n" );
//		cout << dump << "\n";
//
		ServerCmdQueue* cmdQueue = ServerCmdQueue::shared();
		TCPClientInfo cInfo = mgr->getClientInfo();

		cmdQueue->queueCommand(request, cInfo, [=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			
			if(success) {
				mgr->sendReply( "OK \n\r");
			}
			else {
				string error = errorMessage(reply);
				mgr->sendReply( error + "\n\r");
			}
			
			(cb) (success);
		});
		
		return true;
	}
	
	mgr->sendReply(errorStr + "\n\r");
	return false;
};


static bool BackLightCmdHandler( stringvector 		line,
								  CmdLineMgr* 		mgr,
								  boolCallback_t 	cb){
	
	using namespace rest;
	string errorStr;
	string command = line[0];
	string deviceID;
	string onlevel;
	 
	uint8_t dimLevel = 0;

	DeviceIDArgValidator v1;
 
	if(	line.size() > 1)
		deviceID = line[1];
	
	if(	line.size() > 2)
		onlevel = line[2];
	
	if(	deviceID.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(command == "dim" && onlevel.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects backlight level.";
	}
	else if(!v1.validateArg(deviceID)){
		errorStr =  "\x1B[36;1;4m"  + deviceID + "\x1B[0m is not a valid DeviceID.";
	}
	else if(!InsteonDevice::stringToBackLightLevel(string(onlevel), &dimLevel)){
		errorStr =  "\x1B[36;1;4m"  + onlevel + "\x1B[0m is not a valid backlight level.";
	}
	else {
		
		json request;
		request[kREST_command] =  JSON_CMD_BACKLIGHT ;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		request[string(JSON_ARG_LEVEL)] =  dimLevel;
	
		ServerCmdQueue* cmdQueue = ServerCmdQueue::shared();
		TCPClientInfo cInfo = mgr->getClientInfo();

		cmdQueue->queueCommand(request, cInfo, [=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			
			if(success) {
				mgr->sendReply( "OK \n\r");
			}
			else {
				string error = errorMessage(reply);
				mgr->sendReply( error + "\n\r");
			}
			
			(cb) (success);
		});
		
		return true;
	}
	
	mgr->sendReply(errorStr + "\n\r");
	return false;
};




static bool DATEcmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;
	json request;
	request[kREST_command] =  JSON_CMD_DATE ;
	
	TCPClientInfo cInfo = mgr->getClientInfo();
	ServerCmdQueue::shared()->queueCommand(request,cInfo, [=] (json reply, httpStatusCodes_t code) {
		
		string str;
  
		if(reply.count(JSON_ARG_DATE) ) {
			string rfc399 =  reply[string(JSON_ARG_DATE)] ;
  
			using namespace date;
			using namespace std::chrono;

			std::chrono::system_clock::time_point tp;
			std::istringstream in{ rfc399 };
			in >> date::parse("%FT%TZ", tp);
			
	//		auto pdt_now = make_zoned(current_zone(), utc_now);
	//		cout << format("%a, %b %d %Y at %I:%M %p %Z\n", pdt_now);
	//
			time_t tt = system_clock::to_time_t ( tp );
			struct tm * timeinfo = localtime (&tt);
		
			str = string(asctime(timeinfo));  // asctime appends \n
			
			mgr->sendReply(str + "\r");
		}
		
		(cb) (code > 199 && code < 400);
	});
	
	//cmdQueue->queue
	
		return true;
};


static bool SHOWcmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;

	string errorStr;
	string command = line[0];
	string deviceID;
	string onlevel;
	
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceID = line[1];
	
	if(	deviceID.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(!v1.validateArg(deviceID)){
		errorStr =  "\x1B[36;1;4m"  + deviceID + "\x1B[0m is not a valid DeviceID.";
	}
	else {
		
		json request;
		request[kREST_command] =  JSON_CMD_SHOWDEVICE ;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueCommand(request,cInfo, [=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			
			if(success) {
				
				auto dump = reply.dump(4);
				dump = replaceAll(dump, "\n","\r\n" );
				cout << dump << "\n";
		
				if(reply.count(JSON_ARG_LEVEL)) {
					
					int onLevel = reply.at(string(JSON_ARG_LEVEL)).get<int>();
					mgr->sendReply( "level: " + InsteonDevice::onLevelString(onLevel) +"\n\r" );
				}
			}
			else {
				string error = errorMessage(reply);
				mgr->sendReply( error + "\n\r");
			}
			
			(cb) (success);
		});
		
		return true;
	}
	
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
};

static bool InfoCmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;

	string errorStr;
	string command = line[0];
	string deviceID;
	string onlevel;
	
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceID = line[1];
	
	if(	deviceID.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(!v1.validateArg(deviceID)){
		errorStr =  "\x1B[36;1;4m"  + deviceID + "\x1B[0m is not a valid DeviceID.";
	}
	else {
		
		json request;
		request[kREST_command] =  JSON_CMD_SHOWDEVICE ;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		request[string(JSON_ARG_DETAILS)] =  true;
	
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueCommand(request,cInfo, [=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			
			if(success) {
				std::ostringstream oss;
		 
				for (auto& el : reply.items()) {
					oss << setw(14) << el.key() << ": "  <<  setw(0) << el.value() << "\r\n";
				}
				
	 			oss << "\n\r";
		 		mgr->sendReply(oss.str());
				 
			}
			else {
				string error = errorMessage(reply);
				mgr->sendReply( error + "\n\r");
			}
			
			(cb) (success);
		});
		
		return true;
	}
	
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
};

static bool PLMcmdHandler( stringvector line,
								  CmdLineMgr* mgr,
								  boolCallback_t	cb){
	using namespace rest;

	string errorStr;
	string command = line[0];

	if(line.size() < 2){
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects a function.";
	}
	else {
		string subcommand = line[1];
		
		json request;
		TCPClientInfo cInfo = mgr->getClientInfo();
		request[kREST_command] =  JSON_CMD_PLM ;
		bool isValid = false;
		
		
		if(subcommand == JSON_ARG_INFO) {
			request[string(JSON_ARG_FUNCTION)] =  JSON_ARG_INFO ;
			isValid = true;
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
		
		if(isValid){
			ServerCmdQueue::shared()->queueCommand(request,cInfo,[=] (json reply, httpStatusCodes_t code) {
				
				
				bool success = didSucceed(reply);
				if(success) {
				
					if(subcommand == JSON_ARG_INFO) {
						DeviceIDArgValidator vDeviceID;
						ServerCmdArgValidator v;
						
						string strDeviceID;
						string strDeviceInfo;
						if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, strDeviceID)){
							string plmFunc;
							
							if(v.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, strDeviceInfo)){
					
								DeviceID deviceID = DeviceID(strDeviceID);
								DeviceInfo deviceInfo = DeviceInfo(strDeviceInfo);
			
								if(!deviceInfo.isNULL() && !deviceID.isNULL()){
							
									std::ostringstream oss;
									oss << deviceID.string()  << " "  << deviceInfo.descriptionString();
									if(deviceInfo.GetFirmware() > 0){
										oss << ", Version:" << to_string(deviceInfo.GetFirmware());
									}
									oss << "\r\n";
									mgr->sendReply(oss.str());
									return;
								}
	 						}
						}
					}
					
					auto dump = reply.dump(4);
					dump = replaceAll(dump, "\n","\r\n" );
					cout << dump << "\n";
	 				mgr->sendReply( "??\n\r" );
					
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				
				(cb) (success);
			});
			return true;
		}
	}
	
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;

};


static bool VersionCmdHandler( stringvector line,
										CmdLineMgr* mgr,
										boolCallback_t	cb){
	using namespace rest;
	json request;
	request[kREST_command] =  JSON_CMD_VERSION ;
	
	TCPClientInfo cInfo = mgr->getClientInfo();
	
	for (auto& t : cInfo.headers()){
		printf("%s = %s\n", t.first.c_str(), t.second.c_str());
	}

	ServerCmdQueue::shared()->queueCommand(request,cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		
		if(success) {
			std::ostringstream oss;

			if(reply.count(JSON_ARG_VERSION) ) {
				string ver = reply[string(JSON_ARG_VERSION)];
				oss << ver << " ";
			}
	
			if(reply.count(JSON_ARG_TIMESTAMP) ) {
				string timestamp = reply[string(JSON_ARG_TIMESTAMP)];
				oss <<  timestamp;
			}
	
			oss << "\r\n";
			mgr->sendReply(oss.str());
					
		}
		else {
			string error = errorMessage(reply);
			mgr->sendReply( error + "\n\r");
		}
		
		(cb) (success);
		
	});
	
	return true;
};

static bool ListCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
								  boolCallback_t	cb){
	using namespace rest;

	string errorStr;
	string command = line[0];

	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];

		json request;
		TCPClientInfo cInfo = mgr->getClientInfo();
		request[kREST_command] =  JSON_CMD_LIST ;
		bool isValid = false;
	
		if(subcommand == JSON_VAL_ALL) {
			request[string(JSON_ARG_DATABASE)] =  JSON_VAL_ALL ;
			isValid = true;
		}
		else 	if(subcommand == JSON_VAL_VALID) {
			request[string(JSON_ARG_DATABASE)] =  JSON_VAL_VALID ;
			isValid = true;
		}
		else 	if(subcommand == JSON_VAL_DETAILS) {
			request[string(JSON_ARG_DATABASE)] =  JSON_VAL_DETAILS ;
			isValid = true;
		}

		if(isValid) {
			ServerCmdQueue::shared()->queueCommand(request,cInfo,[=] (json reply, httpStatusCodes_t code) {
				bool success = didSucceed(reply);
				if(success) {
					
					std::ostringstream oss;

					string key1 = string(JSON_ARG_DEVICEIDS);
					string key2 = string(JSON_ARG_DETAILS);
		
					if( reply.contains(key1)
						&& reply.at(key1).is_array()){
						auto deviceIDs = reply.at(key1);
						for(auto strDeviceID :deviceIDs) {
							DeviceID deviceID = DeviceID(strDeviceID);
							oss << deviceID.string();
							oss << "\r\n";
						}
						oss << "\r\n";
						mgr->sendReply(oss.str());
						return;
					}
					else if( reply.contains(key2)
							  && reply.at(key2).is_object()){
						auto entries = reply.at(key2);
						
						for (auto& [key, value] : entries.items()) {
							
							DeviceID deviceID = DeviceID(key);

							string strDeviceInfo = value["deviceInfo"];
							DeviceInfo deviceInfo = DeviceInfo(strDeviceInfo);
							
							cout << key << " : " << value << "\n";

							oss << deviceID.string();
							oss << " ";
							
							if(value[string(JSON_VAL_VALID)])
								oss << " ";
							else
								oss << "v";

							oss << " ";
							
							oss << deviceID.nameString();
							
//							oss << " ";
//	 						oss << deviceInfo.descriptionString();
 							oss << "\r\n";
 						}

						oss << "\r\n";
						mgr->sendReply(oss.str());
					}
					else {
					
						// huh?
						auto dump = reply.dump(4);
						dump = replaceAll(dump, "\n","\r\n" );
						cout << dump << "\n";
 						mgr->sendReply( "OK\n\r" );
						return;
	 					}
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				
				(cb) (success);
			});
			return true;
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
 		}
	}

	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
}

static bool StatusCmdHandler( stringvector line,
										CmdLineMgr* mgr,
										boolCallback_t	cb){
	using namespace rest;
	json request;
	request[kREST_command] =  JSON_CMD_STATUS ;
	
	TCPClientInfo cInfo = mgr->getClientInfo();
	
	for (auto& t : cInfo.headers()){
		printf("%s = %s\n", t.first.c_str(), t.second.c_str());
	}

	ServerCmdQueue::shared()->queueCommand(request,cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		
		if(success) {
			std::ostringstream oss;

			if(reply.count(JSON_ARG_STATESTR) ) {
				string status = reply[string(JSON_ARG_STATESTR)];
				oss << status << " ";
			}
 
			oss << "\r\n";
			mgr->sendReply(oss.str());
					
		}
		else {
			string error = errorMessage(reply);
			mgr->sendReply( error + "\n\r");
		}
		
		(cb) (success);
		
	});
	
	return true;
};

static bool BeepCmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;

	string errorStr;
	string command = line[0];
	string deviceID;
	string onlevel;
	
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceID = line[1];
	
	if(	deviceID.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(!v1.validateArg(deviceID)){
		errorStr =  "\x1B[36;1;4m"  + deviceID + "\x1B[0m is not a valid DeviceID.";
	}
	else {
		
		json request;
		request[kREST_command] =  JSON_CMD_BEEP ;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueCommand(request,cInfo, [=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
	
			if(success) {
				mgr->sendReply( "OK \n\r");
			}
			else {
				string error = errorMessage(reply);
				mgr->sendReply( error + "\n\r");
			}
			
			(cb) (success);
		});
		
		return true;
	}
	
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
};


static bool GroupCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
									 boolCallback_t	cb){
	
	using namespace rest;

	string errorStr;
	string command = line[0];

	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];

		json request;
		TCPClientInfo cInfo = mgr->getClientInfo();
		request[kREST_command] =  JSON_CMD_GROUP ;
		bool isValid = false;
	
		if(subcommand == JSON_CMD_LIST) {
			request[string(JSON_ARG_FUNCTION)] =  JSON_CMD_LIST ;
			isValid = true;
		}
  
		if(isValid) {
			ServerCmdQueue::shared()->queueCommand(request,cInfo,[=] (json reply, httpStatusCodes_t code) {
				bool success = didSucceed(reply);
				if(success) {
					
					std::ostringstream oss;

					string key1 = string(JSON_ARG_GROUPIDS);
		
				  if( reply.contains(key1)
							  && reply.at(key1).is_object()){
						auto entries = reply.at(key1);
						
						for (auto& [key, value] : entries.items()) {
							
							groupID_t groupID = stoi( key );
							
							auto deviceIDS = value[string(JSON_ARG_DEVICEIDS)];

							oss  << " " << std::setfill ('0') << std::setw(4) << std::hex << groupID << " ";
							oss  << std::dec << std::setfill (' ')<<  std::setw(3) <<  deviceIDS.size() <<  " ";
							
							string groupName = value[string(JSON_ARG_NAME)];
							oss << "\"" << groupName << "\""  << "\r\n";
							
//							auto deviceIDS = value[string(JSON_ARG_DEVICEIDS)];
//							if(deviceIDS.size() > 0){
//								for(auto devString : deviceIDS){
//									DeviceID deviceID = DeviceID(devString);
//									oss << "   " << deviceID.string()  << "\r\n";
//								}
// 							}
//
						}

	 					mgr->sendReply(oss.str());
					}
					else {
					
						// huh?
						auto dump = reply.dump(4);
						dump = replaceAll(dump, "\n","\r\n" );
						cout << dump << "\n";
						mgr->sendReply( "OK\n\r" );
						return;
						}
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				
				(cb) (success);
			});
			return true;
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}

	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
}



// MARK: -  register commands

void test1(){
	
	auto db = insteon.getDB();

#if 1
	for (const auto group  : db->allGroups()) {
		cout << group << " " <<  db->groupGetName(group)<< "\n";
		auto devices = db->groupGetDevices(group);
		for(auto device : devices){
			cout << "\t" << device.string() << "\n";
		}
		cout  << "\n";
	}
#else
	auto deviceList = db->allDevices();
	
	if(deviceList.size() == 0)
		return;
	
	for(int i = 0; i<10; i++){
		groupID_t group;
		
		db->groupCreate(&group, string("group:") + to_string(i));
		
		int  count = rand() % deviceList.size();
		if(count == 0) count = 1;
		
		for(int j= 0; j <count; j++){
			
			int  offset = rand() % deviceList.size();
			db->groupAddDevice(group, deviceList[offset]);
		}
		
		cout << group << " " <<  db->groupGetName(group) + "\n";
 	}
	
	for (const auto group  : db->allGroups()) {
		
		string newName = db->groupGetName(group) + "xx";
		db->groupSetName(group, newName);
		cout << group << " " <<  db->groupGetName(group)<< "\n";
		auto devices = db->groupGetDevices(group);
		for(auto device : devices){
			cout << "\t" << device.string() << "\n";
		}
		cout  << "\n";
	}
//	
//	for (const auto group  : db->allGroups()) {
//		cout <<  "delete " << group << " " <<  db->groupGetName(group) + "\n";
//		db->groupDelete(group);
//	}
	
#endif
	
}

void registerServerCommands() {
  
	// create the server command processor
	auto cmdQueue = ServerCmdQueue::shared();
		
	// register all our server commands
	cmdQueue->registerCommand( JSON_CMD_SETLEVEL,
									  SetLevelCommand);

	cmdQueue->registerCommand( JSON_CMD_SHOWDEVICE,
									  ShowDeviceCommand);

	cmdQueue->registerCommand( JSON_CMD_DATE,
									  ShowDateCommand);

	cmdQueue->registerCommand( JSON_CMD_PLM,
									  PlmFunctionsCommand);

	cmdQueue->registerCommand( JSON_CMD_GROUP,
									  GroupFunctionsCommand);

	cmdQueue->registerCommand( JSON_CMD_VERSION,
									  VersionCommand);

	cmdQueue->registerCommand( JSON_CMD_LIST,
									  ListDevicesCommand);

	cmdQueue->registerCommand( JSON_CMD_STATUS,
									  StatusCommand);
 
	cmdQueue->registerCommand( JSON_CMD_BEEP,
									  BeepCommand);

	cmdQueue->registerCommand( JSON_CMD_BACKLIGHT,
									  SetBacklightLevelCommand);
 

	 // register command line commands
	 auto cmlR = CmdLineRegistry::shared();
	cmlR->registerCommand("turn-on",  	SETcmdHandler);
	cmlR->registerCommand("turn-off",  SETcmdHandler);
	cmlR->registerCommand("dim",  		SETcmdHandler);

	cmlR->registerCommand("show",  	SHOWcmdHandler);
	cmlR->registerCommand("plm",  	PLMcmdHandler);
	cmlR->registerCommand("date",	DATEcmdHandler);

	cmlR->registerCommand("version",	VersionCmdHandler);

	cmlR->registerCommand("list",	ListCmdHandler);

	cmlR->registerCommand("status",	StatusCmdHandler);
	cmlR->registerCommand("beep",	BeepCmdHandler);
	cmlR->registerCommand("set-backlight",	BackLightCmdHandler);
	cmlR->registerCommand("info",			InfoCmdHandler);
	
	cmlR->registerCommand("group",		GroupCmdHandler);

 }
