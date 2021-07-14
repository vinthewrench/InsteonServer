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
		case Action::ACTION_EXECUTE:
			str = Action::JSON_CMD_EXECUTE;
			break;
		case Action::ACTION_ON:
			str = Action::JSON_CMD_ON;
			break;
		case Action::ACTION_OFF:
			str = Action::JSON_CMD_OFF;
			break;
		case Action::ACTION_SET_KEYPADMASK:
			str = Action::JSON_CMD_KEYPAD_MASK;
			break;


		default:;
	}
	return  str;
}


std::string ActionGroup::string() const {
	return  to_hex<unsigned short>(_rawGroupID);
}

Action::Action(){
	_actionType = ACTION_TYPE_UNKNOWN;
	_cmd = ACTION_INALID;
}

Action::Action(DeviceID deviceID, actionCmd_t cmd, uint8_t level){
	_actionType = ACTION_TYPE_DEVICE;
	_deviceID = deviceID;
	_cmd = cmd;
	_byteVal = level;
}

Action::Action(GroupID groupID, actionCmd_t cmd, uint8_t level){
	_actionType = ACTION_TYPE_GROUP;
	_groupID = groupID;
	_cmd = cmd;
	_byteVal = level;
}

Action::Action(uint8_t deviceGroupID, uint8_t level){
	_actionType = ACTION_TYPE_DEVICEGROUP;
	_deviceGroupID = deviceGroupID;
	_cmd = ACTION_SET_LEVEL;
	_byteVal = level;
}

Action::Action(actionGroupID_t actionGroup){
	_actionType = ACTION_TYPE_ACTIONGROUP;
	_ActionGroupID = actionGroup;
	_cmd = ACTION_EXECUTE;
}
 

void Action::initWithJSON(nlohmann::json j){
	_cmd = ACTION_NONE;
	_actionType = ACTION_TYPE_UNKNOWN;
	_byteVal = 0;

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
		}else 	if(str == JSON_CMD_ON){
			_cmd = ACTION_ON;
		}else 	if(str == JSON_CMD_OFF){
			_cmd = ACTION_OFF;
		}else 	if(str == JSON_CMD_KEYPAD_MASK){
			_cmd = ACTION_SET_KEYPADMASK;
		}
		
		if( j.contains(string(JSON_ACTION_LEVEL))){
			
			string  k  = string(JSON_ACTION_LEVEL);
			uint8_t levelVal = 0;
			
			switch (_cmd) {
				case ACTION_SET_LEVEL:
					if(	InsteonDevice::jsonToLevel(j.at(k), &levelVal)){
						_byteVal = levelVal;
					}
					break;
				case ACTION_SET_LED_BRIGHTNESS:
					if(	InsteonDevice::jsonToBackLightLevel(j.at(k), &levelVal)){
						_byteVal = levelVal;
					}
					break;
					
				default:
					break;
			}
		}
		
		//	if( j.contains(string(JSON_ACTION_KP_MASK))
		//		&& j.at(string(JSON_ACTION_KP_MASK)).is_string()){
		//
		//		string str = j.at(string(JSON_ACTION_KP_MASK));
		//		uint8_t mask = 0;
		//
		// 		if( regex_match( str, std::regex("^[A-Fa-f0-9]{2}$"))
		//			&& ( std::sscanf(str.c_str(), "%hhd", &mask) == 1)){
		//			_byteVal = mask;
		//		}
		//		else if( regex_match(str, std::regex("^0?[xX][0-9a-fA-F]{2}$"))
		//				  && ( std::sscanf(str.c_str(), "%hhx", &mask) == 1)){
		//			_byteVal = mask;
		//		}
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
		
		uint8_t groupID = 0;
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))
			&& ( std::sscanf(str.c_str(), "%hhd", &groupID) == 1)){
			_deviceGroupID = groupID;
			_actionType = ACTION_TYPE_DEVICEGROUP;
		}
		else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{2}$"))
				  && ( std::sscanf(str.c_str(), "%hhx", &groupID) == 1)){
			_deviceGroupID = groupID;
			_actionType = ACTION_TYPE_DEVICEGROUP;
		}
	}
	else if( j.contains(string(JSON_ACTION_GROUP))
			  && j.at(string(JSON_ACTION_GROUP)).is_string()){
		string str = j.at(string(JSON_ACTION_GROUP));
 
		actionGroupID_t actionGroupID = 0;
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{4}$"))
			&& ( std::sscanf(str.c_str(), "%hd", &actionGroupID) == 1)){
			_ActionGroupID = actionGroupID;
			_actionType = ACTION_TYPE_ACTIONGROUP;
		}
		else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{4}$"))
				  && ( std::sscanf(str.c_str(), "%hx", &actionGroupID) == 1)){
			_ActionGroupID = actionGroupID;
			_actionType = ACTION_TYPE_ACTIONGROUP;
		}
	}
	else if( j.contains(string(JSON_KEYPADID))
			&& j.at(string(JSON_KEYPADID)).is_string()){
		string str = j.at(string(JSON_KEYPADID));
		deviceID_t  rawDevID;
		if(str_to_deviceID(str.c_str(), rawDevID)) {
			_deviceID = DeviceID(rawDevID);
			_actionType = ACTION_TYPE_KEYPAD;
		}
	}

	if( j.contains(string(JSON_BUTTONID))
			&& j.at(string(JSON_BUTTONID)).is_string()){
			string str = j.at(string(JSON_BUTTONID));
			
			if( regex_match(string(str), std::regex("^[1-8]$"))){
				uint8_t buttonID;
				if( std::sscanf(str.c_str(), "%hhd", &buttonID) == 1){
					_byteVal = buttonID;
				}
			}
		}
	
	if( j.contains(string(JSON_ACTION_VALUE))
		&& j.at(string(JSON_ACTION_VALUE)).is_string()){
		string str = j.at(string(JSON_ACTION_VALUE));
		
		uint8_t value = 0;
		
		if( std::sscanf(str.c_str(), "%hhd", &value) == 1){
			_byteVal = value;
		}
		else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{2}$"))
				  && ( std::sscanf(str.c_str(), "%hhx", &value) == 1)){
			_byteVal = value;	}
	}

	if( j.contains(string(JSON_ACTIONID))
		&& j.at(string(JSON_ACTIONID)).is_string()){
		string str = j.at(string(JSON_ACTIONID));
		
		actionID_t actionID = 0;
		if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{4}$"))
			&& ( std::sscanf(str.c_str(), "%hd", &actionID) == 1)){
			_actionID = actionID;

		}
		else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{4}$"))
				  && ( std::sscanf(str.c_str(), "%hx", &actionID) == 1)){
			_actionID = actionID;
		}
		else
			_actionType = ACTION_TYPE_UNKNOWN;
	}
	
	
	
	if(_actionType == ACTION_TYPE_UNKNOWN)
		_cmd = ACTION_INALID;
}

Action::Action(json j) {
	initWithJSON(j);
}


Action::Action(std::string str){
	
	_cmd = ACTION_NONE;
	_actionType = ACTION_TYPE_UNKNOWN;
	_byteVal = 0;

	json j;
	j  = json::parse(str);
	initWithJSON(j);
}



std::string Action::idString() const {
	return  to_hex<unsigned short>(_actionID);
}

std::string Action::printString() const {
	std::ostringstream oss;

	switch (_actionType) {
		case ACTION_TYPE_DEVICE:
			
			oss <<  stringForCmd(_cmd)
				<< " <" << _deviceID.string()  << "> "
				<< InsteonDevice::onLevelString(_byteVal);
			
			break;
			
		case ACTION_TYPE_GROUP:
			oss <<  stringForCmd(_cmd)
				<< " Group: " << _groupID.string()  << "  "
				<< InsteonDevice::onLevelString(_byteVal);

			break;
			
		case ACTION_TYPE_DEVICEGROUP:
			oss <<  stringForCmd(_cmd)
				<< " Insteon.Group: " <<  to_hex<uint8_t>(_deviceGroupID, true)  << "  "
				<< InsteonDevice::onLevelString(_byteVal);

			break;

		case ACTION_TYPE_ACTIONGROUP:
			oss <<  stringForCmd(_cmd)
			<< " Action.Group: " <<  to_hex(_ActionGroupID, true);
			break;
	 
		case ACTION_TYPE_KEYPAD:
			
			switch (_cmd) {
				case ACTION_ON:
				case ACTION_OFF:
					oss <<  stringForCmd(_cmd)
					<< " <" << _deviceID.string()  << "> "
					<< "Button(" << to_string(_byteVal) << ")";
			break;
					
				case ACTION_SET_KEYPADMASK:
					oss <<  stringForCmd(_cmd)
					<< " <" << _deviceID.string()  << "> "
					<< "Mask(" <<  to_hex<uint8_t>(_byteVal, true) << ")";
				default:
					break;
			}

		default:
			oss <<  "Invalid";
	}
	
	return  oss.str();
}

const nlohmann::json Action::JSON(){
	json j;

	switch (_actionType) {
		case ACTION_TYPE_DEVICE:
			j[string(JSON_DEVICEID)] = _deviceID.string();
			j[string(JSON_ACTION_CMD)] = stringForCmd(_cmd);
			
			if(_cmd == ACTION_SET_LEVEL)
				j[string(JSON_ACTION_LEVEL)] = InsteonDevice::onLevelString(_byteVal);
			else if(_cmd == ACTION_SET_LED_BRIGHTNESS)
				j[string(JSON_ACTION_LEVEL)] = InsteonDevice::backLightLevelString(_byteVal);
 		break;
			
		case ACTION_TYPE_GROUP:
			j[string(JSON_GROUPID)] = _groupID.string();
			j[string(JSON_ACTION_CMD)] = stringForCmd(_cmd);
			j[string(JSON_ACTION_LEVEL)] = InsteonDevice::onLevelString(_byteVal);
			break;

		case ACTION_TYPE_DEVICEGROUP:
			j[string(JSON_INSTEON_GROUPS)] = to_hex<uint8_t>(_deviceGroupID, true);
			j[string(JSON_ACTION_CMD)] = JSON_CMD_SET;
			j[string(JSON_ACTION_LEVEL)] = InsteonDevice::onLevelString(_byteVal);
			break;
			
		case ACTION_TYPE_ACTIONGROUP:
			j[string(JSON_ACTION_GROUP)] = to_hex<unsigned short>(_ActionGroupID, true);
			j[string(JSON_ACTION_CMD)] = JSON_CMD_EXECUTE;
			break;

		case ACTION_TYPE_KEYPAD:
			j[string(JSON_KEYPADID)] = _deviceID.string();
			j[string(JSON_ACTION_CMD)] = stringForCmd(_cmd);

			switch (_cmd) {
				case ACTION_ON:
				case ACTION_OFF:
					j[string(JSON_BUTTONID)] = to_string(_byteVal);
 				break;
					
				case ACTION_SET_KEYPADMASK:
					j[string(JSON_ACTION_VALUE)] = to_hex<uint8_t>(_byteVal, true);
				default:
					break;
			}
			break;
	 
		default:;
	}
	return j;
}
