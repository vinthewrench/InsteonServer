//
//  LogMgr.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/21/21.
//

#include "LogMgr.hpp"

#include <stdio.h>



LogMgr *LogMgr::sharedInstance = 0;

LogMgr::LogMgr() {
	_logBufferData = false;
	_logFlags = 0;
}


 
void LogMgr::logMessage(logFLag_t level, const char *fmt, ...){

	if((_logFlags & level) == 0) return;
 
	char buff[1024];
	
	va_list args;
	va_start(args, fmt);
	size_t cnt = (size_t)vsprintf(buff, fmt, args);
 
	this->writeToLog((const uint8_t*)buff, cnt);
  
	va_end(args);

}

void LogMgr::dumpTraceData(bool outgoing, const uint8_t* data, size_t len){

	if((_logFlags & LogFlagVerbose) == 0) return;

	char hexDigit[] = "0123456789ABCDEF";

	char 	lineBuf[1024];
	char 	*p = lineBuf;
	
	if(outgoing) {
		p += sprintf(p, "(%02zu) -->  ", len);
	}else {
		p += sprintf(p, "(%02zu) <--  02 ", len +1);
		
		if(len == 0){
			;
		}
 	}
	
	for(size_t i = 0; i<len; i++) {
		*p++ = hexDigit[ data[i] >>4];
		*p++ = hexDigit[ data[i] &0xF];
		*p++ = ' ';
	}
	*p++ = '\n';

	this->writeToLog((const uint8_t*)lineBuf, (size_t) (p-lineBuf));
	
}

void LogMgr::writeToLog(const uint8_t* buf, size_t len){
	
	if(len)
		printf("%.*s",(int)len, buf);
}
