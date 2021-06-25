//
//  DeviceInfo.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#ifndef DeviceInfo_hpp
#define DeviceInfo_hpp

#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

class DeviceInfoNames {

	static DeviceInfoNames *shared() {
			if (!sharedInstance)
				sharedInstance = new DeviceInfoNames;
			return sharedInstance;
		}

	bool initDeviceInfoNamesFromFile(const char * path = "catagories.txt");	// path to database file

private:
	typedef struct  {
		uint8_t		cat;
		uint8_t		subcat;
		std::string	sku;
		std::string	description;
	}insteon_info_t;

	friend class DeviceInfo;
 
	static DeviceInfoNames *sharedInstance;

	DeviceInfoNames();
	~DeviceInfoNames();

	insteon_info_t* findInfo(uint8_t cat, uint8_t subcat);
	std::vector<insteon_info_t> _nameDB;
 };


class DeviceInfo{
	
public:
	DeviceInfo() {
		_cat 		= 0;
		_subcat 	= 0;
		_firmware	= 0;
		_version = 0;
	}
 
	DeviceInfo(uint8_t cat, uint8_t subcat, uint8_t firm = 0, uint8_t vers = 0) {
		_cat 		= cat;
		_subcat 	= subcat;
		_firmware	= firm;
		_version  = vers;
	}
	
	DeviceInfo(uint8_t info[3]) {
		_cat 		= info[0];
		_subcat 	= info[1];
		_firmware	= info[2];
		_version = 0;
	}
	
	DeviceInfo( std::string str);

	bool isKeyPad();
	bool isDimmer();
	 
	inline bool isNULL() {
		return ( _cat == 0  && _subcat == 0 );
	}
	
	bool isEqual(DeviceInfo a) {
		return a._cat == _cat && a._subcat == _subcat;
	}
	
	inline bool operator==(const DeviceInfo& right) const {
		return right._cat == _cat && right._subcat == _subcat;
	}
	
	inline bool operator!=(const DeviceInfo& right) const {
		return !(right._cat == _cat && right._subcat == _subcat);
	}
	
	inline void operator = (const DeviceInfo &right ) {
		_cat 		= right._cat;
		_subcat 	= right._subcat;
		_version 	= right._version;
		_firmware 	= right._firmware;
	}
	
	std::string const string() const {
		static const char* const lut = "0123456789ABCDEF";
		std::string output;
			
		output += '(';
		output += lut[_cat >> 4];
		output += lut[_cat & 0xf];
		output += ',';
		output += lut[_subcat >> 4];
		output += lut[_subcat & 0xf];
		
		if(_firmware > 0){
			output += ',';
			output += lut[_firmware >> 4];
			output += lut[_firmware & 0xf];
			
			if(_version > 0){
				output += ',';
				output += lut[_version >> 4];
				output += lut[_version & 0xf];
		}

	}
		output += ')';
		return output;
	}

	std::string descriptionString();
	std::string skuString();
	
	const char* description_cstr();
 
	uint8_t	GetCat() const {return _cat;};
	uint8_t	GetSubcat() const {return _subcat;};
	uint8_t	GetFirmware() const {return _firmware;};
	
	uint8_t	GetVersion() const {return _version;};
	void 		SetVersion(uint8_t version) { _version = version;};
	
private:
	uint8_t		_cat;
	uint8_t		_subcat;
	uint8_t		_firmware;
	uint8_t		_version;
	
};

#endif /* DeviceInfo_hpp */
