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

#include <stdio.h>
#include "InsteonMgrDefs.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "InsteonPLM.hpp"  // for deviceID_t
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
	
	std::vector<insteon_aldb_t> deviceALDB;

	// dynamic
	map<uint8_t, int> levelMap;  //  (group, level)  -1 == not set
 
}insteon_dbEntry_t;

bool  entryHasCNTL(insteon_dbEntry_t entry);

class InsteonDB {
public:
 
	static const char * DatabaseChangedNotification;
	static const char * DeviceStateChangedNotification;

	InsteonDB();
	~InsteonDB();

	void 	setPlmDeviceID(DeviceID deviceID){
		_plmDeviceID = deviceID;
	}
	
	DeviceID getPlmDeviceID() { return _plmDeviceID;};
	
	void 	clear() {
		_db.clear();
		_plmDeviceID.clear();
	};
	
	size_t count() { return _db.size();};

	bool linkDevice(DeviceID 		deviceID,
						bool		 	isCTRL,		 
						uint8_t 		group,
						DeviceInfo 	deviceInfo,
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
	vector<DeviceID> devicesThatNeedUpdating();
	vector<DeviceID> validDevices();
	vector<DeviceID> devicesUpdateSinceEtag(eTag_t  eTag);
	vector<DeviceID> devicesUpdateSince(time_t time, time_t*  latestUpdate = NULL);

	vector<DeviceID> devicesThatNeedALDB();

	bool saveToCacheFile(string fileName = "");

	bool backupCacheFile(string filepath);
 
	bool restoreFromCacheFile( string fileName = "",
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

	// Add information about device ALDB
	bool addDeviceALDB(DeviceID deviceID, vector<insteon_aldb_t> deviceALDB);
	bool addDeviceALDB(DeviceID deviceID, const insteon_aldb_t deviceALDB);

	// clear local entry for DeviceALDB (not on device)
	bool clearDeviceALDB(DeviceID deviceID);
	
	// groups
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

  	// debugging
	string dumpDB(bool printALDB = false);
	void   dumpDBInfo(std::ostringstream &oss, DeviceID deviceID, bool printALDB = false );

	void printDeviceInfo(DeviceID deviceID, bool printALDB = false);
	void printDB(bool printALDB = false);
	
	void 	setDirectoryPath(std::string path) { _directoryPath = path;};
	string getDirectoryPath() { return _directoryPath;};
	
	string cacheFileNameFromPLM(DeviceID deviceID);
	
private:
	
	string  default_fileName();
	
	void  initDBEntry(insteon_dbEntry_t *newEntry,DeviceID deviceID);
	
	insteon_dbEntry_t*  findDBEntryWithDeviceID(DeviceID deviceID);

	bool foundWithDeviceID(DeviceID deviceID, bool isCTRL, uint8_t group);
	
	void addALDB(insteon_aldb_t aldb);

	void addGroupToDBEntry(insteon_dbEntry_t* entry,
								  std::tuple< bool, uint8_t> newItem);

	void addDeviceALDBToDBEntry(insteon_dbEntry_t* entry,
										 insteon_aldb_t newItem);

	void deviceWasUpdated(DeviceID deviceID);
	void deviceStateWasUpdated(DeviceID deviceID);
 
	mutable std::mutex _mutex;

	time_t							_expired_age;		// how long to wait before we need to ping or validate
	DeviceID						_plmDeviceID;		// Device ID of thePLM associated with this DB

	vector<insteon_dbEntry_t> _db;
	eTag_t							_eTag;		// last change tag
	
	typedef struct {
		string name;
		set<DeviceID> devices;
	} groupInfo_t;

 	map<groupID_t, groupInfo_t> _deviceGroups;

	string 						_directoryPath;
	
	mt19937						_rng;
};
#endif /* InsteonDB_hpp */
