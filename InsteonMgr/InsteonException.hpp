//
//  InsteonException.h
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/24/21.
//

#ifndef InsteonException_h
#define InsteonException_h

#include <stdexcept>

class InsteonException: virtual public std::runtime_error {
	 
protected:

	 int error_number;               ///< Error Number
	 
public:

	 /** Constructor (C++ STL string, int, int).
	  *  @param msg The error message
	  *  @param err_num Error number
	  */
	 explicit
	InsteonException(const std::string& msg, int err_num = 0):
		  std::runtime_error(msg)
		  {
				error_number = err_num;
		  }

	
	 /** Destructor.
	  *  Virtual to allow for subclassing.
	  */
	 virtual ~InsteonException() throw () {}
	 
	 /** Returns error number.
	  *  @return #error_number
	  */
	 virtual int getErrorNumber() const throw() {
		  return error_number;
	 }
};
 


#endif /* InsteonException_h */
