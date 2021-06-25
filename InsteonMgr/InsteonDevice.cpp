//
//  InsteonDevice.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/15/21.
//

#include <regex>
#include <ctype.h>
#include <iostream>

#include "InsteonDevice.hpp"
#include "DeviceID.hpp"
#include "InsteonCmdQueue.hpp"
#include "InsteonALDB.hpp"
#include "InsteonDB.hpp"
#include "Utils.hpp"


#include "LogMgr.hpp"

#define SETUP_CMDQUEUE  \
	auto cmdQueue = InsteonCmdQueue::shared();  \
	if(!cmdQueue->isConnected()) return false;
 
 // MARK: - level string conversion

std::string InsteonDevice::onLevelString(uint8_t onLevel){
	std::string str = "";
	
	if(onLevel == 0)
		str = "off";
	else if(onLevel == 255)
		str = "on";
	else
	{
	  int level = (onLevel / 255.) * 100;
		str = std::to_string (level) + "%";
  }
	return str;
}

std::string InsteonDevice::backLightLevelString(uint8_t onLevel){
	std::string str = "";
	
	if(onLevel == 0)
		str = "off";
	else if(onLevel == 127)
		str = "on";
	else
	{
	  int level = (onLevel / 127) * 100;
		str = std::to_string (level) + "%";
  }
	return str;
}


bool InsteonDevice::jsonToLevel( nlohmann::json j, uint8_t* levelOut){
	
	uint8_t levelVal = 0;
	bool isValid = false;
	
	if( j.is_string()){
		string str = j;
		if(InsteonDevice::stringToLevel(str, &levelVal)){
			isValid = true;
		}
	}
	else if( j.is_number()){
		int num = j;
		if(num >= 0 && num < 256){
			levelVal = num;
			isValid = true;
		}
	}
	else if( j.is_boolean()){
		bool val = j;
		levelVal= val?255:0;
		isValid = true;
	}
	
	if(levelOut)
		*levelOut = levelVal;

	return isValid;
}

bool InsteonDevice::jsonToBackLightLevel( nlohmann::json j, uint8_t* levelOut){
	
	uint8_t levelVal = 0;
	bool isValid = false;
	
	if( j.is_string()){
		string str = j;
		if(InsteonDevice::stringToBackLightLevel(str, &levelVal)){
			isValid = true;
		}
	}
	else if( j.is_number()){
		int num = j;
		if(num >= 0 && num < 127){
			levelVal = num;
			isValid = true;
		}
	}
	else if( j.is_boolean()){
		bool val = j;
		levelVal= val?127:0;
		isValid = true;
	}
	
	if(levelOut)
		*levelOut = levelVal;

	return isValid;
}

bool InsteonDevice::stringToLevel(const std::string str, uint8_t* levelOut){
	bool valid = false;
	
	if(!empty(str)){
		uint8_t dimLevel = 0;
 		const char * param1 = str.c_str();
		int intValue = atoi(param1);
		
		// check for % value
		if(std::regex_match(param1, std::regex("^(?:100|[1-9]?[0-9])?%$"))){
			dimLevel =  255 * (intValue / 100.);
			valid = true;
		}
		// check for level
		else if(std::regex_match(param1, std::regex("^\\b([01]?[0-9][0-9]?|2[0-4][0-9]|25[0-5])$"))){
			dimLevel = intValue;
			valid = true;
		}
		else {
			if(caseInSensStringCompare(str,"off")) {
				dimLevel = 0;
				valid = true;
			}
			if(caseInSensStringCompare(str,"on")) {
				dimLevel = 255;
				valid = true;
			}
		}
		
		if(valid && levelOut)
			*levelOut = dimLevel;
	}
	return valid;
}
 
bool InsteonDevice::stringToBackLightLevel(std::string str, uint8_t* levelOut){
	bool valid = false;
	
	if(!empty(str)){
		uint8_t dimLevel = 0;
		const char * param1 = str.c_str();
		int intValue = atoi(param1);
		
		// check for % value
		if(std::regex_match(param1, std::regex("^(?:100|[1-9]?[0-9])?%$"))){
			dimLevel =  127 * (intValue / 100.);
			valid = true;
		}
		// check for level
		else if(std::regex_match(param1, std::regex("\\b([0-9]|[1-9][0-9]|1[01][0-9]|12[0-7])\\b"))){
			dimLevel = intValue;
			valid = true;
		}
		else {
			if(caseInSensStringCompare(str,"off")) {
				dimLevel = 0;
				valid = true;
			}
			else if(caseInSensStringCompare(str,"on")) {
				dimLevel = 255;
				valid = true;
			}
		}
		
		if(valid && levelOut)
			*levelOut = dimLevel;
	}
	return valid;
}
 

// MARK: - InsteonDeviceGroup

InsteonDeviceGroup::InsteonDeviceGroup(uint8_t groupID){
	_groupID = groupID;
}

InsteonDeviceGroup::~InsteonDeviceGroup(){
}


bool InsteonDeviceGroup::setOnLevel(uint8_t level,
												boolCallback_t callback){
	
	try{
		SETUP_CMDQUEUE;
		
		uint8_t cmd = (level == 0)
		? InsteonParser::CMD_FAST_OFF
		:InsteonParser::CMD_FAST_ON;
		
		cmdQueue->queueMessageToGroup(cmd, level, _groupID,
												[=](auto arg, bool didSucceed) {
			if(callback) {
				callback(didSucceed);
			}
		});
		
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}
}

// MARK: - InsteonDevice

InsteonDevice::InsteonDevice(DeviceID deviceID){
	_deviceID = deviceID;
}

InsteonDevice::~InsteonDevice(){
}


bool InsteonDevice::getOnLevel(std::function<void(uint8_t level, bool didSucceed)> callback) {
	
	try{
		SETUP_CMDQUEUE;
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_GET_ON_LEVEL, 0x00,
									  NULL, 0,
									  [=]( auto arg, bool didSucceed) {
			
			if(callback) {
				callback(arg.reply.cmd[1], didSucceed);
			}
		});
		
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
};



bool InsteonDevice::setOnLevel(uint8_t level,
										 boolCallback_t callback){
	
	try {
		
		SETUP_CMDQUEUE;
		
		uint8_t cmd = (level == 0)
		? InsteonParser::CMD_LIGHT_OFF
		:InsteonParser::CMD_LIGHT_ON;
		
		cmdQueue->queueMessage(_deviceID,
									  cmd, level,
									  NULL, 0,
									  [=]( auto arg, bool didSucceed) {
			if(callback) {
				callback(didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK);
			}
		});
		
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

bool InsteonDevice::beep(boolCallback_t callback){
	
	try{
		SETUP_CMDQUEUE;
		
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_BEEP, 0,
									  NULL, 0,
									  [this,callback]( auto arg, bool didSucceed) {
			if(callback) {
				callback(didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK);
			}
		});
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}


bool InsteonDevice::setLEDBrightness(uint8_t level, boolCallback_t cb){
	
	try{
		SETUP_CMDQUEUE;
		
		// pin brightnes -- its an 7 bit value
		if(level > 127)
			level = 127;
		
		uint8_t buffer[] = {
			0x00, 0x07, level};
		
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_SET_LED_BRIGHTNESS, 0x00,
									  buffer, sizeof(buffer),
									  [=]( auto arg, bool didSucceed) {
			
			if(cb) {
				cb(didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK);
			}
		});
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

bool InsteonDevice::getEngineVersion(std::function<void(uint8_t version, bool didSucceed)> callback){
	
	try{
		SETUP_CMDQUEUE;
		
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_GET_INSTEON_VERSION, 0x00,
									  NULL, 0,
									  [=]( auto arg, bool didSucceed) {
			
			if(callback) {
				callback(arg.reply.cmd[1], didSucceed);
			}
		});
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

// MARK: - InsteonKeypadDevice

bool InsteonKeypadDevice::setNonToggleMask(uint8_t mask, boolCallback_t callback){
	
	try{
		SETUP_CMDQUEUE;
		
		uint8_t buffer[] = {
			0x01, 0x08, mask, 0, 0,0, };
		
		cmdQueue->queueMessage(_deviceID,
									  0x2E, 0x00,
									  buffer, sizeof(buffer),
									  [=]( auto arg, bool didSucceed) {
			
			if(callback) {
				callback(didSucceed);
			}
		});
		
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}
}


bool InsteonKeypadDevice::getKeypadLEDState(std::function<void(uint8_t mask, bool didSucceed)> cb){
	
	try{
		SETUP_CMDQUEUE;
		
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_GET_ON_LEVEL, 0x01,
									  NULL, 0,
									  [=]( auto arg, bool didSucceed) {
			
			if(cb) {
				cb(arg.reply.cmd[1], didSucceed);
			}
			
		});
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
};


bool InsteonKeypadDevice::setKeypadLEDState(uint8_t mask, boolCallback_t cb){
	
	try{
		SETUP_CMDQUEUE;
		
		uint8_t buffer[] = {
			0x01, 0x09, mask, 0, 0,0, };
		
		cmdQueue->queueMessage(_deviceID,
									  0x2E, 0x00,
									  buffer, sizeof(buffer),
									  [=]( auto arg, bool didSucceed) {
			
			if(cb) {
				cb(didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK);
			}
		});
		
		return true;
		
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

bool InsteonKeypadDevice::setKeypadLED(uint8_t button, bool turnOn, boolCallback_t callback){
	
	try{
		SETUP_CMDQUEUE;
		
		if(button == 1){
			//		this->setOnLevel(turnOn?0xFF:0);
			
			//		uint8_t buffer[] = {
			//			0x01, 0x08, 0x00, 0, 0,0, };
			//
			//
			//		cmdQueue->queueMessage(_deviceID,
			//										0x2E, 0x00,
			//										buffer, sizeof(buffer),
			//										[this]( auto arg, bool didSucceed) {
			//		});
			
			
		}
		else
		{
			uint8_t buffer[] = {
				0x2, 0x00};
			
			cmdQueue->queueMessage(_deviceID,
										  0x2E, 0x00,
										  buffer, sizeof(buffer),
										  [=]( auto arg, bool didSucceed) {
				
				//			uint8_t* p =  arg.reply.data;
				
				if(callback) {
					callback(didSucceed);
				}
			});
		}
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}

}

bool InsteonKeypadDevice::setKeyButtonMode(bool eightKey, boolCallback_t callback){
	
	try{
		SETUP_CMDQUEUE;
		
		uint8_t buffer[] = {
			0x00, 0x00};
		
		cmdQueue->queueMessage(_deviceID,
									  0x20, eightKey?0x06:0x07,
									  buffer, sizeof(buffer),
									  [=]( auto arg, bool didSucceed) {
			
			if(callback) {
				callback(didSucceed);
			}
		});
		
		return true;
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

bool InsteonKeypadDevice::test(){
	
	try{
		SETUP_CMDQUEUE;
		
		cmdQueue->queueMessage(_deviceID,
									  InsteonParser::CMD_GET_ON_LEVEL, 0x01,
									  NULL, 0,
									  [=]( auto arg, bool didSucceed) {
			
			
		});
		
		// 	uint8_t buffer[] = {
		//		01, 00};
		//
		//	cmdQueue->queueMessage(_deviceID,
		//								  0x2E, 0x00,
		//								  buffer, sizeof(buffer),
		//								  [=]( InsteonCmdQueue::msgReply_t arg, bool didSucceed) {
		//
		//		if(didSucceed){
		//
		//
		//			uint8_t* data  = &arg.reply.data[0];
		//
		//			for(size_t i = 0; i<14; i++) {
		//				cout << to_hex(data[i]) << " ";
		//			}
		//
		//			cout << "\n";
		//
		//		}
		//
		//	});
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}

//
//typedef struct {
//	InsteonALDB *aldb;
//	std::vector<std::pair<uint8_t,uint8_t>> buttonGroups;
//	boolCallback_t callback;
//	size_t index;
//	size_t fails;
//} linkKeyPadTaskData_t;


bool InsteonKeypadDevice::linkKeyPadButtonsToGroups(InsteonDB* db,
																	 InsteonALDB *aldb,
										 std::vector<std::pair<uint8_t,uint8_t>> buttonGroups,
																	 boolCallback_t callback ){
	try{
		SETUP_CMDQUEUE;
		
		LOG_INFO("Link %d KeyPad buttons to groups\n",buttonGroups.size());
		
		linkKeyPadTaskData_t* task  = (linkKeyPadTaskData_t*)
		malloc(sizeof(linkKeyPadTaskData_t) + sizeof(linkPairs_t) * buttonGroups.size());
		
		// setup the task block
		for(size_t i = 0; i < buttonGroups.size(); i++){
			auto pair =  buttonGroups[i];
			task->pairs[i].button = pair.first;
			task->pairs[i].group = pair.second;
		}
		
		task->count = buttonGroups.size();
		task->deviceID	= _deviceID;
		
		task->fails = 0;
		task->aldb = aldb;
		task->db = db;
		
		this->linkKeyPadButtonsToGroupsInternal(task,callback);
		
		return true;
		
	}
	catch ( const InsteonException& e)  {
		return false;
	}
	
}


void InsteonKeypadDevice::linkKeyPadButtonsToGroupsInternal(linkKeyPadTaskData_t *taskData,
																				boolCallback_t callback){
	if(taskData->count-- == 0) {
		
 		bool success = taskData->fails == 0;
		LOG_INFO("Link KeyPad buttons to groups done\n");

		free(taskData);
		if(callback){
			callback(success);
		}
		return;
	}
	
	linkPairs_t pair = taskData->pairs[taskData->count];
	uint8_t button = pair.button;
	uint8_t group =  pair.group;
 
	uint8_t data[] = {0XFF, 0, button};  // 0xFF = no clean-ups,  byte 2 N?C
 
	bool status = taskData->aldb->addToDeviceALDB(taskData->deviceID, false, group, data,
											  [=]( const insteon_aldb_t* newAldb,  bool didSucceed) {

		if(didSucceed){
			if(newAldb	!= NULL){
				taskData->db->addDeviceALDB(taskData->deviceID, *newAldb);
 				taskData->db->saveToCacheFile();
			}
		}
		else
		{
			taskData->fails++;
		}
 
		linkKeyPadButtonsToGroupsInternal(taskData,callback);
	});
	
	if(!status) {
		//  FAIL!!
		if(callback){
			callback(false);
		}

	}
	
	
	
}

#if 0

{
	/* extenal linking mode */

	uint8_t buffer[] = {
		0x00,0x00,0x00,0x00,};

	START_VERBOSE;
	_cmdQueue->queueMessage(keyPad,
									0x09, 0x01,
									buffer, sizeof(buffer),
									[this]( auto arg, bool didSucceed) {
		
		
	});

#endif


/* LED for keypad */
	
#if 0
	
				for(int i = 1; i < 9; i++ ){
	
					uint8_t mask = 1 << i;
	
					uint8_t buffer[] = {
						0x01, 0x09, mask};
	
					_cmdQueue->queueMessage(keyPad,
													0x2E, 0x00,
													buffer, sizeof(buffer),
													[this]( auto arg, bool didSucceed) {
	
	
						});
				}

#endif
