//
//  InsteonDeviceEventMgr.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 3/5/21.
//
#include "InsteonDeviceEventMgr.hpp"

#include "NotificationCenter.hpp"

InsteonDeviceEventMgr::InsteonDeviceEventMgr(){
	_eventMap.clear();
}

InsteonDeviceEventMgr::~InsteonDeviceEventMgr(){
	
}


void InsteonDeviceEventMgr::registerEvent( DeviceID deviceID,
														uint8_t groupID,
														uint8_t cmd,
														std::string& notification){
	
	deviceID_t rawDevID;
	deviceID.copyToDeviceID_t(rawDevID	);
 	auto key =  std::make_tuple(rawDevID[2],rawDevID[1],rawDevID[0], groupID, cmd );
	_eventMap[key] = notification;
 
}


bool InsteonDeviceEventMgr::handleEvent( DeviceID deviceID,
													 uint8_t groupID,
													 uint8_t cmd){
	
	deviceID_t rawDevID;
	deviceID.copyToDeviceID_t(rawDevID	);
	auto key =  std::make_tuple(rawDevID[2],rawDevID[1],rawDevID[0], groupID, cmd );
 
	if( auto str = _eventMap[key]; ! str.empty()) {
		NotificationCenter::defaultNotificationCenter()->postNotification(str, (void*) &deviceID);
		return true;
 	}
 
	return false;
}
