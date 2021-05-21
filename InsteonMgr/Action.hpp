//
//  ActionMgr.hpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/17/21.
//

#ifndef Action_hpp
#define Action_hpp

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>

#include "json.hpp"

#include "InsteonMgrDefs.hpp"
#include "DeviceID.hpp"
#include "InsteonDevice.hpp"
#include "Utils.hpp"

using namespace std;

typedef  unsigned short actionGroupID_t;
typedef  unsigned short actionID_t;

bool str_to_ActionID(const char* str, actionID_t *actionIDOut = NULL);

class Action {
	friend class InsteonDB;
 
public:
					// these shouldnt change, 		they become persistant
	typedef enum  {
		ACTION_INALID 					= 0,
		ACTION_NONE						= 1,
		ACTION_SET_LEVEL				= 2,
		ACTION_SET_LED_BRIGHTNESS  = 3,
		ACTION_BEEP						= 4,
		ACTION_EXECUTE					= 5,	// execute another action group--

	}actionCmd_t;
	
	// these shouldnt change, 		they become persistant
	typedef enum  {
		ACTION_TYPE_UNKNOWN 		= 0,
		ACTION_TYPE_DEVICE		= 1,
		ACTION_TYPE_GROUP			= 2,
		ACTION_TYPE_DEVICEGROUP	= 3,
		ACTION_TYPE_ACTIONGROUP	= 4,
	
	}actionType_t;

	constexpr static const string_view JSON_ACTIONID 	= "actionID";
	
	constexpr static string_view JSON_DEVICEID 			= "deviceID";
	constexpr static string_view JSON_GROUPID 			= "groupID";
	constexpr static string_view JSON_INSTEON_GROUPS		= "insteon.groups";
	constexpr static string_view JSON_ACTION_GROUP		= "action.group";

	constexpr static string_view JSON_ACTION_LEVEL 		= "level";
	
	constexpr static string_view JSON_ACTION_CMD			= "cmd";
	constexpr static string_view JSON_CMD_SET				= "set";
	constexpr static string_view JSON_CMD_BACKLIGHT		= "backlight";
	constexpr static string_view JSON_CMD_BEEP			= "beep";
	constexpr static string_view JSON_CMD_NONE			= "none";
	constexpr static string_view JSON_CMD_EXECUTE			= "execute";

	Action();
	Action(DeviceID deviceID, actionCmd_t cmd, uint8_t level = 0);
	Action(GroupID groupID, actionCmd_t cmd, uint8_t level = 0);
	Action(uint8_t deviceGroupID, uint8_t level);
	Action(actionGroupID_t actionGroup);
	Action(nlohmann::json j);
	Action(std::string);

	Action( const Action &actIn){
		_cmd = actIn._cmd;
		_level =  actIn._level;
		_actionID = actIn._actionID;
		_actionType = actIn._actionType;

		switch (actIn._actionType) {
			case ACTION_TYPE_DEVICE:
				_deviceID = actIn._deviceID;
				break;

			case ACTION_TYPE_GROUP:
				_groupID = actIn._groupID;
				break;

			case ACTION_TYPE_DEVICEGROUP:
				_deviceGroupID = actIn._deviceGroupID;
				break;

			case ACTION_TYPE_ACTIONGROUP:
				_ActionGroupID = actIn._ActionGroupID;
				break;

			default:
				break;
		}
	}

	inline void operator = (const Action &right ) {
		_cmd = right._cmd;
		_level =  right._level;
		_actionID = right._actionID;
		_actionType = right._actionType;

		switch (right._actionType) {
			case ACTION_TYPE_DEVICE:
				_deviceID = right._deviceID;
				break;

			case ACTION_TYPE_GROUP:
				_groupID = right._groupID;
				break;

			case ACTION_TYPE_DEVICEGROUP:
				_deviceGroupID = right._deviceGroupID;
				break;

			case ACTION_TYPE_ACTIONGROUP:
				_ActionGroupID = right._ActionGroupID;
				break;

			default:
				break;
		}
	}
	
	std::string idString() const;
	std::string printString() const;
	
	const nlohmann::json JSON();
	
	bool isValid() {return (_actionType != ACTION_TYPE_UNKNOWN);};
 
	const actionID_t 	actionID(){return _actionID;};
	const actionType_t actionType(){return _actionType;};
	const uint8_t 		level(){return _level;};
	const actionCmd_t 	cmd(){return _cmd;};

	const DeviceID 	deviceID(){return 	_deviceID;};
	const GroupID 	groupID(){return 	_groupID;};
	const uint8_t 	deviceGroupID(){return 	_deviceGroupID;};

private:
	void initWithJSON(nlohmann::json j);
	
protected:
 	actionType_t _actionType;
	actionCmd_t 	_cmd;
	uint8_t 		_level;
	actionID_t	_actionID;
	
	union{
		DeviceID 				_deviceID;
		GroupID 				_groupID;
		uint8_t  				_deviceGroupID;
		actionGroupID_t  	_ActionGroupID;
	};
};


bool str_to_ActionGroupID(const char* str, actionGroupID_t *actionGroupIDOut = NULL);

class ActionGroup{

public:
	
	ActionGroup() { _rawGroupID = 0;};

 	ActionGroup(actionGroupID_t sid) { _rawGroupID = sid;};

	ActionGroup( const char* str) {
		str_to_ActionGroupID(str, &_rawGroupID);
	};

	ActionGroup( std::string str) {
		str_to_ActionGroupID(str.c_str(), &_rawGroupID);
	};

	inline void copyToActionGroupID(actionGroupID_t * toScene ){
		if(toScene) *toScene = _rawGroupID;
	}

	std::string string() const;

	actionGroupID_t groupID() { return _rawGroupID;};
	
	bool isValid() {
		return ( _rawGroupID != 0);
	}

	bool isEqual(ActionGroup a) {
		return a._rawGroupID  == _rawGroupID ;
	}

	bool isEqual(actionGroupID_t rawSceneID) {
		return rawSceneID  == _rawGroupID ;
	}

	inline bool operator==(const ActionGroup& right) const {
		return right._rawGroupID  == _rawGroupID;
		}

	inline bool operator!=(const ActionGroup& right) const {
		return right._rawGroupID  != _rawGroupID;
	}

	inline void operator = (const ActionGroup &right ) {
		_rawGroupID = right._rawGroupID;
	}
 
private:
	actionGroupID_t _rawGroupID;
};

#endif /* ActionMgr_hpp */
