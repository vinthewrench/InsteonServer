//
//  InsteonValidator.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/11/21.
//

#ifndef InsteonValidator_hpp
#define InsteonValidator_hpp

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
//#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <mutex>
#include <list>

#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "InsteonPLM.hpp"

/* @brief InsteonValidator is a helper class to the cmdQueue
* it is used to check if a device is actually there
*
*/

class InsteonCmdQueue;
 
class InsteonValidator {
 
public:
 
	
	typedef struct  {
		uint8_t		status;
		DeviceID		deviceID;
		
		DeviceInfo 	deviceInfo;
		
		bool			validated;		// was validated (deviceInfo is valid)
		bool			hasVersion;		// received Insteon version
	
		bool			sendACK;			// we got a send ACK from PLM.
		bool			timeOut;			// we timed out
		timeval		ackTime;			// time we sent it.. or last time we NAK

	}validation_result_t;
 

	typedef std::function<void(std::vector<validation_result_t> )> validateCallback_t;
 
	InsteonValidator(InsteonCmdQueue* cmdQueue);
  ~InsteonValidator();

	bool startValidation(std::vector<DeviceID> deviceList, validateCallback_t callback);
	bool cancelValidation();

	bool processPLMresponse(plm_result_t response);

private:
 
	validation_result_t*  findDBEntryWithDeviceID(DeviceID deviceID);

	mutable std::mutex	 _mutex;

	InsteonCmdQueue*		_cmdQueue;
	InsteonPLM*	 		_plm;
	bool					_isValidating;
	

	std::list<validation_result_t> _val;		// devices to validate
	std::vector<validation_result_t> _results;	// devices we have validated

  	validateCallback_t _callback;
	timeval				_startTime;				// used for timeouts (link Timeout)
	uint64_t  			_timeout_VALIDATE;		// how long to wait for Linking
};

#endif /* InsteonValidator_hpp */
