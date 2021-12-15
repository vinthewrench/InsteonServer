//
//  ServerNouns.cpp
//  insteonserver
//
//  Created by Vincent Moscaritolo on 8/26/21.
//


#include <iostream>
#include <chrono>
#include <sys/utsname.h>

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
#include <array>
#include <algorithm>  // std::copy

#include "Utils.hpp"
#include "TimeStamp.hpp"

#include "TCPServer.hpp"
#include "REST/RESTServerConnection.hpp"
#include "ServerCmdQueue.hpp"

#include "ServerCmdValidators.hpp"
#include "ServerCommands.hpp"
#include "CommonIncludes.h"
#include "InsteonDevice.hpp"
#include "LogMgr.hpp"
#include "sleep.h"

#include "Utils.hpp"

// MARK: -


static bool getCPUTemp(double &tempOut) {
	bool didSucceed = false;

//#if defined(__PIE__)
	// return the CPU temp
		{
			try{
				std::ifstream   ifs;
				ifs.open("/sys/class/thermal/thermal_zone0/temp", ios::in);
				if( ifs.is_open()){
					string val;
					ifs >> val;
					ifs.close();
					double temp = std::stod(val);
					temp = temp /1000.0;
					tempOut = temp;
					didSucceed = true;
				}
				
			}
			catch(std::ifstream::failure &err) {
			}
		}
//#else
//	tempOut = 38.459;
//	didSucceed = true;

//#endif
	
	return didSucceed;
}

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
		DeviceID	deviceID;
		// set name
		
			if(v1.getStringFromJSON(JSON_ARG_NAME, url.body(), name)
			&& (db->eventSetName(eventID, name))) {
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


static bool ActionGroupsDetailJSONForGroupID( actionGroupID_t agID, json &entry) {
	using namespace rest;
	using namespace timestamp;
	auto db = insteon.getDB();
 
 	ActionGroup group = ActionGroup(agID);

	if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
		return false;

	entry[string(JSON_ARG_GROUPID)] = group.string();
	entry[string(JSON_ARG_NAME)] =  db->actionGroupGetName(agID);
	
	json actions;
	auto acts = db->actionGroupGetActions(agID);
	for(auto ref :acts){
		Action a1 = ref.get();
		actions[a1.idString()] =  a1.JSON();
	}
	entry[string(JSON_ARG_ACTIONS)] = actions;
	
	return true;
}

static bool ActionGroups_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
													  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	
	auto path = url.path();
	auto queries = url.queries();
	auto headers = url.headers();
	
	bool showDetails = false;

	json reply;
	
	auto db = insteon.getDB();
	vector<DeviceID> deviceIDs;
	
	if(queries.count(string(JSON_VAL_DETAILS))) {
		string str = queries[string(JSON_VAL_DETAILS)];
		if( str == "true" ||  str =="1")
			showDetails = true;
	}


	// GET /groups
	if(path.size() == 1) {
  
		json groupsList;
		auto groupIDs = db->allActionGroupsIDs();
		for(auto groupID : groupIDs){
			json entry;
			
			ActionGroup ag = ActionGroup(groupID);
			if(showDetails){
				
				if(ActionGroupsDetailJSONForGroupID(ag.groupID(), entry) ){
					groupsList[ag.string()] = entry;
					
				}
			}
			else {
				entry[string(JSON_ARG_NAME)] =  db->actionGroupGetName(groupID);
				groupsList[ag.string()] = entry;
				
			}
		}
		
		reply[string(JSON_ARG_GROUPIDS)] = groupsList;
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
		
	}
	// GET /action.groups/XXXX
	else if(path.size() == 2) {
		
	//	ActionGroupsDetailJSONForGroupID
		
	ActionGroup group = ActionGroup(path.at(1));
	
	if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
		return false;

	 	if(ActionGroupsDetailJSONForGroupID(group.groupID(), reply) ){
			
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
			return true;
		}

//
//		ActionGroup group = ActionGroup(path.at(1));
//
//		if(!group.isValid() ||!db->actionGroupIsValid(group.groupID()))
//			return false;
//
//		actionGroupID_t agID = group.groupID();
//
//		reply[string(JSON_ARG_GROUPID)] = group.string();
//		reply[string(JSON_ARG_NAME)] =  db->actionGroupGetName(agID);
//
//		json actions;
//		auto acts = db->actionGroupGetActions(agID);
//		for(auto ref :acts){
//			Action a1 = ref.get();
//			actions[a1.idString()] =  a1.JSON();
//		}
//		reply[string(JSON_ARG_ACTIONS)] = actions;
//
//		makeStatusJSON(reply,STATUS_OK);
//		(completion) (reply, STATUS_OK);
//
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
		LEDBrightnessArgValidator vLED;

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
		else if(vLED.getvalueFromJSON(JSON_ARG_BACKLIGHT, url.body(), level)){
			
			bool queued = insteon.setLEDBrightness(groupID, level,
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
	
		uint8_t ledLevel = 0;

		if( db->getDBLEDBrightness(deviceID, &ledLevel )){
			entry[string(JSON_ARG_BACKLIGHT)]  = ledLevel;
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
	bool getExtInfo = false;

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
	if(queries.count(string(JSON_ARG_EXTINFO))) {
		string str = queries[string(JSON_ARG_EXTINFO)];
		if( str == "true" ||  str =="1")
			getExtInfo = true;
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
					else if(getExtInfo){
						
						insteon.getEXTInfo(deviceID,
												 [=](uint8_t data[14], bool didSucceed){
							
							json reply1 = reply;
							
							
							if(didSucceed){
								std::array<uint, 14> dat;
								for(int i = 0; i < 14; i++) dat[i] = (int)data[i];
								
								reply1[string(JSON_ARG_EXTINFO)] = dat;
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
		
		uint8_t ledLevel = 0;

		if( db->getDBLEDBrightness(deviceID, &ledLevel )){
			reply[string(JSON_ARG_BACKLIGHT)]  = ledLevel;
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
									if(didSucceed){
										
										uint8_t newMask = db->LEDMaskForKeyPad(keypad);
										InsteonKeypadDevice(deviceID).setKeypadLEDState(newMask, [=](bool success){
											
											json reply;
											makeStatusJSON(reply,STATUS_OK);
											(completion) (reply, STATUS_OK);
		
										});
									}
									else {
										json reply;
		
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
			
			DeviceID currentPLM = db->getPlmDeviceID();
			if(currentPLM.isntNULL()){
				reply[string(JSON_ARG_PLMID)] = currentPLM.string();
			}
			else {
				reply[string(JSON_ARG_PLMID)] = NULL;
			}
			
			reply[string(JSON_ARG_FILEPATH)] = path;
			reply[string(JSON_ARG_AUTOSTART)] = db->getPLMAutoStart();
			
			if( insteon.plmInfo(&deviceID, &deviceInfo) ){
				
				reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
				reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
				reply[string(JSON_ARG_COUNT)] = db->count();
			}
			
			double temp;
			if(getCPUTemp(temp)){
				reply[string(JSON_ARG_CPU_TEMP)] =   temp;
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
						insteon.startPLM("",  [=](bool didSucceed, string errorText) {
							
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
								
								string path = db->getPLMPath();
								reply[string(JSON_ARG_FILEPATH)] = path;
								
								DeviceID 	 deviceID;
								DeviceInfo deviceInfo;
								if( insteon.plmInfo(&deviceID, &deviceInfo) ){
									reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
									reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
								}
								
								DeviceID currentPLM = db->getPlmDeviceID();
								if(currentPLM.isntNULL()){
									reply[string(JSON_ARG_PLMID)] = currentPLM.string();
								}
								else {
									reply[string(JSON_ARG_PLMID)] = NULL;
								}
								
								makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "start PLM Failed",errorText );;
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
					
					insteon.stopPLM();
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
					return true;
				}
				else if(str == JSON_VAL_ERASE){
					
					string filepath;
					v1.getStringFromJSON(JSON_ARG_FILEPATH, url.body(), filepath);
					
					try{
						insteon.initPLM(filepath, [=](bool didSucceed, string errorText) {
							json reply;
							
							if(didSucceed){
								
								DeviceID deviceID;
								DeviceInfo deviceInfo;
								
								if( insteon.plmInfo(&deviceID, &deviceInfo) ){
									reply[string(JSON_ARG_DEVICEID)] = 	deviceID.string();
									reply[string(JSON_ARG_DEVICEINFO)] =	deviceInfo.string();
								}
								
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
		else if(subpath == SUBPATH_EXPORT){
			
			DeviceIDArgValidator vDeviceID;
			string str;
			DeviceID plmID;
			json reply;
			
			// optional plm ID
			if(vDeviceID.getvalueFromJSON(JSON_ARG_PLMID, url.body(), str)){
				plmID = DeviceID(str);
			}
			
			if(plmID.isNULL()){
				plmID = db->getPlmDeviceID();
			}
			
			vector<plmDevicesEntry_t> entries;
			if( db->exportPLMlinks(plmID,entries )) {
				
				json devicesEntries;
				for( auto entry : entries){
					
					json devicesEntry;
					
					devicesEntry[string(JSON_ARG_DEVICEID)] = entry.deviceID.string();
					devicesEntry[string(JSON_ARG_GROUPID)] =  to_hex <unsigned char>(entry.cntlID);
					devicesEntry[string(JSON_ARG_NAME)] =  entry.name;

					json aldbDevicesEntries;
					
					for( auto aldbEntry : entry.responders){
						json aldbDevicesEntry;
						aldbDevicesEntry[string(JSON_VAL_ALDB_FLAG)] =   aldbEntry.second?true:false;
						aldbDevicesEntry[string(JSON_VAL_ALDB_GROUP)] =  to_hex <unsigned char>(aldbEntry.first);
						aldbDevicesEntries.push_back(aldbDevicesEntry);
					}
					devicesEntry[string(JSON_VAL_ALDB)] =  aldbDevicesEntries;
					
					string strDeviceID = entry.deviceID.string();
					devicesEntries[strDeviceID] = devicesEntry;
				}
				reply[string(JSON_ARG_DETAILS)] = devicesEntries;
				
				if(plmID.isntNULL()){
					reply[string(JSON_ARG_PLMID)] = plmID.string();
				}
				else {
					reply[string(JSON_ARG_PLMID)] = NULL;
				}
				
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
				
			}
			else {
				makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "exportPLMlinks failed");;
				(completion) (reply, STATUS_INTERNAL_ERROR);
			}
			return true;
		}
		else if(subpath == JSON_VAL_IMPORT){
			
			DeviceIDArgValidator 	vDeviceID;
			auto request =  url.body();
			vector <plmDevicesEntry_t> plmEntries;
			plmEntries.clear();
			
			string key2 = string(JSON_ARG_DETAILS);
			string key3 = string(JSON_ARG_ALDB);
			
			if( request.contains(key2)
				&& request.at(key2).is_object()){
				auto entries = request.at(key2);
				
				for (auto& [_, value] : entries.items()) {
					
					DeviceID 	 deviceID;
					uint8_t	cntlID;
					string 	deviceName;
					string str;
					
					if(vDeviceID.getvalueFromJSON(JSON_ARG_DEVICEID, value, str))
						deviceID = DeviceID(str);
					
					if(v1.getStringFromJSON(JSON_ARG_NAME, value, str))
						deviceName = str;
	 
					v1.getHexByteFromJSON(JSON_ARG_GROUPID, value, cntlID);
					
					plmDevicesEntry_t plmEntry;
					plmEntry.responders.clear();
					
					plmEntry.deviceID = deviceID;
					plmEntry.cntlID = cntlID;
					plmEntry.name = deviceName;
					
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
			
			json reply;
			
			bool queued = insteon.importDevices(plmEntries,  [=](vector<DeviceID> failList,
																				bool didSucceed) {
				json reply;
				
				if(didSucceed){
 
					vector<string> deviceList;
					for(DeviceID deviceID :failList) {
						deviceList.push_back(deviceID.string());
					}
					reply[string(JSON_ARG_DEVICEIDS)] = deviceList;
 
					makeStatusJSON(reply,STATUS_OK);
					(completion) (reply, STATUS_OK);
				}
				else {
					makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "linkDevices Failed", "linkDevices failed" );;
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
		
		if(db->getLatLong(latitude, longitude)) {
			reply[string(JSON_ARG_LATITUDE)] = latitude;
			reply[string(JSON_ARG_LONGITUDE)] = longitude;
			makeStatusJSON(reply,STATUS_OK);
			(completion) (reply, STATUS_OK);
		}
		else {
			makeStatusJSON(reply,STATUS_NO_CONTENT);
			(completion) (reply, STATUS_NO_CONTENT);
		}
		
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

		double temp;
		if(getCPUTemp(temp)){
			reply[string(JSON_ARG_CPU_TEMP)] =   temp;
		}
		
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
			// set the log file
			
			bool success = LogMgr::shared()->setLogFilePath(path);
			if(success){
				db->logFileSetPath(path);

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
	
	vector<pair<uint8_t,bool>> aldbGroups
		{ {0x01, true}, {0x02, true}, {0x03, true}, {0x04, true},
			{0x05, true}, {0x06, true},{0x07, true}, {0x08, true}};

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

	insteon.addToDeviceALDB(deviceID, true, 0x01, [=](bool didSucceed) {
		(cb)(didSucceed);
	});

}

void linkDeviceCompletion(DeviceID	deviceID, ServerCmdQueue::cmdCallback_t completion){
	
	using namespace rest;
	using namespace timestamp;
	auto db = insteon.getDB();

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
			
			if(didSucceed){
				DeviceDetailJSONForDeviceID(deviceID, reply, false);
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addToDeviceALDB failed" );;
				(completion) (reply, STATUS_INTERNAL_ERROR);
			}
			
		});
	}
	else {
		// not a keypad?
		
		link_nonKeypad(deviceID, [=](bool didSucceed){
			json reply;
			
			if(didSucceed){
				DeviceDetailJSONForDeviceID(deviceID, reply, false);
				makeStatusJSON(reply,STATUS_OK);
				(completion) (reply, STATUS_OK);
			}
			else {
				reply[string(JSON_ARG_DEVICEID)] = deviceID.string();
				makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", "addToDeviceALDB failed" );;
				(completion) (reply, STATUS_INTERNAL_ERROR);
			}
			
		});
	}
	
}

static bool Link_NounHandler_PUT(ServerCmdQueue* cmdQueue,
									  REST_URL url,
									  TCPClientInfo cInfo,
									  ServerCmdQueue::cmdCallback_t completion) {
	
	using namespace rest;
	using namespace timestamp;
	json reply;
	bool isValidURL = false;

//	auto db = insteon.getDB();

	ServerCmdArgValidator v1;
	
	auto path = url.path();
	string noun;
	
	if(path.size() > 0) {
		noun = path.at(0);
	}
 
	if(path.size() == 1) {  // just link
		isValidURL = true;
		
		bool queued =  insteon.startLinking(0xFE,
														[=](DeviceID deviceID,
															 bool didSucceed,
															 string error_text){
			
			if(didSucceed){
				linkDeviceCompletion(deviceID, completion);
			}
			else {
				
				json reply;
				makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", error_text);;
				(completion) (reply, STATUS_INTERNAL_ERROR);
			}
		});
		
		if(!queued) {
			makeStatusJSON(reply, STATUS_UNAVAILABLE, "Server is not running" );;
			(completion) (reply, STATUS_UNAVAILABLE);
		}
		
		isValidURL = true;
		
	}
	else if(path.size() == 2) { // link a specific device.
		
		DeviceIDArgValidator vDeviceID;
		
		auto deviceStr = path.at(1);
		if(vDeviceID.validateArg(deviceStr)){
			DeviceID	deviceID = DeviceID(deviceStr);
			
			bool queued =  insteon.linkDevice(deviceID,
														 true,
														 0xFE,
														 "",
														 [=](DeviceID deviceID,
															  bool didSucceed,
															  string error_text) {
				
				if(didSucceed){
					linkDeviceCompletion(deviceID, completion);
				}
				else {
					json reply;
					makeStatusJSON(reply, STATUS_INTERNAL_ERROR, "Link Failed", error_text);;
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
	if(path.size() == 3 && deviceID.isntNULL()) {
		auto str = path.at(2);
		uint16_t address = 0;
		
		if( regex_match(string(str), std::regex("^[0-9a-fA-F]{4}$"))
				  && ( std::sscanf(str.c_str(), "%hx", &address) == 1)){

		
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

// MARK: STATE - NOUN HANDLER

static bool State_NounHandler_GET(ServerCmdQueue* cmdQueue,
											  REST_URL url,
											  TCPClientInfo cInfo,
											  ServerCmdQueue::cmdCallback_t completion) {
	using namespace rest;
	using namespace timestamp;

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

 
		solarTimes_t solar;
		if( insteon.getSolarEvents(solar)) {
			reply["civilSunRise"] = solar.civilSunRiseMins;
			reply["sunRise"] = solar.sunriseMins;
			reply["sunSet"] = solar.sunSetMins;
			reply["civilSunSet"] = solar.civilSunSetMins;
			reply[string(JSON_ARG_LATITUDE)] = solar.latitude;
			reply[string(JSON_ARG_LONGITUDE)] = solar.longitude;
			reply["gmtOffset"] = solar.gmtOffset;
			reply["timeZone"] = solar.timeZoneString;
			reply["midnight"] = solar.previousMidnight;
		}

		reply[string(JSON_ARG_UPTIME)] = solar.upTime;
	   reply[string(JSON_ARG_DATE)] = TimeStamp().RFC1123String();
 	
		reply[string(JSON_ARG_VERSION)] = InsteonMgr::InsteonMgr_Version;
		reply[string(JSON_ARG_BUILD_TIME)]	=  string(__DATE__) + " " + string(__TIME__);
			
		double temp;
		if(getCPUTemp(temp)){
			reply[string(JSON_ARG_CPU_TEMP)] =   temp;
		}
		
		struct utsname buffer;
		if (uname(&buffer) == 0){
			reply[string(JSON_ARG_OS_SYSNAME)] =   string(buffer.sysname);
			reply[string(JSON_ARG_OS_NODENAME)] =   string(buffer.nodename);
			reply[string(JSON_ARG_OS_RELEASE)] =   string(buffer.release);
			reply[string(JSON_ARG_OS_VERSION)] =   string(buffer.version);
			reply[string(JSON_ARG_OS_MACHINE)] =   string(buffer.machine);
		}

		
		makeStatusJSON(reply,STATUS_OK);
		(completion) (reply, STATUS_OK);
		return true;
	}
 
	  return false;
}

static void State_NounHandler(ServerCmdQueue* cmdQueue,
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
			isValidURL = State_NounHandler_GET(cmdQueue,url,cInfo, completion);
			break;
//
//		case HTTP_PUT:
//			isValidURL = State_NounHandler_GET(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_PATCH:
//			isValidURL = State_NounHandler_GET(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_POST:
//			isValidURL = State_NounHandler_GET(cmdQueue,url,cInfo, completion);
//			break;
//
//		case HTTP_DELETE:
//			isValidURL = State_NounHandler_GET(cmdQueue,url,cInfo, completion);
//			break;
  
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
	reply["uptime"]	= insteon.upTime();
 
	solarTimes_t solar;
	if( insteon.getSolarEvents(solar)) {
		reply["civilSunRise"] = solar.civilSunRiseMins;
		reply["sunRise"] = solar.sunriseMins;
		reply["sunSet"] = solar.sunSetMins;
		reply["civilSunSet"] = solar.civilSunSetMins;
		reply[string(JSON_ARG_LATITUDE)] = solar.latitude;
		reply[string(JSON_ARG_LONGITUDE)] = solar.longitude;
		reply["gmtOffset"] = solar.gmtOffset;
		reply["timeZone"] = solar.timeZoneString;
		reply["midnight"] = solar.previousMidnight;
	}

	makeStatusJSON(reply,STATUS_OK);
	(completion) (reply, STATUS_OK);
}

// MARK: -  register server nouns

void registerServerNouns() {
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
	cmdQueue->registerNoun(NOUN_STATE, 	State_NounHandler);

}
