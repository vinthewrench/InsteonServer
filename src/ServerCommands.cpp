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

#include "Utils.hpp"

// MARK: - SERVER DEFINES

constexpr string_view FOO_VERSION_STRING		= "1.0.0";

constexpr string_view NOUN_VERSION		 		= "version";
constexpr string_view NOUN_DATE		 			= "date";
constexpr string_view NOUN_DEVICES	 			= "devices";
constexpr string_view NOUN_STATUS		 		= "status";
constexpr string_view NOUN_PLM			 		= "plm";
constexpr string_view NOUN_GROUPS			 	= "groups";
constexpr string_view NOUN_INSTEON_GROUPS		= "insteon.groups";
constexpr string_view NOUN_ACTION_GROUPS		= "action.groups";
constexpr string_view NOUN_EVENTS				= "events";
constexpr string_view NOUN_EVENTS_GROUPS		= "event.groups";

constexpr string_view NOUN_LINK	 				= "link";
 
constexpr string_view SUBPATH_INFO			 	= "info";
constexpr string_view SUBPATH_DATABASE		 	= "database";
constexpr string_view SUBPATH_RUN_ACTION		= "run.actions";

constexpr string_view JSON_ARG_DEVICEID 		= "deviceID";
constexpr string_view JSON_ARG_GROUPID 		= "groupID";
constexpr string_view JSON_ARG_ACTIONID 		= "actionID";
constexpr string_view JSON_ARG_EVENTID 		= "eventID";
//constexpr string_view JSON_ARG_INSTEON_GROUP = "insteon.group";
//constexpr string_view JSON_ARG_CMD			 	= "cmd";
constexpr string_view JSON_ARG_ACTION			= "action";
constexpr string_view JSON_ARG_TRIGGER			= "trigger";


constexpr string_view JSON_ARG_BEEP 			= "beep";
constexpr string_view JSON_ARG_LEVEL 			= "level";
constexpr string_view JSON_ARG_BACKLIGHT		= "backlight";
constexpr string_view JSON_ARG_VALIDATE		= "validate";
constexpr string_view JSON_ARG_DUMP		 		= "dump";			// for debug datat

constexpr string_view JSON_ARG_DATE				= "date";
constexpr string_view JSON_ARG_VERSION			= "version";
constexpr string_view JSON_ARG_TIMESTAMP		= "timestamp";
constexpr string_view JSON_ARG_DEVICEIDS 		= "deviceIDs";

constexpr string_view JSON_ARG_DEVICEINFO 	= "deviceInfo";
constexpr string_view JSON_ARG_DETAILS 		= "details";
constexpr string_view JSON_ARG_LEVELS 			= "levels";
constexpr string_view JSON_ARG_ACTIONS			= "actions";

constexpr string_view JSON_ARG_STATE			= "state";
constexpr string_view JSON_ARG_STATESTR		= "stateString";
constexpr string_view JSON_ARG_ETAG				= "ETag";
constexpr string_view JSON_ARG_FORCE			= "force";
constexpr string_view JSON_ARG_PROPERTIES		= "properties";
constexpr string_view JSON_ARG_NAME				= "name";
constexpr string_view JSON_ARG_GROUPIDS		= "groupIDs";
constexpr string_view JSON_ARG_EVENTIDS		= "eventIDs";
constexpr string_view JSON_ARG_FILEPATH		= "filepath";

constexpr string_view JSON_VAL_ALL				= "all";
constexpr string_view JSON_VAL_VALID			= "valid";
constexpr string_view JSON_VAL_DETAILS			= "details";
constexpr string_view JSON_VAL_LEVELS			= "levels";


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
	if(path.size() == 1) {;

		json eventList;
		auto eventIDs = db->allEventsIDs();
		for(auto eventID : eventIDs){
			json entry;

 			entry[string(JSON_ARG_NAME)] =  db->eventGetName(eventID);
			eventList[ to_hex<unsigned short>(eventID)] = entry;
 		}

		reply[string(JSON_ARG_EVENTIDS)] = eventList;
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
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
		if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)){
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
	
	// CHECK noun
	if(noun != NOUN_EVENTS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	// is server available?
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
	
	// CHECK noun
	if(noun != NOUN_EVENTS_GROUPS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	// is server available?
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
	
	// CHECK noun
	if(noun != NOUN_ACTION_GROUPS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	// is server available?
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
	// GET /devices/XX.XX.XX
	else if(path.size() == 2) {
		
 		GroupID groupID =  GroupID(path.at(1));
		if(!groupID.isValid() ||!db->groupIsValid(groupID))
			return false;
		
		reply[string(JSON_ARG_GROUPID)] = groupID.string();
		reply[string(JSON_ARG_NAME)] =  db->groupGetName(groupID);
		vector<DeviceID> deviceIDs = db->groupGetDevices(groupID	);
		vector<string> deviceList;
		for(DeviceID deviceID :deviceIDs) {
			deviceList.push_back(deviceID.string());
		}
		reply[string(JSON_ARG_DEVICEIDS)] = deviceList;
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
				makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
	
	// CHECK noun
	if(noun != NOUN_GROUPS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	// is server available?
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
	
	// CHECK noun
	if(noun != NOUN_INSTEON_GROUPS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
						makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
		if(onlyShowChanged && deviceIDs.size() == 0){
			makeStatusJSON(reply,STATUS_NOT_MODIFIED);
			(completion) (reply, STATUS_NOT_MODIFIED);
			return true;
		}
		else if(showDetails){
			json devicesEntries;
			for(DeviceID deviceID :deviceIDs) {
				insteon_dbEntry_t info;
				if( db->getDeviceInfo(deviceID,  &info)) {
					
					string strDeviceID = deviceID.string();
					
					json entry;
					entry[string(JSON_VAL_VALID)] = info.isValidated;
					entry[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
					entry[string(JSON_ARG_NAME)] = 	info.name;
					entry["lastUpdated"] =  TimeStamp(info.lastUpdated).RFC1123String();
 					devicesEntries[strDeviceID] = entry;
				}
				
				reply[string(JSON_ARG_DETAILS)] = devicesEntries;
			}
		}
		else if(showLevels) {
			json devicesEntries;
			for(DeviceID deviceID :deviceIDs) {
				json entry;
	
 				uint8_t  onLevel = 0;
				
				if( db->getDBOnLevel(deviceID, 0x01, &onLevel)) {
					entry[string(JSON_ARG_LEVEL)] = InsteonDevice::onLevelString(onLevel);
					devicesEntries[deviceID.string()] = entry;
					reply[string(JSON_ARG_LEVELS)] = devicesEntries;
				}
			}
			
			if(onlyShowChanged && devicesEntries.size() == 0){
				makeStatusJSON(reply,STATUS_NOT_MODIFIED);
				(completion) (reply, STATUS_NOT_MODIFIED);
				return true;
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
				
				bool queued = insteon.getOnLevel(deviceID, forceLookup,
															[=](uint8_t level, eTag_t eTag, bool didSucceed){
					insteon_dbEntry_t info;
					json reply;
					
					if( db->getDeviceInfo(deviceID,  &info)) {
						
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						reply[string(JSON_ARG_NAME)] = 	info.name;
						reply[string(JSON_VAL_VALID)] = info.isValidated;
						reply[string(JSON_ARG_DEVICEINFO)] = info.deviceInfo.string();
						reply[string(JSON_ARG_ETAG)] = info.eTag;
						reply["lastUpdated"] =  TimeStamp(info.lastUpdated).RFC1123String();
						reply[string(JSON_ARG_LEVEL)]  = level;
						
						if(info.properties.size()){
							json props;
							for(auto prop : info.properties){
								props[prop.first] = prop.second;
							}
							reply[string(JSON_ARG_PROPERTIES)]  = props;
						}
						
						auto groups = db->groupsContainingDevice(deviceID);
						if(groups.size() > 0){
							vector<string> groupList;
							for(GroupID groupID : groups) {
								groupList.push_back(groupID.string());
							}
							reply[string(JSON_ARG_GROUPIDS)] = groupList;
						}
						
						makeStatusJSON(reply,STATUS_OK);
						(completion) (reply, STATUS_OK);
						return true;
					}
					else {
						makeStatusJSON(reply,STATUS_INTERNAL_ERROR);
						(completion) (reply, STATUS_INTERNAL_ERROR);
						return true;
					}
					
				});
				
				if(!queued) {
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
					(completion) (reply, STATUS_UNAVAILABLE);
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
			makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
			
			bool isValidated =  db->isDeviceValidated(deviceID);
			
			// set level
			if(isValidated && vLevel.getvalueFromJSON(JSON_ARG_LEVEL, url.body(), level)){
				
				if(!isValidated) return false;
				
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
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
					makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is busy" );;
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
			else if(v1.getBoolFromJSON(JSON_ARG_VALIDATE, url.body(), shouldValidate)){
				
				insteon.validateDevice(deviceID,[=](bool didSucceed){
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
	
	// CHECK noun
	if(noun != NOUN_DEVICES){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	// is server available?
	if(!insteon.serverAvailable()) {
		makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is unavailable" );;
		(completion) (reply, STATUS_UNAVAILABLE);
		return;
	}
	
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
	
	if(path.size() == 2) {
		
		string subpath =   path.at(1);

		if(subpath == SUBPATH_INFO){
			if( insteon.plmInfo(&deviceID, &deviceInfo) ){
				json reply;
				
				reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
				reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				return true;
			}
			else {
				makeStatusJSON(reply,STATUS_INTERNAL_ERROR);
				(completion) (reply, STATUS_INTERNAL_ERROR);
				return true;
				
			}
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
			if(v1.getStringFromJSON(JSON_ARG_FILEPATH, url.body(), filepath)){
				
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
	
	// CHECK noun
	if(noun != NOUN_PLM){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
 
	switch(url.method()){
		case HTTP_GET:
			isValidURL = PLM_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
			
		case HTTP_PUT:
			isValidURL = PLM_NounHandler_PUT(cmdQueue,url,cInfo, completion);
			break;
	
 		default:
			(completion) (reply, STATUS_INVALID_METHOD);
			return;
	}
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
	
};

// MARK:  OTHER REST NOUN HANDLERS

static void Status_NounHandler(ServerCmdQueue* cmdQueue,
										 REST_URL url,  // entire request
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
	if(noun != NOUN_STATUS){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	auto state = insteon.currentState();
	string stateStr = insteon.currentStateString();
	
	reply[string(JSON_ARG_STATE)] =   state;
	reply[string(JSON_ARG_STATESTR)] =   stateStr;
	
	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

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
	
	reply[string(JSON_ARG_VERSION)] =   FOO_VERSION_STRING;
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
 	reply["latitude"] = solar.latitude;
	reply["longitude"] = solar.longitude;
	reply["gmtOffset"] = solar.gmtOffset;
	reply["timeZone"] = solar.timeZoneString;
	reply["midnight"] = solar.previousMidnight;
	reply["uptime"]	= solar.upTime;

	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

static void Link_NounHandler(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	bool isValidURL = false;
	
	// CHECK METHOD
	if(url.method() != HTTP_PUT ) {
		(completion) (reply, STATUS_INVALID_METHOD);
		return;
	}
	
	auto path = url.path();
	string noun;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
	
	// CHECK sub paths
	if(noun != NOUN_LINK){
		(completion) (reply, STATUS_NOT_FOUND);
		return;
	}
	
	if(path.size() == 1) {  // just link
		isValidURL = true;
		
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		
	}
	else if(path.size() == 2) { // link a specific device.
		
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			insteon.linkDevice(deviceID, true, 0xFE, [=](bool didSucceed) {
				
				if(didSucceed){
					
					insteon.addResponderToDevice(deviceID, 0x01, [=](bool didSucceed) {
						
						json reply;
						reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
						
						if(didSucceed){
							
							makeStatusJSON(reply,STATUS_OK);
							(completion) (reply, STATUS_OK);
						}
						else {
							makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addResponderToDevice failed" );;
							(completion) (reply, STATUS_INTERNAL_ERROR);
						}
					});
				}
				else {
					json reply;
					reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
					makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed" );;
					(completion) (reply, STATUS_INTERNAL_ERROR);
				}
			});
			
			isValidURL = true;
		}}
	
	
	if(!isValidURL) {
		(completion) (reply, STATUS_NOT_FOUND);
	}
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
		const char *kDateFormat = "%a %h-%d-%Y %r";

		ServerCmdArgValidator v1;

		v1.getStringFromJSON("timeZone", reply, tz);
		v1.getLongIntFromJSON("midnight", reply, midnight);
		v1.getLongIntFromJSON("gmtOffset", reply, gmtOffset);
		v1.getLongIntFromJSON("uptime", reply, uptime);
	
		time_t lastMidnight = midnight  - gmtOffset;
	
		if(v1.getStringFromJSON(JSON_ARG_DATE, reply, str)){
			using namespace timestamp;
			time_t tt =  TimeStamp(str).getTime();

			struct tm * timeinfo = localtime (&tt);
			char timeStr[80] = {0};
			::strftime(timeStr, sizeof(timeStr), kDateFormat, timeinfo );
			oss << setw(10) << "TIME: " << setw(0) << string(timeStr);
			if(!tz.empty()) oss << " " << tz;
			oss << "\n\r";
	 	}

		double latitude, longitude;
	
 
		if(v1.getDoubleFromJSON("latitude", reply, latitude)
			&& v1.getDoubleFromJSON("longitude", reply, longitude)){
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
	
	REST_URL url;
	TCPClientInfo cInfo = mgr->getClientInfo();
	
	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];
		
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
		
		if(url.isValid()) {
			
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
							
							string strDeviceInfo = value["deviceInfo"];
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
		
		
		if(subcommand == "info"){
			oss << "GET /plm/info" << " HTTP/1.1\n";
			oss << "Connection: close\n";
			oss << "\n";
			url.setURL(oss.str());
			
			TCPClientInfo cInfo = mgr->getClientInfo();
			ServerCmdQueue::shared()->queueRESTCommand(url, cInfo,[=] (json reply, httpStatusCodes_t code) {
				
				std::ostringstream oss;
				DeviceIDArgValidator vDeviceID;
				ServerCmdArgValidator v1;
				
				bool success = didSucceed(reply);
				
				if(success) {
					std::ostringstream oss;
					
					DeviceID 	 deviceID;
					DeviceInfo deviceInfo;
					string str;
					
					if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, reply, str))
						deviceID = DeviceID(str);
					
					if(v1.getStringFromJSON(JSON_ARG_DEVICEINFO, reply, str))
						deviceInfo = DeviceInfo(str);
					
					oss << deviceID.string();
					oss << "  " << deviceInfo.skuString();
					oss << " " << deviceInfo.descriptionString();
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
		else if(subcommand == "backup"){
			
			if(line.size() < 3){
				errorStr =  "\x1B[36;1;4m"  + command + " " + subcommand + "\x1B[0m expects a valid filepath";
			}
			else {
				oss << "PUT /plm/database"  << " HTTP/1.1\n";
				oss << "Content-Type: application/json; charset=utf-8\n";
				oss << "Connection: close\n";

		 		json request;
				request[string(JSON_ARG_FILEPATH)] =  line[2];
					
				string jsonStr = request.dump(4);
				oss << "Content-Length: " << jsonStr.size() << "\n\n";
				oss << jsonStr << "\n";
				url.setURL(oss.str());

				TCPClientInfo cInfo = mgr->getClientInfo();
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

	if(line.size() < 2){
		errorStr =  "\x1B[36;1;4m"  + command + "\x1B[0m what?.";
	}
	else {
		string subcommand = line[1];
		
		if (subcommand == "list") {
			
			std::ostringstream oss;
			oss << "GET /groups " <<  " HTTP/1.1\n";
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
			oss << "GET /events " <<  " HTTP/1.1\n";
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
				
				if( reply.contains(key1)
					&& reply.at(key1).is_object()){
					auto entries = reply.at(key1);
					
					for (auto& [key, value] : entries.items()) {
						
						oss  << " " << key << " ";
						string eventName = value[string(JSON_ARG_NAME)];
						oss << "\"" << eventName << "\""  << "\r\n";
					}
					
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
	
	cmdQueue->registerNoun(NOUN_STATUS, 	Status_NounHandler);
	cmdQueue->registerNoun(NOUN_VERSION, 	Version_NounHandler);
	cmdQueue->registerNoun(NOUN_DATE, 		Date_NounHandler);
	cmdQueue->registerNoun(NOUN_DEVICES, 	Devices_NounHandler);
	cmdQueue->registerNoun(NOUN_PLM, 		PLM_NounHandler);
	cmdQueue->registerNoun(NOUN_GROUPS,	Groups_NounHandler);
	cmdQueue->registerNoun(NOUN_INSTEON_GROUPS,	InsteonGroups_NounHandler);
	cmdQueue->registerNoun(NOUN_LINK,	Link_NounHandler);
	cmdQueue->registerNoun(NOUN_ACTION_GROUPS, ActionGroups_NounHandler);
	cmdQueue->registerNoun(NOUN_EVENTS,		 Events_NounHandler);
	cmdQueue->registerNoun(NOUN_EVENTS_GROUPS,  EventGroups_NounHandler);
 
	// register command line commands
	auto cmlR = CmdLineRegistry::shared();
	
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
