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

// MARK: - SERVER DEFINES

constexpr string_view NOUN_VERSION		 		= "version";
constexpr string_view NOUN_DATE		 			= "date";
constexpr string_view NOUN_DEVICES	 			= "devices";
constexpr string_view NOUN_STATUS		 		= "status";
constexpr string_view NOUN_LOG		 			= "log";
constexpr string_view NOUN_PLM			 		= "plm";
constexpr string_view NOUN_GROUPS			 	= "groups";
constexpr string_view NOUN_INSTEON_GROUPS	= "insteon.groups";
constexpr string_view NOUN_ACTION_GROUPS		= "action.groups";
constexpr string_view NOUN_EVENTS				= "events";
constexpr string_view NOUN_EVENTS_GROUPS		= "event.groups";
constexpr string_view NOUN_KEYPADS				= "keypads";

constexpr string_view NOUN_CONFIG		 		= "config";

constexpr string_view NOUN_LINK	 				= "link";
 
constexpr string_view SUBPATH_INFO			 	= "info";
constexpr string_view SUBPATH_DATABASE		= "database";
constexpr string_view SUBPATH_RUN_ACTION		= "run.actions";
constexpr string_view SUBPATH_PORT			 	= "port";
constexpr string_view SUBPATH_STATE		 	= "state";
constexpr string_view SUBPATH_FILE	 			= "file";

constexpr string_view JSON_ARG_DEVICEID 		= "deviceID";
constexpr string_view JSON_ARG_GROUPID 		= "groupID";
constexpr string_view JSON_ARG_ACTIONID 		= "actionID";
constexpr string_view JSON_ARG_EVENTID 		= "eventID";
constexpr string_view JSON_ARG_ACTION			= "action";
constexpr string_view JSON_ARG_TRIGGER		= "trigger";

constexpr string_view JSON_ARG_CONFIG		= "config";

constexpr string_view JSON_ARG_BEEP 			= "beep";
constexpr string_view JSON_ARG_LEVEL 			= "level";
constexpr string_view JSON_ARG_BACKLIGHT		= "backlight";
constexpr string_view JSON_ARG_KP_MASK 		= "keyMask"; 	// Keypad LED mask
constexpr string_view JSON_ARG_BUTTONS		= "buttons";
constexpr string_view JSON_ARG_BUTTON			= "button";

constexpr string_view JSON_ARG_VALIDATE		= "validate";
constexpr string_view JSON_ARG_DUMP		 	= "dump";			// for debug datat
constexpr string_view JSON_ARG_MESSAGE		= "message";			// for logfile
 
constexpr string_view JSON_ARG_LOAD 			= "load";
constexpr string_view JSON_ARG_SAVE 			= "save";

constexpr string_view JSON_ARG_DATE			= "date";
constexpr string_view JSON_ARG_VERSION		= "version";
constexpr string_view JSON_ARG_TIMESTAMP		= "timestamp";
constexpr string_view JSON_ARG_DEVICEIDS	= 	"deviceIDs";

constexpr string_view JSON_ARG_IS_KEYPAD		= "isKeyPad";
constexpr string_view JSON_ARG_IS_DIMMER		= "isDimmer";
constexpr string_view JSON_ARG_IS_PLM			= "isPLM";

constexpr string_view JSON_ARG_DEVICEINFO 	= "deviceInfo";
constexpr string_view JSON_ARG_DETAILS 		= "details";
constexpr string_view JSON_ARG_LEVELS 		= "levels";
constexpr string_view JSON_ARG_ACTIONS		= "actions";
constexpr string_view JSON_ARG_ALDB 			= "aldb";

constexpr string_view JSON_ARG_STATE			= "state";
constexpr string_view JSON_ARG_STATESTR		= "stateString";
constexpr string_view JSON_ARG_ETAG			= "ETag";
constexpr string_view JSON_ARG_FORCE			= "force";
constexpr string_view JSON_ARG_PROPERTIES	= "properties";
constexpr string_view JSON_ARG_NAME			= "name";
constexpr string_view JSON_ARG_GROUPIDS		= "groupIDs";
constexpr string_view JSON_ARG_EVENTIDS		= "eventIDs";
constexpr string_view JSON_ARG_FILEPATH		= "filepath";
constexpr string_view JSON_ARG_LOGFLAGS		= "logflags";
constexpr string_view JSON_ARG_COUNT			= "count";
constexpr string_view JSON_ARG_ISCNTRL		= "cntrl";  // used for linking devices
constexpr string_view JSON_ARG_AUTOSTART		= "auto.start";
constexpr string_view JSON_ARG_REMOTETELNET	= "allow.remote.telnet";

constexpr string_view JSON_ARG_TIMED_EVENTS	= "events.timed";
constexpr string_view JSON_ARG_FUTURE_EVENTS	= "events.future";

constexpr string_view JSON_ARG_LATITUDE		= "latitude";
constexpr string_view JSON_ARG_LONGITUDE		= "longitude";
 
constexpr string_view JSON_VAL_ALL				= "all";
constexpr string_view JSON_VAL_VALID			= "valid";
constexpr string_view JSON_VAL_DETAILS		= "details";
constexpr string_view JSON_VAL_LEVELS			= "levels";
constexpr string_view JSON_VAL_ALDB			= "aldb";

constexpr string_view JSON_VAL_ALDB_FLAG		= "aldb.flag";
constexpr string_view JSON_VAL_ALDB_ADDR		= "aldb.address";
constexpr string_view JSON_VAL_ALDB_GROUP	= "aldb.group";

constexpr string_view JSON_VAL_START			= "start";
constexpr string_view JSON_VAL_STOP			= "stop";
constexpr string_view JSON_VAL_RESET			= "reset";


// MARK: -

// MARK:  EVENTS NOUN HANDLERS


static bool Events_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;

	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;

	auto db = insteon.getDB();
	
	// GET /events
	if(path.size() == 1) {
	 
		json eventList;
		
		auto eventIDs = db->allEventsIDs();

		for(auto eventID : eventIDs){
			json entry;
			
			entry[string(JSON_ARG_NAME)] =  db->eventGetName(eventID);
			
			auto ref = db->eventsGetEvent(eventID);
			if(ref) {
				Event event = ref->get();
				Action act = event.getAction();
				if(act.isValid()){
					entry[string(JSON_ARG_ACTION)]= act.JSON();
				}

				EventTrigger trig = event.getEventTrigger();
				if(trig.isValid()){
					entry[string(JSON_ARG_TRIGGER)]= trig.JSON();
				}
 			}
			
			eventList[ to_hex<unsigned short>(eventID)] = entry;
		}
		
		reply[string(JSON_ARG_EVENTIDS)] = eventList;

		// create a sorted vector of timed events
		vector<std::pair<string, int16_t>> timedEvents;
 		solarTimes_t solar;
		insteon.getSolarEvents(solar);

		time_t now = time(NULL);
		struct tm* tm = localtime(&now);
		time_t localNow  = (now + tm->tm_gmtoff);
		vector<eventID_t> fEvents  = db->eventsInTheFuture(solar, localNow);

		vector<string> futureEvents;
			for(auto eventID : fEvents){
				futureEvents.push_back(to_hex<unsigned short>(eventID));
 	 	}

		if(futureEvents.size()){
			reply[string(JSON_ARG_FUTURE_EVENTS	)] = futureEvents;
		}

		for(auto eventID : eventIDs){
			auto ref = db->eventsGetEvent(eventID);
			if(ref) {
				Event event = ref->get();
				EventTrigger trig = event.getEventTrigger();
				if(trig.isTimed()){

					int16_t minsFromMidnight = 0;
					if(trig.calculateTriggerTime(solar,minsFromMidnight)) {
						timedEvents.push_back(make_pair( event.idString(), minsFromMidnight));
					}
				}
			}
		}
		if(timedEvents.size() > 0){
			sort(timedEvents.begin(), timedEvents.end(),
				  [] (const pair<string, int16_t>& a,
						const pair<string, int16_t>& b) { return a.second < b.second; });

	 		reply[string(JSON_ARG_TIMED_EVENTS)] = timedEvents;
 		}

 		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;

	}
		// GET /events/XXXX
		else if(path.size() == 2) {
			
			eventID_t eventID;
			
			if( !str_to_EventID(path.at(1).c_str(), &eventID) || !db->eventsIsValid(eventID))
				return false;
	 
			auto ref = db->eventsGetEvent(eventID);
			if(ref) {
				Event event = ref->get();
				Action act = event.getAction();
				if(act.isValid()){
					reply[string(JSON_ARG_ACTION)]= act.JSON();
				}
				
				EventTrigger trig = event.getEventTrigger();
				if(trig.isValid()){
					reply[string(JSON_ARG_TRIGGER)]= trig.JSON();
				}

				reply[string(JSON_ARG_EVENTID)] = event.idString();
				reply[string(JSON_ARG_NAME)] 	=  db->eventGetName(eventID);
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
		
 //			json actions;
//			auto acts = db->actionGroupGetActions(agID);
//			for(auto ref :acts){
//				Action a1 = ref.get();
//				actions[a1.idString()] =  a1.JSON();
//			}
//			reply[string(JSON_ARG_ACTIONS)] = actions;
 	}
 
	return false;
}

static bool Events_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
													  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	string subpath;
	
	if(path.size() > 1){
		subpath =   path.at(1);
	}
	
	 if(path.size() == 3) {
		if( subpath == SUBPATH_RUN_ACTION) {
			
			eventID_t eventID;
			
			if( !str_to_EventID(path.at(2).c_str(), &eventID) || !db->eventsIsValid(eventID))
				return false;
	 
		 
				bool queued = insteon.executeEvent(eventID, [=]( bool didSucceed){
				
				json reply;
				
				if(didSucceed){
					reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Run Event Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
				(completion) (reply, STATUS_UNAVAILABLE);
				return true;
			}
			return true;
		}
	}
	return false;
}

static bool Events_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		
		eventID_t eventID;
		
		if( !str_to_EventID(path.at(1).c_str(), &eventID) || !db->eventsIsValid(eventID))
			return false;
 
		string name;
		// set name
		DeviceID	deviceID;
		if(db->eventSetName(eventID, name)) {
				reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
				reply[string(JSON_ARG_NAME)] = name;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		}
		
	 
	return false;
	
}

static bool createEventFromJSON( json  &j, Event& event) {
	
	bool statusOK = false;
	Event evt;
	
	ServerCmdArgValidator 	v1;
	DeviceIDArgValidator 	vDeviceID;
	string str;
	
	Action action;
	EventTrigger trigger;
	
	if( j.contains(JSON_ARG_ACTION)
		&& j.at(string(JSON_ARG_ACTION)).is_object()){
		auto a = j.at(string(JSON_ARG_ACTION));
		action = Action(a);
	}
	
	if(action.isValid()
		&&  j.contains(JSON_ARG_TRIGGER)
		&& j.at(string(JSON_ARG_TRIGGER)).is_object()){
		auto t = j.at(string(JSON_ARG_TRIGGER));
		trigger = EventTrigger(t);
	};
	
	evt = Event(trigger, action);
	statusOK = evt.isValid();
	
	if(statusOK){
		event = evt;
	}
	
	return statusOK;
}

 
static bool Events_NounHandler_POST(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;

	auto db = insteon.getDB();
	
	if(path.size() == 1) {
		
		string name;
		// Create event
		
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			
			eventID_t eventID;
			if(db->eventFind(name, &eventID)){
				name = db->eventGetName(eventID);
				
				reply[string(JSON_ARG_NAME)] = name;
				reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
				makeStatusJSON(reply, STATUS_CONFLICT, "Name already used" );;
				(completion) (reply, STATUS_CONFLICT);
				return true;
			}
			else {
				
				Event event;
				 
				if(!createEventFromJSON(url.body(), event ) || !event.isValid())
				{
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
					
				event.setName(name);
				
				if (db->eventSave(event, &eventID)) {
					reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
					reply[string(JSON_ARG_NAME)] = name;
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
					return true;
				}
				else {
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
			}
		}
	}
	
	
	return false;
}


static bool Events_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
														  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	ActionGroup group;
	
	eventID_t eventID;
	
	if( !str_to_EventID(path.at(1).c_str(), &eventID) || !db->eventsIsValid(eventID))
		return false;

	if(path.size() == 2) {
		if(db->eventDelete(eventID)){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_EVENTID)] = to_hex<unsigned short>(eventID);
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return true;
		
	}
	return false;
}

static void Events_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
		
//	// is server available?
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Events_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
 			isValidURL = Events_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PATCH:
 			isValidURL = Events_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
	 
		case HTTP_POST:
			isValidURL = Events_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;
 
		case HTTP_DELETE:
 			isValidURL = Events_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
}

// MARK:  EVENT GROUPS NOUN HANDLERS

static bool EventGroups_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	// GET /event.groups
	if(path.size() == 1) {;
		
		json groupsList;
		auto groupIDs = db->allEventGroupIDs();
		for(auto groupID : groupIDs){
			json entry;
			
			entry[string(JSON_ARG_NAME)] =  db->eventGroupGetName(groupID);
			groupsList[ EventGroupID_to_string(groupID)] = entry;
		}
		
		reply[string(JSON_ARG_GROUPIDS)] = groupsList;
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
		
	}
		// GET /event.groups/XXXX
		else if(path.size() == 2) {
			 
			eventGroupID_t groupID;
			
			if( !str_to_EventGroupID(path.at(1).c_str(), &groupID) || !db->eventGroupIsValid(groupID))
				return false;
	 
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			reply[string(JSON_ARG_NAME)] =  db->eventGroupGetName(groupID);
	 		auto eventIDS = db->eventGroupGetEventIDs(groupID);
			
			vector<string> ids;
			for (auto evtID : eventIDS) {
				ids.push_back(EventID_to_string(evtID));
			}
		 	reply[string(JSON_ARG_EVENTID	)] = ids;

			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);

	}
	else {
		
	}
	
	return false;
}


static bool EventGroups_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
													  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
 
	ServerCmdArgValidator v1;
 
	eventGroupID_t groupID;
 
	  if(path.size() < 1) {
		  return false;
	  }
	  
	  if( !str_to_EventGroupID(path.at(1).c_str(), &groupID)
		  || !db->eventGroupIsValid(groupID))
		  return false;
	
	
	string str;
	if(v1.getStringFromJSON(JSON_ARG_EVENTID, url.body(), str)){
	 	eventID_t eventID;
		
		if( ! str_to_EventID(str.c_str(), &eventID) || !db->eventsIsValid(eventID))
			return false;
	 
		if(db->eventGroupAddEvent(groupID, eventID)){
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			reply[string(JSON_ARG_EVENTID)] = EventID_to_string(eventID);
 			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			reply[string(JSON_ARG_EVENTID)] = EventID_to_string(eventID);
	
	 		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return  true;
	}

	return false;
}

static bool EventGroups_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
 
	eventGroupID_t groupID;

	if(path.size() < 1) {
		return false;
	}
	
	if( !str_to_EventGroupID(path.at(1).c_str(), &groupID)
		|| !db->eventGroupIsValid(groupID))
		return false;
 
	
	if(path.size() == 2) {
		string name;
		// set name
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			if(db->eventGroupSetName(groupID, name)) {
				reply[string(JSON_ARG_GROUPID)] =  EventGroupID_to_string(groupID);
				reply[string(JSON_ARG_NAME)] = name;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_GROUPID)] =  EventGroupID_to_string(groupID);
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		}
	}
	return false;
	
}

static bool EventGroups_NounHandler_POST(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 1) {
		
		string name;
		// Create group
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			
			eventGroupID_t groupID;
			if(db->eventGroupFind(name, &groupID)){
				name = db->eventGroupGetName(groupID);
			}
			else {
				if (! db->eventGroupCreate(&groupID, name)) {
					reply[string(JSON_ARG_NAME)] = name;
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
			}
		 
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			reply[string(JSON_ARG_NAME)] = name;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
		}
	}

	return false;
}



static bool EventGroups_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
														  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	eventGroupID_t groupID;

	if(path.size() < 1) {
		return false;
	}
	
	if( !str_to_EventGroupID(path.at(1).c_str(), &groupID)
		|| !db->eventGroupIsValid(groupID))
		return false;
 
	if(path.size() == 2) {
 		if(db->eventGroupDelete(groupID)){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return true;
	}
	else if(path.size() == 3) {
		
		eventID_t eventID;
		
		if( !str_to_EventID(path.at(2).c_str(), &eventID)
			|| !db->eventsIsValid(eventID))
			return false;
		
		if(db->eventGroupRemoveEvent(groupID, eventID)){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = EventGroupID_to_string(groupID);
			reply[string(JSON_ARG_EVENTID)] = EventID_to_string(eventID);
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
	}
 	return false;
}

static void EventGroups_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	
//	// is server available?
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = EventGroups_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = EventGroups_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PATCH:
			isValidURL = EventGroups_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
	 
		case HTTP_POST:
			isValidURL = EventGroups_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;
 
		case HTTP_DELETE:
			isValidURL = EventGroups_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
}

// MARK:  ACTION GROUPS NOUN HANDLERS

static bool ActionGroups_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	// GET /groups
	if(path.size() == 1) {;
		
		json groupsList;
		auto groupIDs = db->allActionGroupsIDs();
		for(auto groupID : groupIDs){
			json entry;
			
			ActionGroup ag = ActionGroup(groupID);
			entry[string(JSON_ARG_NAME)] =  db->actionGroupGetName(groupID);
			groupsList[ag.string()] = entry;
		}
		
		reply[string(JSON_ARG_GROUPIDS)] = groupsList;
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
		
	}
		// GET /action.groups/XXXX
		else if(path.size() == 2) {
			ActionGroup group = ActionGroup(path.at(1));
			
			if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
				return false;

			actionGroupID_t agID = group.groupID();
	
			reply[string(JSON_ARG_GROUPID)] = group.string();
			reply[string(JSON_ARG_NAME)] =  db->actionGroupGetName(agID);

			json actions;
			auto acts = db->actionGroupGetActions(agID);
			for(auto ref :acts){
				Action a1 = ref.get();
				actions[a1.idString()] =  a1.JSON();
			}
			reply[string(JSON_ARG_ACTIONS)] = actions;
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);

 	}
	else {
		
	}
	
	return false;
}


static bool ActionGroups_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
													  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	string subpath;
	
	if(path.size() > 1){
		subpath =   path.at(1);
	}
	
	if(path.size() == 2) {
		ActionGroup group = ActionGroup(subpath);
		
		if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
			return false;
		
		Action	 act(url.body());
		if(act.isValid()){
			
			actionID_t  actionID;
			if(db->actionGroupAddAction(group.groupID(), act, &actionID)) {
				reply[string(JSON_ARG_GROUPID)] = group.string();
				reply[string(JSON_ARG_ACTIONID)] = to_hex<unsigned short>(actionID);
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				return true;
			}
		}
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Create Failed",  "Invalid Action Specified" );;
		(completion) (reply, STATUS_BAD_REQUEST);
		return true;
	}
	else if(path.size() == 3) {
		if( subpath == SUBPATH_RUN_ACTION) {
			
			ActionGroup group = ActionGroup(path.at(2));
			
			if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
				return false;
			
			bool queued = insteon.executeActionGroup(group.groupID(),
														[=]( bool didSucceed){
				
				json reply;
				
				if(didSucceed){
					reply[string(JSON_ARG_GROUPID)] = group.string();
		 			makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					reply[string(JSON_ARG_GROUPID)] = group.string();
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Run Group Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
				(completion) (reply, STATUS_UNAVAILABLE);
				return true;
			}
			return true;
		}
	}
	return false;
}

static bool ActionGroups_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		
		ActionGroup group = ActionGroup(path.at(1));
 
		if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
			return false;

		string name;
		// set name
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			if(db->actionGroupSetName(group.groupID(), name)) {
				reply[string(JSON_ARG_GROUPID)] = group.string();
				reply[string(JSON_ARG_NAME)] = name;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_GROUPID)] = group.string();
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
			
			return true;
		}
		
	}
	return false;
}

static bool ActionGroups_NounHandler_POST(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 1) {
		
		string name;
		// Create group
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			
			actionGroupID_t groupID;
			if(db->actionGroupFind(name, &groupID)){
				name = db->actionGroupGetName(groupID);
			}
			else {
				if (! db->actionGroupCreate(&groupID, name)) {
					reply[string(JSON_ARG_NAME)] = name;
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
			}
		
			ActionGroup ag = ActionGroup(groupID);

			reply[string(JSON_ARG_GROUPID)] = ag.string();
			reply[string(JSON_ARG_NAME)] = name;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
		}
	}

	return false;
}



static bool ActionGroups_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
														  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	ActionGroup group;
	
	if(path.size() > 1){
		group = ActionGroup(path.at(1));
		
		if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
			return false;
	}
	
	if(path.size() == 2) {
		if(db->actionGroupDelete(group.groupID())){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = group.string();
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return true;
		
	}
	else if(path.size() == 3) {
		
		string str = path.at(2);
		actionID_t  actionID;
		
		if( str_to_ActionID(str.c_str(), &actionID)
			&& db-> actionGroupIsValidActionID(group.groupID(),actionID)){
			
				if( db->actionGroupRemoveAction(group.groupID(),actionID)) {
					makeStatusJSON(reply,STATUS_NO_CONTENT);
					(completion) (reply, STATUS_NO_CONTENT);
				}
			else
			{
				reply[string(JSON_ARG_GROUPID)] = group.string();
				reply[string(JSON_ARG_ACTIONID)] =  to_hex<unsigned short>(actionID);
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);

			}
				return true;
		}
	}
	return false;
}

static void ActionGroups_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	
//	// is server available?
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = ActionGroups_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = ActionGroups_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PATCH:
			isValidURL = ActionGroups_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
	 
		case HTTP_POST:
 			isValidURL = ActionGroups_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;
 
		case HTTP_DELETE:
 			isValidURL = ActionGroups_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
}

// MARK:  GROUPS NOUN HANDLERS

static bool Groups_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	// GET /groups
	if(path.size() == 1) {;
		
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
			groupsList[groupID.string()] = entry;
		}
		reply[string(JSON_ARG_GROUPIDS)] = groupsList;
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
		
	}
	// GET /groups/XXXX
	else if(path.size() == 2) {
		
 		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;
		
		json groupsList;
		
		json entry;
		entry[string(JSON_ARG_NAME)] =  db->groupGetName(groupID);
		vector<DeviceID> deviceIDs = db->groupGetDevices(groupID	);
		vector<string> deviceList;
		for(DeviceID deviceID :deviceIDs) {
			deviceList.push_back(deviceID.string());
		}
		entry[string(JSON_ARG_DEVICEIDS)] = deviceList;
		groupsList[groupID.string()] = entry;
		
		
		reply[string(JSON_ARG_GROUPIDS)] = groupsList;
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);

		return true;
	}
	else {
		
	}
	
	return false;
}

static bool Groups_NounHandler_PUT(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
 
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		
		DeviceIDArgValidator vDeviceID;
		DeviceLevelArgValidator vLevel;
		
		string str;
		int level;
		
		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;
		
		// add device?
		if( vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, url.body(), str)){
			DeviceID  deviceID = DeviceID(str);
			
			if(deviceID.isNULL())
				return false;
			
			if(db->groupAddDevice(groupID, deviceID)){
				reply[string(JSON_ARG_GROUPID)] = groupID.string();
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				
			}
			else {
				reply[string(JSON_ARG_GROUPID)] = groupID.string();
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		}
		// set group level
		else if(vLevel.getvalueFromJSON(JSON_ARG_LEVEL, url.body(), level)){
			
			bool queued = insteon.setOnLevel(groupID, level,
														[=]( bool didSucceed){
				
				json reply;
				
				if(didSucceed){
					reply[string(JSON_ARG_GROUPID)] = groupID.string();
					reply[string(JSON_ARG_LEVEL)]  = level;
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					reply[string(JSON_ARG_GROUPID)] = groupID.string();
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
				(completion) (reply, STATUS_UNAVAILABLE);
				return true;
			}
			return true;
		}
	}
	
	return false;
}

static bool Groups_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		
		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;
		
		string name;
		// set name
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			if(db->groupSetName(groupID, name)) {
				reply[string(JSON_ARG_GROUPID)] = groupID.string();
				reply[string(JSON_ARG_NAME)] = name;
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_GROUPID)] = groupID.string();
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		}
		
	}
	return false;
	
}


static bool Groups_NounHandler_POST(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 1) {
		
		string name;
		// Create group
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			
			GroupID groupID;
			if(db->groupFind(name, &groupID)){
				name = db->groupGetName(groupID);
			}
			else {
				if (! db->groupCreate(&groupID, name)) {
					reply[string(JSON_ARG_NAME)] = name;
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
			}
			
			reply[string(JSON_ARG_GROUPID)] = groupID.string();
			reply[string(JSON_ARG_NAME)] = name;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
		}
	}

	return false;
}


static bool Groups_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		
		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;
		
		if(db->groupDelete(groupID)){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = groupID.string();
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return true;
		
	}
	else if(path.size() == 3) {
		DeviceIDArgValidator vDeviceID;

		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;

		auto deviceStr = path.at(2);
		if(!vDeviceID.validateArg(deviceStr))
			return false;
			
		DeviceID	deviceID = DeviceID(deviceStr);
	
		if(db->groupRemoveDevice(groupID,deviceID)){
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		else {
			reply[string(JSON_ARG_GROUPID)] = groupID.string();
			reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
			(completion) (reply, STATUS_BAD_REQUEST);
		}
		return true;
 
	}

	return false;
}


static void Groups_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	

//	// is server available?
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Groups_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = Groups_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PATCH:
			isValidURL = Groups_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
	 
		case HTTP_POST:
			isValidURL = Groups_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;
 
		case HTTP_DELETE:
			isValidURL = Groups_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
}

// MARK:  INSTEON.GROUPS NOUN HANDLERS

static void InsteonGroups_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
												  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
 
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
//
	// CHECK METHOD
	if(url.method() != HTTP_PUT ) {
		(completion) (reply, STATUS_INVALID_METHOD);
		return;
	}
	
	if(path.size() == 2) {
		
		DeviceLevelArgValidator vLevel;
		int level;
		
		auto groupStr = path.at(1);
		
		if( regex_match(string(groupStr), std::regex("^[A-Fa-f0-9]{2}$"))){
			uint8_t groupID;
			if( std::sscanf(groupStr.c_str(), "%hhx", &groupID) == 1){
				
				// set level
				if(vLevel.getvalueFromJSON(JSON_ARG_LEVEL, url.body(), level)){
					
					bool queued = InsteonDeviceGroup(groupID).setOnLevel(level,
																						  [=](bool didSucceed){
						json reply;
						if(didSucceed){
							makeStatusJSON(reply,STATUS_OK);
							(completion) (reply, STATUS_OK);
						}
						else {
							makeStatusJSON(reply, STATUS_BAD_REQUEST, "Insteon Group Set Failed" );;
							(completion) (reply, STATUS_BAD_REQUEST);
						}
					});
					
					if(!queued) {
						makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
						(completion) (reply, STATUS_UNAVAILABLE);
					}
					return;
				}
			}
		}
	}
	
	(completion) (reply, STATUS_NOT_FOUND);
}



 
// MARK:  DEVICES NOUN HANDLERS

static bool DeviceDetailJSONForDeviceID( DeviceID deviceID, json &entry, bool showALDB) {
	using namespace rest;
	using namespace timestamp;

	auto db = insteon.getDB();

	insteon_dbEntry_t info;
	
	if( db->getDeviceInfo(deviceID,  &info)) {
		entry[string(JSON_ARG_DEVICEID)] = deviceID.string();
		entry[string(JSON_ARG_NAME)] = 	info.name;
		entry[string(JSON_VAL_VALID)] = info.isValidated;
		entry[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
		entry["lastUpdated"] =  TimeStamp(info.lastUpdated).RFC1123String();
		entry[string(JSON_ARG_IS_KEYPAD)]  =  info.deviceInfo.isKeyPad();
		entry[string(JSON_ARG_IS_DIMMER)] =  info.deviceInfo.isDimmer();
		entry[string(JSON_ARG_IS_PLM)] =  info.deviceInfo.isPLM();

		if(info.properties.size()){
			json props;
			for(auto prop : info.properties){
				props[prop.first] = prop.second;
			}
			entry[string(JSON_ARG_PROPERTIES)]  = props;
		}
		
		auto groups = db->groupsContainingDevice(deviceID);
		if(groups.size() > 0){
			vector<string> groupList;
			for(GroupID groupID : groups) {
				groupList.push_back(groupID.string());
			}
			entry[string(JSON_ARG_GROUPIDS)] = groupList;
		}

		if(showALDB){
			
			json aldbJSON;
			
 	 		for(auto aldb :info.deviceALDB){
				json aldbEntry;
				DeviceID aldbDev = DeviceID(aldb.devID);
				aldbEntry[string(JSON_ARG_DEVICEID)] = aldbDev.string();
				aldbEntry[string(JSON_VAL_ALDB_FLAG)] =  to_hex <unsigned char>(aldb.flag);
				aldbEntry[string(JSON_VAL_ALDB_ADDR)] =  to_hex <unsigned short>(aldb.address);
				aldbEntry[string(JSON_VAL_ALDB_GROUP)] =  to_hex <unsigned char>(aldb.group);
				
				aldbJSON[to_hex <unsigned short>(aldb.address)] = aldbEntry;
 			}
	 		
			entry[string(JSON_VAL_ALDB)] = aldbJSON;
		}
		
		
		uint8_t group = 0x01;		// only on group 1
		uint8_t level = 0;
		eTag_t lastTag = 0;

		if( db->getDBOnLevel(deviceID, group, &level, &lastTag) ){
			entry[string(JSON_ARG_LEVEL)]  = level;
			entry[string(JSON_ARG_ETAG)] = info.eTag;
		}
		
		return true;
	}
	
	return false;
}

static bool Devices_NounHandler_GET(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	bool showDetails = false;
	bool showALDB = false;
	bool showLevels = false;
	bool forceLookup = false;
	bool onlyShowChanged = false;

	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	if(queries.count(string(JSON_VAL_DETAILS))) {
		string str = queries[string(JSON_VAL_DETAILS)];
		if( str == "true" ||  str =="1")
			showDetails = true;
	}
	if(queries.count(string(JSON_ARG_ALDB))) {
		string str = queries[string(JSON_ARG_ALDB)];
		if( str == "true" ||  str =="1")
			showALDB = true;
	}
	
	
	if(queries.count(string(JSON_ARG_FORCE))) {
		string str = queries[string(JSON_ARG_FORCE)];
		if( str == "true" ||  str =="1")
			forceLookup = true;
	}

	
	if(queries.count(string(JSON_VAL_LEVELS))) {
		string str = queries[string(JSON_VAL_LEVELS)];
		if( str == "true" ||  str =="1")
			showLevels = true;
	}

	// GET /devices
	if(path.size() == 1) {
		ServerCmdArgValidator v1;
		string str;
		
		//			 only valid devices?
		if(queries.count(string(JSON_VAL_VALID))) {
			str = queries[string(JSON_VAL_VALID)];
			if( str == "true" ||  str == "1")
				deviceIDs = db->validDevices();
		}
		else if(v1.getStringFromMap("If-None-Match", url.headers(), str)){
			char* p;
			long eTag = strtol(str.c_str(), &p, 0);
			if(*p == 0){
				deviceIDs = db->devicesUpdateSinceEtag(eTag);
			}
			else {
				deviceIDs = db->devicesUpdateSinceEtag(0);
			}
			
			reply[string(JSON_ARG_ETAG)] = db->lastEtag();
			onlyShowChanged = true;
		}
		else if(v1.getStringFromMap("If-Modified-Since", url.headers(), str)){

			deviceIDs = db->allDevices();
			using namespace timestamp;
			time_t time =  TimeStamp(str).getTime();
			time_t lastUpdate =  ::time(NULL);  // in case we have no updates
			deviceIDs = db->devicesUpdateSince(time, &lastUpdate);
			reply["lastUpdated"] =  TimeStamp(lastUpdate).RFC1123String();
			onlyShowChanged = true;
		}
		else
		{
			// simple List all devices.
			deviceIDs = db->allDevices();
		}
//		if(onlyShowChanged && deviceIDs.size() == 0){
//
//			reply[string(JSON_ARG_ETAG)] = db->lastEtag();
//
//						makeStatusJSON(reply,STATUS_OK);
//						(completion) (reply, STATUS_OK);
//
////			makeStatusJSON(reply,STATUS_NOT_MODIFIED);
////			(completion) (reply, STATUS_NOT_MODIFIED);
// 		return true;
//		}
//		else
		if(showDetails){
			json devicesEntries;
			for(DeviceID deviceID :deviceIDs) {
				
				json entry;
				if(DeviceDetailJSONForDeviceID(deviceID	, entry, showALDB) ){
					string strDeviceID = deviceID.string();
					devicesEntries[strDeviceID] = entry;
				}
			}
			reply[string(JSON_ARG_DETAILS)] = devicesEntries;
			reply[string(JSON_ARG_ETAG)] = db->lastEtag();
 		}
	
 
	/*
				
				insteon_dbEntry_t info;
				if( db->getDeviceInfo(deviceID,  &info)) {
					
					string strDeviceID = deviceID.string();
					
					json entry;
					entry[string(JSON_VAL_VALID)] = info.isValidated;
					entry[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
					entry[string(JSON_ARG_NAME)] = 	info.name;
					entry["lastUpdated"] =  TimeStamp(info.lastUpdated).RFC1123String();
					
					entry[string(JSON_ARG_IS_KEYPAD)]  =  info.deviceInfo.isKeyPad();
					entry[string(JSON_ARG_IS_DIMMER)] =  info.deviceInfo.isDimmer();
					entry[string(JSON_ARG_IS_PLM)] =  info.deviceInfo.isPLM();
	
 					devicesEntries[strDeviceID] = entry;
				}
				
				reply[string(JSON_ARG_DETAILS)] = devicesEntries;
				
		*/
				
		 
		else if(showLevels) {
			json devicesEntries;
			for(DeviceID deviceID :deviceIDs) {
				json entry;
	
 				uint8_t  onLevel = 0;
				
				if( db->getDBOnLevel(deviceID, 0x01, &onLevel)) {
					entry[string(JSON_ARG_LEVEL)] = onLevel; // InsteonDevice::onLevelString(onLevel);
					devicesEntries[deviceID.string()] = entry;
				}
				
			}
			reply[string(JSON_ARG_LEVELS)] = devicesEntries;
			reply[string(JSON_ARG_ETAG)] = db->lastEtag();
 
			
//			if(onlyShowChanged && devicesEntries.size() == 0){
//				makeStatusJSON(reply,STATUS_NOT_MODIFIED);
//				(completion) (reply, STATUS_NOT_MODIFIED);
//				return true;
//			}
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
		return true;
	}
	
	// GET /devices/XX.XX.XX
	else if(path.size() == 2) {
		
		if(path.at(1) == JSON_ARG_DUMP) {
			
			string dump = db->dumpDB(showDetails);
			reply[string(JSON_ARG_DUMP)] = dump;
			
 			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
	 
			return true;
		}
		else {
			DeviceIDArgValidator vDeviceID;
			
			auto deviceStr = path.at(1);
			if(vDeviceID.validateArg(deviceStr)){
				DeviceID	deviceID = DeviceID(deviceStr);
				
				json reply;
				if(DeviceDetailJSONForDeviceID(deviceID	, reply, showALDB) ){
	
					if(forceLookup) {
		 
						insteon.getOnLevel(deviceID, true,
																	[=](uint8_t level, eTag_t eTag, bool didSucceed){
							json reply1 = reply;
							
							if(didSucceed){
								reply1[string(JSON_ARG_LEVEL)] = level;
								reply1[string(JSON_ARG_ETAG)]= eTag;
 							}
							
							makeStatusJSON(reply1,STATUS_OK);
							(completion) (reply1, STATUS_OK);
	 
						});
					}
					else {
						
						makeStatusJSON(reply,STATUS_OK);
						(completion) (reply, STATUS_OK);
						return true;
		
					}
				
				} else {
					makeStatusJSON(reply,STATUS_INTERNAL_ERROR);
					(completion) (reply, STATUS_INTERNAL_ERROR);
					return true;
	
				}
 				return true;
			}
		}
	}
	
	return false;
}

static bool Devices_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
													ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	auto db = insteon.getDB();
	
	DeviceID	deviceID;
	
	if(path.size() > 1) {
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
		}
		else
		{
			return false;
		}
	}
	
	if(path.size() == 2) {
		
		// delete device
		
		// is the device in our DB?
		if(!db->getDeviceInfo(deviceID, NULL)){
			return false;
		}
		
		bool queued =  insteon.unlinkDevice(deviceID,
														[=](bool didSucceed){
			json reply;
			if(didSucceed){
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		});
		if(!queued) {
			makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
			(completion) (reply, STATUS_UNAVAILABLE);
		}
		return true;
		
	}
	else if(path.size() == 3) {
		// delete device property
		insteon_dbEntry_t info;
		if( db->getDeviceInfo(deviceID,  &info)) {
			
			reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
			
			auto property = path.at(2);
			if(db->removeDeviceProperty(deviceID, property)){
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				return true;
			}
		}
	}
	
	return false;
}

static bool Devices_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	if(path.size() == 2) {
		
		DeviceIDArgValidator vDeviceID;
		DeviceLevelArgValidator vLevel;
		LEDBrightnessArgValidator vLED;
		ServerCmdArgValidator v1;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			int level;
			bool shouldBeep = false;
			bool shouldValidate = false;
			uint8_t buttonMask	= 0;
			bool isValidated = false;
			
			insteon_dbEntry_t info;
			if( db->getDeviceInfo(deviceID, &info)){
				isValidated =  info.isValidated;
			}
	  
			// set level
			if(isValidated && vLevel.getvalueFromJSON(JSON_ARG_LEVEL, url.body(), level)){
				
				
				bool queued = insteon.setOnLevel(deviceID, level,
															[=](eTag_t eTag, bool didSucceed){
					json reply;
					if(didSucceed){
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						reply[string(JSON_ARG_ETAG)] = eTag;
						reply[string(JSON_ARG_LEVEL)]  = level;
						makeStatusJSON(reply,STATUS_OK);
						(completion) (reply, STATUS_OK);
					}
					else {
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
						(completion) (reply, STATUS_BAD_REQUEST);
					}
				});
				
				if(!queued) {
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
					(completion) (reply, STATUS_UNAVAILABLE);
					return false;
				}
				return true;
			}
			// set backlight
			else if(isValidated && vLED.getvalueFromJSON(JSON_ARG_BACKLIGHT, url.body(), level)){
				
				bool queued = insteon.setLEDBrightness(deviceID, level,
																	[=]( bool didSucceed){
					json reply;
					if(didSucceed){
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						reply[string(JSON_ARG_LEVEL)]  = level;
						makeStatusJSON(reply,STATUS_OK);
						(completion) (reply, STATUS_OK);
					}
					else {
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						makeStatusJSON(reply, STATUS_BAD_REQUEST, "Set Failed" );;
						(completion) (reply, STATUS_BAD_REQUEST);
					}
				});
				
				if(!queued) {
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
					(completion) (reply, STATUS_UNAVAILABLE);
					return false;
				}
				return true;
			}
			
			//beep
			
			else if(isValidated && v1.getBoolFromJSON(JSON_ARG_BEEP, url.body(), shouldBeep)){
				
				InsteonDevice(deviceID).beep([=](bool didSucceed){
					json reply;
					
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				});
				return true;
			}
			
			else if(isValidated && v1.getByteFromJSON(JSON_ARG_KP_MASK, url.body(), buttonMask)){
				
				if(!info.deviceInfo.isKeyPad()){
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Device is not a keypad" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return false;
	
				}
				
				InsteonKeypadDevice(deviceID).setKeypadLEDState(buttonMask, [=](bool didSucceed){
					json reply;
					
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				});
				return true;
			}
			
			else if(v1.getBoolFromJSON(JSON_ARG_VALIDATE, url.body(), shouldValidate)){
				
				bool queued =  insteon.validateDevice(deviceID,[=](bool didSucceed){
					json reply;
					
					if(didSucceed){
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						makeStatusJSON(reply,STATUS_OK);
						(completion) (reply, STATUS_OK);
					}
					else {
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						makeStatusJSON(reply, STATUS_BAD_REQUEST, "Validate Failed" );;
						(completion) (reply, STATUS_BAD_REQUEST);
					}
				});
				
				if(!queued) {
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
					(completion) (reply, STATUS_UNAVAILABLE);
					return false;
				}
				return true;
			}
			
			if(!isValidated){
				json reply;
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Validate Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);

			}
		}
	}
	return false;
}

static bool Devices_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	auto body 	= url.body();
	
	json reply;
	
	auto db = insteon.getDB();
	
	if(path.size() == 2) {
		DeviceIDArgValidator vDeviceID;
		ServerCmdArgValidator v1;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			insteon_dbEntry_t info;
 			if( db->getDeviceInfo(deviceID,  &info)) {
				
				bool didUpdate = false;
				
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();

				for(auto it =  body.begin(); it != body.end(); ++it) {
					string key = Utils::trim(it.key());

					if(body[it.key()].is_string()){
						string value = Utils::trim(it.value());
						
						if(db->updateDeviceProperty(deviceID, key, value)){
							didUpdate = true;
						}
						 
					} else if(body[it.key()].is_null()){
						// delete property
						if(db->removeDeviceProperty(deviceID, key)){
							didUpdate = true;
						}
					}
				}
				
				if(didUpdate){
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Update Failed" );;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
				return true;
			}
		}
	}
	
	return false;
}

static void Devices_NounHandler(ServerCmdQueue* cmdQueue,
										  REST_URL url,
										  TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	
//	// is server available?
//	if(!insteon.serverAvailable()) {
//		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
//		(completion) (reply, STATUS_UNAVAILABLE);
//		return;
//	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Devices_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = Devices_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
	
		case HTTP_DELETE:
			isValidURL = Devices_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PATCH:
			isValidURL = Devices_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
			
	 
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
};

// MARK: KEYPADS NOUN HANDLERS

static bool KeypadDetailJSONForDeviceID( DeviceID deviceID, json &reply) {
	using namespace rest;
	using namespace timestamp;
	
	auto db = insteon.getDB();
	
	if(auto keypad = db->findKeypadEntryWithDeviceID(deviceID); keypad != NULL) {
		
		insteon_dbEntry_t info;
		
		reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
			
		// mirror device properties
		if( db->getDeviceInfo(deviceID,  &info)) {
			reply[string(JSON_ARG_NAME)] = 	info.name;
			reply[string(JSON_VAL_VALID)] = info.isValidated;
			reply[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
			reply["lastUpdated"] =  TimeStamp(info.lastUpdated).RFC1123String();
			
			if(info.properties.size()){
				json props;
				for(auto prop : info.properties){
					props[prop.first] = prop.second;
				}
				reply[string(JSON_ARG_PROPERTIES)]  = props;
			}
		}
		
		reply[string(JSON_ARG_CONFIG)] = int(keypad->buttonCount);
 
		json keyActions;
		json buttons;
		
		for(const auto& [kc, buttonEntry] : keypad->buttons)  {
			
			json button;
			json buttonAction;
			
			button[string(JSON_ARG_NAME)] = 	buttonEntry.buttonName;
			button[string(JSON_ARG_LEVEL)] = 	buttonEntry.isOn?"on":"off";
	 
			for(const auto& [cmd, action] : buttonEntry.actions)  {
				
				string cmdStr = to_hex <unsigned char> (cmd,true);
				switch (cmd) {
					case InsteonParser::CMD_LIGHT_ON:
						cmdStr = "on";
						break;
						
					case InsteonParser::CMD_LIGHT_OFF:
						cmdStr = "off";
						break;
						
					default:
						break;
				}
				
				buttonAction[ cmdStr] = Action(action).JSON();
			}
			
			button[string(JSON_ARG_ACTIONS)] = buttonAction;
			buttons[to_string( kc)] = button;
		}
		
		reply[string(JSON_ARG_BUTTONS)] = buttons;
		return true;
	}
	
	return false;
}
static bool Keypads_NounHandler_GET(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	bool showDetails = false;
	bool forceLookup = false;

	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	if(queries.count(string(JSON_VAL_DETAILS))) {
		string str = queries[string(JSON_VAL_DETAILS)];
		if( str == "true" ||  str =="1")
			showDetails = true;
	}

	if(queries.count(string(JSON_ARG_FORCE))) {
		string str = queries[string(JSON_ARG_FORCE)];
		if( str == "true" ||  str =="1")
			forceLookup = true;
	}

	// GET /keypads
	if(path.size() == 1) {
		
		deviceIDs = db->allKeypads();

		if(showDetails){
			json devicesEntries;
			for(DeviceID deviceID :deviceIDs) {
				
				json entry;
				if(KeypadDetailJSONForDeviceID(deviceID	, entry) ){
					string strDeviceID = deviceID.string();
					devicesEntries[strDeviceID] = entry;
				}
			}
			reply[string(JSON_ARG_DETAILS)] = devicesEntries;
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;

		}
		
		else {
			vector<string> deviceList;
			
			for(DeviceID deviceID :deviceIDs) {
				deviceList.push_back(deviceID.string());
			}
			reply[string(JSON_ARG_DEVICEIDS)] = deviceList;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
			
		}
	}
	// GET /keypads/XX.XX.XX
	else if(path.size() == 2) {
		
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			json reply;
			if( KeypadDetailJSONForDeviceID(deviceID, reply )) {
				
				if(forceLookup){
		
						InsteonKeypadDevice(deviceID).getButtonConfiguration( [=](bool eightKey, bool didSucceed){
							json reply1 = reply;
							
							if(didSucceed) {
								
								db->setKeypadButtonCount(deviceID, eightKey?8:6);
								reply1[string(JSON_ARG_CONFIG)] =  eightKey?8:6;
							}

							makeStatusJSON(reply1,STATUS_OK);
							(completion) (reply1, STATUS_OK);
						});
					return true;

				}
				else {
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
					return true;

				}
					
			}
	
		}
		
	}
	return false;
}

static bool Keypads_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	auto db = insteon.getDB();
	DeviceID  deviceID;
	
	bool isValidated = false;

	if(path.size() == 3) {
		
		// keypad/<deviceID>/buttonID/
		
		DeviceIDArgValidator vDeviceID;
		DeviceLevelArgValidator vLevel;
	
		auto deviceStr = path.at(1);
		auto buttonStr = path.at(2);
		
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
			
			insteon_dbEntry_t info;
			if( db->getDeviceInfo(deviceID, &info)){
	//			isValidated =  info.isValidated;
				isValidated = true;
			}
			
			if( regex_match(string(buttonStr), std::regex("^[1-8]$"))){
				uint8_t buttonID;
				if( std::sscanf(buttonStr.c_str(), "%hhd", &buttonID) == 1){
				
					if(auto keypad = db->findKeypadEntryWithDeviceID(deviceID); keypad != NULL)
						if(auto button =  db->findKeypadButton(keypad, buttonID) ; button != NULL){
							
							// set button level
							int level;
							if(isValidated && vLevel.getvalueFromJSON(JSON_ARG_LEVEL, url.body(), level)){
								
								uint8_t cmd = level > 0? InsteonParser::CMD_LIGHT_ON: InsteonParser::CMD_LIGHT_OFF;
								
								bool queued = insteon.runActionForKeypad(deviceID, buttonID, cmd,
																					  [=]( bool didSucceed){
									json reply;
									
									if(didSucceed){
										makeStatusJSON(reply,STATUS_OK);
										(completion) (reply, STATUS_OK);
									}
									else {
										makeStatusJSON(reply, STATUS_BAD_REQUEST, "Run Keypad Failed" );;
										(completion) (reply, STATUS_BAD_REQUEST);
									}
								});
		 
								if(!queued) {
									makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
									(completion) (reply, STATUS_UNAVAILABLE);
									return true;
								}
								return true;
 							}
						}
				}
			}
		}
	}
 	 
	return false;
}

static bool Keypads_NounHandler_POST(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	auto body 	= url.body();
	
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	ServerCmdArgValidator v1;
	
	auto db = insteon.getDB();
	
	DeviceID	deviceID;
	uint8_t buttonID = 0;
	
	bool didUpdate = false;
	
	if(path.size() > 1) {
		
		auto deviceStr = path.at(1);
		
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
			
			string buttonStr;
			
			if(v1.getStringFromJSON(JSON_ARG_BUTTON, url.body(), buttonStr)){
				
				// validate button ID
				
				if( regex_match(string(buttonStr), std::regex("^[1-8]$"))){
					std::sscanf(buttonStr.c_str(), "%hhd", &buttonID);
					
				}
			}
			
			
			// is this an existing keypad?
			if(auto keypad = db->findKeypadEntryWithDeviceID(deviceID); keypad != NULL) {
				
				// Guard on buttonID
				if(buttonID == 0) {
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "No Button was specified" );;
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
				
				didUpdate = db->createKeypadButton(deviceID, buttonID);
			}
			// we are creating a new keypad.
			
			else {
				
				// Guard on no buttonID

				if(buttonID != 0) {
					makeStatusJSON(reply, STATUS_BAD_REQUEST, "Create the keypad before creating a button." );
					(completion) (reply, STATUS_BAD_REQUEST);
					return true;
				}
				
				didUpdate = db->createKeypad(deviceID);
 			}
		}
	}
	
	if(didUpdate){
		reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Update Failed" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
	return true;
}

static bool Keypads_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	auto body 	= url.body();
	
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	ServerCmdArgValidator v1;
	
	auto db = insteon.getDB();
	
	DeviceID	deviceID;
	keypad_dbEntry_t* keypad = NULL;
	uint8_t actionID = 0;
	uint8_t buttonID = 0;

	bool didUpdate = false;
	
	if(path.size() > 1) {
		auto deviceStr = path.at(1);
		
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
			keypad = db->findKeypadEntryWithDeviceID(deviceID);
		}
	}
	// guard on keypad
	if(!keypad) return false;
	
	
	// PATCH /keypads/XX.XX.XX/
	if(path.size() == 2) {
		
		int buttonCount = 0;
		
		// set button Count
		v1.getIntFromJSON(JSON_ARG_CONFIG, url.body(), buttonCount);
		
		// Guard Button Count
		if(buttonCount != 8 && buttonCount != 6) {
			
			makeStatusJSON(reply, STATUS_BAD_REQUEST, "Keypads either have 6 or 8 buttons " );;
			(completion) (reply, STATUS_BAD_REQUEST);
			return true;
		}
		
		bool queued = InsteonKeypadDevice(deviceID).setButtonConfiguration(buttonCount == 8, [=](bool didSucceed){
			json reply;
			
			if(didSucceed){
				db->setKeypadButtonCount(deviceID, buttonCount);

				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				makeStatusJSON(reply, STATUS_BAD_REQUEST, "Update Failed" );;
				(completion) (reply, STATUS_BAD_REQUEST);
			}
		});
		
		if(!queued) {
			makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
			(completion) (reply, STATUS_UNAVAILABLE);
			return true;
		}
		return true;
	}

	// PATCH /keypads/XX.XX.XX/<1-8>
	if(path.size() > 2) {
		auto buttonStr = path.at(2);
		
		keypad_Button_t* button = NULL;
		if( regex_match(string(buttonStr), std::regex("^[1-8]$"))){
			if( std::sscanf(buttonStr.c_str(), "%hhd", &buttonID) == 1){
				button =  db->findKeypadButton(keypad, buttonID);
			}
		}
		// guard on button
		if(buttonID == 0) return false;
	}
  
	// update keypad button property
	
	if(path.size() == 3) {
			
		string name;
		// set button Name
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
			didUpdate = db->setNameForKeyPadButton(keypad->deviceID, buttonID, name);
		}
 	}
	
	// update keypad button property
	// PATCH /keypads/XX.XX.XX/<1-8>/<on-off>
	
	else if(path.size() == 4) {
		auto actionIDStr = path.at(3);
		
		if(actionIDStr ==  "on"){
			actionID = InsteonParser::CMD_LIGHT_ON;
		} else if(actionIDStr ==  "off"){
			actionID = InsteonParser::CMD_LIGHT_OFF;
		} else
			// guard on actionID
			return false;
		
		json  j =  url.body();
		
		if( j.contains(JSON_ARG_ACTION)
			&& j.at(string(JSON_ARG_ACTION)).is_object()){
			auto a = j.at(string(JSON_ARG_ACTION));
			Action action = Action(a);
			
			if(action.isValid()) {
				didUpdate = db->setActionForKeyPadButton(keypad->deviceID, buttonID, actionID, action);
			}
		}
		
	}
 
	if(didUpdate){
		reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
	}
	else {
		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Update Failed" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
	return true;
}
 



static bool Keypads_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	auto body 	= url.body();
	
	json reply;
	
	DeviceIDArgValidator vDeviceID;
	ServerCmdArgValidator v1;
	
	auto db = insteon.getDB();
	
	keypad_dbEntry_t* keypad = NULL;
	keypad_Button_t* button = NULL;
	
	DeviceID	deviceID;
	uint8_t buttonID = 0;

	bool didUpdate = false;
	
	if(path.size() > 2) {
		auto deviceStr = path.at(1);
		auto buttonStr = path.at(2);
		
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
			keypad = db->findKeypadEntryWithDeviceID(deviceID);
			
			// guard on keypad
			if(!keypad) return false;
			
			if( regex_match(string(buttonStr), std::regex("^[1-8]$"))){
				if( std::sscanf(buttonStr.c_str(), "%hhd", &buttonID) == 1){
					button =  db->findKeypadButton(keypad, buttonID);
				}
			}
		}
	}
	
	// guard on button
	if(!button) return false;

	didUpdate = db->removeKeypadButton(deviceID, buttonID);
	
	
	if(didUpdate){
		makeStatusJSON(reply,STATUS_NO_CONTENT);
		(completion) (reply, STATUS_NO_CONTENT);
	}
	else {
		reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
		reply[string(JSON_ARG_BUTTON)] = to_string(buttonID);

		makeStatusJSON(reply, STATUS_BAD_REQUEST, "Delete Failed" );;
		(completion) (reply, STATUS_BAD_REQUEST);
	}
	return true;
}

static void Keypads_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,  // entire request
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	switch(url.method()){
		case HTTP_GET:
 			isValidURL = Keypads_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PUT:
			isValidURL = Keypads_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PATCH:
			isValidURL = Keypads_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_POST:
			isValidURL = Keypads_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_DELETE:
			isValidURL = Keypads_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
}


// MARK: PLM NOUN HANDLERS

static bool PLM_NounHandler_GET(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	
	DeviceID deviceID;
	DeviceInfo deviceInfo;
	
	auto db = insteon.getDB();

	if(path.size() == 2) {
		
		string subpath =   path.at(1);
		
		if(subpath == SUBPATH_INFO){
			
			auto state = insteon.currentState();
			string stateStr = insteon.currentStateString();
			
			reply[string(JSON_ARG_STATE)] 		=   state;
			reply[string(JSON_ARG_STATESTR)] 	=   stateStr;
			
			string path = db->getPLMPath();
			reply[string(JSON_ARG_FILEPATH)] = path;
			reply[string(JSON_ARG_AUTOSTART)] = db->getPLMAutoStart();
			reply[string(JSON_ARG_REMOTETELNET)] = db->getAllowRemoteTelnet();
			
			if( insteon.plmInfo(&deviceID, &deviceInfo) ){
				
				reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
				reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
				reply[string(JSON_ARG_COUNT)] = db->count();
			}
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
		}
		
	}

	return false;
}

static bool PLM_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
 
	json reply;
	
	if(path.size() == 2) {
		
		string subpath =   path.at(1);
		if(subpath == SUBPATH_DATABASE){
			
			string filepath;
			
			if(v1.getStringFromJSON(JSON_ARG_SAVE, url.body(), filepath)){
				
				bool success = db->backupCacheFile(filepath);
				if(success){
					
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					string lastError =  string("Error: ") + to_string(errno);
					string lastErrorString = string(::strerror(errno));
					
					makeStatusJSON(reply, STATUS_BAD_REQUEST, lastError, lastErrorString);;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
				return true;
			}
			else if(v1.getStringFromJSON(JSON_ARG_LOAD, url.body(), filepath)){
				bool success = insteon.loadCacheFile(filepath);
				if(success){
					
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					string lastError =  string("Error: ") + to_string(errno);
					string lastErrorString = string(::strerror(errno));
					
					makeStatusJSON(reply, STATUS_BAD_REQUEST, lastError, lastErrorString);;
					(completion) (reply, STATUS_BAD_REQUEST);
				}
				return true;

			}
		}
		else if(subpath == SUBPATH_STATE){
			
			string str;
			
			auto currentState = insteon.currentState();
			string stateStr = insteon.currentStateString();
		
			if(v1.getStringFromJSON(JSON_ARG_STATE, url.body(), str)){
				
				std::transform(str.begin(), str.end(), str.begin(), ::tolower);
				if(str == JSON_VAL_START){
					
					switch (currentState) {
						case InsteonMgr::STATE_SETUP:
						case InsteonMgr::STATE_NO_PLM:
						case InsteonMgr::STATE_PLM_ERROR:
						case InsteonMgr::STATE_PLM_STOPPED:
 							break;
							
						default:
 							makeStatusJSON(reply, STATUS_BAD_REQUEST, "PLM in wrong state",stateStr );
							(completion) (reply, STATUS_BAD_REQUEST);
							return true;
							break;
					}
				  
					try{
						insteon.begin("",  [=](bool didSucceed) {
							
							if(didSucceed){
								
								insteon.syncPLM( [=](bool didSucceed) {
									
									if(didSucceed){
										
										// start validation process in background
										insteon.validatePLM( [](bool didSucceed) {
											
											// let this run in background
										});
										
										DeviceID 	 deviceID;
										DeviceInfo deviceInfo;
										json reply;
										
										if( insteon.plmInfo(&deviceID, &deviceInfo) ){
											reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
											reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
										}
										
										string path = db->getPLMPath();
										reply[string(JSON_ARG_FILEPATH)] = path;
										reply[string(JSON_ARG_COUNT)] = db->count();
					
										auto state = insteon.currentState();
										string stateStr = insteon.currentStateString();
										
										reply[string(JSON_ARG_STATE)] 		=   state;
										reply[string(JSON_ARG_STATESTR)] 	=   stateStr;
	
										makeStatusJSON(reply,STATUS_OK);
										(completion) (reply, STATUS_OK);
									}
									else {
										json reply;
										
										makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "syncPLM Failed");;
										(completion) (reply, STATUS_INTERNAL_ERROR);
									}
								});
							}
							else {
								json reply;
								makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "start PLM Failed");;
								(completion) (reply, STATUS_INTERNAL_ERROR);
							}
						});
					}
				 
					  catch ( const InsteonException& e)  {
						  
						  json reply;
						  makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "start PLM Failed", e.what());;
						  (completion) (reply, STATUS_INTERNAL_ERROR);
 					  }
			 		 return true;
				}
				else 	if(str == JSON_VAL_STOP){
					
					insteon.stop();
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
					return true;
				}
				else 	if(str == JSON_VAL_RESET){
					
//					insteon.stop();
					try{
						insteon.erasePLM( [=](bool didSucceed) {
							json reply;
							
							if(didSucceed){
								
								makeStatusJSON(reply,STATUS_OK);
								(completion) (reply, STATUS_OK);
							}
							else {
								makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "syncPLM Failed");;
								(completion) (reply, STATUS_INTERNAL_ERROR);
							}
						});
					}
					
					catch ( const InsteonException& e)  {
						
						json reply;
						makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "start PLM Failed", e.what());;
						(completion) (reply, STATUS_INTERNAL_ERROR);
					}
					return true;
				}
  			}
 		}
	}
	return false;
}

static bool PLM_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
 
	json reply;
	
	if(path.size() == 2) {
		
		string subpath =   path.at(1);
		
		if(subpath == SUBPATH_PORT){
			
			bool didPatch = false;
			
			string filepath;
			bool  autostart = false;
			bool  remoteTelnet = false;
	
			if(v1.getStringFromJSON(JSON_ARG_FILEPATH, url.body(), filepath)){
				db->setPLMpath(filepath);
				didPatch = true;
			}
	
			if(v1.getBoolFromJSON(JSON_ARG_AUTOSTART, url.body(), autostart)){
				db->setPLMAutoStart(autostart);
				didPatch = true;
			}

			if(v1.getBoolFromJSON(JSON_ARG_REMOTETELNET, url.body(), remoteTelnet)){
				db->setAllowRemoteTelnet(	remoteTelnet);
				didPatch = true;
			}
 
			if(didPatch){
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				return true;
			}
		}
	}
	return false;
}


static void PLM_NounHandler(ServerCmdQueue* cmdQueue,
									 REST_URL url,
									 TCPClientInfo cInfo,
									 ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	 
	switch(url.method()){
		case HTTP_GET:
			isValidURL = PLM_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = PLM_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PATCH:
			isValidURL = PLM_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
			
 		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
};

// MARK: CONFIG - NOUN HANDLER



static bool Config_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
 
	if(path.size() == 1){
		
		double latitude, longitude;
	 
		db->getLatLong(latitude, longitude);
		reply[string(JSON_ARG_LATITUDE)] = latitude;
		reply[string(JSON_ARG_LONGITUDE)] = longitude;
		
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
	}
	return false;
}

static bool Config_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
												 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	json reply;
	
	ServerCmdArgValidator v1;
	auto db = insteon.getDB();
	
	if(path.size() == 1) {
				
		double latitude, longitude;
		
		if(v1.getDoubleFromJSON(JSON_ARG_LATITUDE, url.body(), latitude)
			&& v1.getDoubleFromJSON(JSON_ARG_LONGITUDE, url.body(), longitude)){
			
			if(latitude != 0 && longitude != 0){
				db->setLatLong(latitude, longitude);
				insteon.refreshSolarEvents();
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				return true;
			}
		}
	}
	return false;
}

static void Config_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
 
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Config_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
	//		isValidURL = Config_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PATCH:
			isValidURL = Config_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;
	 
		case HTTP_POST:
//			isValidURL = Config_NounHandler_POST(cmdQueue,url,cInfo, completion);
			break;
 
		case HTTP_DELETE:
//			isValidURL = Config_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
}



// MARK: STATUS - NOUN HANDLER

static bool Status_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;

	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;

 
	// GET /status
	if(path.size() == 1) {
	 
		auto state = insteon.currentState();
		string stateStr = insteon.currentStateString();
		
		reply[string(JSON_ARG_STATE)] =   state;
		reply[string(JSON_ARG_STATESTR)] =   stateStr;
		
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
 	}
 
	  return false;
}

static void Status_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,  // entire request
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Status_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
//
//		case HTTP_PUT:
//			isValidURL = Status_NounHandler_PUT(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_PATCH:
//			isValidURL = Status_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_POST:
//			isValidURL = Status_NounHandler_POST(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_DELETE:
//			isValidURL = Status_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
//			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
}

// MARK: LOG - NOUN HANDLER

static bool Log_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;

	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	json reply;
	auto db = insteon.getDB();

	string subpath;
	
	if(path.size() > 1){
		subpath =   path.at(1);
	}

	// GET /log
	if(subpath == SUBPATH_STATE) {
  
		reply[string(JSON_ARG_LOGFLAGS)] =  LogMgr::shared()->_logFlags;
 
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
	}
	else if(subpath == SUBPATH_FILE) {
		
		string path;
		db->logFileGetPath(path);
		reply[string(JSON_ARG_FILEPATH)] =  path;
 	
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
	}
		
	  return false;
}

static bool Log_NounHandler_PUT(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	ServerCmdArgValidator v1;
 
	json reply;
	string subpath;

	auto db = insteon.getDB();

	if(path.size() > 1){
		subpath =   path.at(1);
	}

 	if(subpath == SUBPATH_STATE) {
		uint8_t logFlags;
	
		if(v1.getByteFromJSON(JSON_ARG_LOGFLAGS, url.body(), logFlags)){
			
			LogMgr::shared()->_logFlags = logFlags;
			db->logFileSetFlags(logFlags);

			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;

		}
	}
	else if(subpath == SUBPATH_FILE) {
		
		string path;
		
		if(v1.getStringFromJSON(JSON_ARG_FILEPATH, url.body(), path)){
			//VINNIE		// set the log file
			
			LogMgr::shared()->setLogFilePath(path);
			db->logFileSetPath(path);
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
			
		}
	}
 
	return false;
}

static bool Log_NounHandler_PATCH(ServerCmdQueue* cmdQueue,
												REST_URL url,
												TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	ServerCmdArgValidator v1;
 
	json reply;
 
	string str;
	
	if(v1.getStringFromJSON(JSON_ARG_MESSAGE, url.body(), str)){
		
		LogMgr::shared()->logTimedStampString(str);
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
	}

	return false;
}

static void Log_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,  // entire request
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Log_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PUT:
			isValidURL = Log_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PATCH:
			isValidURL = Log_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
			break;

//		case HTTP_POST:
//			isValidURL = Log_NounHandler_POST(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_DELETE:
//			isValidURL = Log_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
//			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
}

// MARK: LINK - NOUN HANDLER

static bool Link_NounHandler_GET(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	DeviceIDArgValidator vDeviceID;
	DeviceID	deviceID;
	auto db = insteon.getDB();

	bool isValidURL = false;

	
	auto path = url.path();

	if(path.size() < 1) {
		return false;
	}
 
	
	if(path.size() > 1) {
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
		}
	}
	
	if(path.size() == 2) { // update ALDB from a device
	
		bool queued = insteon.updateALDBfromDevice(deviceID, [=](bool didSucceed) {
			json reply;
			  
			if(didSucceed){
				
				insteon_dbEntry_t info;
				
				if( db->getDeviceInfo(deviceID,  &info)) {
					
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				
					json aldbJSON;
					
					for(auto aldb :info.deviceALDB){
						json aldbEntry;
						DeviceID aldbDev = DeviceID(aldb.devID);
						aldbEntry[string(JSON_ARG_DEVICEID)] = aldbDev.string();
						aldbEntry[string(JSON_VAL_ALDB_FLAG)] =  to_hex <unsigned char>(aldb.flag);
						aldbEntry[string(JSON_VAL_ALDB_ADDR)] =  to_hex <unsigned short>(aldb.address);
						aldbEntry[string(JSON_VAL_ALDB_GROUP)] =  to_hex <unsigned char>(aldb.group);
						
						aldbJSON[to_hex <unsigned short>(aldb.address)] = aldbEntry;
					}
					
					reply[string(JSON_VAL_ALDB)] = aldbJSON;
				}
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			  else {
				  makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Delete Failed", "updateALDBfromDevice failed" );;
				  (completion) (reply, STATUS_INTERNAL_ERROR);
			  }

		  });
		  
		  if(!queued) {
			  makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
			  (completion) (reply, STATUS_UNAVAILABLE);
		  }
		  return true;
	}
	
	return isValidURL;
}

// Linking keypad we add in responder for group 1-8
static void link_Keypad(DeviceID	deviceID, boolCallback_t cb){
	
	using namespace rest;
	auto db = insteon.getDB();
	
	vector<pair<bool,uint8_t>> aldbGroups
	{ {true, 0x01}, {true, 0x02}, {true, 0x03}, {true, 0x04},
		{true, 0x05}, {true, 0x06},{true, 0x07}, {true, 0x08}};
	
	insteon.addToDeviceALDB(deviceID, aldbGroups, [=](bool didSucceed) {
		
		if(!didSucceed){ (cb)(false); return; }
		
		// create a keypad entry
		db->createKeypad(deviceID);
		
		// update it with button count
		
		InsteonKeypadDevice(deviceID).getButtonConfiguration( [=](bool eightKey, bool didSucceed){
			if(!didSucceed){ (cb)(false); return; }
			db->setKeypadButtonCount(deviceID, eightKey?8:6);
			(cb)(true);
		});
	});
}

// Linking non keypad we add in master for group 1 ?

static void link_nonKeypad(DeviceID	deviceID, boolCallback_t cb){

	using namespace rest;

	insteon.addToDeviceALDB(deviceID, false, 0x01, [=](bool didSucceed) {
		(cb)(didSucceed);
	});

}


static bool Link_NounHandler_PUT(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	bool isValidURL = false;

	auto db = insteon.getDB();

	ServerCmdArgValidator v1;
 	
	auto path = url.path();
	string noun;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
 
	if(path.size() == 1) {  // just link
		isValidURL = true;
		
		
		///  ViNNIE WRITE THIS CODE
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
	}
	else if(path.size() == 2) { // link a specific device.
		
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			bool queued =  insteon.linkDevice(deviceID, true, 0xFE, [=](bool didSucceed) {
				
				if(didSucceed){
		
					// what kind of device did we link?
					insteon_dbEntry_t info;
					if( ! db->getDeviceInfo(deviceID,  &info)){
						json reply;
						makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "getDeviceInfo failed" );
						(completion) (reply, STATUS_INTERNAL_ERROR);
						return ;
					}

			
					// link additional ALDB entries
	
					if(info.deviceInfo.isKeyPad()) {
						// is it a keypad?
			
						link_Keypad(deviceID, [=](bool didSucceed){
						
							json reply;
							reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
			 
						if(didSucceed){
							makeStatusJSON(reply,STATUS_OK);
							(completion) (reply, STATUS_OK);
						}
						else {
							makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addToDeviceALDB failed" );;
							(completion) (reply, STATUS_INTERNAL_ERROR);
						}

						});
					}
					else {
						// not a keypad?
			
						link_nonKeypad(deviceID, [=](bool didSucceed){
							json reply;
							reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
			 
							if(didSucceed){
								makeStatusJSON(reply,STATUS_OK);
								(completion) (reply, STATUS_OK);
							}
							else {
								makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addToDeviceALDB failed" );;
								(completion) (reply, STATUS_INTERNAL_ERROR);
							}

						});
					}
				}
				else {
					json reply;
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
					makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed" );;
					(completion) (reply, STATUS_INTERNAL_ERROR);
				}
			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
				(completion) (reply, STATUS_UNAVAILABLE);
			}
 
			isValidURL = true;
		}}
	else if(path.size() == 3) { //  add To DeviceALDB
		
		//   link/33.4F.F6/group
		// isCNTRL = true?
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		auto groupStr = path.at(2);
		
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			if( regex_match(string(groupStr), std::regex("^[A-Fa-f0-9]{2}$"))){
				uint8_t groupID;
				bool isCNTRL = false;
				if( std::sscanf(groupStr.c_str(), "%hhx", &groupID) == 1){
					
					v1.getBoolFromJSON(JSON_ARG_ISCNTRL, url.body(), isCNTRL);
					
					bool queued =  insteon.addToDeviceALDB(deviceID, isCNTRL, groupID, [=](bool didSucceed) {
						json reply;
						
						if(didSucceed){
							
							makeStatusJSON(reply,STATUS_OK);
							(completion) (reply, STATUS_OK);
						}
						else {
							makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addToDeviceALDB failed" );;
							(completion) (reply, STATUS_INTERNAL_ERROR);
						}
					});
					
					if(!queued) {
						makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
						(completion) (reply, STATUS_UNAVAILABLE);
					}
					
					isValidURL = true;
				}
			}
		}
	}
	
	return isValidURL;
 }


static bool Link_NounHandler_DELETE(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	DeviceIDArgValidator vDeviceID;
	DeviceID	deviceID;
  
	auto path = url.path();

	if(path.size() < 1) {
		return false;
	}
 
	
	if(path.size() > 1) {
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			deviceID = DeviceID(deviceStr);
		}
	}
 
	// DELETE /link/XX.XX.XX/XXXX
	if(path.size() == 3 && !deviceID.isNULL()) {
		auto str = path.at(2);
		uint16_t address = 0;
		
		if( regex_match(string(str), std::regex("^[0-9a-fA-F]{4}$"))
				  && ( std::sscanf(str.c_str(), "%hx", &address) == 1)){

			
			printf(" Do something with ADLB addr:%04x on deviceID: %s\n",
					 address, deviceID.string().c_str() );
			
			// do something with ADLB addr on deviceID
			
			bool queued = insteon.removeEntryFromDeviceALDB(deviceID, address, [=](bool didSucceed) {
				json reply;
				
				if(didSucceed){

					makeStatusJSON(reply,STATUS_NO_CONTENT);
					(completion) (reply, STATUS_NO_CONTENT);
				}
				else {
					makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Delete Failed", "removeEntryFromDeviceALDB failed" );;
					(completion) (reply, STATUS_INTERNAL_ERROR);
				}

			});
			
			if(!queued) {
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
				(completion) (reply, STATUS_UNAVAILABLE);
 			}
			return true;
		}
		
	}
	return false;
}

static void Link_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,  // entire request
										 TCPClientInfo cInfo,
										 ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	json reply;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	string noun;
	
	bool isValidURL = false;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	switch(url.method()){
		case HTTP_GET:
			isValidURL = Link_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;

		case HTTP_PUT:
			isValidURL = Link_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;

//		case HTTP_PATCH:
//			isValidURL = Link_NounHandler_PATCH(cmdQueue,url,cInfo, completion);
//			break;

//		case HTTP_POST:
//			isValidURL = Log_NounHandler_POST(cmdQueue,url,cInfo, completion);
//			break;
//
		case HTTP_DELETE:
			isValidURL = Link_NounHandler_DELETE(cmdQueue,url,cInfo, completion);
			break;
  
		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
}

// MARK:  OTHER REST NOUN HANDLERS


static void Version_NounHandler(ServerCmdQueue* cmdQueue,
										  REST_URL url,
										  TCPClientInfo cInfo,
										  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	json reply;
	
	// CHECK METHOD
	if(url.method() != HTTP_GET ) {
		(completion) (reply, STATUS_INVALID_METHOD);
		return;
	}
	
	auto path = url.path();
	string noun;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	// CHECK sub paths
	if(noun != NOUN_VERSION){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	reply[string(JSON_ARG_VERSION)] = InsteonMgr::InsteonMgr_Version;
	reply[string(JSON_ARG_TIMESTAMP)]	=  string(__DATE__) + " " + string(__TIME__);
	
	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
};

static void Date_NounHandler(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	string plmFunc;

	// CHECK METHOD
	if(url.method() != HTTP_GET ) {
		(completion) (reply, STATUS_INVALID_METHOD);
		return;
	}
	
	auto path = url.path();
	string noun;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	// CHECK sub paths
	if(noun != NOUN_DATE){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	reply["date"] = TimeStamp().RFC1123String();

	solarTimes_t solar;
	insteon.getSolarEvents(solar);

	reply["civilSunRise"] = solar.civilSunRiseMins;
	reply["sunRise"] = solar.sunriseMins;
	reply["sunSet"] = solar.sunSetMins;
	reply["civilSunSet"] = solar.civilSunSetMins;
 	reply[string(JSON_ARG_LATITUDE)] = solar.latitude;
	reply[string(JSON_ARG_LONGITUDE)] = solar.longitude;
	reply["gmtOffset"] = solar.gmtOffset;
	reply["timeZone"] = solar.timeZoneString;
	reply["midnight"] = solar.previousMidnight;
	reply["uptime"]	= solar.upTime;

	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

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

void registerServerCommands() {
	
	// create the server command processor
	auto cmdQueue = ServerCmdQueue::shared();
	 
	cmdQueue->registerNoun(NOUN_CONFIG, 	Config_NounHandler);
	cmdQueue->registerNoun(NOUN_STATUS, 	Status_NounHandler);
	cmdQueue->registerNoun(NOUN_VERSION, 	Version_NounHandler);
	cmdQueue->registerNoun(NOUN_DATE, 		Date_NounHandler);
	cmdQueue->registerNoun(NOUN_LOG, 		Log_NounHandler);
	cmdQueue->registerNoun(NOUN_DEVICES, 	Devices_NounHandler);
	cmdQueue->registerNoun(NOUN_PLM, 		PLM_NounHandler);
	cmdQueue->registerNoun(NOUN_GROUPS,	Groups_NounHandler);
	cmdQueue->registerNoun(NOUN_INSTEON_GROUPS,	InsteonGroups_NounHandler);
	cmdQueue->registerNoun(NOUN_LINK,	Link_NounHandler);
	cmdQueue->registerNoun(NOUN_ACTION_GROUPS, ActionGroups_NounHandler);
	cmdQueue->registerNoun(NOUN_EVENTS,		 Events_NounHandler);
	cmdQueue->registerNoun(NOUN_EVENTS_GROUPS,  EventGroups_NounHandler);
 	cmdQueue->registerNoun(NOUN_KEYPADS,  Keypads_NounHandler);
	
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
