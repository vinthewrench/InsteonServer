//
//  InsteonMgr.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/15/21.
//

#ifndef InsteonMgr_hpp
#define InsteonMgr_hpp

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <thread>			//Needed for std::thread

#include "InsteonMgrDefs.hpp"
#include "InsteonPLM.hpp"
#include "InsteonDB.hpp"
#include "InsteonALDB.hpp"
#include "InsteonLinking.hpp"
#include "InsteonCmdQueue.hpp"
#include "InsteonValidator.hpp"
#include "InsteonException.hpp"
#include "InsteonDeviceEventMgr.hpp"
#include "ScheduleMgr.hpp"

class InsteonMgr {
 
public:
	
	static const char* 	InsteonMgr_Version;

	typedef enum  {
		STATE_UNKNOWN = 0,
		STATE_INIT,
		STATE_SETUP,
		STATE_NO_PLM,
		STATE_PLM_INIT,
		STATE_PLM_ERROR,
		STATE_PLM_STOPPED,
		STATE_RESETING,
		STATE_SETUP_PLM,
		
		STATE_READING_PLM,
		STATE_VALIDATING,
		STATE_READY,
		STATE_LINKING,
		STATE_UPDATING,
		
//		STATE_NONE ,		// for substate
	}mgr_state_t;
 
	InsteonMgr();
	~InsteonMgr();
	
	
	void startPLM(string plmPath = "",  // path to PLM Serial
				  std::function<void(bool didSucceed, string error_text)> callback = NULL);

	void initPLM(string plmPath = "",  // path to PLM Serial
					  std::function<void(bool didSucceed, string error_text)> callback = NULL);

	void stopPLM();

	void syncPLM(boolCallback_t callback);
	void readPLM(boolCallback_t callback);
	void validatePLM(boolCallback_t callback = NULL);
	void savePLMCacheFile();
	bool loadCacheFile(string filePath = "");
	
	
	mgr_state_t currentState() {return _state;};
	bool serverAvailable() { return ( _state == STATE_READY
												|| _state == STATE_VALIDATING
												|| _state == STATE_UPDATING); };
	
	string currentStateString();

	bool plmInfo( DeviceID* = NULL, DeviceInfo* = NULL);
		
	bool startLinking(uint8_t groupID = 0xfE,
							std::function<void(DeviceID deviceID,
													 bool didSucceed,
													 string error_text)> callback = NULL);

	bool cancelLinking();
	
 	bool linkDevice(DeviceID deviceID,
						 bool isCTRL = true,
						 uint8_t groupID = 0xfE,
						 string deviceName = "",
						 std::function<void(DeviceID deviceID,
												  bool didSucceed,
												  string error_text)> callback = NULL);


	bool importDevices(vector<plmDevicesEntry_t> devices,
						  std::function<void(vector<DeviceID> failedDevices,  bool didSucceed)> callback = NULL);

	bool addToDeviceALDB(DeviceID deviceID,
								vector<pair<uint8_t,bool>> aldbGroups, // <bool isCNTL, uint8_t groupID>
								boolCallback_t callback = NULL);
 
	bool addToDeviceALDB(DeviceID deviceID,
								bool isCNTL, uint8_t groupID = 0x01,
								boolCallback_t callback = NULL);
	
	bool removeEntryFromDeviceALDB(DeviceID deviceID, uint16_t address,
											 boolCallback_t callback = NULL);
 
	bool updateALDBfromDevice(DeviceID deviceID,
											 boolCallback_t callback = NULL);
 
	bool unlinkDevice(DeviceID deviceID,
						 boolCallback_t callback = NULL);
	
	bool linkKeyPadButtonsToGroups(DeviceID deviceID,
											 vector<pair<uint8_t,uint8_t>> buttonGroups,
											 boolCallback_t callback = NULL);

	bool getDeviceInfo(DeviceID deviceID, insteon_dbEntry_t * info);

	bool validateDevice(DeviceID deviceID,
						 boolCallback_t callback = NULL);

	void setExpiredDelay(time_t delayInSeconds);

	// device set/get (always group 1)
	
	bool getOnLevel(DeviceID deviceID, bool forceLookup = false,
						 std::function<void(uint8_t level, eTag_t eTag,  bool didSucceed)> callback = NULL);
	
	bool setOnLevel(DeviceID deviceID, uint8_t onLevel = 0,
						 std::function<void(eTag_t eTag,  bool didSucceed)> callback = NULL);

	bool setLEDBrightness(DeviceID deviceID, uint8_t level,
									boolCallback_t callback = NULL);
	
	bool getLEDBrightness(DeviceID deviceID,
						 std::function<void(uint8_t level, eTag_t eTag,  bool didSucceed)> callback = NULL);
	
	bool setKeypadLEDState(DeviceID deviceID, uint8_t mask,
								  boolCallback_t callback = NULL);

	
	bool getEXTInfo(DeviceID deviceID,
										 std::function<void( uint8_t data[14],  bool didSucceed)> callback = NULL);
	
	bool runActionForKeypad(DeviceID deviceID, uint8_t buttonID, uint8_t cmd,
								  boolCallback_t callback = NULL);
 
	// groups set
	bool setOnLevel(GroupID groupID, uint8_t onLevel = 0,
						 std::function<void(bool didSucceed)> callback = NULL);

	bool setLEDBrightness(GroupID groupID, uint8_t Level,
						 std::function<void(bool didSucceed)> callback = NULL);

	// action groups
	bool executeActionGroup(actionGroupID_t actionGroupID,
						 std::function<void(bool didSucceed)> callback = NULL);

	bool runAction(Action action,
						std::function<void(bool didSucceed)> callback = NULL);
	// events
	bool executeEvent(eventID_t eventID,
						 std::function<void(bool didSucceed)> callback = NULL);

	bool getSolarEvents(solarTimes_t &solar);
	bool refreshSolarEvents();
	long upTime();

	// events
	
	void registerEvent( DeviceID deviceID,
							 uint8_t groupID,
							 uint8_t cmd,
							 std::string& notification);

	InsteonDB*			getDB() {return &_db; };
  
private:
	
	bool 					_running;				//Flag for starting and terminating the main loop
	std::thread 			_thread;				//Internal thread
	
	void run();

	mgr_state_t				_state;
	deviceID_t				_lastDevID;		// device we are waiting for

	PLMStream 				_stream;
	InsteonPLM				_plm;
	InsteonDB					_db;
	InsteonDeviceEventMgr _eventMgr;
	
	InsteonCmdQueue*			_cmdQueue;		// device state managmenr
	InsteonALDB*				_aldb;		 	// used for syncing ALDB
	InsteonLinking*			_linking;		// used for linking
	InsteonValidator*		_validator;	// device validation
 
	DeviceID					_plmDeviceID;
	DeviceInfo 				_plmDeviceInfo;

	bool 					_isSetup;		// indicates that we talked to PLM and data is loaded
	bool 					_hasPLM;		// indicates that we have PLM
	bool 					_hasInfo;		// indicates that we got PLM Info

	void 					startSetupPLM();

	vector<DeviceID>  _importDevices_failList; // internal used by linkDevices
	bool 					importDevices_Internal(vector<plmDevicesEntry_t> devices,
														boolCallback_t callback = NULL);

	bool 					syncALDB();
	void 					syncALDBContinue();
	size_t					aldbTaskCount;		// needed for readALDBContinue tasks.
	void 					updateLevels();

	void  					startDeviceValidation();

	plm_result_t 		handleResponse(uint64_t timeout);
	bool  					processBroadcastEvents(plm_result_t response);
	
	void					idleLoop();		// do stuff here when we are not busy.
	
	timeval			_startTime;			// used for timeouts (IR REQ/Linkink)
	uint64_t     	_timeout_LINKING;	// how long to wait LINKING

	bool				_shouldRunStartupEvents;	// we still need to run startup events.
	time_t	     		_expired_delay;		// how long to wait before we need to ping or validate
	time_t	     		_nextValidationCheck; // next scheuled time to do an validation check;

};


#endif /* InsteonMgr_hpp */
