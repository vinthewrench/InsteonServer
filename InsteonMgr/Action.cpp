//
//  ActionMgr.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/17/21.
//

#include "Action.hpp"
#include <sstream>
#include <string>
#include <regex>

#include "json.hpp"
#include "Utils.hpp"

#include "InsteonParser.hpp"

using namespace nlohmann;
using namespace std;


bool str_to_ActionID(const char* str, actionID_t *actionIDOut){
	
	bool status = false;
	
	actionGroupID_t val = 0;
 
	status = sscanf(str, "%hx", &val) == 1;
	
	if(actionIDOut)  {
		*actionIDOut = val;
	}
	
	return status;
}


bool str_to_ActionGroupID(const char* str, actionGroupID_t *actionGroupIDOut){
	
	bool status = false;
	
	actionGroupID_t val = 0;
 
	status = sscanf(str, "%hx", &val) == 1;
	
	if(actionGroupIDOut)  {
		*actionGroupIDOut = val;
	}
	
	return status;
}

static std::string stringForCmd(Action::actionCmd_t cmd) {
	
	string str = "";
	
	switch(cmd) {
		case Action::ACTION_NONE:
			str = Action::JSON_CMD_NONE;
			break;
		case Action::ACTION_SET_LEVEL:
			str = Action::JSON_CMD_SET;
			break;
		case Action::ACTION_SET_LED_BRIGHTNESS:
			str = Action::JSON_CMD_BACKLIGHT;
			break;
		case Action::ACTION_BEEP:
			str = Action::JSON_CMD_BEEP;
			break;
		default:;
	}
	return  str;
}


std::string ActionGroup::string() const {
	return  to_hex<unsigned short>(_rawGroupID,false);
}

Action::Action(){
	_actionType = ACTION_TYPE_UNKNOWN;
	_cmd = ACTION_INALID;
}

Action::Action(DeviceID deviceID, actionCmd_t cmd, uint8_t level){
	_actionType = ACTION_TYPE_DEVICE;
	_deviceID = deviceID;
	_cmd = cmd;
	_level = level;
}

Action::Action(GroupID groupID, actionCmd_t cmd, uint8_t level){
	_actionType = ACTION_TYPE_GROUP;
	_groupID = groupID;
	_cmd = cmd;
	_level = level;
}

Action::Action(uint8_t deviceGroupID, uint8_t level){
	_actionType = ACTION_TYPE_DEVICEGROUP;
	_deviceGroupID = deviceGroupID;
	_cmd = ACTION_SET_LEVEL;
	_level = level;
}

Action::Action(actionGroupID_t actionGroup){
	_actionType = ACTION_TYPE_ACTIONGROUP;
	_ActionGroupID = actionGroup;
	_cmd = ACTION_EXECUTE;
}
 

void Action::initWithJSON(nlohmann::json j){
	_cmd = ACTION_NONE;
	_actionType = ACTION_TYPE_UNKNOWN;
	_level = 0;

	if( j.contains(string(JSON_ACTION_CMD))
		&& j.at(string(JSON_ACTION_CMD)).is_string()){
		string str = j.at(string(JSON_ACTION_CMD));
		
		if(str == JSON_CMD_SET){
			_cmd = ACTION_SET_LEVEL;
		}else 	if(str == JSON_CMD_BACKLIGHT){
			_cmd = ACTION_SET_LED_BRIGHTNESS	;
		}else 	if(str == JSON_CMD_BEEP){
			_cmd = ACTION_BEEP;
		}else 	if(str == JSON_CMD_EXECUTE){
			_cmd = ACTION_EXECUTE;
		}
	}

	if( j.contains(string(JSON_ACTION_LEVEL))){
		
		string  k  = string(JSON_ACTION_LEVEL);
		uint8_t levelVal = 0;
		if(	InsteonDevice::jsonToLevel(j.at(k), &levelVal)){
			_level = levelVal;
		}
	}

	if( j.contains(string(JSON_DEVICEID))
		&& j.at(string(JSON_DEVICEID)).is_string()){
		
		string str = j.at(string(JSON_DEVICEID));
		deviceID_t  rawDevID;
		if(str_to_deviceID(str.c_str(), rawDevID)) {
			_deviceID = DeviceID(rawDevID);
			_actionType = ACTION_TYPE_DEVICE;
		}
	}
	else if( j.contains(string(JSON_GROUPID))
			  && j.at(string(JSON_GROUPID)).is_string()){
		
		string str = j.at(string(JSON_GROUPID));
		groupID_t  rawGroupID;
		if(str_to_GroupID(str.c_str(), &rawGroupID)) {
			_groupID = GroupID(rawGroupID);
			_actionType = ACTION_TYPE_GROUP;
		}
	}
	else if( j.contains(string(JSON_INSTEON_GROUPS))
			  && j.at(string(JSON_INSTEON_GROUPS)).is_string()){
		string str = j.at(string(JSON_INSTEON_GROUPS));
 
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))){
			uint8_t groupID;
			if( std::sscanf(str.c_str(), "%hhx", &groupID) == 1){
				_deviceGroupID = groupID;
				_actionType = ACTION_TYPE_DEVICEGROUP;
			}
		}
	}
	else if( j.contains(string(JSON_ACTION_GROUP))
			  && j.at(string(JSON_ACTION_GROUP)).is_string()){
		string str = j.at(string(JSON_ACTION_GROUP));
 
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{4}$"))){
			actionGroupID_t actionGroupID;
			if( std::sscanf(str.c_str(), "%hx", &actionGroupID) == 1){
				_ActionGroupID = actionGroupID;
				_actionType = ACTION_TYPE_ACTIONGROUP;
			}
		}
	}

	if( j.contains(string(JSON_ACTIONID))
		&& j.at(string(JSON_ACTIONID)).is_string()){
		string str = j.at(string(JSON_ACTIONID));
		
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))){
			actionID_t actionID;
			if( std::sscanf(str.c_str(), "%hx", &actionID) == 1){
				_actionID = actionID;
			}
		}
		else
			_actionType = ACTION_TYPE_UNKNOWN;
		};

	if(_actionType == ACTION_TYPE_UNKNOWN)
		_cmd = ACTION_INALID;
}

Action::Action(json j) {
	initWithJSON(j);
}


Action::Action(std::string str){
	
	_cmd = ACTION_NONE;
	_actionType = ACTION_TYPE_UNKNOWN;
	_level = 0;

	json j;
	j  = json::parse(str);
	initWithJSON(j);
}



std::string Action::idString() const {
	return  to_hex<unsigned short>(_actionID,false);
}

nlohmann::json Action::JSON(){
	json j;

	switch (_actionType) {
		case ACTION_TYPE_DEVICE:
			j[string(JSON_DEVICEID)] = _deviceID.string();
			j[string(JSON_ACTION_CMD)] = stringForCmd(_cmd);
			
			if(_cmd == ACTION_SET_LEVEL || _cmd == ACTION_SET_LED_BRIGHTNESS)
				j[string(JSON_ACTION_LEVEL)] = _level;
			break;
			
		case ACTION_TYPE_GROUP:
			j[string(JSON_GROUPID)] = _groupID.string();
			j[string(JSON_ACTION_CMD)] = stringForCmd(_cmd);
			j[string(JSON_ACTION_LEVEL)] = _level;
			break;

		case ACTION_TYPE_DEVICEGROUP:
			j[string(JSON_INSTEON_GROUPS)] = to_hex<unsigned short>(_deviceGroupID,false);
			j[string(JSON_ACTION_CMD)] = JSON_CMD_SET;
			j[string(JSON_ACTION_LEVEL)] = _level;
			break;
			
		case ACTION_TYPE_ACTIONGROUP:
			j[string(JSON_ACTION_GROUP)] = to_hex<unsigned short>(_ActionGroupID,false);
			j[string(JSON_ACTION_CMD)] = JSON_CMD_EXECUTE;
			break;

		default:;
	}
	return j;
}
