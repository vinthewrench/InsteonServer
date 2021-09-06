//
//  ServerCommands.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 4/9/21.
//

#include <iostream>
#include <chrono>

#include "ServerCmdQueue.hpp"
#include "CmdLineHelp.hpp"
#include <regex>
#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <unistd.h>			//Needed for access()
#include <string>
#include <string.h>
#include <map>

#include "Utils.hpp"
#include "TimeStamp.hpp"

#include "TCPServer.hpp"
#include "Telnet/TelnetServerConnection.hpp"
#include "REST/RESTServerConnection.hpp"
#include "ServerCmdQueue.hpp"
#include "Telnet/CmdLineRegistry.hpp"

#include "ServerCmdValidators.hpp"
#include "ServerCommands.hpp"
#include "CommonIncludes.h"
#include "InsteonDevice.hpp"
#include "LogMgr.hpp"
#include "sleep.h"

#include "Utils.hpp"

// MARK: - COMMAND LINE FUNCTIONS Utilities


static void getCurrentPLMInfo( CmdLineMgr* mgr,
									std::function<void(DeviceID, DeviceInfo, bool)> cb = NULL) {
	
	using namespace rest;
 	REST_URL url;
	TCPClientInfo cInfo = mgr->getClientInfo();

	std::ostringstream oss;
	oss << "GET /plm/info" << " HTTP/1.1\n";
	oss << "Connection: close\n";
	oss << "\n";
	url.setURL(oss.str());

	ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		DeviceID 	 deviceID;
		DeviceInfo deviceInfo;
		
		if(success){
			
			DeviceIDArgValidator vDeviceID;
			ServerCmdArgValidator v1;
			string str;
			
			if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str)){
				
				deviceID = DeviceID(str);
				
				if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
					deviceInfo = DeviceInfo(str);
				
				if(cb) (cb) (deviceID,deviceInfo, true);
				return;
			}
		}
		
		if(cb) (cb) (deviceID,deviceInfo, false);
	});
}

// create a map for deviceIDs and Names

static void getDeviceNames( CmdLineMgr* mgr,
									std::function<void(map<DeviceID, string>)> callback = NULL) {
	using namespace rest;
	
	TCPClientInfo cInfo = mgr->getClientInfo();
	ServerCmdQueue::shared()->queueRESTCommand(REST_URL("GET /devices?details=1\n\n")
															 
															 , cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		map<DeviceID, string> deviceMap;
		deviceMap.clear();
		
		bool success = didSucceed(reply);
		if(success){
			
			string key2 = string(JSON_ARG_DETAILS);
			
			if( reply.contains(key2)
				&& reply.at(key2).is_object()){
				auto entries = reply.at(key2);
				
				for (auto& [key, value] : entries.items()) {
					
					DeviceID deviceID = DeviceID(key);
					
					if( value.contains(string(JSON_ARG_NAME))
						&& value.at(string(JSON_ARG_NAME)).is_string()){
						string deviceName  = value.at(string(JSON_ARG_NAME));
						
						deviceMap[deviceID] = deviceName;
					}
					
				}
			};
		}
		
		if(callback) (callback)(deviceMap);
	});
}


static void breakDuration(unsigned long secondsIn, tm &tm){
	
	long  remainingSeconds = secondsIn;
		
	tm.tm_mday =  (int)(remainingSeconds/SECS_PER_DAY);
	remainingSeconds = secondsIn - (tm.tm_mday * SECS_PER_DAY);
	
	tm.tm_hour =  (int)(remainingSeconds/SECS_PER_HOUR);
	remainingSeconds = secondsIn -   ((tm.tm_mday * SECS_PER_DAY) + (tm.tm_hour * SECS_PER_HOUR));
	
	tm.tm_min = (int)remainingSeconds/SECS_PER_MIN;
	remainingSeconds = remainingSeconds - (tm.tm_min * SECS_PER_MIN);
	
	tm.tm_sec = (int) remainingSeconds;
}

// MARK: - COMMAND LINE FUNCTIONS

static bool VersionCmdHandler( stringvector line,
										CmdLineMgr* mgr,
										boolCallback_t	cb){
	using namespace rest;
	TCPClientInfo cInfo = mgr->getClientInfo();
	//
	//	for (auto& t : cInfo.headers()){
	//		printf("%s = %s\n", t.first.c_str(), t.second.c_str());
	//	}
	
	// simulate URL
	REST_URL url("GET /version\n\n");
	ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		
		if(success) {
			std::ostringstream oss;
			
			if(reply.count(JSON_ARG_VERSION) ) {
				string ver = reply[string(JSON_ARG_VERSION)];
				oss << ver << ", ";
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


static bool WelcomeCmdHandler( stringvector line,
										CmdLineMgr* mgr,
										boolCallback_t	cb){
	
	std::ostringstream oss;
	
	// add friendly info here
	oss << "Welcome to Insteon Manager: ";
	mgr->sendReply(oss.str());

	VersionCmdHandler( {"version"}, mgr, cb);
	return true;
}


static bool DATECmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;
	TCPClientInfo cInfo = mgr->getClientInfo();
	
	REST_URL url("GET /date\n\n");
	ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
	
		std::ostringstream oss;

		string str;
		long  gmtOffset;
		long 	midnight;
		long 	uptime;
	
		string tz;
		double dd, dd1;

		const char *kTimeFormat = "%r";
		ServerCmdArgValidator v1;

		v1.getStringFromJSON("timeZone", reply, tz);
		v1.getLongIntFromJSON("midnight", reply, midnight);
		v1.getLongIntFromJSON("gmtOffset", reply, gmtOffset);
		v1.getLongIntFromJSON("uptime", reply, uptime);
	
		time_t lastMidnight = midnight  - gmtOffset;
	
		if(v1.getStringFromJSON(JSON_ARG_DATE, reply, str)){
			using namespace timestamp;
			
			time_t tt =  TimeStamp(str).getTime();
			
			oss << setw(10) << "TIME: " << setw(0) <<  TimeStamp(tt).ClockString(false);
			if(!tz.empty()) oss << " " << tz;
			oss << "\n\r";
		}

		int offsetInt = int(gmtOffset/ (60*60));
		oss << setw(10) << "OFFSET: " << setw(0) ;
		if(gmtOffset > 0){
			oss << "GMT +" +  to_string(offsetInt) ;
		}
		else {
			oss << "GMT " + to_string(offsetInt) ;
		}
		oss << "\n\r";

		double latitude, longitude;
 
		if(v1.getDoubleFromJSON(JSON_ARG_LATITUDE, reply, latitude)
			&& v1.getDoubleFromJSON(JSON_ARG_LONGITUDE, reply, longitude)){
			oss << setw(10) << "LAT/LONG: " << setw(0) << latitude << ", " << longitude << "\n\r";
 		}

		if(v1.getDoubleFromJSON("sunRise", reply, dd)
			&& v1.getDoubleFromJSON("civilSunRise", reply, dd1)){
	
			char timeStr[80] = {0};
			time_t time = 0;
			struct tm timeinfo = {0};
			time = lastMidnight + (dd1 * 60);
			localtime_r(&time, &timeinfo);
			::strftime(timeStr, sizeof(timeStr), kTimeFormat, &timeinfo );
			
			oss << setw(10) << "SUNRISE: "  << setw(0);
			oss << string(timeStr) << " - ";
			
			time = lastMidnight + (dd * 60);
			localtime_r(&time, &timeinfo);
			::strftime(timeStr, sizeof(timeStr), kTimeFormat, &timeinfo );
			oss << string(timeStr) <<  "\n\r";
		}
	
		if(v1.getDoubleFromJSON("sunSet", reply, dd)
			&& v1.getDoubleFromJSON("civilSunSet", reply, dd1)){
			
			char timeStr[80] = {0};
			time_t time = 0;
			struct tm timeinfo = {0};
			
			time = lastMidnight + (dd * 60);
			localtime_r(&time, &timeinfo);
			::strftime(timeStr, sizeof(timeStr), kTimeFormat, &timeinfo );
			
			oss << setw(10) << "SUNSET: "  << setw(0);
			oss << string(timeStr) << " - ";
			
			time = lastMidnight + (dd1 * 60);
			localtime_r(&time, &timeinfo);
			::strftime(timeStr, sizeof(timeStr), kTimeFormat, &timeinfo );
			oss << string(timeStr) <<  "\n\r";
		}
		
		if(uptime){
			char timeStr[80] = {0};
			tm tm;
			breakDuration(uptime, tm);
	 
			if(tm.tm_mday > 0){
				
				sprintf(timeStr, "%d %s, %01d:%02d:%02d" ,
								  tm.tm_mday, (tm.tm_mday>1?"Days":"Day"),
								  tm.tm_hour, tm.tm_min, tm.tm_sec);
			}
			else {
				sprintf(timeStr, "%01d:%02d:%02d" ,
								  tm.tm_hour, tm.tm_min, tm.tm_sec);
	 		}

			oss << setw(10) << "UPTIME: " << setw(0) << timeStr <<  "\n\r";;
 		}
		
  
		oss << "\r\n";
		mgr->sendReply(oss.str());

		(cb) (code > 199 && code < 400);
	});
	
	//cmdQueue->queue
	
	return true;
};

static bool StatusCmdHandler( stringvector line,
									  CmdLineMgr* mgr,
									  boolCallback_t	cb){
	using namespace rest;
	TCPClientInfo cInfo = mgr->getClientInfo();
	//
	//	for (auto& t : cInfo.headers()){
	//		printf("%s = %s\n", t.first.c_str(), t.second.c_str());
	//	}
	
	REST_URL url("GET /status\n\n");
	ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		
		if(success) {
			std::ostringstream oss;

			ServerCmdArgValidator v1;

			if(reply.count(JSON_ARG_STATESTR) ) {
				string status = reply[string(JSON_ARG_STATESTR)];
				oss << status;
			}
			
			double cpuTemp;
			if (v1.getDoubleFromJSON(JSON_ARG_CPU_TEMP ,reply, cpuTemp)){
				oss << "\r\nCPU Temp: " << cpuTemp << "°C" ;
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
	bool showDetails = false;
	bool showALDB = false;
	
	REST_URL url;
	TCPClientInfo cInfo = mgr->getClientInfo();
	
	string subcommand;
	
	if(line.size() < 2){
		url.setURL("GET /devices?details=1\n\n");
	}
	else {
		subcommand = line[1];
		
		if(subcommand == JSON_VAL_ALL) {
			url.setURL("GET /devices?details=1\n\n");
		}
		else 	if(subcommand == JSON_VAL_VALID) {
			url.setURL("GET /devices?valid=1&details=1\n\n");
		}
		else 	if(subcommand == JSON_VAL_DETAILS) {
			url.setURL("GET /devices?details=1\n\n");
			showDetails = true;
		}
		else 	if(subcommand == JSON_VAL_ALDB) {
			url.setURL("GET /devices?details=1&aldb=1\n\n");
			showALDB = true;
			showDetails = true;
		}
	}
	
	if(url.isValid()) {
		
		getCurrentPLMInfo(mgr, [=](DeviceID plmDeviceID, DeviceInfo plmDeviceInfo, bool hasPLM){
			
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
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
						//					oss << "\r\n";
						mgr->sendReply(oss.str());
					}
					else if( reply.contains(key2)
							  && reply.at(key2).is_object()){
						auto entries = reply.at(key2);
						
						for (auto& [key, value] : entries.items()) {
							
							DeviceID deviceID = DeviceID(key);
							
							string strDeviceInfo = value[string(JSON_ARG_DEVICEINFO)];
							DeviceInfo deviceInfo = DeviceInfo(strDeviceInfo);
							
							oss << deviceID.string();
							oss << " ";
							
							if(value[string(JSON_VAL_VALID)])
								oss << " ";
							else
								oss << "v";
							
							oss << " ";
							
							if( value.contains(string(JSON_ARG_NAME))
								&& value.at(string(JSON_ARG_NAME)).is_string()){
								string deviceName  = value.at(string(JSON_ARG_NAME));
								
								oss << setiosflags(ios::left);
								if(showDetails){
									constexpr size_t maxlen = 20;
									size_t len = deviceName.size();
									if(len<maxlen){
										oss << setw(maxlen);
										oss << deviceName;
									}
									else {
										oss << deviceName.substr(0,maxlen -1);
										oss << "…";
									}
									oss << " ";
									oss << setw(9);
									oss << deviceInfo.skuString();
									oss << setw(0);
									oss << " ";
									oss << deviceInfo.descriptionString();
								}
								else {
									oss << deviceName;
								}
								oss << resetiosflags(ios::left);
								
							};
							
							oss << "\r\n";
							
							if( value.contains(JSON_ARG_ALDB)
								&& value.at(string(JSON_ARG_ALDB)).is_object()){
								auto entries = value.at(string(JSON_ARG_ALDB));
								
								for (auto& [aldbKey, aldbEntry] : entries.items()) {
									
									DeviceIDArgValidator vDeviceID;
									ServerCmdArgValidator v1;
									
									DeviceID 	 deviceID;
									uint8_t	flag;
									uint8_t	group;
									
									string str;
									
									if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, aldbEntry, str))
										deviceID = DeviceID(str);
									
									v1.getHexByteFromJSON(JSON_VAL_ALDB_GROUP, aldbEntry, group);
									v1.getHexByteFromJSON(JSON_VAL_ALDB_FLAG, aldbEntry, flag);
									
									//	Bit 6: 1 = Controller (Master) of Device ID, 0 = Responder to (Slave of) Device ID
										bool isRESP = (flag & 0x40) == 0x00;
				
									if(hasPLM  && plmDeviceID == deviceID)
										oss << " -> " ;
									else
										oss << "    ";
									
									oss <<  aldbKey  << ": ";
									oss << deviceID.string() << " ";
									
									oss << (isRESP?"[":" ") << to_hex<uint8_t>(group) <<  (isRESP?"]":" ");
		 							oss <<  "\r\n";
								}
								oss << "\r\n";
							}
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
		});
		
		return true;
	}
	else {
		errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
	}
	
	
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
}


static bool SETCmdHandler( stringvector 		line,
								  CmdLineMgr* 		mgr,
								  boolCallback_t 	cb){
	
	using namespace rest;
	string errorStr;
	string command = line[0];
	string deviceIDStr;
	string onlevel;
	
	uint8_t dimLevel = 0;
	
	REST_URL url;
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceIDStr = line[1];
	
	if(command == "turn-on") {
		onlevel = "on";
	} else if(command == "turn-off") {
		onlevel = "off";
	} else if(command == "dim" && 	line.size() > 2){
		onlevel = line[2];
	}
	
	if(	deviceIDStr.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(command == "dim" && onlevel.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects level.";
	}
	else if(!v1.validateArg(deviceIDStr)){
		errorStr =  "\x1B[36;1;4m"  + deviceIDStr + "\x1B[0m is not a valid DeviceID.";
	}
	else if(!InsteonDevice::stringToLevel(string(onlevel), &dimLevel)){
		errorStr =  "\x1B[36;1;4m"  + onlevel + "\x1B[0m is not a valid device level.";
	}
	else {
		
		DeviceID deviceID = DeviceID(deviceIDStr);
		
		std::ostringstream oss;
		oss << "PUT /devices/" << deviceID.string() << " HTTP/1.1\n";
		oss << "Content-Type: application/json; charset=utf-8\n";
		oss << "Connection: close\n";
		
		json request;
		request[string(JSON_ARG_LEVEL)] =  dimLevel;
		
		string jsonStr = request.dump(4);
		oss << "Content-Length: " << jsonStr.size() << "\n\n";
		oss << jsonStr << "\n";
		
		url.setURL(oss.str());
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
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

static bool SHOWcmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;
	
	string errorStr;
	string command = line[0];
	string deviceIDStr;
	
	REST_URL url;
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceIDStr = line[1];
	
	if(	deviceIDStr.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(!v1.validateArg(deviceIDStr)){
		errorStr =  "\x1B[36;1;4m"  + deviceIDStr + "\x1B[0m is not a valid DeviceID.";
	}
	else {
		
		DeviceID deviceID = DeviceID(deviceIDStr);
		
		std::ostringstream oss;
		oss << "GET /devices/" << deviceID.string() << " HTTP/1.1\n";
		oss << "Connection: close\n";
		oss << "\n";
		url.setURL(oss.str());
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			
			if(success) {
				std::ostringstream oss;
				
				DeviceIDArgValidator vDeviceID;
				ServerCmdArgValidator v1;
				DeviceLevelArgValidator vDevLevel;
				
				DeviceID 	 deviceID;
				DeviceInfo deviceInfo;
				int 		 onLevel;
				long 		 eTag;
				bool 		 valid;
				string		 deviceName;
				
				string str;
				
				if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str))
					deviceID = DeviceID(str);
				
				if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
					deviceInfo = DeviceInfo(str);
				
				if(v1.getStringFromJSON("lastUpdated", reply, str)){
					// figure out time
				}
				
				vDevLevel.getvalueFromJSON(JSON_ARG_LEVEL, reply, onLevel);
				v1.getLongIntFromJSON(JSON_ARG_ETAG, reply, eTag);
				v1.getBoolFromJSON(JSON_VAL_VALID, reply, valid);
				v1.getStringFromJSON(JSON_ARG_NAME, reply, deviceName);
	 
				oss << deviceID.string();
				if(!valid) oss << " (v)";
				oss << " " << InsteonDevice::onLevelString(onLevel);
 				oss << " \"" << deviceName << "\"";
				oss << "  " << deviceInfo.skuString();
				oss << " " << deviceInfo.descriptionString();
				oss << "\n\r";
			 
			 
				if( reply.contains(string(JSON_ARG_PROPERTIES))
					&& reply.at(string(JSON_ARG_PROPERTIES)).is_object()){
					auto  props  = reply.at(string(JSON_ARG_PROPERTIES));
					
					for (auto it = props.begin(); it != props.end(); ++it) {
						oss << " " << setiosflags(ios::right) <<  setw(10) << it.key()  << ": "
						<< setiosflags(ios::left) <<  setw(0) << it.value() << "\n\r";
					}
				}
				
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


static bool BeepCmdHandler( stringvector line,
									CmdLineMgr* mgr,
									boolCallback_t	cb){
	using namespace rest;
	
	string errorStr;
	string command = line[0];
	string deviceIDStr;
	string onlevel;
	
	DeviceIDArgValidator v1;
	REST_URL url;
	
	if(	line.size() > 1)
		deviceIDStr = line[1];
	
	if(	deviceIDStr.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(!v1.validateArg(deviceIDStr)){
		errorStr =  "\x1B[36;1;4m"  + deviceIDStr + "\x1B[0m is not a valid DeviceID.";
	}
	else {
		
		DeviceID deviceID = DeviceID(deviceIDStr);
		
		std::ostringstream oss;
		oss << "PUT /devices/" << deviceID.string() << " HTTP/1.1\n";
		oss << "Content-Type: application/json; charset=utf-8\n";
		oss << "Connection: close\n";
		
		json request;
		request[string(JSON_ARG_DEVICEID)] =  line[1];
		request[string(JSON_ARG_BEEP)] = true;
		
		string jsonStr = request.dump(4);
		oss << "Content-Length: " << jsonStr.size() << "\n\n";
		oss << jsonStr << "\n";
		url.setURL(oss.str());
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
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

static bool BackLightCmdHandler( stringvector 		line,
										  CmdLineMgr* 		mgr,
										  boolCallback_t 	cb){
	
	using namespace rest;
	string errorStr;
	string command = line[0];
	string deviceIDStr;
	string onlevel;
	
	uint8_t dimLevel = 0;
	
	REST_URL url;
	DeviceIDArgValidator v1;
	
	if(	line.size() > 1)
		deviceIDStr = line[1];
	
	if(	line.size() > 2)
		onlevel = line[2];
	
	if(	deviceIDStr.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects deviceID.";
	}
	else if(command == "backlight" && onlevel.empty()) {
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects backlight level.";
	}
	else if(!v1.validateArg(deviceIDStr)){
		errorStr =  "\x1B[36;1;4m"  + deviceIDStr + "\x1B[0m is not a valid DeviceID.";
	}
	else if(!InsteonDevice::stringToBackLightLevel(string(onlevel), &dimLevel)){
		errorStr =  "\x1B[36;1;4m"  + onlevel + "\x1B[0m is not a valid backlight level.";
	}
	else {
		
		DeviceID deviceID = DeviceID(deviceIDStr);
		
		std::ostringstream oss;
		oss << "PUT /devices/" << deviceID.string() << " HTTP/1.1\n";
		oss << "Content-Type: application/json; charset=utf-8\n";
		oss << "Connection: close\n";
		
		json request;
		request[string(JSON_ARG_BACKLIGHT)] =  dimLevel;
		
		string jsonStr = request.dump(4);
		oss << "Content-Length: " << jsonStr.size() << "\n\n";
		oss << jsonStr << "\n";
		
		url.setURL(oss.str());
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
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


static bool PLMCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
								  boolCallback_t	cb){
	using namespace rest;
	string errorStr;
	string command = line[0];
	
	REST_URL url;
	std::ostringstream oss;

	if(line.size() < 2){
		errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects a function.";
	}
	else {
		string subcommand = line[1];
		TCPClientInfo cInfo = mgr->getClientInfo();
		
		if(subcommand == "info"){
			oss << "GET /plm/info" << " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
		
	 		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
							
				bool success = didSucceed(reply);
				
				if(success) {
					std::ostringstream oss;
					DeviceIDArgValidator vDeviceID;
					ServerCmdArgValidator v1;
		
					string path;
					int 	deviceCount = 0;
					string str;
					bool  autostart = false;
					bool  remoteTelnet = false;
			
					if(reply.count(JSON_ARG_STATESTR) ) {
						string status = reply[string(JSON_ARG_STATESTR)];
						oss  << setw(11) << "STATE: " <<  setw(0) << status << "\n\r";
					}
					
					if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str)){
						DeviceID 	 deviceID;
						DeviceInfo deviceInfo;
							
						deviceID = DeviceID(str);

						if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
							deviceInfo = DeviceInfo(str);
		
						oss <<  setw(11)  << "PLM: " <<  setw(0)
						<< "<"   << deviceID.string() << "> " << deviceInfo.skuString()
						<<  " " << deviceInfo.descriptionString();
		
						if(v1.getIntFromJSON(JSON_ARG_COUNT, reply, deviceCount))
							oss << ", " << deviceCount  << " devices.";
						oss << "\n\r";
					}
					
					if(vDeviceID.getvalueFromJSON(JSON_ARG_PLMID, reply, str)){
						DeviceID 	 plmID = DeviceID(str);
						oss  << setw(11) << "PLM ID: " <<  setw(0) << plmID.string() << "\n\r";
					}
					
					if(v1.getStringFromJSON(JSON_ARG_FILEPATH, reply, str))
						path = str;

					if(!path.empty()) {
						oss  << setw(11) << "PORT: " <<  setw(0) << path << "\n\r";
					}
					
					if(v1.getBoolFromJSON(JSON_ARG_AUTOSTART, reply, autostart)){
						oss  << setw(11) << "STARTUP: " <<  setw(0) << (autostart?"Auto":"Off") << "\n\r";
					}
		
					if(v1.getBoolFromJSON(JSON_ARG_REMOTETELNET, reply, remoteTelnet)){
						oss  << setw(11) << "REMOTETELNET: " <<  setw(0) << (remoteTelnet?"On":"Off") << "\n\r";
					}
						
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
		else if(subcommand == "start"){
			
			oss << "PUT /plm/state"  << " HTTP/1.1\n";
			oss << "Content-Type: application/json; charset=utf-8\n";
			oss << "Connection: close\n";
			
			json request;
			request[string(JSON_ARG_STATE)] =  JSON_VAL_START;
			
			string jsonStr = request.dump(4);
			oss << "Content-Length: " << jsonStr.size() << "\n\n";
			oss << jsonStr << "\n";
			url.setURL(oss.str());
			
			bool running = true;
			
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=,&running] (json reply, httpStatusCodes_t code) {
				
				std::ostringstream oss;
				DeviceIDArgValidator vDeviceID;
				ServerCmdArgValidator v1;

				bool success = didSucceed(reply);
				
				running = false;
				mgr->sendReply("\n\r");
	
				if(success){
					
					DeviceID 	 deviceID;
					DeviceInfo deviceInfo;
					string path;
					int 	deviceCount = 0;
					string str;
					
					if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str))
						deviceID = DeviceID(str);
					
					if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
						deviceInfo = DeviceInfo(str);
					
					if(v1.getStringFromJSON(JSON_ARG_FILEPATH, reply, str))
						path = str;
					
					oss << "PLM started.\n\r";
					
					if(reply.count(JSON_ARG_STATESTR) ) {
						string status = reply[string(JSON_ARG_STATESTR)];
						oss  << setw(10) << "STATE: " <<  setw(0) << status << "\n\r";
					}
					
					oss <<  setw(10)  << "PLM: " <<  setw(0)
					<< "<"   << deviceID.string() << "> " << deviceInfo.skuString()
					<<  " " << deviceInfo.descriptionString();
					
					if(v1.getIntFromJSON(JSON_ARG_COUNT, reply, deviceCount))
						oss << ", " << deviceCount  << " devices.";
					oss << "\n\r";
					
					
					if(!path.empty()) {
						oss  << setw(10) << "PORT: " <<  setw(0) << path << "\n\r";
					}
					
					mgr->sendReply(oss.str());
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				(cb) (success);
			});
			
			while (running){
				mgr->sendReply(".");
				sleep_ms(500);
			}

			
			return true;

		}
		else if(subcommand == "stop"){
			
			oss << "PUT /plm/state"  << " HTTP/1.1\n";
			oss << "Content-Type: application/json; charset=utf-8\n";
			oss << "Connection: close\n";
			
			json request;
			request[string(JSON_ARG_STATE)] =  JSON_VAL_STOP;
			
			string jsonStr = request.dump(4);
			oss << "Content-Length: " << jsonStr.size() << "\n\n";
			oss << jsonStr << "\n";
			url.setURL(oss.str());
			
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
				
				std::ostringstream oss;
				bool success = didSucceed(reply);
				
				if(success){
					mgr->sendReply("OK\n\r");
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				(cb) (success);
			});
			
			return true;
		}
		else if(subcommand == "backup"){
			
			if(line.size() < 3){
				errorStr =  "\x1B[36;1;4m"  + command + " " + subcommand + "\x1B[0m expects a valid filepath";
			}
			else {
				oss << "PUT /plm/database"  << " HTTP/1.1\n";
				oss << "Content-Type: application/json; charset=utf-8\n";
				oss << "Connection: close\n";

		 		json request;
				request[string(JSON_ARG_SAVE)] =  line[2];
					
				string jsonStr = request.dump(4);
				oss << "Content-Length: " << jsonStr.size() << "\n\n";
				oss << jsonStr << "\n";
				url.setURL(oss.str());

				ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
					
					std::ostringstream oss;
					bool success = didSucceed(reply);
					
					if(success){
						mgr->sendReply("OK\n\r");
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
		else if(subcommand == "path"){
			
			if(line.size() < 3){
				errorStr =  "\x1B[36;1;4m"  + command + " " + subcommand + "\x1B[0m expects a valid filepath";
			}
			else {
				
				string path = line[2];
				
				if( access(path.c_str(), W_OK | R_OK) != 0) {
				
					std::ostringstream oss;
					oss << "  Error: " << to_string(errno)
						<< " " << string(::strerror(errno)) << "\n\r";
					mgr->sendReply(oss.str());
					
					(cb) (false);
					return false;
			}
	 
				oss << "PATCH /plm/port"  << " HTTP/1.1\n";
				oss << "Content-Type: application/json; charset=utf-8\n";
				oss << "Connection: close\n";
				
				json request;
				request[string(JSON_ARG_FILEPATH)] =  line[2];
				
				string jsonStr = request.dump(4);
				oss << "Content-Length: " << jsonStr.size() << "\n\n";
				oss << jsonStr << "\n";
				url.setURL(oss.str());
				
				ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
					
					std::ostringstream oss;
					bool success = didSucceed(reply);
					
					if(success){
						mgr->sendReply("OK\n\r");
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

		else if(subcommand == "erase"){
			
			DeviceIDArgValidator v1;
			string plmPath;
			
			if(line.size() > 2){
				plmPath = line[2];
			}
			
			mgr->sendReply("  ERASE PLM Y/N ?");
			mgr->waitForChar({'Y', 'N', 'y', 'n'}, [=]( bool success, char ch) {
				
				REST_URL url;
				
				if(!success){
					return;
				}
				
				if(tolower(ch) != 'y') {
					mgr->sendReply("... Aborted\n\r");
					(cb) (false);
					return;
				}
				
				std::ostringstream oss;
				mgr->sendReply("\n\r   Start PLM Erase ...");

				oss << "PUT /plm/state"  << " HTTP/1.1\n";
				oss << "Content-Type: application/json; charset=utf-8\n";
				oss << "Connection: close\n";
				
				json request;
				request[string(JSON_ARG_STATE)] =  JSON_VAL_ERASE;

				if(!plmPath.empty()){
					request[string(JSON_ARG_FILEPATH)] =  plmPath;
				}
				
				string jsonStr = request.dump(4);
				oss << "Content-Length: " << jsonStr.size() << "\n\n";
				oss << jsonStr << "\n";
				url.setURL(oss.str());
				
				TCPClientInfo cInfo = mgr->getClientInfo();
				
				ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
					
					bool success = didSucceed(reply);
		
					if(success) {
						std::ostringstream oss;
						
						ServerCmdArgValidator 	v1;
						DeviceIDArgValidator 	vDeviceID;
						string str;
						
						oss << "OK\n\r";
						if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str)){
							DeviceID 	 deviceID;
							DeviceInfo deviceInfo;
							
							deviceID = DeviceID(str);
							
							if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
								deviceInfo = DeviceInfo(str);
							
							oss  << "   PLM: "
							<< "<"   << deviceID.string() << "> " << deviceInfo.skuString()
							<<  " " << deviceInfo.descriptionString();
							oss << "\n\r";
						}
						mgr->sendReply( oss.str());
					}
					else {
						string error = errorMessage(reply);
						mgr->sendReply( "Failed!\n\r   " + error + "\n\r");
					}
					
					(cb) (success);
				});
			});
			return true;
		}

		else if(subcommand == "export"){
			
			DeviceIDArgValidator v1;
			DeviceID plmID;
 
			if(line.size() > 2){
	 			plmID = DeviceID(line[2]);
			}
			
			oss << "PUT /plm/export"  << " HTTP/1.1\n";
			oss << "Content-Type: application/json; charset=utf-8\n";
			oss << "Connection: close\n";
			
			json request;
			if(plmID.isntNULL()){
				request[string(JSON_ARG_PLMID)] =  plmID.string();
			}
					
			string jsonStr = request.dump(4);
			oss << "Content-Length: " << jsonStr.size() << "\n\n";
			oss << jsonStr << "\n";
			url.setURL(oss.str());
			
			TCPClientInfo cInfo = mgr->getClientInfo();
			
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
				
				bool success = didSucceed(reply);
				
				if(success) {
					
					std::ostringstream oss;

					ServerCmdArgValidator 	v1;
					DeviceIDArgValidator 	vDeviceID;
					string str;
 
					vector <plmDevicesEntry_t> plmEntries;
					plmEntries.clear();
					DeviceID 	 plmID;
	
					if(vDeviceID.getvalueFromJSON(JSON_ARG_PLMID, reply, str)){
							plmID = DeviceID(str);
					}

					string key2 = string(JSON_ARG_DETAILS);
					string key3 = string(JSON_ARG_ALDB);
					
					if( reply.contains(key2)
						&& reply.at(key2).is_object()){
						auto entries = reply.at(key2);
						
						for (auto& [_, value] : entries.items()) {
							
							DeviceID 	 	deviceID;
							uint8_t		cntlID;
							string			name;
							string 		str;
							
							
							if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, value, str))
								deviceID = DeviceID(str);
							
							v1.getStringFromJSON(JSON_ARG_NAME, value, name);
							v1.getHexByteFromJSON(JSON_ARG_GROUPID, value, cntlID);
							
							plmDevicesEntry_t plmEntry;
							plmEntry.responders.clear();
							
							plmEntry.deviceID = deviceID;
							plmEntry.cntlID = cntlID;
							plmEntry.name =  name;
							
							if( value.contains(key3)
								&& value.at(key3).is_array()){
								auto aldbEntries = value.at(key3);
								
								for( auto aldbEntry : aldbEntries) {
									uint8_t		aldbGroup;
									bool 			aldbFlag;
									
									v1.getHexByteFromJSON(JSON_VAL_ALDB_GROUP, aldbEntry, aldbGroup);
									v1.getBoolFromJSON(JSON_VAL_ALDB_FLAG, aldbEntry, aldbFlag);
									plmEntry.responders.push_back( make_pair(aldbGroup,aldbFlag));
									
								}
							}
							plmEntries.push_back(plmEntry);
						}
					}
					
					if(plmID.isntNULL()){
						oss  << setw(11) << "PLM Entries for  " <<  setw(0) << plmID.string() << "\n\r";
					}
					else {
						oss << "PLM Entries\n\r";
					}
					
					for(auto entry:plmEntries) {
						oss <<  " " << entry.deviceID.string() << " "
						<< to_hex<unsigned char>(entry.cntlID) ;
						
						for(auto res:entry.responders){
							uint8_t group = res.first;
							bool isCNTL = res.second;
							
							if(isCNTL)
								oss << "\t[" << to_hex<unsigned char>(group) << "]" ;
							else
								oss << "\t" << to_hex<unsigned char>(group);
						}
						oss << "\t" << entry.name;
						oss << "\n\r";
					}
					
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
		else if(subcommand == "import"){
			
			REST_URL url;

			string  		  filePath;
			std::ifstream	ifs;

			vector<plmDevicesEntry_t> entries;
			
			entries.clear();
			
			if(	line.size() > 2)
				filePath = line[2];
			
			if(filePath.empty()) {
				errorStr =  "Command: \x1B[36;1;4m"  + command + " "  + subcommand + "\x1B[0m expects a file path.";
				mgr->sendReply(errorStr + "\n\r");
				return false;
			}
			
			try{
				ifs.open(filePath, ios::in);
				
				if(!ifs.is_open()) {
					errorStr =  "Could not open file: \x1B[36;1;4m"  + filePath + "\x1B[0m";
					mgr->sendReply(errorStr + "\n\r");
					return false;
				}
				
				string line;
					while ( std::getline(ifs, line) ) {
					
					vector<string> v = split<string>(line, "\t");
					if(v.size() < 2) continue;
					
					plmDevicesEntry_t entry;
					entry.responders.clear();
					
					entry.deviceID = DeviceID(v[0]);
					if(entry.deviceID.isNULL()) continue;
					if( sscanf(v[1].c_str(), "%hhx", &entry.cntlID) != 1) continue;
					
					for(int i = 2; i < v.size(); i++) {
						uint8_t grp;
						auto str = v[i];
						
						if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))
							&& ( std::sscanf(str.c_str(), "%hhd", &grp) == 1)){
							entry.responders.push_back(make_pair(grp,true));
						}
						else if( regex_match(string(str), std::regex("^\\[[A-Fa-f0-9]{2}\\]$"))
								  && ( std::sscanf(str.c_str(), "[%hhd]", &grp) == 1)){
							entry.responders.push_back(make_pair(grp,false));
						}
						else if(i == (v.size()-1) && !v[i].empty())
						{
							entry.name = v[i];
						}
					}
					entries.push_back(entry);
				}
				
				if(entries.size() == 0){
					errorStr =  "No entries found in file: \x1B[36;1;4m"  + filePath + "\x1B[0m";
					mgr->sendReply(errorStr + "\n\r");
					return false;
				}
				
				json devicesEntries;
				
				for(auto entry: entries){
					
					json devicesEntry;
					devicesEntry[string(JSON_ARG_DEVICEID)] = entry.deviceID.string();
					devicesEntry[string(JSON_ARG_GROUPID)] =  to_hex <unsigned char>(entry.cntlID);
					devicesEntry[string(JSON_ARG_NAME)] = entry.name;

					json aldbDevicesEntries;
					for( auto aldbEntry : entry.responders){
						json aldbDevicesEntry;
						aldbDevicesEntry[string(JSON_VAL_ALDB_FLAG)] =   aldbEntry.second;
						aldbDevicesEntry[string(JSON_VAL_ALDB_GROUP)] =  to_hex <unsigned char>(aldbEntry.first);
						aldbDevicesEntries.push_back(aldbDevicesEntry);
					}
					
					devicesEntry[string(JSON_VAL_ALDB)] =  aldbDevicesEntries;
					
					string strDeviceID = entry.deviceID.string();
					devicesEntries[strDeviceID] = devicesEntry;
				}
				
				json request;
				std::ostringstream oss;
				
				oss << "PUT /plm/import"  << " HTTP/1.1\n";
				oss << "Content-Type: application/json; charset=utf-8\n";
				oss << "Connection: close\n";
				
				request[string(JSON_ARG_STATE)] =  JSON_VAL_IMPORT;
				request[string(JSON_ARG_DETAILS)] = devicesEntries;
				
				string jsonStr = request.dump(4);
				oss << "Content-Length: " << jsonStr.size() << "\n\n";
				oss << jsonStr << "\n";
				url.setURL(oss.str());
				
				TCPClientInfo cInfo = mgr->getClientInfo();
				
				ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
					
					bool success = didSucceed(reply);
					
					if(success) {
					
						std::ostringstream oss;
						
						string key1 = string(JSON_ARG_DEVICEIDS);
	 
						if( reply.contains(key1)
							&& reply.at(key1).is_array()){
							auto deviceIDs = reply.at(key1);
							
							if(deviceIDs.size() > 0) {
								oss <<  "  Failed to link devices: ";
								for(auto strDeviceID :deviceIDs) {
									DeviceID deviceID = DeviceID(strDeviceID);
									oss << deviceID.string();
									oss << " ";
								}
									oss << "\n\r";
							}
						}
						
						oss <<  "OK\n\r" ;
						mgr->sendReply(oss.str());
					}
					else {
						string error = errorMessage(reply);
						mgr->sendReply( "Failed!\n\r   " + error + "\n\r");
					}
					(cb) (success);
				});
				
				return true;
			}
			catch(std::ifstream::failure &err) {
				errorStr =  err.what();
				mgr->sendReply(errorStr + "\n\r");
				
				return false;
			}
			
			
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}
	mgr->sendReply(errorStr + "\n\r");
	return false;
}

static bool GroupCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
									 boolCallback_t	cb){

	using namespace rest;

	string errorStr;
	string command = line[0];
	REST_URL url;
	TCPClientInfo cInfo = mgr->getClientInfo();

	bool isQuery = false;
	
	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];
		uint16_t groupID = 0;

		if (subcommand == "list") {
			
			std::ostringstream oss;
			oss << "GET /groups " <<  " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
			isQuery = true;
		}
		else  if( regex_match(string(subcommand), std::regex("^[A-Fa-f0-9]{1,4}$"))
					&& (std::sscanf(subcommand.c_str(), "%hx", &groupID) == 1)){
			
			if(line.size() > 2){
				string onlevel = line[2];
				uint8_t dimLevel = 0;

				if(!InsteonDevice::stringToLevel(string(onlevel), &dimLevel)){
					errorStr =  "\x1B[36;1;4m"  + onlevel + "\x1B[0m is not a valid device level.";
				}
				else {
					std::ostringstream oss;
					oss << "PUT /groups/" << to_hex<unsigned short>(groupID) << " HTTP/1.1\n";
					oss << "Content-Type: application/json; charset=utf-8\n";
					oss << "Connection: close\n";
					
					json request;
					request[string(JSON_ARG_LEVEL)] =  dimLevel;
					
					string jsonStr = request.dump(4);
					oss << "Content-Length: " << jsonStr.size() << "\n\n";
					oss << jsonStr << "\n";
					url.setURL(oss.str());
					isQuery = false;

				}
	
			} else {
				
				std::ostringstream oss;
				oss << "GET /groups/" << to_hex<unsigned short>(groupID) <<  " HTTP/1.1\n";
				oss << "Connection: close\n";
				oss << "\n";
				url.setURL(oss.str());
				isQuery = true;
			}
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}
 
	if(url.isValid()) {
		if(!isQuery){
			
			// not a query - just do command
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
				
				bool success = didSucceed(reply);
				if(success) {
					mgr->sendReply( "OK\n\r" );
				}
				else {
					string error = errorMessage(reply);
					mgr->sendReply( error + "\n\r");
				}
				
			});
		}
		else {
 			// its a query so get a list of device names to make the display more useful
			getDeviceNames(mgr, [=](map<DeviceID, string> deviceMap){
					ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
					
					bool success = didSucceed(reply);
					if(success) {

						std::ostringstream oss;
						string key1 = string(JSON_ARG_GROUPIDS);
						
						if( reply.contains(key1)
							&& reply.at(key1).is_object()){
							auto entries = reply.at(key1);
							
							for (auto& [key, value] : entries.items()) {
								
								GroupID groupID =  GroupID(key);
								auto deviceIDS = value[string(JSON_ARG_DEVICEIDS)];
								
								oss  << " " << groupID.string() << " ";
								oss  << std::dec << std::setfill (' ')<<  std::setw(3) <<  deviceIDS.size() <<  " ";
								
								string groupName = value[string(JSON_ARG_NAME)];
								oss << "\"" << groupName << "\""  << "\r\n";
								
								if(deviceIDS.size() > 0){
									for(auto devString : deviceIDS){
										DeviceID deviceID = DeviceID(devString);
										oss << "   " << deviceID.string();
									
										if(deviceMap.count(deviceID)) {
											string name = deviceMap.at(deviceID);
											oss << " " << name;
										}
										oss << "\r\n";
									}
								}
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

			});

		}
		return true;
	}
  
	mgr->sendReply(errorStr + "\n\r");
	(cb)(false);
	return false;
}


static bool InsteonGroupCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
											  boolCallback_t	cb){
	
	using namespace rest;
	
	string errorStr;
	string command = line[0];
	REST_URL url;
	
	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];
		
		if ((subcommand == "off") || (subcommand == "on")) {
			std::ostringstream oss;
			oss << "PUT /insteon.groups/" << "FF " << " HTTP/1.1\n";
			oss << "Content-Type: application/json; charset=utf-8\n";
			oss << "Connection: close\n";
			
			json request;
			request[string(JSON_ARG_LEVEL)] =  (subcommand == "on" ?255:0);
			
			string jsonStr = request.dump(4);
			oss << "Content-Length: " << jsonStr.size() << "\n\n";
			oss << jsonStr << "\n";
			url.setURL(oss.str());
		}
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}
	
	if(url.isValid()) {
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
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
}


static bool DumpCmdHandler( stringvector line,
										CmdLineMgr* mgr,
										boolCallback_t	cb){
	using namespace rest;
	TCPClientInfo cInfo = mgr->getClientInfo();
	REST_URL url;
 
	url.setURL("GET /devices/dump HTTP/1.1\n\n");
	
	if(	line.size() > 1){
		string subcommand = line[1];
		if(subcommand == JSON_VAL_ALL) {
			url.setURL("GET /devices/dump?details=1 HTTP/1.1\n\n");
		}
	}

	// simulate URL
	ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
		
		bool success = didSucceed(reply);
		
		if(success) {
			std::ostringstream oss;
			
			if(reply.count(JSON_ARG_DUMP) ) {
				string dump = reply[string(JSON_ARG_DUMP)];
				dump = replaceAll(dump, "\n","\r\n" );
				oss << dump;
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

static bool ActionCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
									 boolCallback_t	cb){

	using namespace rest;

	string errorStr;
	REST_URL url;
	
	string command = line[0];
	string arg1;
	string subcommand;
	
	if(	line.size() > 1)
		subcommand = line[1];
	
	if(	line.size() > 2)
		arg1 = line[2];
 

	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		
		if (subcommand == "list") {
			std::ostringstream oss;
			oss << "GET /action.groups " <<  " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
		}
		else if (subcommand == "details") {
			if(	arg1.empty()) {
				errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects actionID.";
			}
			else {
				std::ostringstream oss;
				oss << "GET /action.groups/" <<  arg1 << " HTTP/1.1\n";
				oss << "Connection: close\n";
				oss << "\n";
				url.setURL(oss.str());
			}
		}
		else if (subcommand == "run") {
			std::ostringstream oss;
			oss << "PUT /action.groups/run.actions/" <<  arg1 << " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
		}
		
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}
 
	if(url.isValid()) {
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			if(success) {
				
				std::ostringstream oss;
				
				string key1 = string(JSON_ARG_GROUPIDS);
				string key2 = string(JSON_ARG_ACTIONS);
				
				if( reply.contains(key1)
					&& reply.at(key1).is_object()){
					auto entries = reply.at(key1);
					
					for (auto& [key, value] : entries.items()) {
						
						ActionGroup actionGroupID = ActionGroup(key);
						
						oss  << " " << actionGroupID.string() << " ";
						string groupName = value[string(JSON_ARG_NAME)];
						oss << "\"" << groupName << "\""  << "\r\n";
					}
					
					mgr->sendReply(oss.str());
				}
				else if( reply.contains(key2)
						  && reply.at(key2).is_object()){
					auto entries = reply.at(key2);
					
					for (auto& [key, value] : entries.items()) {
						
						oss  << " " << key << " ";
						if(value.is_object()){
							Action act = Action(value);
							oss  <<  act.printString();
						}
						oss << "\r\n";
						
					}
					mgr->sendReply(oss.str());
				}
				else {
					
					if(subcommand == "run"){
						mgr->sendReply( "OK\n\r" );
					}
					else {
						// huh?
						auto dump = reply.dump(4);
						dump = replaceAll(dump, "\n","\r\n" );
						cout << dump << "\n";
						mgr->sendReply( "OK\n\r" );
					}
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
}

static bool EventsCmdHandler( stringvector line,
								  CmdLineMgr* mgr,
									 boolCallback_t	cb){

	using namespace rest;

	string errorStr;
	REST_URL url;
	
	string command = line[0];
	string arg1;
	string subcommand;
	
	if(	line.size() > 1)
		subcommand = line[1];
	
	if(	line.size() > 2)
		arg1 = line[2];
 

	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		
		if (subcommand == "list") {
			std::ostringstream oss;
			oss << "GET /events" <<  " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
		}
		else if (subcommand == "details") {
			if(	arg1.empty()) {
				errorStr =  "Command: \x1B[36;1;4m"  + command + "\x1B[0m expects actionID.";
			}
			else {
				std::ostringstream oss;
				oss << "GET /events/" <<  arg1 << " HTTP/1.1\n";
				oss << "Connection: close\n";
				oss << "\n";
				url.setURL(oss.str());
			}
		}
		else if (subcommand == "run") {
			std::ostringstream oss;
			oss << "PUT /events/run.actions/" <<  arg1 << " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
		}
		
		else {
			errorStr =  "Command: \x1B[36;1;4m"  + subcommand + "\x1B[0m is an invalid function for " + command;
		}
	}
 
	if(url.isValid()) {
		
		TCPClientInfo cInfo = mgr->getClientInfo();
		ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
			
			bool success = didSucceed(reply);
			if(success) {
				
				std::ostringstream oss;
				
				string key1 = string(JSON_ARG_EVENTIDS);
				string keyTimed = string(JSON_ARG_TIMED_EVENTS);
				string keyAction = string(JSON_ARG_ACTION);
				string keyTrigger= string(JSON_ARG_TRIGGER);
				string keyfuture = string(JSON_ARG_FUTURE_EVENTS	);
	 
	 
				if( reply.contains(key1)
					&& reply.at(key1).is_object()){
					auto entries = reply.at(key1);
			
					vector<string> timedEvents;
					vector<string> futureEvents;
					
					if( reply.contains(keyTimed)){
						auto ent = reply.at(keyTimed);
						for (auto& [_, val] : ent.items()) {
							timedEvents.push_back( val[0]);
	 					}
					}
		 
					if( reply.contains(keyfuture)
						&& reply.at(keyfuture).is_array()){
						auto futureIDS = reply.at(keyfuture);
						for(string eventID :futureIDS) {
							futureEvents.push_back(eventID);
						}
					}
					
					for(auto key : timedEvents){
						
						if(entries.count(key)){
							
							json value = entries.at(key);
							Action act;
							EventTrigger trig;
							
							string eventName = value[string(JSON_ARG_NAME)];

							if( value.contains(keyAction)
								&& value.at(keyAction).is_object()){
								act = Action(value.at(keyAction));
							}
							
							if( value.contains(keyTrigger)
								&& value.at(keyTrigger).is_object()){
								trig = EventTrigger(value.at(keyTrigger));
							}
							
							oss  << " " << key << " " ;
							// print event name
							oss << setiosflags(ios::left);
							constexpr size_t maxlen = 20;
								size_t len = eventName.size();
								if(len<maxlen){
									oss << setw(maxlen);
									oss << eventName;
								}
								else {
									oss << eventName.substr(0,maxlen -1);
									oss << "…";
								}
			
							oss << " ";
							
							
							if(find(futureEvents.begin(), futureEvents.end(), key) != futureEvents.end())
								oss << "* ";
							else
								oss << "  ";
						 
							oss   << setw(32) << trig.printString() <<  setw(0) <<  " ";
							oss << act.printString() <<  " ";
							oss << "\n\r";
							
						}
					}
				 
					/*for (auto& [key, value] : entries.items()) {
						
						Action act;
						EventTrigger trig;
			
						string key2 = string(JSON_ARG_ACTION);
						string key3 = string(JSON_ARG_TRIGGER);
						string eventName = value[string(JSON_ARG_NAME)];
		
						if( value.contains(key2)
							&& value.at(key2).is_object()){
							act = Action(value.at(key2));
						}
						
						if( value.contains(key3)
							&& value.at(key3).is_object()){
							trig = EventTrigger(value.at(key3));
						}
		
						oss  << " " << key << " " ;
						// print event name
						oss << setiosflags(ios::left);
						constexpr size_t maxlen = 20;
							size_t len = eventName.size();
							if(len<maxlen){
								oss << setw(maxlen);
								oss << eventName;
							}
							else {
								oss << eventName.substr(0,maxlen -1);
								oss << "…";
							}
		
						oss << " ";
						if(act.isValid() && trig.isValid()){
							oss   << trig.printString() <<  " ";
	//						oss  << act.printString() <<  " ";
						}
					}
		*/
					mgr->sendReply(oss.str());
				}
				else {
					Action act;
					EventTrigger trig;
					string eventName;
					string eventID;
					
					string key1 = string(JSON_ARG_NAME);
					string key2 = string(JSON_ARG_ACTION);
					string key3 = string(JSON_ARG_TRIGGER);
					string key4 = string(JSON_ARG_EVENTID);
					
					if( reply.contains(key1)
						&& reply.at(key1).is_string())
						eventName =  reply.at(key1);
					
					if( reply.contains(key4)
						&& reply.at(key4).is_string())
						eventID =  reply.at(key4);
					
					if( reply.contains(key2)
						&& reply.at(key2).is_object()){
						act = Action(reply.at(key2));
					}
					
					if( reply.contains(key3)
						&& reply.at(key3).is_object()){
						trig = EventTrigger(reply.at(key3));
					}
		 
					if(act.isValid() && trig.isValid()){
						oss  << " " << eventID << " " << "\"" << eventName << "\""  << "\r\n";
						oss  << " trigger: " << trig.printString() <<  "\r\n";
						oss  << " action: " << act.printString() <<  "\r\n";
					}
					else if(subcommand == "run"){
						mgr->sendReply( "OK\n\r" );
					}
					else {
						// huh?
						auto dump = reply.dump(4);
						dump = replaceAll(dump, "\n","\r\n" );
						cout << dump << "\n";
						mgr->sendReply( "OK\n\r" );
					}
					
					mgr->sendReply(oss.str());
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
}



// MARK: -  register commands

void registerCommandsLineFunctions() {
	
	// register command line commands
	auto cmlR = CmdLineRegistry::shared();
	
	cmlR->registerCommand(CmdLineRegistry::CMD_WELCOME ,	WelcomeCmdHandler);
	
	cmlR->registerCommand("version",	VersionCmdHandler);
	cmlR->registerCommand("date",		DATECmdHandler);
	cmlR->registerCommand("list",		ListCmdHandler);
	cmlR->registerCommand("status",		StatusCmdHandler);
	cmlR->registerCommand("turn-on",  	SETCmdHandler);
	cmlR->registerCommand("turn-off",  SETCmdHandler);
	cmlR->registerCommand("dim",  		SETCmdHandler);
	cmlR->registerCommand("show",  		SHOWcmdHandler);
	cmlR->registerCommand("beep",		BeepCmdHandler);
	cmlR->registerCommand("set-backlight",	BackLightCmdHandler);
	cmlR->registerCommand("plm",  		PLMCmdHandler);
	cmlR->registerCommand("group",		GroupCmdHandler);
	cmlR->registerCommand("all",			InsteonGroupCmdHandler);
	cmlR->registerCommand("dump",		 DumpCmdHandler);
	cmlR->registerCommand("action",		 ActionCmdHandler);
	cmlR->registerCommand("events",		 EventsCmdHandler);
 
	CmdLineHelp::shared()->setHelpFile("helpfile.txt");
}
 
