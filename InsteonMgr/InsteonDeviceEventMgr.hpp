//
//  InsteonDeviceEventMgr.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 3/5/21.
//

#ifndef InsteonDeviceEventMgr_hpp
#define InsteonDeviceEventMgr_hpp

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <tuple>

 #include <mutex>

#include "DeviceID.hpp"
#include "InsteonDeviceEventMgr.hpp"
 
class InsteonMgr;

class InsteonDeviceEventMgr {
	friend InsteonMgr;
	
	InsteonDeviceEventMgr();
	~InsteonDeviceEventMgr();

public:
	
	void registerEvent( DeviceID deviceID,
							 uint8_t groupID,
							 uint8_t cmd,
							 std::string& notification);

	
	bool handleEvent( DeviceID deviceID,
						  uint8_t groupID,
						  uint8_t cmd);
	
private:
 
	typedef std::tuple<uint8_t, uint8_t, uint8_t,  uint8_t, uint8_t > eventKey_t;
	std::map<eventKey_t, std::string> _eventMap;  
 
};


#endif /* InsteonDeviceEventMgr_hpp */
