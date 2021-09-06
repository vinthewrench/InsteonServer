//
//  InsteonDB.hpppp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/24/21.
//

#ifndef InsteonDB_hpp
#define InsteonDB_hpp

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <mutex>
#include <random>
#include <set>
#include <vector>
#include <tuple>


#include <stdio.h>
#include "InsteonMgrDefs.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "Action.hpp"
#include "Event.hpp"

//#include "InsteonDBValidator.hpp"
//#include "InventoryMgmt.hpp"
 
#include <map>
#include <vector>
#include <string>

 using namespace std;


typedef struct  {
	
	// persistent - (saved in cache)
	DeviceID		deviceID;
	vector< tuple< bool, uint8_t>>  groups;  // isCntr / group
	bool 			isValidated;
 	DeviceInfo 	deviceInfo;
	time_t			lastUpdated;
	time_t			lastLevelUpdate;

	eTag_t			eTag;		// last updtated tag
	string			name;

	map<string ,string> properties;  
 	std::vector<insteon_aldb_t> deviceALDB;

	// dynamic
	map<uint8_t, int> levelMap;  //  (group, level)  -1 == not set
}insteon_dbEntry_t;

typedef struct  {
	string			buttonName;
	bool 			isOn;
	map<uint8_t, Action> actions; // (cmd, actions)
}keypad_Button_t;

typedef struct  {
	DeviceID		deviceID;
	uint8_t		buttonCount;
	map<uint8_t, keypad_Button_t> buttons;
}keypad_dbEntry_t;

bool  entryHasCNTL(insteon_dbEntry_t entry);


// list of devices we would need to export to a new PlM
typedef struct {
	DeviceID				deviceID;
	uint8_t 				cntlID;
	string  				name;
	vector<pair<uint8_t, bool>> 	responders;
} plmDevicesEntry_t;
 
class InsteonDB {
public:
 
	static const char * DatabaseChangedNotification;
	static const char * DeviceStateChangedNotification;
 
	static InsteonDB *shared() {
			if (!sharedInstance)
				sharedInstance = new InsteonDB;
			return sharedInstance;
		}

	static InsteonDB *sharedInstance;

	InsteonDB();
	~InsteonDB();

// MARK: - config info
	bool 	setPLMpath(string plmPath);
	string getPLMPath(){ return _plmPath;};
	
	void 	setPlmDeviceID(DeviceID deviceID){
		_plmDeviceID = deviceID;
	}
	DeviceID getPlmDeviceID() { return _plmDeviceID;};

	void  setPLMAutoStart(bool autoStartPLM);
	bool  getPLMAutoStart();

	void  setAllowRemoteTelnet(bool remoteTelnet);
	bool  getAllowRemoteTelnet();

	void  setTelnetPort(int port);
	int  	getTelnetPort();
	void  setRESTPort(int port);
	int  	getRESTPort();
	

	bool setLatLong(double latitude, double longitude);
	bool getLatLong(double &latitude, double &longitude);

	// MARK: - database API
 
	void 	clear() {
		_db.clear();
		_plmDeviceID.clear();
	};
	
	size_t count() { return _db.size();};

	eTag_t lastEtag() { return  _eTag;};
	
	bool linkDevice(DeviceID 		deviceID,
						bool		 	isCTRL,		 
						uint8_t 		group,
						DeviceInfo 	deviceInfo,
						string 		deviceName = "",
						bool 			isValidated = false
						);

	void validateDeviceInfo(DeviceID deviceID,
								 DeviceInfo* info  = NULL);

	// Quick Check for validatated Device
	bool isDeviceValidated(DeviceID deviceID);

	bool refreshDevice(DeviceID deviceID);
	
	bool setDBOnLevel(DeviceID deviceID, uint8_t group = 0x01,
							uint8_t onLevel = 0, eTag_t *eTag =NULL);
	
	bool getDBOnLevel(DeviceID deviceID, uint8_t group = 0x01,
							uint8_t *onLevel = NULL, eTag_t *eTag =NULL );

	vector<DeviceID> allDevices();
	vector<DeviceID> devicesThatNeedValidation();
	vector<DeviceID> validDevices();
	vector<DeviceID> devicesUpdateSinceEtag(eTag_t  eTag);
	vector<DeviceID> devicesUpdateSince(time_t time, time_t*  latestUpdate = NULL);

	vector<DeviceID> devicesThatNeedALDB();

	vector<DeviceID> devicesRespondingToInsteonGroup(uint8_t group);
	
	bool saveToCacheFile(string filePath = "");

	bool backupCacheFile(string filepath);
 
	bool restoreFromCacheFile( string filePath = "",
									 time_t validityDuration = 60*60*24		// how long is valid?
									  );
	
	// create a set of vectors to add and remove to aldb to get it to match DB
 	bool syncALDB(vector<insteon_aldb_t> entriesIn,
 					  vector<insteon_aldb_t> *plmRemove,
					  vector<insteon_aldb_t> *plmAdd);
 
	// copy any ALDB entries from PLM  to DB
	bool addPLMEntries(vector<insteon_aldb_t> entries);
 
	// return information about device
	bool getDeviceInfo(DeviceID deviceID, insteon_dbEntry_t * info);

	bool updateDeviceProperty(DeviceID deviceID, string key, string value);
	bool removeDeviceProperty(DeviceID deviceID, string key);

	// Add information about device ALDB
	bool updateDeviceALDB(DeviceID deviceID, vector<insteon_aldb_t> deviceALDB);
	bool updateDeviceALDB(DeviceID deviceID, const insteon_aldb_t deviceALDB);
	bool setDeviceALDB(DeviceID deviceID, vector<insteon_aldb_t> deviceALDB);

//	bool removeEntryFromDeviceALDB(DeviceID targetDevice, uint16_t address);

	// clear local entry for DeviceALDB (not on device)
	bool clearDeviceALDB(DeviceID deviceID);
	bool removeDevice(DeviceID deviceID);
	
	bool exportPLMlinks(DeviceID plmID,  vector<plmDevicesEntry_t> &entries);
	
	// MARK: -   keypads
	vector<DeviceID>		allKeypads();
	keypad_dbEntry_t*	findKeypadEntryWithDeviceID(DeviceID deviceID);
	keypad_Button_t*		findKeypadButton(keypad_dbEntry_t* entry, uint8_t buttonID );
	Action*				actionForKeypad(DeviceID deviceID, uint8_t buttonID, uint8_t cmd);
	bool					createKeypad(DeviceID deviceID);
	bool					createKeypadButton(DeviceID deviceID, uint8_t buttonID);
	bool					removeKeypadButton(DeviceID deviceID, uint8_t buttonID);
	bool  					setActionForKeyPadButton(DeviceID deviceID, uint8_t buttonID, uint8_t cmd, Action action);
	bool  					setNameForKeyPadButton(DeviceID deviceID, uint8_t buttonID, string name);
	bool  					setKeypadButtonCount(DeviceID deviceID, uint8_t buttonCount );
	bool					invokeKeyPadButton(DeviceID deviceID, uint8_t buttonID, uint8_t cmd);
	uint8_t 				LEDMaskForKeyPad(keypad_dbEntry_t* keypad);

	
	// MARK: -  groups
	bool groupIsValid(GroupID groupID);
	bool groupCreate(GroupID* groupID, const string name);
	bool groupDelete(GroupID groupID);
	bool groupFind(string name, GroupID* groupID);
	bool groupSetName(GroupID groupID, string name);
	string groupGetName(GroupID groupID);
	bool groupAddDevice(GroupID groupID, DeviceID deviceID);
	bool groupRemoveDevice(GroupID groupID, DeviceID deviceID);
	vector<DeviceID> groupGetDevices(GroupID groupID);
	vector<GroupID> allGroups();
	vector<GroupID> groupsContainingDevice(DeviceID deviceID);

	// MARK: -   actions
	bool actionGroupIsValid(actionGroupID_t actionGroupID);
	bool actionGroupCreate(actionGroupID_t* actionGroupID, const string name);
	bool actionGroupDelete(actionGroupID_t actionGroupID);
	bool actionGroupFind(string name, actionGroupID_t* actionGroupID);
	bool actionGroupSetName(actionGroupID_t actionGroupID, string name);
	string actionGroupGetName(actionGroupID_t actionGroupID);
	bool actionGroupAddAction(actionGroupID_t actionGroupID,  Action action, actionID_t* actionIDOut = NULL);
	bool actionGroupRemoveAction(actionGroupID_t actionGroupID, actionID_t actionID);
	bool actionGroupIsValidActionID(actionGroupID_t actionGroupID, actionID_t actionID);
	vector<reference_wrapper<Action>> actionGroupGetActions(actionGroupID_t groupID);
	vector<actionGroupID_t> allActionGroupsIDs();
	
	// MARK: -   events
	bool eventsIsValid(eventID_t eventID);
	bool eventSave(Event event, eventID_t* eventIDOut = NULL);
	bool eventFind(string name, eventID_t* eventID);
	bool eventDelete(eventID_t eventID);
	bool eventSetName(eventID_t eventID, string name);
	string eventGetName(eventID_t eventID);
	optional<reference_wrapper<Event>> eventsGetEvent(eventID_t eventID);
	vector<eventID_t> allEventsIDs();
	vector<eventID_t> matchingEventIDs(EventTrigger trig);
	vector<eventID_t> eventsMatchingAppEvent(EventTrigger::app_event_t appEvent);
	vector<eventID_t> eventsThatNeedToRun(solarTimes_t &solar, time_t localNow);
	vector<eventID_t> eventsInTheFuture(solarTimes_t &solar, time_t localNow);
	bool eventSetLastRunTime(eventID_t eventID, time_t localNow);
	void reconcileEventGroups(const solarTimes_t &solar, time_t localNow);

	// MARK: -  event groups
	bool eventGroupIsValid(eventGroupID_t eventGroupID);
	bool eventGroupCreate(eventGroupID_t* eventGroupID, const string name);
	bool eventGroupDelete(eventGroupID_t eventGroupID);
	bool eventGroupFind(string name, eventGroupID_t* eventGroupID);
	bool eventGroupSetName(eventGroupID_t eventGroupID, string name);
	string eventGroupGetName(eventGroupID_t eventGroupID);
	bool eventGroupAddEvent(eventGroupID_t eventGroupID,  eventID_t eventID);
	bool eventGroupRemoveEvent(eventGroupID_t eventGroupID, eventID_t eventID);
	bool eventGroupContainsEventID(eventGroupID_t eventGroupID, eventID_t eventID);
	vector<eventID_t> eventGroupGetEventIDs(eventGroupID_t eventGroupID);
	vector<eventGroupID_t> allEventGroupIDs();
	
	// MARK: -  API Secrets
	bool apiSecretCreate(string APIkey, string APISecret);
	bool apiSecretDelete(string APIkey);
	bool apiSecretSetSecret(string APIkey, string APISecret);
	bool apiSecretGetSecret(string APIkey, string &APISecret);
	bool apiSecretMustAuthenticate();
	
	// MARK: -  LogFile prefs
	bool logFileSetFlags(uint8_t logFlags);
	bool logFileGetFlags(uint8_t &logFlags);
	bool logFileSetPath(string path);
	bool logFileGetPath(string &path);
 

	// MARK: -   debugging
	string dumpDB(bool printALDB = false);
	void   dumpDBInfo(std::ostringstream &oss, DeviceID deviceID, bool printALDB = false );
		
	string cacheFileNameFromPLM(DeviceID deviceID);

	// MARK: - private
private:
 
	string  default_filePath();
	
	// config info
	string 	_plmPath;
	double		_longitude;
	double 	_latitude;
	string 	_logFilePath;
	uint8_t 	_logFileFlags;
	bool		_autoStartPLM;
	bool		_allowRemoteTelnet;
	int 		_telnetPort;
	int 		_restPort;

	void  initDBEntry(insteon_dbEntry_t *newEntry,DeviceID deviceID);
	
	insteon_dbEntry_t*  findDBEntryWithDeviceID(DeviceID deviceID);

	bool foundWithDeviceID(DeviceID deviceID, bool isCTRL, uint8_t group);
	
	void addALDB(insteon_aldb_t aldb);

	void addGroupToDBEntry(insteon_dbEntry_t* entry,
								  std::tuple< bool, uint8_t> newItem);

	void updateDeviceALDBToDBEntry(insteon_dbEntry_t* entry,
										 insteon_aldb_t newItem);

	void deviceWasUpdated(DeviceID deviceID);
	void deviceStateWasUpdated(DeviceID deviceID);
 	
	
	mutable std::mutex _mutex;

	time_t							_expired_age;		// how long to wait before we need to ping or validate
	DeviceID						_plmDeviceID;		// Device ID of thePLM associated with this DB

	vector<insteon_dbEntry_t> _db;
	eTag_t							_eTag;		// last change tag
	
	map<uint8_t , set<DeviceID>>  _insteonGroupMap;

	typedef struct {
		string name;
		set<DeviceID> devices;
	} groupInfo_t;

 	map<groupID_t, groupInfo_t> _deviceGroups;

	typedef struct {
		string name;
		map<actionID_t, Action> actions;
	} actionGroupInfo_t;

	map<actionGroupID_t, actionGroupInfo_t> _actionGroups;
 
	map<eventID_t, Event> 		_events;
	
	typedef struct {
		string name;
		set<eventID_t>  eventIDs;
	} eventGroupInfo_t;

	map<eventID_t, eventGroupInfo_t> 		_eventsGroups;
	map <DeviceID,keypad_dbEntry_t>			_keyPads;
	
	map<string,  string> 						_APISecrets;

	mt19937						_rng;
};
#endif /* InsteonDB_hpp */
