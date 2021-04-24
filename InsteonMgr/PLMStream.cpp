//
//  PLMStream.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/13/21.
//

#include "PLMStream.hpp"
#include "InsteonPLM.hpp"
#include "LogMgr.hpp"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>                                                  // Serial Port IO Controls
#include <sys/time.h>
#include <limits.h>
#include <sys/uio.h>
 #include <unistd.h>


PLMStream::PLMStream(){
	
}

PLMStream::~PLMStream(){
	
}

bool PLMStream::begin(const char * path){
	int error = 0;
	
	return begin(path, error);
}

bool PLMStream::begin(const char * path,  int & errorOut){
	
	struct termios tty;
 	bzero (&tty,  sizeof tty);

	_isSetup = false;

	_fd = open( path, O_RDWR| O_NOCTTY  | O_NONBLOCK | O_NDELAY );
	
	if(_fd == -1){
		errorOut = errno;
		return false;
	}
	
	/* Error Handling */
	if ( tcgetattr ( _fd, &tty ) != 0 ) {
		return false;
 	}
	
	/* Save old tty parameters */
	_savetty = tty;

	/* Set Baud Rate */
	cfsetospeed (&tty, (speed_t)B19200); //	Set write  Speed as 19200
	cfsetispeed (&tty, (speed_t)B19200); //  Set Read  Speed as 19200

 	/* Setting other Port Stuff */
	tty.c_cflag     &=  ~(tcflag_t)PARENB;            // Make 8n1
	tty.c_cflag     &=  ~(tcflag_t)CSTOPB;
	tty.c_cflag     &=  ~(tcflag_t)CSIZE;
	tty.c_cflag     |=  CS8;
	
	tty.c_cflag     &=  ~(tcflag_t)CRTSCTS;           // no flow control
	tty.c_cc[VMIN]   =  1;                  // read doesn't block
	tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
	tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

 	/* Make raw */
	cfmakeraw(&tty);
	
	/* Flush Port, then applies attributes */
	tcflush( _fd, TCIFLUSH );
	
	if ( tcsetattr ( _fd, TCSANOW, &tty ) != 0) {
		return false;
 	}

//	// flush anything else left
//	while(true){
//		u_int8_t c;
//		size_t n =  (size_t)::read( _fd, &c, 1 );
//		if(n == 1){
//			printf("%02X ", c);
//		}
//		if(n == 0) break;
// 		if(n == SIZE_MAX) break;
//	}

	_hasChar = false;
	_isSetup = true;
	
	return true;
}

bool PLMStream::isOpen(){
 	return _isSetup;
	
};
	



void PLMStream::stop(){

	if(_isSetup){
		tcsetattr ( _fd, TCSANOW, &_savetty );
		close(_fd);
 	}
	
	_isSetup = false;

}

ssize_t PLMStream::write(const uint8_t* buf, size_t len){
	
	if(!_isSetup) return EBADF;
 
	TRACE_DATA_OUT( buf,len);
	
	return ::write(_fd, buf, len);
 }


int PLMStream::available(){
	
	if(!_isSetup) return 0;
 
	if(_hasChar) return 1;

 	u_int8_t c;
  	size_t n =  (size_t)::read( _fd, &c, 1 );

	if(n == -1){
		int lastError = errno;
		
		// no chars waiting
		if(lastError == EAGAIN)
			return 0;
		
		// device is broken
 		if(lastError == ENXIO ) {
			_isSetup = false;
			return -1;
		}
		
	}else if(n == 1) {
		_hasChar = true;
		_peek_c = c;
		return 1;
  	}
 	
	return 0;
}


 int PLMStream::read(){

	 if(available()){
		 _hasChar = false;
		 return _peek_c;
	 }
	 
	return -1;
}


void PLMStream::printf(const char *fmt, ...){
	
	if(!_isSetup) return;
	
	char buff[1024];
	
	va_list args;
	va_start(args, fmt);
	size_t cnt = (size_t)vsprintf(buff, fmt, args);
	
	this->write((const uint8_t*)buff,cnt);

	//	self->flush();
	
	va_end(args);
}

#define CALL_RETRY(retvar, expression) do { \
	 retvar = (expression); \
} while (retvar == -1 && errno == EINTR);

int PLMStream::sleep(struct timeval timeout){
	
	fd_set set;
 
	/* Initialize the file descriptor set. */
	FD_ZERO (&set);
	FD_SET (_fd, &set);
 
	/* select returns 0 if timeout, 1 if input available, -1 if error. */
	 int retVal = 0;
  
	 CALL_RETRY (retVal, select (FD_SETSIZE,
												  &set, NULL, NULL,
												  &timeout));
	 return retVal;

}
