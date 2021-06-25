//
//  InsteonDevice.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/15/21.
//

#ifndef InsteonDevice_hpp
#define InsteonDevice_hpp

#include "InsteonMgrDefs.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "json.hpp"

#include <vector>
#include <utility>
#include <stdlib.h>

class InsteonMgr;
class InsteonALDB;
class InsteonDB;

class InsteonDeviceGroup {
	
  public:
	InsteonDeviceGroup(uint8_t groupID);
	  ~InsteonDeviceGroup();
 
	bool setOnLevel(uint8_t level, boolCallback_t callback = NULL);

protected:
	uint8_t _groupID;

};

 
class InsteonDevice {
 
public:
	InsteonDevice(DeviceID deviceID);
	~InsteonDevice();

	bool beep(boolCallback_t callback = NULL);

	bool setOnLevel(uint8_t level, boolCallback_t callback = NULL);

	bool getOnLevel(std::function<void(uint8_t level, bool didSucceed)> callback);

	bool setLEDBrightness(uint8_t level, boolCallback_t callback = NULL);
	
	bool getEngineVersion(std::function<void(uint8_t version, bool didSucceed)> callback);
	
	// class functions
	static std::string onLevelString(uint8_t level);
	static bool stringToLevel(const std::string str, uint8_t* levelOut = NULL);
	static bool jsonToLevel( nlohmann::json j, uint8_t* levelOut = NULL);
	
	
	static bool stringToBackLightLevel(std::string str, uint8_t* levelOut = NULL);
	static bool jsonToBackLightLevel( nlohmann::json j, uint8_t* levelOut = NULL);
	static std::string backLightLevelString(uint8_t level);

protected:
	DeviceID _deviceID;
};


class InsteonKeypadDevice : InsteonDevice{
 
	friend InsteonMgr;
	
	public:
	
	InsteonKeypadDevice(DeviceID deviceID):InsteonDevice(deviceID){};
	
	bool setKeyButtonMode(bool eightKey, boolCallback_t callback = NULL);
  
 	bool setNonToggleMask(uint8_t mask, boolCallback_t callback = NULL);
	
	bool setKeypadLED(uint8_t button, bool turnOn, boolCallback_t callback = NULL);

	bool getKeypadLEDState(std::function<void(uint8_t mask, bool didSucceed)> callback);
	bool setKeypadLEDState(uint8_t mask, boolCallback_t callback = NULL);


	bool test();
	
protected:
	bool linkKeyPadButtonsToGroups( InsteonDB* db,
											 InsteonALDB *aldb,
											 std::vector<std::pair<uint8_t,uint8_t>> buttonGroups,
											 boolCallback_t callback = NULL);
	private:

	typedef struct {
			uint8_t group;
		  uint8_t button;
	  }linkPairs_t;
	
	typedef struct {
		InsteonDB			*db;
		InsteonALDB 		*aldb;
		DeviceID			deviceID;
		size_t count;
		size_t fails;
		linkPairs_t pairs[];
 	} linkKeyPadTaskData_t;
 
	void linkKeyPadButtonsToGroupsInternal(linkKeyPadTaskData_t *taskData,
														boolCallback_t callback);
 
 };

	

#endif /* InsteonDevice_hpp */
