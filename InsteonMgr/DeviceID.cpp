//
//  DeviceID.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#include "DeviceID.hpp"
#include <map>
#include <string>
#include <iterator>
 

// MARK: - device database

DeviceNames *DeviceNames::sharedInstance = 0;

DeviceNames::DeviceNames(){
	_nameMap.clear();
	this->initDeviceNamesFromFile();
}

DeviceNames::~DeviceNames(){
	
}


bool DeviceNames::initDeviceNamesFromFile(const char * path) {
	
	bool statusOk = false;
	
	FILE * fp;
	
	if( (fp = fopen (path, "r")) == NULL)
		return false;
	
	// was the db already filled?
	if(!_nameMap.empty() ){
		_nameMap.clear();
	}
	
	char buffer[256];
	while(fgets(buffer, sizeof(buffer), fp) != NULL) {
		
		deviceID_t	devID;
		char 			name[128];
		
		size_t Len = strlen(buffer);
		if (Len == 0 || buffer[Len-1] != '\n') {
			continue;
		}
		
		char *p = buffer;
		
		int cnt = sscanf(p,"%hhx.%hhx.%hhx\t%[^\n]", &devID[2],&devID[1],&devID[0], name);
		if(cnt !=4) continue;
	 
 		DeviceID deviceID = DeviceID(devID);
		_nameMap.insert(std::make_pair(deviceID, std::string(name) ));
	
	}
	
	fclose(fp);
	
	statusOk = !_nameMap.empty();
	
	return statusOk;
	
}

std::string DeviceID::nameString() const {
		
	DeviceNames* nameDB = DeviceNames::shared();
	return nameDB->_nameMap[_rawDevID];
}
