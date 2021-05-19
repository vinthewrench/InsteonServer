//
//  DeviceID.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#ifndef DeviceID_hpp
#define DeviceID_hpp

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include "Utils.hpp"

#include "InsteonMgrDefs.hpp"
#include "InsteonParser.hpp"

#define name_cstr() nameString().c_str()

class GroupID{

public:
	
	GroupID() { _rawGroupID = 0;};

	GroupID(groupID_t gid) { _rawGroupID = gid;};

	GroupID( const char* str) {
		str_to_GroupID(str, &_rawGroupID);
	};

	GroupID( std::string str) {
		str_to_GroupID(str.c_str(), &_rawGroupID);
	};

	
	inline void copyToGroupID_t(groupID_t * toGroup ){
		if(toGroup) *toGroup = _rawGroupID;
	}

	std::string string() const {
		return  to_hex<unsigned short>(_rawGroupID,false);
	}

	bool isValid() {
		return ( _rawGroupID != 0);
	}

	bool isEqual(GroupID a) {
		return a._rawGroupID  == _rawGroupID ;
	}
	
	bool isEqual(groupID_t rawGroupID) {
 		return rawGroupID  == _rawGroupID ;
	}
	
	inline bool operator==(const GroupID& right) const {
		return right._rawGroupID  == _rawGroupID;
		}
	
	inline bool operator!=(const GroupID& right) const {
		return right._rawGroupID  != _rawGroupID;
	}
	
	inline void operator = (const GroupID &right ) {
		_rawGroupID = right._rawGroupID;
 	}
 
private:
	groupID_t _rawGroupID;
};


class DeviceID;

class DeviceNames {
public:
	
	static DeviceNames *shared() {
		if (!sharedInstance)
			sharedInstance = new DeviceNames;
		return sharedInstance;
	}
	
	bool initDeviceNamesFromFile(const char * path = "deviceNames.txt");	// path to database file
	
private:
	
	friend class DeviceID;
	
	static DeviceNames *sharedInstance;
	
	DeviceNames();
	~DeviceNames();
	
	std::map<DeviceID, std::string> _nameMap;
};

class DeviceID{
	
public:
	
	DeviceID() {
		_rawDevID[0] = 0;
		_rawDevID[1] = 0;
		_rawDevID[2] = 0;
	}
	
	DeviceID( const deviceID_t _devIn) {
		copyDevID(_devIn, _rawDevID);
	};

	DeviceID( deviceID_t _devIn) {
		copyDevID(_devIn, _rawDevID);
	};
	
	DeviceID( const DeviceID &_devIn){
		_rawDevID[2] = _devIn._rawDevID[2];
		_rawDevID[1] = _devIn._rawDevID[1];
		_rawDevID[0] = _devIn._rawDevID[0];
 	}
	
	DeviceID( const char* str) {
		str_to_deviceID(str, _rawDevID);
	};

	DeviceID( std::string str) {
		str_to_deviceID(str.c_str(), _rawDevID);
	};

	void clear() {
		_rawDevID[0] = 0;
		_rawDevID[1] = 0;
		_rawDevID[2] = 0;
	};
	
	inline void copyToDeviceID_t(deviceID_t toDev ){
		copyDevID(_rawDevID, toDev);
	}
	
	bool isNULL() {
		return ( _rawDevID[0] == 0
				  && _rawDevID[1] == 0
				  && _rawDevID[2] == 0
				  );
	}
	
	bool isEqual(DeviceID a) {
		return a._rawDevID[0] == _rawDevID[0]
		&& a._rawDevID[1] == _rawDevID[1]
		&& a._rawDevID[2] == _rawDevID[2];
	}
	
	bool isEqual(deviceID_t rawDevID) {
		return rawDevID[0] == _rawDevID[0]
		&& rawDevID[1] == _rawDevID[1]
		&& rawDevID[2] == _rawDevID[2];
	}
	
	inline bool operator==(const DeviceID& right) const {
		return right._rawDevID[0] == _rawDevID[0]
		&& right._rawDevID[1] == _rawDevID[1]
		&& right._rawDevID[2] == _rawDevID[2];
	}
	
	inline bool operator!=(const DeviceID& right) const {
		return  !(right._rawDevID[0] == _rawDevID[0]
					 && right._rawDevID[1] == _rawDevID[1]
					 && right._rawDevID[2] == _rawDevID[2]);
	}
	
	inline void operator = (const DeviceID &right ) {
		_rawDevID[0] = right._rawDevID[0];
		_rawDevID[1] = right._rawDevID[1];
		_rawDevID[2] = right._rawDevID[2];
	}
 
	constexpr bool operator < (const DeviceID& right) const
	{
		
		uint32_t a = ((uint32_t) _rawDevID[2] << 16) | ((uint32_t)_rawDevID[1] << 8)  | ((uint32_t)_rawDevID[0]);
		uint32_t b = ((uint32_t)right._rawDevID[2] << 16) | ((uint32_t)right._rawDevID[1] << 8)  | ((uint32_t)right._rawDevID[0]);
		
		return( b < a);
	}
	
 	std::string string() const {
		static const char* const lut = "0123456789ABCDEF";
		std::string output;
		
		output += lut[_rawDevID[2] >> 4];
		output += lut[_rawDevID[2] & 0xf];
		output += '.';
		output += lut[_rawDevID[1] >> 4];
		output += lut[_rawDevID[1] & 0xf];
		output += '.';
		output += lut[_rawDevID[0] >> 4];
		output += lut[_rawDevID[0] & 0xf];
		return output;
	}
	
	std::string  nameString() const;
	
	//	const char* name_cstr();
	
private:
	deviceID_t _rawDevID;
	
};

#endif /* DeviceID_hpp */
