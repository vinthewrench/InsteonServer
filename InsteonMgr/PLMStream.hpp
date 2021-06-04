//
//  PLMStream.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/13/21.
//

#ifndef PLMStream_hpp
#define PLMStream_hpp

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <time.h>
#include <termios.h>

#include <stdexcept>

class PLMStreamException: virtual public std::runtime_error {
	 
protected:

	 int error_number;               ///< Error Number
	 
public:

	 /** Constructor (C++ STL string, int, int).
	  *  @param msg The error message
	  *  @param err_num Error number
	  */
	 explicit
	PLMStreamException(const std::string& msg, int err_num = 0):
		  std::runtime_error(msg)
		  {
				error_number = err_num;
		  }
	
	 /** Destructor.
	  *  Virtual to allow for subclassing.
	  */
	 virtual ~PLMStreamException() throw () {}
	 
	 /** Returns error number.
	  *  @return #error_number
	  */
	 virtual int getErrorNumber() const throw() {
		  return error_number;
	 }
};
 

class PLMStream  {

	friend class InsteonMgr;
	friend class InsteonPLM;

public:
	PLMStream();
	~PLMStream();

	bool begin(const char * path);
	bool begin(const char * path, int &error);
	void stop();
	
	int sleep(struct timeval timeout);
 
	bool isOpen();

	 ssize_t write(const uint8_t*, size_t);
 
	 int available();  // 0, 1 or error
 	 int read();
 
	void printf(const char *fmt, ...);
 
private:
	
	int _fd;
	struct termios _savetty;
	bool 				_isSetup;

	bool 				_hasChar;
	u_int8_t 			_peek_c;

};

#endif /* PLMStream_hpp */
