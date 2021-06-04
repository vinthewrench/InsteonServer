//
//  Event.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/23/21.
//

#include "Event.hpp"
#include "json.hpp"
#include <regex>
#include "TimeStamp.hpp"

using namespace nlohmann;


constexpr string_view JSON_ARG_ACTION			= "action";
constexpr string_view JSON_ARG_TRIGGER			= "trigger";
constexpr string_view JSON_ARG_EVENTID 		= "eventID";
constexpr string_view JSON_ARG_NAME				= "name";

constexpr string_view JSON_DEVICEID 			= "deviceID";
constexpr string_view JSON_INSTEON_GROUP 		= "insteon.group";
constexpr string_view JSON_CMD			 		= "cmd";

constexpr string_view JSON_TIME_BASE			= "timeBase";
constexpr string_view JSON_TIME_OFFSET		 	= "mins";

// MARK: - EventTrigger()

EventTrigger::EventTrigger(){
	_eventType  					= EVENT_TYPE_UNKNOWN;
}
 
EventTrigger::EventTrigger(uint8_t insteonGroup){
	_eventType  						= EVENT_TYPE_DEVICE;
	_deviceEvent.cmd				= 0;
	_deviceEvent.deviceID			= DeviceID();
	_deviceEvent.insteonGroup 	= insteonGroup;
	_deviceEvent.hasDeviceID 	= false;
	_deviceEvent.hasGroupID 		= true;
	_deviceEvent.hasCmd	 		= false;
}

EventTrigger::EventTrigger(DeviceID  deviceID){
	_eventType  					= EVENT_TYPE_DEVICE;
	_deviceEvent.cmd			= 0;
	_deviceEvent.deviceID		= deviceID;
	_deviceEvent.insteonGroup = 0;
	_deviceEvent.hasDeviceID = true;
	_deviceEvent.hasGroupID 	= false;
	_deviceEvent.hasCmd	 	= false;
}

//EventTrigger::EventTrigger(uint8_t 	cmd){
//	_eventType  					= EVENT_TYPE_DEVICE;
//	_deviceEvent.cmd			= cmd;
//	_deviceEvent.deviceID		=  DeviceID();
//	_deviceEvent.insteonGroup = 0
//}

EventTrigger::EventTrigger(DeviceID deviceID, uint8_t insteonGroup, uint8_t cmd ){
	_eventType  						= EVENT_TYPE_DEVICE;
	_deviceEvent.cmd				= cmd;
	_deviceEvent.deviceID			= deviceID;
	_deviceEvent.insteonGroup 	= insteonGroup;
	
	_deviceEvent.hasDeviceID 	= !deviceID.isNULL();
	_deviceEvent.hasGroupID 		= true;
	_deviceEvent.hasCmd	 		= cmd != 0;
}
 
EventTrigger::EventTrigger(tod_offset_t timeBase, int16_t timeOfDay){
	_eventType  					= EVENT_TYPE_TIME;
	_timeEvent.timeBase		= timeBase;
	_timeEvent.timeOfDay		= timeOfDay;
	_timeEvent.lastRun			= 0;
}


void EventTrigger::copy(const EventTrigger &evt1, EventTrigger *evt2){
		
	evt2->_eventType	 	= evt1._eventType;
	
	switch (evt1._eventType) {
		case EVENT_TYPE_DEVICE:
			evt2->_deviceEvent = evt1._deviceEvent;
			break;
	
		case EVENT_TYPE_TIME:
			evt2->_timeEvent = evt1._timeEvent;
				break;
	
		default:
			break;
	}
}

EventTrigger::EventTrigger(nlohmann::json j){
	initWithJSON(j);
}

EventTrigger::EventTrigger(std::string str){
	_eventType = EVENT_TYPE_UNKNOWN;
 
	json j;
	j  = json::parse(str);
	initWithJSON(j);
}


void EventTrigger::initWithJSON(nlohmann::json j){
	
	_eventType = EVENT_TYPE_UNKNOWN;

	if( j.contains(string(JSON_DEVICEID))
		|| j.contains(string(JSON_INSTEON_GROUP))
		|| j.contains(string(JSON_CMD))){
		
		_eventType = EVENT_TYPE_DEVICE;
	}
	else 	if( j.contains(string(JSON_TIME_OFFSET))
				&& j.contains(string(JSON_TIME_BASE))) {

		_eventType = EVENT_TYPE_TIME;
	}

	if(_eventType == EVENT_TYPE_DEVICE) {
		
		_deviceEvent.deviceID = DeviceID();
		_deviceEvent.insteonGroup = 0;
		_deviceEvent.cmd = 0;
		_deviceEvent.hasDeviceID = false;
		_deviceEvent.hasGroupID 	= false;
		_deviceEvent.hasCmd	 	= false;
		
		if( j.contains(string(JSON_DEVICEID))
			&& j.at(string(JSON_DEVICEID)).is_string()){
			string str = j.at(string(JSON_DEVICEID));
			deviceID_t  rawDevID;
			if(str_to_deviceID(str.c_str(), rawDevID)) {
				_deviceEvent.deviceID = DeviceID(rawDevID);
				_deviceEvent.hasDeviceID = !_deviceEvent.deviceID.isNULL();
			}
		}
		
		if( j.contains(string(JSON_INSTEON_GROUP))
			&& j.at(string(JSON_INSTEON_GROUP)).is_string()){
			string str = j.at(string(JSON_INSTEON_GROUP));
			
			uint8_t groupID = 0;
			if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))
				&& ( std::sscanf(str.c_str(), "%hhd", &groupID) == 1)){
				_deviceEvent.insteonGroup = groupID;
				_deviceEvent.hasGroupID = true;
			}
			else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{2}$"))
					  && ( std::sscanf(str.c_str(), "%hhx", &groupID) == 1)){
				_deviceEvent.insteonGroup = groupID;
				_deviceEvent.hasGroupID = true;
			}
		}
		
		if( j.contains(string(JSON_CMD))
			&& j.at(string(JSON_CMD)).is_string()){
			string str = j.at(string(JSON_CMD));
			
			uint8_t cmd = 0;
			if( regex_match(string(str), std::regex("^[A-Fa-f0-9]{2}$"))
				&& ( std::sscanf(str.c_str(), "%hhd", &cmd) == 1)){
				_deviceEvent.cmd = cmd;
				_deviceEvent.hasCmd = cmd != 0;
			}
			else if( regex_match(string(str), std::regex("^0?[xX][0-9a-fA-F]{2}$"))
					  && ( std::sscanf(str.c_str(), "%hhx", &cmd) == 1)){
				_deviceEvent.cmd = cmd;
				_deviceEvent.hasCmd = cmd != 0;
			}
		}
	}
	else	if(_eventType == EVENT_TYPE_TIME) {
		
		_timeEvent.timeBase = TOD_INVALID;
		_timeEvent.timeOfDay = 0;
		_timeEvent.lastRun = 0;

		if( j.contains(string(JSON_TIME_BASE))
			&& j.at(string(JSON_TIME_BASE)).is_number()){
			_timeEvent.timeBase = j.at(string(JSON_TIME_BASE));
		}

		if( j.contains(string(JSON_TIME_OFFSET))
			&& j.at(string(JSON_TIME_OFFSET)).is_number()){
			_timeEvent.timeOfDay = j.at(string(JSON_TIME_OFFSET));
		}
	}
}


nlohmann::json EventTrigger::JSON(){
	json j;
	
	switch(_eventType){
		case EVENT_TYPE_DEVICE:
		{
			json j1;
			
			if(!_deviceEvent.deviceID.isNULL() && _deviceEvent.hasDeviceID){
				j1[string(JSON_DEVICEID)] = _deviceEvent.deviceID.string();
			}
			
			if(_deviceEvent.hasGroupID){
				j1[string(JSON_INSTEON_GROUP)] =  to_hex<uint8_t>(_deviceEvent.insteonGroup,true);
			}
			
			if(_deviceEvent.hasCmd){
				j1[string(JSON_CMD)] =  to_hex<uint8_t>(_deviceEvent.cmd,true);
			}
			
			// maybe this should be a subset
			if(!j1.is_null())
				j = j1;
			
		}
			break;
			
		case EVENT_TYPE_TIME:
		{
			json j1;
			
			j1[string(JSON_TIME_OFFSET)] =  _timeEvent.timeOfDay;
			j1[string(JSON_TIME_BASE)] 	=  _timeEvent.timeBase;
			j = j1;
		}
			break;
		default:;
	}
	
	return j;
}

const std::string EventTrigger:: printString(){
	std::ostringstream oss;
	using namespace timestamp;

	auto j = JSON();
	
	 if(_eventType == EVENT_TYPE_TIME){

		 solarTimes_t solar;
		 ScheduleMgr::shared()->getSolarEvents(solar);
			int16_t minsFromMidnight = 0;
		 
		 // when does it need to run today
		 calculateTriggerTime(solar,minsFromMidnight);
		time_t schedTime = solar.previousMidnight + (minsFromMidnight * SECS_PER_MIN) ;
		 string timeString = TimeStamp(schedTime).ClockString();
		 
		 string offsetMinutes = "";
		 
		 if(_timeEvent.timeOfDay > 0)
			 offsetMinutes = " + " + to_string(_timeEvent.timeOfDay) + " minutes";
		 else if(_timeEvent.timeOfDay < 0)
			 offsetMinutes =  " - " + to_string(abs(_timeEvent.timeOfDay)) + " minutes";
		
	 	 switch(_timeEvent.timeBase){
			 case TOD_SUNRISE:
				 oss << timeString << " (Sunrise"  << offsetMinutes << ")" ;
				 break;
				 
			 case TOD_SUNSET:
				 oss << timeString << " (Sunset"  << offsetMinutes << ")" ;
			 break;
				 
			 case TOD_CIVIL_SUNRISE:
				 oss << timeString << " (Civil Sunrise" << offsetMinutes << ")" ; ;
				 break;
				 
			 case TOD_CIVIL_SUNSET:
				 oss << timeString << " (Civil Sunset"  << offsetMinutes << ")";
				 break;
				 
			 case TOD_ABSOLUTE:
	 			 oss << timeString;
 				 break;
				 
			 case TOD_INVALID:
				 oss <<  "Invalid Time:";
				 break;
		 }
	}
	else	{
		oss << j.dump();
	}
	return  oss.str();

}
 
/*
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

 */
bool EventTrigger::isValid(){
	return (_eventType != EVENT_TYPE_UNKNOWN);
}
bool EventTrigger::isTimed(){
	return (_eventType == EVENT_TYPE_TIME);
}
bool EventTrigger::isDevice(){
	return (_eventType == EVENT_TYPE_DEVICE);
}
 
bool EventTrigger::setLastRun(time_t time){
	if(_eventType == EVENT_TYPE_TIME){
		_timeEvent.lastRun = time;
		return true;
	} else return false;
}

bool EventTrigger::shouldTriggerFromDeviceEvent(EventTrigger a) {
	bool result = false;
	
	if(_eventType !=  a._eventType)
		return false;
	
	if(_eventType == EVENT_TYPE_DEVICE){
		if(_deviceEvent.hasDeviceID && a._deviceEvent.hasDeviceID)
			if(!_deviceEvent.deviceID.isEqual(a._deviceEvent.deviceID))
				return false;

		if(_deviceEvent.hasGroupID && a._deviceEvent.hasGroupID)
			if(_deviceEvent.insteonGroup != (a._deviceEvent.insteonGroup))
				return false;

		if(_deviceEvent.hasCmd && a._deviceEvent.hasCmd)
			if(_deviceEvent.cmd != (a._deviceEvent.cmd))
				return false;

		result = true;
	}
 	else if(_eventType == EVENT_TYPE_TIME){
	
 	}
 
	return result;
}



bool EventTrigger::shouldTriggerInFuture(const solarTimes_t &solar, time_t localNow){
	
	bool result = false;
	
	if(_eventType == EVENT_TYPE_TIME){
		
		int16_t minsFromMidnight = 0;
		
		// when does it need to run today
		if(calculateTriggerTime(solar,minsFromMidnight)) {
			time_t schedTime = solar.previousMidnight + (minsFromMidnight * SECS_PER_MIN) ;
//
//			printf("\n sched: %s \n", timestamp::TimeStamp(schedTime).ClockString().c_str());
//			printf(" now:  %s \n", timestamp::TimeStamp(localNow).ClockString().c_str());
//
			if( schedTime > localNow) {
						result = true;
			}
		};
	}
	
	return result;
	
}

bool EventTrigger::shouldTriggerFromTimeEvent(const solarTimes_t &solar, time_t localNow){
	
	bool result = false;
	
	if(_eventType == EVENT_TYPE_TIME){
		
		int16_t minsFromMidnight = 0;
		
		// when does it need to run today
		if(calculateTriggerTime(solar,minsFromMidnight)) {
			time_t schedTime = solar.previousMidnight + (minsFromMidnight * SECS_PER_MIN) ;
//		
//			printf("\n sched: %s \n", timestamp::TimeStamp(schedTime).ClockString().c_str());
//			printf(" now:  %s \n", timestamp::TimeStamp(localNow).ClockString().c_str());
//	
			if( schedTime < localNow) {
				if( _timeEvent.lastRun  == 0
					||  _timeEvent.lastRun < solar.previousMidnight )
					result = true;
			}
		};
	}
	
	return result;
}


bool EventTrigger::calculateTriggerTime(const solarTimes_t &solar, int16_t &minsFromMidnight) {
	
	bool result = false;
	
	if(_eventType == EVENT_TYPE_TIME){
		int16_t actualTime  = _timeEvent.timeOfDay;
		
		switch(_timeEvent.timeBase){
			case TOD_SUNRISE:
				actualTime = solar.sunriseMins + actualTime;
				result = true;
				break;
				
			case TOD_SUNSET:
				actualTime = solar.sunSetMins + actualTime;
				result = true;
				break;
				
			case TOD_CIVIL_SUNRISE:
				actualTime = solar.civilSunRiseMins + actualTime;
				result = true;
				break;
				
			case TOD_CIVIL_SUNSET:
				actualTime = solar.civilSunSetMins + actualTime;
				result = true;
				break;
				
			case TOD_ABSOLUTE:
				actualTime = actualTime;
				result = true;
				break;
				
			case TOD_INVALID:
				break;
		}
		
		if(result)
			minsFromMidnight = actualTime;
	}
 
	return result;
}


// MARK: - EventGroup
 
bool str_to_EventGroupID(const char* str, eventGroupID_t *eventGroupIDOut){
	bool status = false;
	
	eventID_t val = 0;
 	status = sscanf(str, "%hx", &val) == 1;
	
	if(eventGroupIDOut)  {
		*eventGroupIDOut = val;
	}
	
	return status;
};

 
string  EventGroupID_to_string(eventGroupID_t eventGroupID){
		return to_hex<unsigned short>(eventGroupID);
}

// MARK: - Event()
 
bool str_to_EventID(const char* str, eventID_t *eventIDOut){
	bool status = false;
	
	eventID_t val = 0;
 
	status = sscanf(str, "%hx", &val) == 1;
	
	if(eventIDOut)  {
		*eventIDOut = val;
	}
	
	return status;

};

 
string  EventID_to_string(eventID_t eventID){
		return to_hex<unsigned short>(eventID);
}


Event::Event(){
	_rawEventID = 0;
	_trigger 	= EventTrigger();
	_action 		= Action();
}

Event::Event(EventTrigger trigger, Action action){
	_rawEventID = 0;
	_trigger = trigger;
	_action = action;
}

Event::Event(nlohmann::json j){
	
	_rawEventID = 0;

	if( j.contains(JSON_ARG_TRIGGER)
		&& j.at(string(JSON_ARG_TRIGGER)).is_object()) {
		auto jT = j.at(string(JSON_ARG_TRIGGER));
		_trigger = EventTrigger(jT);
	}
	
	if( j.contains(JSON_ARG_ACTION)
		&& j.at(string(JSON_ARG_ACTION)).is_object()) {
		auto jA = j.at(string(JSON_ARG_ACTION));
		_action = Action(jA);
	}
}


void Event::copy(const Event &evt1, Event *evt2){
		
	evt2->_rawEventID	 	= evt1._rawEventID;
	evt2->_name				= evt1._name;
	evt2->_trigger			= evt1._trigger;
	evt2->_action			= evt1._action;
 }

 
nlohmann::json Event::JSON(){
	json j;
	
	auto jA = _action.JSON();
	auto jT = _trigger.JSON();
	
	j[string(JSON_ARG_ACTION)] = jA;
	j[string(JSON_ARG_TRIGGER)] = jT;
	
	j[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(_rawEventID);
	if(!_name.empty()) j[string(JSON_ARG_NAME)] = _name;
	
	return j;
}

bool Event::isValid(){
	return (_action.isValid() && _trigger.isValid());
}
