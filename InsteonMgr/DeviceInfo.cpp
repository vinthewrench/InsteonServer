//
//  DeviceInfo.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#include "DeviceInfo.hpp"

DeviceInfoNames *DeviceInfoNames::sharedInstance = 0;

DeviceInfoNames::DeviceInfoNames(){
	_nameDB.clear();
	this->initDeviceInfoNamesFromFile();
}

DeviceInfoNames::~DeviceInfoNames(){
	
}


bool DeviceInfoNames::initDeviceInfoNamesFromFile(const char * path) {
 
	bool statusOk = false;
	
	FILE * fp;

	if( (fp = fopen (path, "r")) == NULL)
		return false;
  
 
	// was the db already filled?
	if(!_nameDB.empty() ){
		_nameDB.clear();
	}

	char buffer[256];
	while(fgets(buffer, sizeof(buffer), fp) != NULL) {
		
		int cat;
		int subcat;
		char sku[32];
		char description[128];
		
		size_t Len = strlen(buffer);
		if (Len == 0 || buffer[Len-1] != '\n') {
			continue;
		}
		
		char *p = buffer;

		int cnt = sscanf(p,"%x\t%x\t%[^\t]\t%[^\n]", &cat, &subcat, sku, description);
		if(cnt <4) continue;
	 
		insteon_info_t  entry;
		
		entry.cat = cat;
		entry.subcat = subcat;
		entry.sku = std::string(sku);
		entry.description = std::string(description);
		
		_nameDB.push_back(entry);
	}

	fclose(fp);
	
	statusOk = !_nameDB.empty();
  
	return statusOk;

}

DeviceInfoNames::insteon_info_t* DeviceInfoNames::findInfo(uint8_t cat, uint8_t subcat)
{
	
	for (auto e = _nameDB.begin(); e != _nameDB.end(); e++) {
		if(e->cat == cat && e->subcat == subcat){
			return  &(*e);
		}
	}
	return NULL;
}


std::string DeviceInfo::descriptionString(){
	
	std::string result;
	
	DeviceInfoNames* nameDB = DeviceInfoNames::shared();
	
	DeviceInfoNames::insteon_info_t* entry = nameDB->findInfo(_cat, _subcat);
	if(entry){
		result = entry->description;
	}
	
	return result;
}

std::string DeviceInfo::skuString(){
	
	std::string result;
	
	DeviceInfoNames* nameDB = DeviceInfoNames::shared();
	DeviceInfoNames::insteon_info_t* entry = nameDB->findInfo(_cat, _subcat);
	if(entry){
		result = entry->sku;
	}
	
	return result;
}

const char*  DeviceInfo::description_cstr() {
	
	DeviceInfoNames* nameDB = DeviceInfoNames::shared();
	DeviceInfoNames::insteon_info_t* entry = nameDB->findInfo(_cat, _subcat);
	if(entry){
		return entry->description.c_str();
	}

	return "";
}


DeviceInfo::DeviceInfo( std::string str){
 
	int cat;
	int subcat;
	int firmware;
	int version;
	int count = std::sscanf( str.c_str(), "(%x,%x,%x,%x", &cat, &subcat, &firmware, &version);
	
	if(count > 0) _cat = cat;
	if(count > 1) _subcat = subcat;
	if(count > 2) _firmware = firmware;
	if(count > 3) _version = version;
}
