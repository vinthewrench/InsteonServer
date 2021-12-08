//
//  ScheduleMgr.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/26/21.
//

#include "ScheduleMgr.hpp"

#include "sunset.h"

#include "InsteonMgrDefs.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "Action.hpp"
#include "Event.hpp"


ScheduleMgr *ScheduleMgr::sharedInstance = NULL;

ScheduleMgr::ScheduleMgr(){
	_cachedSolar.previousMidnight = 0;
	_latitude = numeric_limits<double>::max();
	_longitude = numeric_limits<double>::max();
	_startTime = time(NULL);
}

void ScheduleMgr::setLatLong(double latitude, double longitude){
	_cachedSolar.previousMidnight = 0;
	_latitude = latitude;
	_longitude = longitude;

}

bool  ScheduleMgr::calculateSolarEvents(){
	
	if(_latitude == numeric_limits<double>::max()
		|| _longitude == numeric_limits<double>::max())
		return  false;
	
	_cachedSolar.previousMidnight = 0;

	solarTimes_t solar = {0};
	
	time_t now = time(NULL);
	struct tm* tm = localtime(&now);
	
	solar.gmtOffset = tm->tm_gmtoff;
	solar.previousMidnight =  previousMidnight((now + tm->tm_gmtoff));
	
	_sun.setPosition(_latitude, _longitude, (double) (tm->tm_gmtoff / SECS_PER_HOUR));
	_sun.setCurrentDate(tm->tm_year + 1900,  tm->tm_mon + 1, tm->tm_mday);
	solar.sunSetMins 			= _sun.calcSunset();
	solar.sunriseMins 			= _sun.calcSunrise();
	solar.civilSunSetMins 		= _sun.calcCivilSunset();
	solar.civilSunRiseMins 	= _sun.calcCivilSunrise();
	solar.latitude 				= _latitude;
	solar.longitude 			= _longitude;
	solar.timeZoneString		= string(tm->tm_zone);

	_cachedSolar = solar;
	return true;
}

bool ScheduleMgr::getSolarEvents(solarTimes_t& events){

	bool success = true;

	// try and get cached one based on previous midnight.
	
	time_t now = time(NULL);
	struct tm* tm = localtime(&now);
	time_t previousMidnight =  previousMidnight((now + tm->tm_gmtoff));

 	if(previousMidnight !=  _cachedSolar.previousMidnight){
		success = calculateSolarEvents();
	}
	
	_cachedSolar.upTime = now - _startTime;
	events = _cachedSolar;
	
	return success;
}


long ScheduleMgr::upTime(){
	time_t now = time(NULL);
	return ( now - _startTime);
}
