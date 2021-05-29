//
//  Event.hpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/23/21.
//

#ifndef Event_hpp
#define Event_hpp

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
#include "Action.hpp"
#include "ScheduleMgr.hpp"

using namespace std;

class EventTrigger {
	
	// these shouldnt change, 		they become persistant
	typedef enum  {
		EVENT_TYPE_UNKNOWN 		= 0,
		EVENT_TYPE_DEVICE			= 1,
		EVENT_TYPE_TIME			= 2,
	}eventType_t;

	typedef enum {
		TOD_INVALID 	= 0,
		TOD_ABSOLUTE	= 1,
		TOD_SUNRISE		= 2,
		TOD_SUNSET		= 3,
		TOD_CIVIL_SUNRISE = 4,
		TOD_CIVIL_SUNSET 	= 5,
	} tod_offset_t;

	typedef struct {
		DeviceID 		deviceID;
		uint8_t		insteonGroup;
		uint8_t		cmd;
		bool 			hasDeviceID 	: 1;
		bool 			hasGroupID 	: 1;
		bool 			hasCmd 		: 1;
	} deviceEventInfo_t;

	typedef struct {
		tod_offset_t				timeBase;
		int16_t 					timeOfDay;
		time_t						lastRun;
	} timeEventInfo_t;

public:
	EventTrigger();

	EventTrigger(const EventTrigger &etIn){
	 		copy(etIn, this);
	}

	EventTrigger(uint8_t insteonGroup);
	EventTrigger(DeviceID deviceID);
	EventTrigger(DeviceID deviceID, uint8_t insteonGroup, uint8_t cmd = 0);
	EventTrigger(tod_offset_t timeBase, int16_t timeOfDay);
	
	EventTrigger(std::string);
	EventTrigger(nlohmann::json j);
	nlohmann::json JSON();
 	const std::string printString();
	 
	bool isValid();

	bool shouldTriggerFromDeviceEvent(EventTrigger a);
 
	bool shouldTriggerFromTimeEvent(const solarTimes_t &solar, time_t time);
 
	bool calculateTriggerTime(const solarTimes_t &solar, int16_t &minsFromMidnight);

	bool setLastRun(time_t time);
	
	time_t getLastRun(){
		return (_eventType == EVENT_TYPE_TIME)?_timeEvent.lastRun:0;
	}

	inline void operator = (const EventTrigger &right ) {
		copy(right, this);
	}

private:
	
	void initWithJSON(nlohmann::json j);
	void copy(const EventTrigger &evt, EventTrigger *eventOut);

	eventType_t			_eventType;
	
	union{
		deviceEventInfo_t 	_deviceEvent;
		timeEventInfo_t 		_timeEvent;
	};
};


typedef  unsigned short eventID_t;
typedef  unsigned short eventGroupID_t;

bool str_to_EventGroupID(const char* str, eventGroupID_t *eventGroupIDOut = NULL);
string  EventGroupID_to_string(eventGroupID_t eventGroupID);

bool str_to_EventID(const char* str, eventID_t *eventIDOut = NULL);
string  EventID_to_string(eventID_t eventID);

class Event {
	
	public:
	friend class InsteonDB;

	Event();
	Event(EventTrigger trigger, Action action);
	Event(nlohmann::json j);
	nlohmann::json JSON();
		
	const eventID_t 	eventID(){return _rawEventID;};
	bool isValid();
 
	std::string idString() const {
		return  to_hex<unsigned short>(_rawEventID);
	}

	std::string getName() const {
		return  _name;
	}

	void setName(std::string name){
		_name = name;
	}

	void setAction(Action act){
		_action = act;
	}
	
	reference_wrapper<Action> getAction() {
		return  std::ref(_action);
	}

	
	void setEventTrigger(EventTrigger trig){
		_trigger = trig;
	}
	
	reference_wrapper<EventTrigger> getEventTrigger() {
		return  std::ref(_trigger);
	}
	
	void setLastRun(time_t time){
		_trigger.setLastRun(time);
	}

	time_t getLastRun(){
		return (_trigger.getLastRun()) ;
	}


	bool isEqual(Event a) {
		return a._rawEventID  == _rawEventID ;
	}

	bool isEqual(eventID_t eventID) {
		return eventID  == _rawEventID ;
	}

	inline bool operator==(const Event& right) const {
		return right._rawEventID  == _rawEventID;
		}

	inline bool operator!=(const Event& right) const {
		return right._rawEventID  != _rawEventID;
	}

	inline void operator = (const Event &right ) {
		copy(right, this);
	}
	
 
private:
	
 	void copy(const Event &evt, Event *eventOut);
	
	eventID_t				_rawEventID;
	std::string			_name;
	EventTrigger			_trigger;
	Action					_action;
};

#endif /* Event_hpp */
