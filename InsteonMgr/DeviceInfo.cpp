//
//  DeviceInfo.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#include "DeviceInfo.hpp"
#include <utility>
#include <vector>
#include <set>

using namespace std;


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



bool DeviceInfo::isPLM(){
	
	return (_cat == 0x03);
}


bool DeviceInfo::isKeyPad(){
	 
	set< pair<uint8_t,uint8_t>> devices =
	{
		{0x01, 0x1B},	// KeypadLinc Dimmer[2486DWH6]
		{0x01, 0x41},	// KeypadLinc Dimmer[2334-222]
		{0x01, 0x42},	// KeypadLinc Dimmer[2334-232]
		{0x01, 0x1C},	// KeypadLinc Dimmer[2486DWH8]
		{0x02, 0x0F},	// KeypadLinc On/Off[2486SWH6]
		{0x02, 0x1E},	// KeypadLinc On/Off (Dual-Band)[2487S]
		{0x02, 0x2C},	// KeypadLinc On/Off (Dual-Band, 50/60 Hz)[2487S]
	};

	return (devices.find(make_pair(_cat, _subcat)) != devices.end());
}
 
bool DeviceInfo::isDimmer(){

	set< pair<uint8_t,uint8_t>> devices =
	{
		{0x01,0x01}, // 2476D	SwitchLinc Dimmer
		{0x01,0x02}, // 2475D	In-LineLinc Dimmer
		{0x01,0x03}, // 2876DB	ICON Dimmer Switch
		{0x01,0x04}, // 2476DH	SwitchLinc Dimmer (High Wattage)
		{0x01,0x05}, // 2484DWH8	Keypad Countdown Timer w/ Dimmer
		{0x01,0x06}, // 2456D2	LampLinc Dimmer (2-Pin)
		{0x01,0x08}, // 2476DT	SwitchLinc Dimmer Count-down Timer
		{0x01,0x09}, // 2486D	KeypadLinc Dimmer
		{0x01,0x0B}, // 2632-422	Insteon Dimmer Module, France (869 MHz)
		{0x01,0x0C}, // 2486DWH8	KeypadLinc Dimmer
		{0x01,0x0E}, // 2457D2	LampLinc (Dual-Band)
		{0x01,0x0F}, // 2632-432	Insteon Dimmer Module, Germany (869 MHz)
		{0x01,0x11}, // 2632-442	Insteon Dimmer Module, UK (869 MHz)
		{0x01,0x12}, // 2632-522	Insteon Dimmer Module, Aus/NZ (921 MHz)
		{0x01,0x13}, // 2676D-B	ICON SwitchLinc Dimmer Lixar/Bell Canada
		{0x01,0x17}, // 2466D	ToggleLinc Dimmer
		{0x01,0x18}, // 2474D	Icon SwitchLinc Dimmer Inline Companion
		{0x01,0x19}, // 2476D	SwitchLinc Dimmer [with beeper]
		{0x01,0x1A}, // 2475D	In-LineLinc Dimmer [with beeper]
		{0x01,0x1B}, // 2486DWH6	KeypadLinc Dimmer
		{0x01,0x1C}, // 2486DWH8	KeypadLinc Dimmer
		{0x01,0x1D}, // 2476DH	SwitchLinc Dimmer (High Wattage)[beeper]
		{0x01,0x1E}, // 2876DB	ICON Switch Dimmer
		{0x01,0x1F}, // 2466Dx	ToggleLinc Dimmer [with beeper]
		{0x01,0x20}, // 2477D	SwitchLinc Dimmer (Dual-Band)
		{0x01,0x21}, // 2472D	OutletLinc Dimmer (Dual-Band)
		{0x01,0x24}, // 2474DWH	SwitchLinc 2-Wire Dimmer (RF)
		{0x01,0x25}, // 2475DA2	In-LineLinc 0-10VDC Dimmer/Dual-SwitchDB
		{0x01,0x2D}, // 2477DH	SwitchLinc-Dimmer Dual-Band 1000W
		{0x01,0x2F}, // 2484DST6	KeypadLinc Schedule Timer with Dimmer
		{0x01,0x30}, // 2476D	SwitchLinc Dimmer
		{0x01,0x31}, // 2478D	SwitchLinc Dimmer 240V-50/60Hz Dual-Band
		{0x01,0x32}, // 2475DA1	In-LineLinc Dimmer (Dual Band)
		{0x01,0x34}, // 2452-222	Insteon DIN Rail Dimmer (915 MHz)
		{0x01,0x35}, // 2442-222	Insteon Micro Dimmer (915 MHz)
		{0x01,0x36}, // 2452-422	Insteon DIN Rail Dimmer (869 MHz)
		{0x01,0x37}, // 2452-522	Insteon DIN Rail Dimmer (921 MHz)
		{0x01,0x38}, // 2442-422	Insteon Micro Dimmer (869 MHz)
		{0x01,0x39}, // 2442-522	Insteon Micro Dimmer (921 MHz)
		{0x01,0x3D}, // 2446-422	Insteon Ballast Dimmer (869 MHz)
		{0x01,0x3E}, // 2446-522	Insteon Ballast Dimmer (921 MHz)
		{0x01,0x3F}, // 2447-422	Insteon Fixture Dimmer (869 MHz)
		{0x01,0x40}, // 2447-522	Insteon Fixture Dimmer (921 MHz)
		{0x01,0x41}, // 2334-222	Keypad Dimmer Dual-Band, 8 Button
		{0x01,0x42}, // 2334-232	Keypad Dimmer Dual-Band, 6 Button
		{0x01,0x50}, // 2632-452	Insteon Dimmer Module, Chile (915 MHz)
	};

 	return (devices.find(make_pair(_cat, _subcat)) != devices.end());
}
