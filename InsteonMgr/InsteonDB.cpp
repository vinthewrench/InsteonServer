//
//  InsteonDB.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/24/21.
//
#include <time.h>
#include <random>
#include <climits>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <regex>

#include "InsteonDB.hpp"
#include "LogMgr.hpp"
#include "NotificationCenter.hpp"
#include "InsteonDevice.hpp"



using namespace std;

const char *  InsteonDB::DatabaseChangedNotification = "InsteonMgr::DatabaseChangedNotification";
const char *  InsteonDB::DeviceStateChangedNotification = "InsteonMgr::DeviceStateChangedNotification";

constexpr static string_view KEY_START_CONFIG 		= "config-start";
constexpr static string_view KEY_END_CONFIG 			= "config-end";
constexpr static string_view KEY_CONFIG_PORT			= "port";
constexpr static string_view KEY_CONFIG_LATLONG		= "lat-long";
 
constexpr static string_view KEY_START_DEVICE 		= "device-start";
constexpr static string_view KEY_END_DEVICE 			= "device-end";
constexpr static string_view KEY_START_GROUP 			= "group-start";
constexpr static string_view KEY_END_GROUP			= "group-end";
constexpr static string_view KEY_START_ACTIONGROUP 	= "action_group-start";
constexpr static string_view KEY_END_ACTIONGROUP		= "action_group-end";
constexpr static string_view KEY_START_EVENT 			= "event-start";
constexpr static string_view KEY_END_EVENT			= "event-end";

constexpr static string_view KEY_START_EVENTGROUP 	= "event_group-start";
constexpr static string_view KEY_END_EVENTGROUP		= "event_group-end";

constexpr static string_view KEY_DEVICE_NAME			= "name";
constexpr static string_view KEY_DEVICE_ALDB			= "aldb";
constexpr static string_view KEY_DEVICE_UPDATED		= "updated";
constexpr static string_view KEY_DEVICE_CNTL			= "cntl";
constexpr static string_view KEY_DEVICE_RESP			= "resp";
constexpr static string_view KEY_GROUP_DEVICEID		= "dev";
constexpr static string_view KEY_ACTIONGROUP_ACTION	= "act";
constexpr static string_view KEY_EVENT_TRIGGER		= "trigger";
constexpr static string_view KEY_EVENT_EVENTID		= "event";



InsteonDB::InsteonDB() {
	
	_expired_age = 60*60*24*30;
	_directoryPath = "";
	_db.clear();
	_deviceGroups.clear();
	_actionGroups.clear();
	_eventsGroups.clear();
	_events.clear();
	_eTag = 0;
	
	// create RNG engine
	constexpr std::size_t SEED_LENGTH = 8;
  std::array<uint_fast32_t, SEED_LENGTH> random_data;
  std::random_device random_source;
  std::generate(random_data.begin(), random_data.end(), std::ref(random_source));
  std::seed_seq seed_seq(random_data.begin(), random_data.end());
	_rng =  std::mt19937{ seed_seq };
 
}

InsteonDB::~InsteonDB() {
	
	_db.clear();
	
}

// MARK: - public API

bool InsteonDB::linkDevice(DeviceID 	deviceID,
								  bool		 	isCTRL,
								  uint8_t 	group,
								  DeviceInfo	deviceInfo,
								  bool 		isValidated
								  ){
	
	std::lock_guard<std::mutex> lock(_mutex);
	bool didAdd = false;
	
		
	if(auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL)
	{
		addGroupToDBEntry(existing,
								make_tuple(isCTRL,  group));
		
		if(isCTRL){
			existing->deviceInfo	= deviceInfo;
		}
		existing->isValidated = isValidated;
		existing->lastUpdated = time(NULL);
	}
	else {
		insteon_dbEntry_t newEntry;
		initDBEntry(&newEntry,  deviceID);
		
		addGroupToDBEntry(&newEntry,
								make_tuple(isCTRL,  group));
		if(isCTRL){
			newEntry.deviceInfo	= deviceInfo;
		}
		
		newEntry.isValidated = isValidated;
		newEntry.lastUpdated = time(NULL);
		newEntry.levelMap.clear();

		_db.push_back(newEntry);
	}
	
	deviceWasUpdated(deviceID);
	
	return didAdd;
}


void InsteonDB::validateDeviceInfo(DeviceID deviceID,
											  DeviceInfo* info ){
	std::lock_guard<std::mutex> lock(_mutex);
 
	auto existing = findDBEntryWithDeviceID(deviceID);
	
	if(info){
		LOG_INFO("\tDB UPDATE %s %s rev %02x\n",
					deviceID.string().c_str(),
						info->string().c_str(),
					info->GetFirmware());
		
		if(existing){
			existing->deviceInfo = *info;
			existing->lastUpdated = time(NULL);
			existing->isValidated = true;
			existing->eTag = _eTag++;
		}
	}
	else
	{
		existing->lastUpdated = time(NULL);
		existing->isValidated = false;
		existing->eTag = _eTag++;
	}
	
	deviceWasUpdated(deviceID);
}

bool InsteonDB::refreshDevice(DeviceID deviceID){
	
	std::lock_guard<std::mutex> lock(_mutex);
	bool didRefresh = false;
 
	if(	auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL){
		
		existing->lastUpdated = time(NULL);
		existing->eTag = _eTag++;

		didRefresh = true;
		
		deviceWasUpdated(deviceID);
	}
	
	return didRefresh;
}


bool InsteonDB::updateDeviceProperty(DeviceID deviceID, string key, string value){
	bool didUpdate = false;

	{
		std::lock_guard<std::mutex> lock(_mutex);
		if(	auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL){
			
			if(key == KEY_DEVICE_NAME){
				existing->name = value;
			}
			else {
				existing->properties[key] = value;
			}
			didUpdate = true;
		}
	}
	
	if(didUpdate)
		saveToCacheFile();
	
	return didUpdate;
}

bool InsteonDB::removeDeviceProperty(DeviceID deviceID, string key){
	bool didUpdate = false;

	{
		std::lock_guard<std::mutex> lock(_mutex);
		if(	auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL){
			
			if(key == KEY_DEVICE_NAME){
				existing->name = "";
				didUpdate = true;
			}
			else {
				if(existing->properties.count(key)) {
					existing->properties.erase(key);
					didUpdate = true;
				}
			}
		}
	}
	
	if(didUpdate)
		saveToCacheFile();
	
	return didUpdate;

}

vector<DeviceID> InsteonDB::allDevices(){
	std::lock_guard<std::mutex> lock(_mutex);
	vector< DeviceID> val_list;
	
	for (auto e = _db.begin(); e != _db.end(); e++) {
		val_list.push_back(e->deviceID);
	}
	
	return val_list;

}

vector<DeviceID> InsteonDB::validDevices(){
	std::lock_guard<std::mutex> lock(_mutex);
	vector< DeviceID> val_list;
	
	for (auto e = _db.begin(); e != _db.end(); e++) {
		if(!e->isValidated) continue;
		val_list.push_back(e->deviceID);
	}
	
	return val_list;
}


vector<DeviceID> InsteonDB::devicesThatNeedUpdating(){
	
	std::lock_guard<std::mutex> lock(_mutex);
	vector< DeviceID> val_list;
	
	time_t expiredTime = time(NULL) - _expired_age;
	
	for (auto e = _db.begin(); e != _db.end(); e++) {
		
		bool hasCTRL = false;
		
		for(auto group : e->groups){
			if( get<0>(group)){
				hasCTRL = true;
				break;
			}
		}
		
		if(hasCTRL
			&&(
				!e->isValidated
				|| e->lastUpdated == 0
				|| e->lastUpdated < expiredTime
				|| e->deviceInfo.GetVersion()	== 0
				))
		{
			val_list.push_back(e->deviceID);
		}
	}
	
	return val_list;
}
 
vector<DeviceID> InsteonDB::devicesThatNeedALDB(){
	vector< DeviceID> dev_list;

	std::lock_guard<std::mutex> lock(_mutex);

	for (auto e = _db.begin(); e != _db.end(); e++) {
		
		if(e->isValidated
			&& e->deviceALDB.size() == 0) {
			dev_list.push_back(e->deviceID);
		}
	}
	
	return dev_list;
}

vector<DeviceID> InsteonDB::devicesUpdateSinceEtag(eTag_t  eTag){
	vector< DeviceID> dev_list;

	std::lock_guard<std::mutex> lock(_mutex);
	for (auto e = _db.begin(); e != _db.end(); e++) {
		
		if(!e->isValidated) break;
		eTag_t val = e->eTag;
		if(val > eTag){
			dev_list.push_back(e->deviceID);
		}
		
	}

 	return dev_list;
}

vector<DeviceID> InsteonDB::devicesUpdateSince(time_t time,  time_t*  latestUpdateOut ){
	vector< DeviceID> dev_list;
	
	time_t latestUpdate = 0;
	
	std::lock_guard<std::mutex> lock(_mutex);
	for (auto e = _db.begin(); e != _db.end(); e++) {
		if(!e->isValidated)  continue;
		
		time_t updated = e->lastUpdated > e->lastLevelUpdate ? e->lastUpdated: e->lastLevelUpdate;
		
		if(latestUpdate < updated )
			latestUpdate = updated;
 
		if(updated > time)
			dev_list.push_back(e->deviceID);
	}
	
	if(latestUpdateOut)
		*latestUpdateOut = latestUpdate;
 
	return dev_list;
	
}

void InsteonDB::addALDB(insteon_aldb_t aldb){
	
	std::lock_guard<std::mutex> lock(_mutex);
	bool isController = (aldb.flag & 0x40) == 0x40;
	DeviceID deviceID = DeviceID(aldb.devID);
	
	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		addGroupToDBEntry(entry,
								make_tuple(isController, aldb.group));
		
		// only update on mismatch?
		if(isController){
			if( entry->deviceInfo != DeviceInfo(aldb.info)) {
				entry->deviceInfo = DeviceInfo(aldb.info);
			}
		}
		entry->eTag = _eTag++;
	}
	else  {
		insteon_dbEntry_t newEntry;
		initDBEntry(&newEntry,deviceID);
		
		addGroupToDBEntry(&newEntry,
								make_tuple(isController, aldb.group));
		
		if(isController){
			newEntry.deviceInfo = DeviceInfo(aldb.info);
		}
		newEntry.eTag = _eTag++;

		newEntry.levelMap.clear();
		_db.push_back(newEntry);
		
	}
}


bool InsteonDB::getDeviceInfo(DeviceID deviceID, insteon_dbEntry_t * info){
 	std::lock_guard<std::mutex> lock(_mutex);
	bool found = false;

	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
	 
		if(info){
			*info = *(entry);
		}
		found = true;
	}

	return found;
}
 
bool InsteonDB::isDeviceValidated(DeviceID deviceID){
	std::lock_guard<std::mutex> lock(_mutex);
	bool valid = false;
 
	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
	 	if(entry->isValidated)
			valid = true;
	}

	return valid;
}

// Add information about device ALDB
bool InsteonDB::addDeviceALDB(DeviceID deviceID, const insteon_aldb_t deviceALDB){
	bool didUpdate = false;
	
	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		std::lock_guard<std::mutex> lock(_mutex);
		addDeviceALDBToDBEntry(entry, deviceALDB);
		entry->eTag = _eTag++;
 		didUpdate = true;
	};
	
	if(didUpdate)
		deviceWasUpdated(deviceID);

	return didUpdate;

}

// insert a vector of insteon_aldb_t
bool InsteonDB::addDeviceALDB(DeviceID deviceID, std::vector<insteon_aldb_t> deviceALDB){
	bool didUpdate = false;

	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		std::lock_guard<std::mutex> lock(_mutex);
		
		for(auto aldb :deviceALDB){
			addDeviceALDBToDBEntry(entry, aldb);
		}
		entry->eTag = _eTag++;
		didUpdate = true;
	}

	if(didUpdate)
		deviceWasUpdated(deviceID);

	return didUpdate;
}

bool InsteonDB::clearDeviceALDB(DeviceID deviceID){
	bool didUpdate = false;

	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		std::lock_guard<std::mutex> lock(_mutex);
		
		entry->eTag = _eTag++;
		entry->deviceALDB.clear();
		didUpdate = true;
	}

	if(didUpdate)
		deviceWasUpdated(deviceID);

	return didUpdate;

}

bool InsteonDB::removeDevice(DeviceID deviceID){

	bool didUpdate = false;
	
	// remove the device from all groups containing it.
	for(auto groupID : groupsContainingDevice(deviceID)){
		groupRemoveDevice(groupID, deviceID);
	}

	{ // lock durring erase.
		std::lock_guard<std::mutex> lock(_mutex);
 
	// remove it from the database
		for (auto e = _db.begin(); e != _db.end();) {
			if(e->deviceID == deviceID){
				_db.erase(e);
				didUpdate = true;
			}
			else {
				e++;
			}
		}
	}
 
	if(	didUpdate)
		saveToCacheFile();
	return didUpdate;
}


// MARK: - dynamic data

bool InsteonDB::setDBOnLevel(DeviceID deviceID, uint8_t group,
									  uint8_t onLevel, eTag_t *eTag){

	bool needsUpdate = false;
	eTag_t lastTag = 0;
	
	{
		std::lock_guard<std::mutex> lock(_mutex);
		
		if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
			
			if(entry->levelMap.count(group) == 0) {
				needsUpdate = true;
			}
			else if (entry->levelMap[group] != onLevel) {
				needsUpdate = true;
			}
			
			if(needsUpdate){
				entry->levelMap[group] = onLevel;
				entry->eTag = _eTag++;
				entry->lastLevelUpdate = time(NULL);
			}
			
			lastTag = entry->eTag;
		}
	}
	
	if(eTag)
		*eTag = lastTag;

	if(needsUpdate)
		deviceStateWasUpdated(deviceID);
 
	return needsUpdate;
}

bool InsteonDB::getDBOnLevel(DeviceID deviceID, uint8_t group, uint8_t *onLevel, eTag_t *eTag)
{
	
	bool found = false;
	
	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		std::lock_guard<std::mutex> lock(_mutex);
		
		found = (entry->levelMap.count(group) != 0);
	 
		if(found){
			if(onLevel)
				*onLevel = entry->levelMap[group];
			if(eTag)
				*eTag = entry->eTag;
		}
		
	} 	return found;
}


// MARK: - Config Info

bool 	InsteonDB::setPLMpath(string plmPath){
	_plmPath = plmPath;
	saveToCacheFile();
	return true;
};

 
bool InsteonDB::setLatLong(double latitude, double longitude){
	
	_latitude = latitude;
	_longitude = longitude;

	saveToCacheFile();
	return true;
}

bool InsteonDB::getLatLong(double &latitude, double &longitude){
	
	if(_latitude == 0 && _longitude == 0)
		return false;
	
	latitude = _latitude;
	longitude = _longitude;
	return true;
}

// MARK: - PLM tools

bool InsteonDB::addPLMEntries(vector<insteon_aldb_t> aldbEntries){
	
	for (insteon_aldb_t aldb : aldbEntries)
	{
		addALDB(aldb);
	}
	return true;
}

 
bool InsteonDB::syncALDB(vector<insteon_aldb_t> aldbIn,
							vector<insteon_aldb_t> *plmRemoveOut,
							vector<insteon_aldb_t> *plmAddOut){
	
	std::lock_guard<std::mutex> lock(_mutex);

	bool hasChanges = false;
	
	vector<insteon_aldb_t> aldbDelete;
	vector<insteon_aldb_t> aldbAdd;
	
	//look for devices in ALDB but not in DB,
	for (auto aldb : aldbIn)
	{
		DeviceID deviceID = DeviceID(aldb.devID);
		bool isController = (aldb.flag & 0x40) == 0x40;

		if(! foundWithDeviceID(deviceID, isController,aldb.group) )
			aldbDelete.push_back(aldb);
	}

	// /look for devices in DB but ALDB
	for(auto e: _db){
		
		for(auto group :e.groups){
			bool 		isCNTRL =  get<0>(group);
			uint8_t 	gid 	= get<1>(group);

			bool found = false;
			
			for(auto aldb: aldbIn){
				DeviceID deviceID = DeviceID(aldb.devID);
				bool isController = (aldb.flag & 0x40) == 0x40;

				if(deviceID == e.deviceID
					&& gid == aldb.group
					&& isCNTRL == isController){
					found = true;
					break;
				}
			}
			
			if(!found){
				insteon_aldb_t aldb;
				e.deviceID.copyToDeviceID_t(aldb.devID);
				aldb.flag = isCNTRL?0x40:00;
				aldb.group = gid;
				aldb.info[0] = e.deviceInfo.GetCat();
				aldb.info[1] = e.deviceInfo.GetSubcat();
				aldb.info[2] = e.deviceInfo.GetFirmware();
				aldbAdd.push_back(aldb);
			}
		}
	}
 
	hasChanges = aldbDelete.size() > 0
					|| aldbAdd.size() > 0;
	
	if(plmRemoveOut)
		*plmRemoveOut = aldbDelete;

	if(plmAddOut)
		*plmAddOut = aldbAdd;
	
	return hasChanges;
}


// MARK: -  device cachefile

string  InsteonDB::default_fileName(){
	string fileName = "PLMDB.txt";
	return fileName;
 }


static const char kDateFormat[] = "%Y-%m-%d %H:%M:%S";

bool InsteonDB::backupCacheFile(string filepath){
		
	std::lock_guard<std::mutex> lock(_mutex);
	bool statusOk = false;
	
	std::ofstream ofs;
	
	try{
		ofs.open(filepath);
		
		if(ofs.fail())
			return false;
			
		// save configuration info
 
		{
			time_t now = time(NULL);
			char str[26] = {};
			strftime(str, sizeof(str), kDateFormat, gmtime(&now));
			
			ofs << "## INSTEON DATABASE " <<  string(str) << "\n\n";
			
			ofs << KEY_START_CONFIG << ": "<< "\n";
			if(!_plmPath.empty())
				ofs << KEY_CONFIG_PORT << ": " << _plmPath << "\n";
			
			if(_latitude != 0 && _longitude != 0) {
				ofs << KEY_CONFIG_LATLONG << ": " ;
				ofs << setprecision(11)  << _latitude << " " << _longitude << "\n";
 			}
 
 			ofs << KEY_END_CONFIG << ":\n";
			ofs << "\n";

	//		ofs << "## PLM " << _plmDeviceID.string() << "  "<<  string(str) << "\n\n";
		}
		 
		// save device Info
		for (auto entry : _db) {
			
			string timeString = "";
			
			time_t	 updateTime = entry.isValidated?entry.lastUpdated:0;
			char timeStr[40] = {0};
			if(updateTime > 0.0){
				strftime(timeStr, sizeof(timeStr), kDateFormat, gmtime(&updateTime));
				timeString = string(timeStr);
			}
			
			string deviceName =  entry.name; //			deviceID.nameString();
			string description = 	entry.deviceInfo.descriptionString();

			ofs << KEY_START_DEVICE << ": " << entry.deviceID.string() << "\n";
			ofs << KEY_DEVICE_NAME << ": " << entry.name << "\n";
			// do this twice.. first the controllers then the responders
			
			if(timeString.size()){
				ofs <<  KEY_DEVICE_UPDATED << ": " << timeString << "\n";
			}
			
			for(auto group :entry.groups){
				bool isCNTRL =  get<0>(group);
				if(!isCNTRL) continue;
				
				uint8_t gid 	= get<1>(group);
				ofs << KEY_DEVICE_CNTL << ": ";
				ofs << to_hex <unsigned char>(isCNTRL?0x40:0x00) << " ";
				ofs << to_hex(gid) << " ";
				ofs << to_hex(entry.deviceInfo.GetCat()) << " ";
				ofs << to_hex(entry.deviceInfo.GetSubcat()) << " ";
				ofs << to_hex(entry.deviceInfo.GetFirmware()) << " ";
				ofs << to_hex(entry.deviceInfo.GetVersion()) << "\n";
				}
			
			for(auto group :entry.groups){
				bool isCNTRL =  get<0>(group);
	 			if(isCNTRL) continue;
				
				uint8_t gid 	= get<1>(group);
				ofs << KEY_DEVICE_RESP << ": ";
				ofs << to_hex <unsigned char>(isCNTRL?0x40:0x00) << " ";
				ofs << to_hex(gid) << "\n";
			}
			
			for(auto aldb :entry.deviceALDB){
				
				DeviceID aldbDev = DeviceID(aldb.devID);
				ofs << KEY_DEVICE_ALDB << ": ";
				ofs << to_hex <unsigned short>(aldb.address) << " ";
				ofs << to_hex <unsigned char>(aldb.flag) << " ";
				ofs << to_hex <unsigned char>(aldb.group) << " ";
				ofs <<  aldbDev.string() << " ";
				ofs << to_hex <unsigned char>(aldb.info[0]) << " ";
				ofs << to_hex <unsigned char>(aldb.info[1]) << "\n";
			}
			
			for(auto prop :entry.properties){
				ofs << prop.first << ": " <<  prop.second << "\n";
			}

			ofs << KEY_END_DEVICE << ":\n";
			ofs << "\n";
			
		}
		
		// save Group Info
		for (const auto& [groupID, _] : _deviceGroups) {
			
			groupInfo_t* info =  &_deviceGroups[groupID];
			
			ofs <<  KEY_START_GROUP << ": " << to_hex <unsigned short>(groupID) << " " << info->name << "\n";
			
			for(const auto &deviceID : info->devices){
				ofs << KEY_GROUP_DEVICEID << ": " <<  deviceID.string() << "\n";
	 		}
		
			ofs << KEY_END_GROUP <<  ":\n\n";
		}
		
		// save action Groups
		for (const auto& [actionGroupID, _] : _actionGroups) {
			
			ActionGroup actionGroup =  ActionGroup(actionGroupID);
			actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
			
			ofs <<   KEY_START_ACTIONGROUP << ": "  << actionGroup.string() << " " << info->name << "\n";
			
			for ( auto &i : info->actions) {
				Action act = i.second;
				ofs << KEY_ACTIONGROUP_ACTION << ": ";
				ofs << act.idString() << " ";
				nlohmann::json j = act.JSON();
				ofs << j.dump() << "\n";
			}
			
			ofs << KEY_END_ACTIONGROUP <<  ":\n\n";
		}
 
		for (const auto& [eventID, _] : _events) {
			
			Event* evt =  &_events[eventID];

			ofs <<   KEY_START_EVENT << ": "  << evt->idString() << " " << evt->_name << "\n";
				 
			Action act = evt->getAction();
			EventTrigger trig = evt->getEventTrigger();
	 
			ofs << KEY_ACTIONGROUP_ACTION << ": ";
	 		ofs << act.JSON().dump() << "\n";
			ofs << KEY_EVENT_TRIGGER << ": ";
			ofs << trig.JSON().dump() << "\n";
			ofs << KEY_END_EVENT <<  ":\n\n";
		}
		
		// save event Groups
		
		for (const auto& [eventGroupID, _] : _eventsGroups) {
			
			eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];

 			ofs <<   KEY_START_EVENTGROUP << ": "  << to_hex <unsigned short>( eventGroupID)
						<< " " << info->name << "\n";
	 
			for ( auto  i : info->eventIDs) {
				ofs << KEY_EVENT_EVENTID << ": "  << to_hex <unsigned short>( i)  << "\n";
	 		}
			
			ofs << KEY_END_EVENTGROUP <<  ":\n\n";
		}
	 
		ofs.flush();
		ofs.close();
			
		statusOk = true;
  	}
 	catch(std::ofstream::failure &writeErr) {
			statusOk = false;
	}
	return statusOk;
}
 

bool InsteonDB::saveToCacheFile(string fileName ){
	
	// debug  printDB();
	
	if(fileName.size() == 0)
		fileName = default_fileName();
	
	string path = _directoryPath + fileName;
	
	return backupCacheFile(path);
}

typedef enum  {
	RESTORE_UNKNOWN = 0,
	RESTORE_CONFIG,
	RESTORE_DEVICE,
	RESTORE_GROUP,
	RESTORE_ACTION_GROUP,
	RESTORE_EVENT,
	RESTORE_EVENT_GROUP,

}restoreState_t;

bool InsteonDB::restoreFromCacheFile(string fileName,
												 time_t validityDuration){
	
	
	bool 				statusOk = false;
	time_t 			now 	=  time(NULL);
	restoreState_t 	state = RESTORE_UNKNOWN;
	std::ifstream	ifs;
	
	// create a file path
	if(fileName.size() == 0)
		fileName = default_fileName();
	string path = _directoryPath + fileName;
	
	LOG_INFO("READ_CACHEFILE: %s\n", path.c_str());

	try{
		string line;
		std::lock_guard<std::mutex> lock(_mutex);
		
		// start with a fresh database
		_db.clear();
		_eTag++;
		
		groupID_t				groupID = 0;
		actionGroupID_t		actionGroupID = 0;
		DeviceID 			 	deviceID;
		eventID_t				eventID = 0;
		eventGroupID_t		eventGroupID = 0;
	
		// open the file
		ifs.open(path, ios::in);
		if(!ifs.is_open()) return false;
		
		
		while ( std::getline(ifs, line) ) {
			
			// split the line looking for a token: and rest and ignore comments
			line = Utils::trimStart(line);
			if(line.size() == 0) continue;
			if(line[0] == '#')  continue;
			size_t pos = line.find(":");
			if(pos ==  string::npos) continue;
			string  token = line.substr(0, pos);
			std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			string  rest = line.substr(pos+1);
			rest = Utils::trim(string(rest));
			
			const char *p = rest.c_str() ;
			int n;
			int cnt = 0;
			
			// set the state based on token
			if(token == KEY_START_CONFIG) {
				state = RESTORE_CONFIG;
				continue;
			}
			else if(token == KEY_END_CONFIG){
				state = RESTORE_UNKNOWN;
				continue;
			}
			else if(token == KEY_START_DEVICE){
				deviceID_t	devID;
				
				if( sscanf(p, "%hhx.%hhx.%hhx %n",
							  &devID[2], &devID[1], &devID[0] ,&n) == 3){
					
					deviceID = DeviceID(devID);
					
					// are we already in the DB
					if( findDBEntryWithDeviceID(deviceID) ) continue;
					insteon_dbEntry_t newEntry;
					initDBEntry(&newEntry,  devID);
					_db.push_back(newEntry);
					state = RESTORE_DEVICE;
					continue;
				}
				
			}
			else if(token == KEY_END_DEVICE){
				state = RESTORE_UNKNOWN;
				continue;
			}
			else if(token == KEY_START_GROUP){
				
				if( sscanf(p, "%hx %n", &groupID ,&n) == 1){
					p+=n;
					string groupName = Utils::trimStart(string(p));
					
					groupInfo_t info;
					info.name = groupName;
					info.devices.clear();
					_deviceGroups[groupID] = info;
					state = RESTORE_GROUP;
				}
				continue;
			}
			else if(token == KEY_END_GROUP){
				state = RESTORE_UNKNOWN;
				continue;
			}
			else if(token == KEY_START_ACTIONGROUP){
				if( sscanf(p, "%hx %n", &actionGroupID ,&n) == 1){
					p+=n;
					string groupName = Utils::trimStart(string(p));
					actionGroupInfo_t info;
					info.name = groupName;
					info.actions.clear();
					_actionGroups[actionGroupID] = info;
					state = RESTORE_ACTION_GROUP;
				}
				continue;
			}
			else if(token == KEY_END_ACTIONGROUP){
				state = RESTORE_UNKNOWN;
				eventID = 0;
				continue;
			}
	
			else if(token == KEY_START_EVENTGROUP){
				
				if( sscanf(p, "%hx %n", &eventGroupID ,&n) == 1){
					p+=n;
					string groupName = Utils::trimStart(string(p));
					eventGroupInfo_t info;
					info.name = groupName;
					info.eventIDs.clear();
					_eventsGroups[eventGroupID] = info;
					state = RESTORE_EVENT_GROUP;
				}
			}

			else if(token == KEY_END_EVENTGROUP){
				state = RESTORE_UNKNOWN;
				eventGroupID = 0;
				continue;
			}


			else if(token == KEY_START_EVENT){
				if( sscanf(p, "%hx %n", &eventID ,&n) == 1){
					p+=n;
					
				string eventName = Utils::trimStart(string(p));
				
					Event event = Event();
					event._rawEventID	=  eventID;
					event._name = eventName;
					_events[eventID] = event;
					state = RESTORE_EVENT;
				}
				continue;
			}
			else if(token == KEY_END_EVENT){
				state = RESTORE_UNKNOWN;
				continue;
			}
	
			
			// process line based on state.
			
			switch( state){
				case RESTORE_CONFIG:
				{
					if(token == KEY_CONFIG_PORT){
						_plmPath  = string(p);
						break;
					}
					else if(token == KEY_CONFIG_LATLONG){
						
						double d1 = 0;
						double d2 = 0;
						
						if( sscanf(p, "%lf %lf%n", &d1, &d2, &n) == 2) {
							_latitude = d1;
							_longitude = d2;
						}
						
					}
					
				}
					break;
					
				case RESTORE_DEVICE:
				{
					insteon_dbEntry_t* entry = findDBEntryWithDeviceID(deviceID);
					if(!entry) {
						state = RESTORE_UNKNOWN;
						break;
					}
					
					if(token == KEY_DEVICE_NAME){
						entry->name = string(p);
						break;
					}
					
					else if(token == KEY_DEVICE_UPDATED){
						time_t		lastUpdated = 0;
						struct tm tm;
						if(strptime(p, kDateFormat,  &tm) != NULL) {
							lastUpdated = timegm(&tm);
						}
						
						//  if the entry doesnt have an updated we need validate it later.
						bool isStillValid = ( lastUpdated + validityDuration >= now);
						entry->isValidated = isStillValid;
						entry->lastUpdated = lastUpdated;
						entry->lastLevelUpdate = time(NULL);
						entry->eTag = _eTag;
						break;
					}
					
					else if(token	== KEY_DEVICE_CNTL || token	== KEY_DEVICE_RESP){
						uint8_t		flag 	= 0;
						uint8_t		group = 0;
						uint8_t		cat	= 0;
						uint8_t		subcat = 0;
						uint8_t		firmware = 0;
						uint8_t		version = 0;  // Insteon engine version  0x01 for i1, 0x02 vor o2
						
						if( sscanf(p, "%hhx %hhx%n", &flag,  &group, &n) == 2) {
							
							bool isCTRL = (flag & 0x40) == 0x40;
							p += n;
							
							addGroupToDBEntry(entry,
													make_tuple(isCTRL,  group));
							
							if(isCTRL){
								cnt = sscanf(p, "%hhx %hhx %hhx %hhx%n",
												 &cat,  &subcat,  &firmware, &version, &n);
								
								//  if doesnt have the data then this need to be vbalidated.
								if(cnt == 4) {
									entry->deviceInfo  = DeviceInfo(cat,subcat,firmware,version );
								}
							}
						}
					}
					else if(token	== KEY_DEVICE_ALDB ) {
						// database deviceALDB
						uint16_t		addr = 0;
						uint8_t		flag 	= 0;
						uint8_t		group = 0;
						uint8_t		cat	= 0;
						uint8_t		subcat = 0;
						deviceID_t	aldbDevID;
						
						if( sscanf(p, "%hx %hhx %hhx %hhx.%hhx.%hhx %hhx %hhx%n",
									  &addr, &flag,  &group,
									  &aldbDevID[2], &aldbDevID[1], &aldbDevID[0],
									  &cat,  &subcat,
									  &n) == 8){
							p += n;
							
							insteon_aldb_t aldb;
							aldb.address = addr;
							copyDevID(aldbDevID, aldb.devID);
							aldb.flag = flag;
							aldb.group = group;
							aldb.info[0] = cat;
							aldb.info[1] = subcat;
							aldb.info[2] = 0;
							addDeviceALDBToDBEntry(entry, aldb);
						}
						
					}
					else // general property
					{
						entry->properties[token] = rest;
					}
				}
					break;
					
				case RESTORE_GROUP:
				{
					if(token	== KEY_GROUP_DEVICEID){
						deviceID_t	devID;
						
						// scan for a line starting with a deviceID
						cnt = sscanf(p, "%hhx.%hhx.%hhx %n",
										 &devID[2], &devID[1], &devID[0] ,&n);
						if(cnt ==  3) {
							
							DeviceID deviceID = DeviceID(devID);
							if(auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL) {
								
								if(_deviceGroups.count(groupID) != 0){
									groupInfo_t* info  =  &_deviceGroups[groupID];
									info->devices.insert(deviceID);
								}
							}
						}
					}
				}
					break;
					
				case RESTORE_ACTION_GROUP:
				{
					
					if(token == KEY_ACTIONGROUP_ACTION) {
						// loading actionGroupID;
						actionID_t actionID = 0;
						// scan for a line starting with a action ID
						if( sscanf(p, "%hx %n", &actionID ,&n) < 1) continue;
						p += n;
						
						Action action = Action(string(p));
						if(!action.isValid()) continue;
						action._actionID = actionID;
						
						if(_actionGroups.count(actionGroupID) != 0){
							actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
							
							if(info->actions.count(actionID) == 0){
								info->actions[actionID] = action;
							}
						}
					}
				}
					break;
					
				case RESTORE_EVENT: {
					
					if(token == KEY_ACTIONGROUP_ACTION) {
						Action action = Action(string(p));
						if(!action.isValid()) continue;
						
						if(_events.count(eventID) >0 ){
							auto evt = &_events[eventID];
							evt->_action = action;
						}
						
					} else if(token == KEY_EVENT_TRIGGER){
						EventTrigger trig = EventTrigger(string(p));
						if(!trig.isValid()) continue;
						
						if(_events.count(eventID) >0 ){
							auto evt = &_events[eventID];
							evt->_trigger = trig;
						}
					}
					continue;
				}
					break;
					
				case RESTORE_EVENT_GROUP: {
					if(token == KEY_EVENT_EVENTID){
						
						// loading eventGroupID;
						eventID_t eventID = 0;
						// scan for a line starting with a event ID
						if( sscanf(p, "%hx %n", &eventID ,&n) < 1) continue;
						p += n;
						
						if(_eventsGroups.count(eventGroupID) != 0){
							eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
							info->eventIDs.insert(eventID);
						}
					}
				}
					break;
					
				case RESTORE_UNKNOWN:
					break;
			}
		}
		
		statusOk = true;
		ifs.close();

	}
	catch(std::ifstream::failure &err) {
		
		LOG_INFO("READ_CACHEFILE:FAIL: %s\n", err.what());
		statusOk = false;
	}
	
	return statusOk;
}


string InsteonDB::cacheFileNameFromPLM(DeviceID deviceID) {
	return (deviceID.string() + ".txt");
}

//MARK: - actionGroup API

bool InsteonDB::actionGroupIsValid(actionGroupID_t actionGroupID){


	return (_actionGroups.count(actionGroupID) > 0);
}
 
bool InsteonDB::actionGroupCreate(actionGroupID_t* actionGroupIDOut, const string name){
	
	std::uniform_int_distribution<long> distribution(LONG_MIN,LONG_MAX);
	actionGroupID_t actionGroupID;

	do {
		actionGroupID = distribution(_rng);
	}while( _actionGroups.count(actionGroupID) > 0);

	actionGroupInfo_t info;
	info.name = name;
	info.actions.clear();
	_actionGroups[actionGroupID] = info;
 
	saveToCacheFile();
	
	if(actionGroupIDOut)
		*actionGroupIDOut = actionGroupID;
	return true;
}

bool InsteonDB::actionGroupDelete(actionGroupID_t actionGroupID){

 
	if(_actionGroups.count(actionGroupID) == 0)
		return false;

	_actionGroups.erase(actionGroupID);
	saveToCacheFile();

	return true;
}

 
bool InsteonDB::actionGroupFind(string name, actionGroupID_t* actionGroupIDOut){

	for(auto g : _actionGroups) {
		auto info = &g.second;

		if (strcasecmp(name.c_str(), info->name.c_str()) == 0){
				if(actionGroupIDOut){
				*actionGroupIDOut =  g.first;
			}
			return true;
		}
	}
	return false;
}


bool InsteonDB::actionGroupSetName(actionGroupID_t actionGroupID, string name){

	if(_actionGroups.count(actionGroupID) == 0)
		return false;
 
	actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
	info->name = name;

	saveToCacheFile();
	return true;
}


string InsteonDB::actionGroupGetName(actionGroupID_t actionGroupID){

	if(_actionGroups.count(actionGroupID) == 0)
		return "";
 
	actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
	return info->name;
}


bool InsteonDB::actionGroupAddAction(actionGroupID_t actionGroupID,
												 Action action,
												 actionID_t* actionIDOut){
	
	if(_actionGroups.count(actionGroupID) == 0)
		return false;
 
	actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
	// create an actionID unique to this actionGroup
	
	std::uniform_int_distribution<long> distribution(LONG_MIN,LONG_MAX);
	actionID_t aid;

	do {
		aid = distribution(_rng);
	}while(info->actions.count(aid) > 0 );
	
	action._actionID = aid;
	info->actions[aid] = action;
	
	if(actionIDOut)
		*actionIDOut = aid;
	
	saveToCacheFile();

	return true;
}


bool InsteonDB::actionGroupRemoveAction(actionGroupID_t actionGroupID, actionID_t actionID){

	if(_actionGroups.count(actionGroupID) == 0)
		return false;
 
	actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
	info->actions.erase(actionID);
 	saveToCacheFile();
	return true;
}

bool InsteonDB::actionGroupIsValidActionID(actionGroupID_t actionGroupID, actionID_t actionID){
	
	if(_actionGroups.count(actionGroupID) == 0)
		return false;
	
	actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
	return info->actions.count(actionID) > 0;
	
}


vector<reference_wrapper<Action>> InsteonDB::actionGroupGetActions(actionGroupID_t actionGroupID){

	vector<reference_wrapper<Action>> actions;

	if(_actionGroups.count(actionGroupID) > 0){
		actionGroupInfo_t* info  =  &_actionGroups[actionGroupID];
		
		for ( auto &i : info->actions) {
			actions.push_back(std::ref(i.second));
		}
	}

	return actions;
}


vector<actionGroupID_t> InsteonDB::allActionGroupsIDs(){
	vector<actionGroupID_t> actionGroupIDs;
	
	for(auto const& aid: _actionGroups)
		actionGroupIDs.push_back(aid.first);
	
	return actionGroupIDs;
}


//MARK: - group API

bool InsteonDB::groupFind(string name, GroupID* groupIDOut){

	for(auto g : _deviceGroups) {
		auto info = &g.second;
		
		if (strcasecmp(name.c_str(), info->name.c_str()) == 0){
				if(groupIDOut){
 				*groupIDOut = GroupID(g.first);
  			}
			return true;
		}
	}
	return false;
}

bool InsteonDB::groupCreate(GroupID* groupIDOut, const string name){
	
	std::uniform_int_distribution<long> distribution(SHRT_MIN,SHRT_MAX);
	
	groupID_t gid;
	
	do {
		gid = distribution(_rng);
	}while( _deviceGroups.count(gid) > 0);

	groupInfo_t info;
	info.name = name;
	info.devices.clear();
	_deviceGroups[gid] = info;
	
	saveToCacheFile();
	
	if(groupIDOut)
		*groupIDOut = GroupID(gid);
	return true;
}


bool InsteonDB::groupDelete(GroupID groupID){
	
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);
	 
	if(_deviceGroups.count(gid) == 0)
		return false;

	_deviceGroups.erase(gid);
	saveToCacheFile();

	return true;
}

bool InsteonDB::groupSetName(GroupID groupID, string name){

	groupID_t gid;
	groupID.copyToGroupID_t(&gid);

	if(_deviceGroups.count(gid) == 0)
		return false;

	groupInfo_t* info =  &_deviceGroups[gid];
	info->name = name;
	
	saveToCacheFile();

	return true;
}


bool InsteonDB::groupAddDevice(GroupID groupID, DeviceID deviceID){
	
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);

	// is group valid
	if(_deviceGroups.count(gid) == 0)
		return false;
	
	// is device in database
	if(!findDBEntryWithDeviceID(deviceID))
		return false;

	groupInfo_t* info  =  &_deviceGroups[gid];
	info->devices.insert(deviceID);
	saveToCacheFile();
	return true;
}

bool InsteonDB::groupRemoveDevice(GroupID groupID, DeviceID deviceID){
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);

	// is group valid
	if(_deviceGroups.count(gid) == 0)
		return false;
	
	// is device in the group
	groupInfo_t* info  =  &_deviceGroups[gid];
	if(info->devices.count(deviceID) == 0)
		return false;

	info->devices.erase(deviceID);
 	saveToCacheFile();
	return true;
}

string InsteonDB::groupGetName(GroupID groupID) {
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);

	if(_deviceGroups.count(gid) == 0)
		return "";
	
	groupInfo_t* info  =  &_deviceGroups[gid];
 
	return info->name;
}

vector<DeviceID> InsteonDB::groupGetDevices(GroupID groupID){
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);
	vector<DeviceID> devices;

	if(_deviceGroups.count(gid) > 0){
		groupInfo_t* info  =  &_deviceGroups[gid];

		for(auto device : info->devices){
			devices.push_back(device);
		}
	}
	 
	return devices;
}

vector<GroupID> InsteonDB::allGroups(){
	vector<GroupID> groups;
 
	for (const auto& [key, _] : _deviceGroups) {
		groups.push_back( GroupID(key));
	}
	
	return groups;
}

vector<GroupID> InsteonDB::groupsContainingDevice(DeviceID deviceID){
	vector<GroupID> groups;
 
	for(auto g : _deviceGroups) {
		groupInfo_t* info = &g.second;
	
		if(info->devices.count(deviceID) != 0){
			groups.push_back( GroupID(g.first));
		}
 	}
	return groups;
}


bool InsteonDB::groupIsValid(GroupID groupID){
	groupID_t gid;
	groupID.copyToGroupID_t(&gid);

	return (_deviceGroups.count(gid) > 0);
}

//MARK: - Events API


bool InsteonDB::eventsIsValid(eventID_t eid){
	
	return(_events.count(eid) > 0);
}


bool InsteonDB::eventSave(Event event, eventID_t* eventIDOut){
	
	std::uniform_int_distribution<long> distribution(SHRT_MIN,SHRT_MAX);
	eventID_t eid;

	do {
		eid = distribution(_rng);
	}while( _events.count(eid) > 0);

	event._rawEventID = eid;
	_events[eid] = event;
	
	saveToCacheFile();
 
	if(eventIDOut)
		*eventIDOut = eid;

	return true;
}

 
bool InsteonDB::eventDelete(eventID_t eventID){
	
	if(_events.count(eventID) == 0)
		return false;
	 
	// remove from any event groups first
	for (const auto& [eventGroupID, _] : _eventsGroups) {
		eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
		
		if(info->eventIDs.count(eventID)){
			info->eventIDs.erase(eventID);
		}
 	}
	
	_events.erase(eventID);
	saveToCacheFile();
	return true;
}

bool InsteonDB::eventSetName(eventID_t eventID, string name){
	
	if(_events.count(eventID) == 0)
		return false;
	
	Event* evt =  &_events[eventID];
	evt->_name = name;
 
	saveToCacheFile();
	
	return true;
}

string InsteonDB::eventGetName(eventID_t eventID) {
	
	if(_events.count(eventID) == 0)
		return "";

	Event* evt =  &_events[eventID];
	return evt->_name;
}

bool InsteonDB::eventFind(string name, eventID_t* eventIDOut){
	
	for(auto e : _events) {
		auto event = &e.second;
		
		if (strcasecmp(name.c_str(), event->_name.c_str()) == 0){
				if(eventIDOut){
				*eventIDOut = e.first;
			}
			return true;
		}
	}
	return false;
}

optional<reference_wrapper<Event>> InsteonDB::eventsGetEvent(eventID_t eventID){

	if(_events.count(eventID) >0 ){
		return  ref(_events[eventID]);
	}
	 
	return optional<reference_wrapper<Event>> ();
}

vector<eventID_t> InsteonDB::allEventsIDs(){
	vector<eventID_t> events;
 
 for (const auto& [key, _] : _events) {
	 events.push_back( key);
 	}
	
	return events;
}

vector<eventID_t> InsteonDB::matchingEventIDs(EventTrigger trig){
	vector<eventID_t> events;
	
	for (auto& [key, evt] : _events) {
		if(evt._trigger.shouldTriggerFromDeviceEvent(trig))
			events.push_back( key);
	};
		
	return events;
}

vector<eventID_t> InsteonDB::eventsThatNeedToRun(solarTimes_t &solar, time_t localNow){
	vector<eventID_t> events;

	for (auto& [key, evt] : _events) {
		if(evt._trigger	.shouldTriggerFromTimeEvent(solar, localNow))
			events.push_back( key);
	};
		
	return events;
}

vector<eventID_t> InsteonDB::eventsInTheFuture(solarTimes_t &solar, time_t localNow){
	vector<eventID_t> events;

	for (auto& [key, evt] : _events) {
		if(evt._trigger	.shouldTriggerInFuture(solar, localNow))
			events.push_back( key);
	};
		
	return events;
}

bool InsteonDB::eventSetLastRunTime(eventID_t eventID, time_t localNow){
	
	if(_events.count(eventID) == 0)
		return false;
	
	Event* evt =  &_events[eventID];
	return evt->_trigger.setLastRun(localNow);
}

//MARK: - event group API

// event groups
bool InsteonDB::eventGroupIsValid(eventGroupID_t eventGroupID){
	return (_eventsGroups.count(eventGroupID) > 0);
}

bool InsteonDB::eventGroupCreate(eventGroupID_t* eventGroupIDOut, const string name){
	
	std::uniform_int_distribution<long> distribution(LONG_MIN,LONG_MAX);
	eventGroupID_t eventGroupID;

	do {
		eventGroupID = distribution(_rng);
	}while( _eventsGroups.count(eventGroupID) > 0);

	eventGroupInfo_t info;
	info.name = name;
	info.eventIDs.clear();
	_eventsGroups[eventGroupID] = info;
 
	saveToCacheFile();
	
	if(eventGroupIDOut)
		*eventGroupIDOut = eventGroupID;
	return true;
}

bool InsteonDB::eventGroupDelete(eventGroupID_t eventGroupID){
 
	if(_eventsGroups.count(eventGroupID) == 0)
		return false;

	_eventsGroups.erase(eventGroupID);
	saveToCacheFile();

	return true;
}


bool InsteonDB::eventGroupFind(string name, eventGroupID_t* eventGroupIDOut){
	
	for(auto g : _eventsGroups) {
		auto info = &g.second;
		
		if (strcasecmp(name.c_str(), info->name.c_str()) == 0){
			if(eventGroupIDOut){
				*eventGroupIDOut =  g.first;
			}
			return true;
		}
	}
	return false;
}


bool InsteonDB::eventGroupSetName(eventGroupID_t eventGroupID, string name){

	if(_eventsGroups.count(eventGroupID) == 0)
		return false;
 
	eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
	info->name = name;

	saveToCacheFile();
	return true;
}


string InsteonDB::eventGroupGetName(eventGroupID_t eventGroupID){

	if(_eventsGroups.count(eventGroupID) == 0)
		return "";
 
	eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
	return info->name;
}



bool InsteonDB::eventGroupAddEvent(eventGroupID_t eventGroupID,  eventID_t eventID){

	if(_eventsGroups.count(eventGroupID) == 0)
		return false;

	if(!eventsIsValid(eventID))
		return false;

	eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
	info->eventIDs.insert(eventID);
	
	saveToCacheFile();

	return true;
}

bool InsteonDB::eventGroupRemoveEvent(eventGroupID_t eventGroupID, eventID_t eventID){
	
	if(_eventsGroups.count(eventGroupID) == 0)
		return false;
	
	eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
	
	if(info->eventIDs.count(eventID) == 0)
		return false;

	info->eventIDs.erase(eventID);

	saveToCacheFile();
	return true;
}

bool InsteonDB::eventGroupContainsEventID(eventGroupID_t eventGroupID, eventID_t eventID){
	
	if(_eventsGroups.count(eventGroupID) == 0)
		return false;
	
	eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
	
	return(info->eventIDs.count(eventID) != 0);
}
 
vector<eventID_t> InsteonDB::eventGroupGetEventIDs(eventGroupID_t eventGroupID){
	vector<eventID_t> eventIDs;

	if(_eventsGroups.count(eventGroupID) != 0){
		eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];
		std::copy(info->eventIDs.begin(), info->eventIDs.end(), std::back_inserter(eventIDs));
	}

	return eventIDs;
}

vector<eventGroupID_t> InsteonDB::allEventGroupIDs(){
	vector<eventGroupID_t> eventGroupIDs;
 
 for (const auto& [key, _] : _eventsGroups) {
	 eventGroupIDs.push_back( key);
	}
	
	return eventGroupIDs;
}


void InsteonDB::reconcileEventGroups(const solarTimes_t &solar, time_t localNow){

	// event groups prevent us from running needless events when we reboot.
	// we only run the last elligable one for setting a steady state
	
	long nowMins = (localNow - solar.previousMidnight) / SECS_PER_MIN;

	for (const auto& [eventGroupID, _] : _eventsGroups) {
		eventGroupInfo_t* info  =  &_eventsGroups[eventGroupID];

		// create a map all all events that need to run
		map <int16_t, eventID_t> eventMap;
		
		for(auto eventID : info->eventIDs ){
			Event* evt =  &_events[eventID];
			int16_t minsFromMidnight = 0;
			
			if(evt->_trigger.calculateTriggerTime(solar,minsFromMidnight)){
				if(minsFromMidnight <= nowMins){
					eventMap[minsFromMidnight] = eventID;
				}
			}
		};
	 
		if(eventMap.size() > 0){
			// delete the last one
			eventMap.erase(prev(eventMap.end()));
			
		// mark the rest as ran
			for(auto item : eventMap ){
				auto eventID  = item.second;
				eventSetLastRunTime(eventID, localNow);
			}

		}
	}
	
}
 

//MARK: - private API


void  InsteonDB::initDBEntry(insteon_dbEntry_t *newEntry, DeviceID deviceID){
	
	if (!newEntry)
		throw std::invalid_argument("no insteon_dbEntry_t");
	
	bzero(newEntry, sizeof(insteon_dbEntry_t));
	newEntry->deviceID =  deviceID;
	newEntry->isValidated = false;
	newEntry->groups.clear();
	newEntry->levelMap.clear();
	newEntry->deviceALDB.clear();
	newEntry->properties.clear();
}


insteon_dbEntry_t* InsteonDB::findDBEntryWithDeviceID(DeviceID deviceID){
	
	for (auto e = _db.begin(); e != _db.end(); e++) {
		if(e->deviceID == deviceID)
			return &(*e);
	}
	
	return NULL;
}


void InsteonDB::addGroupToDBEntry(insteon_dbEntry_t* entry,
											 tuple< bool, uint8_t> newItem) {
	
	if (!entry)
		throw std::invalid_argument("no insteon_dbEntry_t");
	
	bool foundOne = false;
	
	for (auto e = entry->groups.begin(); e != entry->groups.end(); e++) {
		if( *e == newItem){
			foundOne = true;
			break;
		}
	}
	
	if(!foundOne){
		entry->groups.push_back(newItem);
		deviceWasUpdated(entry->deviceID);
	}
}


void InsteonDB::addDeviceALDBToDBEntry(insteon_dbEntry_t* entry,
													insteon_aldb_t newItem) {
	
	if (!entry)
		throw std::invalid_argument("no insteon_dbEntry_t");

	bool foundOne = false;
	
	for (auto e = entry->deviceALDB.begin(); e != entry->deviceALDB.end(); e++) {
		if( e->address == newItem.address){
			foundOne = true;
			break;
		}
	}
	
	if(!foundOne){
		entry->deviceALDB.push_back(newItem);
	}

}

bool InsteonDB::foundWithDeviceID(DeviceID deviceID, bool isCTRL, uint8_t GroupID) {
	
	if(auto entry = findDBEntryWithDeviceID(deviceID); entry != NULL) {
		
		for(auto group :entry->groups){
			bool cntl =  get<0>(group);
			uint8_t gid 	= get<1>(group);
			
			if(isCTRL == cntl && GroupID == gid)
				return true;
		}
	}
	return false;
}


// for notifcations
void InsteonDB::deviceWasUpdated(DeviceID deviceID){
	
	NotificationCenter::defaultNotificationCenter()->postNotification(DatabaseChangedNotification,
																							(void*) &deviceID);
}
void InsteonDB::deviceStateWasUpdated(DeviceID deviceID){
	
	NotificationCenter::defaultNotificationCenter()->postNotification(DeviceStateChangedNotification,
																							(void*) &deviceID);
}



// MARK: - debugging

string InsteonDB::dumpDB(bool printALDB)
{
	std::ostringstream oss;

	oss << "--------------------- " << _db.size()  << " device entries --------------------- \n";
 
	for (insteon_dbEntry_t entry : _db) {
		dumpDBInfo(oss, entry.deviceID, printALDB);
	}

	oss <<  "--------------------------------------------------------------\n";
 	return oss.str();
}


void InsteonDB::dumpDBInfo(std::ostringstream &oss, DeviceID deviceID, bool printALDB ){
 
	auto entry = findDBEntryWithDeviceID(deviceID);
	if(!entry) return;
	
	oss<< setfill(' ');
	oss << (entry->isValidated?"V":" ") << ( entry->lastUpdated?"U":" ") << " ";
	oss << setiosflags(ios::left);

	string deviceName = entry->name;
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
	oss << setw(0);
	oss << " " << entry->deviceID.string() << " ";
	 
	oss << setw(3);

	if(entry->levelMap.size() == 1){
		auto firstEntry = *entry->levelMap.begin();
		uint8_t onLevel = firstEntry.second;
		oss << InsteonDevice::onLevelString(onLevel);
	}
	else
		oss << "  ";
	
	oss << " ";
	{
		static const char* const lut = "0123456789ABCDEF";
		
		std::vector<uint8_t> cntlGrp;
		std::vector<uint8_t> respGrp;
		
		for(auto group :entry->groups){
			bool isCNTRL =  get<0>(group);
			uint8_t gid 	= get<1>(group);
			
			if(isCNTRL){
				cntlGrp.push_back(gid);
			}
			else {
				respGrp.push_back(gid);
				
			}
			//				if(isCNTRL) groupStr += "[";
			//				groupStr += lut[gid >> 4];
			//				groupStr += lut[gid & 0xf];
			//				if(isCNTRL) groupStr += "]";
			//				groupStr += " ";
		}
		
		// sort them
		std::sort(cntlGrp.begin(), cntlGrp.end(),
					 [] (uint8_t const& a, uint8_t const& b) { return a < b; });
		
		std::sort(respGrp.begin(), respGrp.end(),
					 [] (uint8_t const& a, uint8_t const& b) { return a < b; });
		
		std::string groupStr;
		
		int cnt = (int)cntlGrp.size();
		if(cnt > 0) {
			groupStr += "[";
			for(auto group :cntlGrp){
				if(group > 0xf){
					groupStr += lut[group >> 4];
				}
				groupStr += lut[group & 0xf];
				if(--cnt)
					groupStr += " ";
			}
			groupStr += "] ";
		}
		
		cnt = (int)respGrp.size();
		
		if(respGrp.size() > 0) {
			for(auto group :respGrp){
				if(group > 0xf){
					groupStr += lut[group >> 4];
				}
				groupStr += lut[group & 0xf];
				
				if(--cnt)
					groupStr += " ";
			}
		}
		
		oss << setiosflags(ios::left) << setw(16) << groupStr;
 	}
	
	if(!entry->deviceInfo.isNULL())
	{
		string version = "??";
		if(entry->deviceInfo.GetVersion() == 1)
			version = "i2";
		else if(entry->deviceInfo.GetVersion() == 2)
			version = "i2CS";
		
		oss << setw(0);
		
		oss << " (";
		
		oss << to_hex(entry->deviceInfo.GetCat()) << ",";
		oss << to_hex(entry->deviceInfo.GetSubcat()) << ",";
		oss << to_hex(entry->deviceInfo.GetFirmware()) << ") ";
		oss << setw(4) << version ;
		oss <<  setw(0) << " " <<  entry->deviceInfo.descriptionString();
 	}

	oss << "\n";
	
	if(printALDB){
		for(auto e :entry->deviceALDB){
			
			
		//	Bit 6: 1 = Controller (Master) of Device ID, 0 = Responder to (Slave of) Device ID
			bool isRESP = (e.flag & 0x40) == 0x00;
			
			DeviceID aldbDev = DeviceID(e.devID);
			DeviceInfo info = DeviceInfo(e.info);
		
			oss << setfill('0') << setw(4) << hex << (int) e.address << " ";
			oss << (isRESP?" ":"[") << setfill('0') << setw(2) << hex << (int) e.group  << ( isRESP?" ":"]") << " ";
			oss <<  aldbDev.string() << " " << info.string();
			auto aldbEntry = findDBEntryWithDeviceID(aldbDev);
			if(aldbEntry && aldbEntry->name.size())
				oss << " " << aldbEntry->name;
			oss << "\n";
			}
		oss << "\n";
	}

}


/*

void InsteonDB::printDB(bool printALDB){
	
	printf("\n--------------------- %2ld device entries  ---------------------\n", _db.size());
	
	
	for (insteon_dbEntry_t entry : _db) {
		printDeviceInfo(entry.deviceID,printALDB);
	}
	printf("--------------------------------------------------------------\n");
}


void InsteonDB::printDeviceInfo(DeviceID deviceID, bool printALDB){
	
	auto entry = findDBEntryWithDeviceID(deviceID);
	
	if(entry){
		printf("%s%s ",
				 entry->isValidated?"V":" ",
				 entry->lastUpdated?"U":" ");
		
		printf("%-20.20s", entry->deviceID.name_cstr());
		printf(" %s ",entry->deviceID.string().c_str());
		
		if(entry->levelMap.size() == 1){
			
			auto firstEntry = *entry->levelMap.begin();
 			uint8_t onLevel = firstEntry.second;
			if(onLevel == 0)
				printf("off ");
			else if(onLevel == 255)
				printf("on  ");
			else
 				printf("%3d ",(int) (onLevel / 255.) * 100 );
		}
		else
	 		printf("??? ");
  
		
		{
			static const char* const lut = "0123456789ABCDEF";
			
			std::vector<uint8_t> cntlGrp;
			std::vector<uint8_t> respGrp;
			
			for(auto group :entry->groups){
				bool isCNTRL =  get<0>(group);
				uint8_t gid 	= get<1>(group);
				
				if(isCNTRL){
					cntlGrp.push_back(gid);
				}
				else {
					respGrp.push_back(gid);
					
				}
				//				if(isCNTRL) groupStr += "[";
				//				groupStr += lut[gid >> 4];
				//				groupStr += lut[gid & 0xf];
				//				if(isCNTRL) groupStr += "]";
				//				groupStr += " ";
			}
			
			// sort them
			std::sort(cntlGrp.begin(), cntlGrp.end(),
						 [] (uint8_t const& a, uint8_t const& b) { return a < b; });
			
			std::sort(respGrp.begin(), respGrp.end(),
						 [] (uint8_t const& a, uint8_t const& b) { return a < b; });
			
			std::string groupStr;
			
			int cnt = (int)cntlGrp.size();
			if(cnt > 0) {
				groupStr += "[";
				for(auto group :cntlGrp){
					if(group > 0xf){
						groupStr += lut[group >> 4];
					}
					groupStr += lut[group & 0xf];
					if(--cnt)
						groupStr += " ";
				}
				groupStr += "] ";
			}
			
			cnt = (int)respGrp.size();
			
			if(respGrp.size() > 0) {
				for(auto group :respGrp){
					if(group > 0xf){
						groupStr += lut[group >> 4];
					}
					groupStr += lut[group & 0xf];
					
					if(--cnt)
						groupStr += " ";
				}
			}
			
			printf("%-16.16s",groupStr.c_str());
			
		}
		if(!entry->deviceInfo.isNULL())
		{
			string version = "??";
			if(entry->deviceInfo.GetVersion() == 1)
				version = "i2";
			else if(entry->deviceInfo.GetVersion() == 2)
				version = "i2CS";
			
			printf("  (%02X,%02X %02x) %-4s %-20.20s",
					 entry->deviceInfo.GetCat(),
					 entry->deviceInfo.GetSubcat(),
					 entry->deviceInfo.GetFirmware(),
					 version.c_str(),
					 entry->deviceInfo.description_cstr());
			
		}
		
		printf("\n");
		if(printALDB){
			for(auto e :entry->deviceALDB){
				
			//	Bit 6: 1 = Controller (Master) of Device ID, 0 = Responder to (Slave of) Device ID
				bool isRESP = (e.flag & 0x40) == 0x00;
				
				DeviceID aldbDev = DeviceID(e.devID);
				DeviceInfo info = DeviceInfo(e.info);
				
				string plmName = aldbDev.nameString()	;
				
				printf("\t%04x %s%02x%s %s %s %s \n" ,
						 e.address,
						 isRESP?" ":"[",  e.group, isRESP?" ":"]",
						 aldbDev.string().c_str(),
						 info.string().c_str(),
						 plmName.c_str()
						 );
				
			}
			printf("\n");
		}
		
	}
}
 
*/
