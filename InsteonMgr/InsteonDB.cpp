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

#include "InsteonDB.hpp"
#include "LogMgr.hpp"
#include "NotificationCenter.hpp"
#include "InsteonDevice.hpp"



using namespace std;

const char *  InsteonDB::DatabaseChangedNotification = "InsteonMgr::DatabaseChangedNotification";
const char *  InsteonDB::DeviceStateChangedNotification = "InsteonMgr::DeviceStateChangedNotification";
 

InsteonDB::InsteonDB() {
	
	_expired_age = 60*60*24*30;
	_directoryPath = "";
	_db.clear();
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
		LOG_INFO("\tDB UPDATE %s \"%s\"  %s rev %02x\n",
					deviceID.string().c_str(),
					deviceID.name_cstr(),
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
		
		if(!e->isValidated) break;
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
		
		if(!e->isValidated) break;
		
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
	
	string fileName = "PLMDB_" + _plmDeviceID.string() + ".txt";
	return fileName;
 }


static const char kDateFormat[] = "%Y-%m-%d %H:%M:%S";

bool InsteonDB::backupCacheFile(string filepath){
	
	// debug  printDB();
	
	std::lock_guard<std::mutex> lock(_mutex);
	bool statusOk = false;
	
	std::ofstream ofs;
	
	try{
		ofs.open(filepath);
		
		if(ofs.fail())
			return false;
			
		ofs << "## PLM " << _plmDeviceID.string() << " - " <<_plmDeviceID.nameString() << "\n";
		ofs << "PLMID: "  << _plmDeviceID.string() << "\n\n";
	
		// save device Info
		ofs << "DEVICES-START:" << "\n";
		for (auto entry : _db) {

			time_t	 updateTime = entry.isValidated?entry.lastUpdated:0;
			char timeStr[40] = {0};
			if(updateTime > 0.0){
				strftime(timeStr, sizeof(timeStr), kDateFormat, gmtime(&updateTime));
			}
			else
			{
				//			LOG_ERROR("skipped saving: %s\n" ,entry.deviceID.string().c_str());
				//			continue;
			}
			
			string deviceName =  entry.deviceID.nameString();
			string description = 	entry.deviceInfo.descriptionString();
			
			if(deviceName.size() > 0){
				ofs << "## " << entry.deviceID.nameString();
				if(description.size())
					ofs << " - " << description;
				ofs << "\n";
			}

			// do this twice.. first the controllers then the responders
			
			for(auto group :entry.groups){
				bool isCNTRL =  get<0>(group);
				if(!isCNTRL) continue;
				
				uint8_t gid 	= get<1>(group);
				
				ofs << entry.deviceID.string() << " ";
				ofs  << hex;
				ofs << setfill('0') << setw(2) << (int)(isCNTRL?0x40:0x00) << " ";
				ofs << setfill('0') << setw(2) <<(int)gid << " ";
				ofs << setfill('0') << setw(2) << (int)entry.deviceInfo.GetCat() << " ";
				ofs << setfill('0') << setw(2) << (int)entry.deviceInfo.GetSubcat() << " ";
				ofs << setfill('0') << setw(2) << (int)entry.deviceInfo.GetFirmware() << " ";
				ofs << setfill('0') << setw(2) << (int)entry.deviceInfo.GetVersion() << " ";
				ofs << timeStr << "\n";
	 		}
			
			for(auto group :entry.groups){
				bool isCNTRL =  get<0>(group);
				if(isCNTRL) continue;
				
				uint8_t gid 	= get<1>(group);
				
				ofs << entry.deviceID.string() << " ";
				ofs  << hex;
				ofs << setfill('0') << setw(2) << (isCNTRL?0x40:0x00) << " ";
				ofs << setfill('0') << setw(2) <<(int)gid << "\n";
		}
			
			for(auto aldb :entry.deviceALDB){
				
				DeviceID aldbDev = DeviceID(aldb.devID);
				
				ofs << entry.deviceID.string() << " ";
				ofs  << hex;
				ofs << setfill('0') << setw(4) << aldb.address << " ";
				ofs << setfill('0') << setw(2) <<(int)aldb.flag << " ";
				ofs << setfill('0') << setw(2) <<(int)aldb.group << " ";
				ofs <<  aldbDev.string() << " ";
				ofs << setfill('0') << setw(2) << (int)aldb.info[0] << " ";
				ofs << setfill('0') << setw(2) << (int)aldb.info[1] << "\n";
			}
	 	ofs << "\n";

		}
		ofs << "DEVICES-END:" << "\n\n";
	
		
		// save Group Info
		for (const auto& [groupID, _] : _deviceGroups) {
			
			groupInfo_t* info =  &_deviceGroups[groupID];
			
			ofs << "GROUP-START: "
				<< setfill('0') << setw(4) << hex << groupID
				<< setfill(' ') << setw(0) << " " << info->name << "\n";
		 
			
			for(const auto &deviceID : info->devices){
				ofs << deviceID.string() << "\n";
	 		}
		
			ofs << "GROUP-END:" << "\n\n";
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
	RESTORE_DEVICES,
	RESTORE_GROUP
}restoreState_t;

bool InsteonDB::restoreFromCacheFile(string fileName,
												 time_t validityDuration){


	std::lock_guard<std::mutex> lock(_mutex);
	time_t now =  time(NULL);
	FILE * fp;
	restoreState_t state = RESTORE_UNKNOWN;
	
	// start with a fresh database
	_db.clear();
	
	if(fileName.size() == 0)
		fileName = default_fileName();
 
	string path = _directoryPath + fileName;
	
	if( (fp = fopen (path.c_str(), "r")) == NULL)
		return false;

	_eTag++;

	char buffer[96];
	groupID_t	groupID = 0;

	state = RESTORE_DEVICES;
	while(fgets(buffer, sizeof(buffer), fp) != NULL) {
		
		deviceID_t	devID;
		uint8_t		flag 	= 0;
		uint8_t		group = 0;
		uint8_t		cat	= 0;
		uint8_t		subcat = 0;
		uint16_t		addr = 0;
		deviceID_t	aldbDevID;
	
		uint8_t		firmware = 0;
		uint8_t		version = 0;  // Insteon engine version  0x01 for i1, 0x02 vor o2
		time_t			lastUpdated = 0;
		
		bool 			isStillValid = false;
		
		size_t len = strlen(buffer);
		if (len == 0) continue;
		if (len == 1 && buffer[0] == '\n' ) continue;
		
		char *p = buffer;
		int n;
		int cnt = 0;
		
		// skip ws
		while(isspace(*p) && (*p !='\0')) p++;
		if(*p == '#') continue;
	 
		// check for PLMID: xx.xx.xx == _plmDeviceID
		char token[17] = {0};
		cnt = sscanf(p, "%16[^ :]:%hhx.%hhx.%hhx %n", token,
						 &devID[2], &devID[1], &devID[0],
						 &n);
		if(cnt == 4){
			if(strncmp(token, "PLMID", 5) == 0){
				DeviceID plmID = DeviceID(devID);
				if(plmID != _plmDeviceID)
					return false;
				else
					continue;
			}
		}
		
	 
		// check for mode change
		char delim = 0;
 		if(sscanf(p, "%16[^ :]%c %n", token,&delim, &n) == 2
			&& delim == ':'){
		 
			p+=n;
			if(strncmp(token, "GROUP-START", 11) == 0){
				
				if( sscanf(p, "%hx %n", &groupID ,&n) == 1){
					p+=n;
					size_t length = strlen(p);
					while(length > 0 && isspace((unsigned char)p[length-1]))
							  length--;
					p[length] = '\0';
			 
					groupInfo_t info;
					info.name = string(p);
					info.devices.clear();
					_deviceGroups[groupID] = info;
					state = RESTORE_GROUP;
					}
			}
			else if(strncmp(token, "GROUP-END", 9) == 0){
				state = RESTORE_DEVICES;
	 		}
			else if(strncmp(token, "DEVICES-START", 13) == 0){
				state = RESTORE_DEVICES;
			}
			else if(strncmp(token, "DEVICES-END", 11) == 0){
				state = RESTORE_UNKNOWN;
			}
			continue;
	}
		
		if(state == RESTORE_DEVICES){
			
			// scan for a line starting with a deviceID
			cnt = sscanf(p, "%hhx.%hhx.%hhx %n",
							 &devID[2], &devID[1], &devID[0] ,&n);
			
			if(cnt < 3) continue;
			p += n;
			
			DeviceID deviceID = DeviceID(devID);
			auto existing = findDBEntryWithDeviceID(deviceID);
			
			// check if this is a device entry or ALDB
			for(len = 0; !isspace(*(p+len)); len++) ;
			
			if(len == 4){
				
				// database deviceALDB
				cnt = sscanf(p, "%hx %hhx %hhx %hhx.%hhx.%hhx %hhx %hhx%n",
								 &addr, &flag,  &group,
								 &aldbDevID[2], &aldbDevID[1], &aldbDevID[0],
								 &cat,  &subcat,
								 &n);
				
				if(cnt < 8) continue;
				p += n;
				
				// guard deviceALDB entries must appear before responders..
				if(existing == NULL) continue;
				
				insteon_aldb_t aldb;
				aldb.address = addr;
				copyDevID(aldbDevID, aldb.devID);
				aldb.flag = flag;
				aldb.group = group;
				aldb.info[0] = cat;
				aldb.info[1] = subcat;
				aldb.info[2] = 0;
				addDeviceALDBToDBEntry(existing, aldb);
				
			}else if(len == 2){
				
				// Database entry
				cnt = sscanf(p, "%hhx %hhx%n",
								 &flag,  &group, &n);
				
				if(cnt < 2) continue;
				p += n;
				
				bool isCTRL = (flag & 0x40) == 0x40;
				
				if(isCTRL){
					cnt = sscanf(p, "%hhx %hhx %hhx %hhx%n",
									 &cat,  &subcat,  &firmware, &version, &n);
					
					//  if doesnt have the data then this need to be vbalidated.
					if(cnt == 4) {
						p += n;
						
						// yes a space preceeds the formatting for timeStr
						char timeStr[21] = {0};
						cnt = sscanf(p, " %20[^\n]",  timeStr );
						if(cnt == 1){
							struct tm tm;
							
							if(strptime(timeStr, kDateFormat,  &tm) != NULL) {
								lastUpdated = timegm(&tm);
							}
						}
						
						if( lastUpdated + validityDuration >= now)
							isStillValid = true;
					}
				}
				
				
				// guard codeControl entries must appear before responders..
				if((existing == NULL) && !isCTRL) continue;
				
				if(existing == NULL) {
					// create a new entry for Controllers only
					insteon_dbEntry_t newEntry;
					initDBEntry(&newEntry,  devID);
					
					addGroupToDBEntry(&newEntry,
											make_tuple(true,  group));
					
					newEntry.deviceInfo	= DeviceInfo(cat,subcat,firmware,version );
					
					newEntry.isValidated = isStillValid;
					newEntry.lastUpdated = lastUpdated;
					newEntry.lastLevelUpdate = time(NULL);
					newEntry.eTag = _eTag;
					
					newEntry.levelMap.clear();
					_db.push_back(newEntry);
				}
				else {
					// update the entry - typically for responders?
					
					addGroupToDBEntry(existing,
											make_tuple(isCTRL,  group));
					
					// not sure if we allow multiple controllers for a device?
					if(isCTRL){
						existing->deviceInfo	= DeviceInfo(cat,subcat,firmware, version );
						existing->lastUpdated = lastUpdated?:lastUpdated;
						existing->lastLevelUpdate = time(NULL);
						existing->isValidated = existing->isValidated?:isStillValid;
						existing->eTag = _eTag;
					}
				}
			}
			
		}
		else if(state == RESTORE_GROUP){
			
			// scan for a line starting with a deviceID
			cnt = sscanf(p, "%hhx.%hhx.%hhx %n",
							 &devID[2], &devID[1], &devID[0] ,&n);
			
			if(cnt < 3) continue;
			
			DeviceID deviceID = DeviceID(devID);
			
			if(auto existing = findDBEntryWithDeviceID(deviceID); existing != NULL) {
		
				if(_deviceGroups.count(groupID) != 0){
					groupInfo_t* info  =  &_deviceGroups[groupID];
					info->devices.insert(deviceID);
 				}
  			}
 		}
		else  break;
		
	}
	fclose(fp);
	
	return true;
}

string InsteonDB::cacheFileNameFromPLM(DeviceID deviceID) {
	return (deviceID.string() + ".txt");
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
	
	std::uniform_int_distribution<long> distribution(LONG_MIN,LONG_MAX);
	
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


//MARK: - private API


void  InsteonDB::initDBEntry(insteon_dbEntry_t *newEntry, DeviceID deviceID){
	
	if (!newEntry)
		throw std::invalid_argument("no insteon_dbEntry_t");
	
	bzero(newEntry, sizeof(insteon_dbEntry_t));
	newEntry->deviceID =  deviceID;
	newEntry->groups.clear();
	newEntry->isValidated = false;
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
		dumpDBInfo(oss, entry.deviceID,printALDB);
	}

	oss <<  "--------------------------------------------------------------\n";
 	return oss.str();
}


void InsteonDB::dumpDBInfo(std::ostringstream &oss, DeviceID deviceID, bool printALDB ){
 
	auto entry = findDBEntryWithDeviceID(deviceID);
	if(!entry) return;
	
	oss << (entry->isValidated?"V":" ") << ( entry->lastUpdated?"U":" ") << " ";
	oss << setiosflags(ios::left);

	constexpr size_t maxlen = 20;
		size_t len = deviceID.nameString().size();
		if(len<maxlen){
			oss << setw(maxlen);
			oss << deviceID.nameString();
		}
		else {
			oss << deviceID.nameString().substr(0,maxlen -1);
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
		
		oss << setfill('0') << setw(2) << hex
		<< entry->deviceInfo.GetCat() << ","
		<< entry->deviceInfo.GetSubcat() << ","
		<< entry->deviceInfo.GetFirmware() << ") ";
		oss << setw(4) << version <<  setw(0) << " " <<  entry->deviceInfo.descriptionString();
 	}

	oss << "\n";
	
	if(printALDB){
		for(auto e :entry->deviceALDB){
			
		//	Bit 6: 1 = Controller (Master) of Device ID, 0 = Responder to (Slave of) Device ID
			bool isRESP = (e.flag & 0x40) == 0x00;
			
			DeviceID aldbDev = DeviceID(e.devID);
			DeviceInfo info = DeviceInfo(e.info);
			
			string plmName = aldbDev.nameString()	;
			
			oss << setfill('0') << setw(4) << hex << e.address;
			oss << (isRESP?" ":"[") << setfill('0') << setw(2) << hex << e.group  << ( isRESP?" ":"]") << " ";
			oss <<  aldbDev.string() << " " << info.string() << " " << plmName << "\n";
			}
		oss << "\n";
	}

}




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
 
