//
//  TimeStamp.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 5/6/21.
//

#include "TimeStamp.hpp"
#include <time.h>

using namespace std;
 
namespace timestamp {
	static const char *kDateFormat = "%a, %d %b %Y %T GMT";

	TimeStamp::TimeStamp() {
		_time = time(NULL);
	}

	TimeStamp::TimeStamp(const string str){
		struct tm tm;
		if(::strptime( str.c_str(), kDateFormat, &tm)){
			_time = ::timegm(&tm);
 		}
 	}

	std::string TimeStamp::RFC1123String(){
		enum { RFC1123_GMT_LEN = 29, RFC1123_GMT_SIZE };
		char timeStr[RFC1123_GMT_SIZE] = {0};
	 	::strftime(timeStr, sizeof(timeStr), kDateFormat,  gmtime(&_time));
		return string(timeStr);
	}

}
 
