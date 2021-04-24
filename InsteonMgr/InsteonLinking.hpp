//
//  InsteonLinking.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#ifndef InsteonLinking_hpp
#define InsteonLinking_hpp

 #include <stdlib.h>
 #include <stdint.h>
 #include <stdbool.h>
//#include <string.h>
//#include <time.h>
//#include <sys/time.h>
 #include <unistd.h>
 
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "InsteonPLM.hpp"

/* @brief InsteonLinking is a helper class to the cmdQueue
* it is used to handle the start and cancel link process.
* also it respondes to  the IM_LINKING_COMPLETED  and CMD_ASSIGN_TO_GROUP
*
*/


class InsteonCmdQueue;
 
class InsteonLinking {
 
public:
	
	typedef enum {
		LINK_INVALID = 0,
		LINK_SUCCESS,
		LINK_TIMEOUT,
		LINK_CANCELED,
		LINK_FAILED,
	}link_status_t;

	typedef struct  {
		uint8_t		status;
		uint8_t		linkCode;
		uint8_t		groupID;
		DeviceID		deviceID;
		DeviceInfo 	deviceInfo;
	}link_result_t;
 
	typedef std::function<void(link_result_t)> linkCallback_t;
	
	InsteonLinking(InsteonCmdQueue* cmdQueue);
  ~InsteonLinking();

	bool startLinking(uint8_t link_code, uint8_t groupID, linkCallback_t callback);
	
	bool linkDevice(DeviceID deviceID,
						 bool isCTRL,
						 uint8_t groupID,
						 linkCallback_t callback);

	bool cancelLinking();

	bool processPLMresponse(plm_result_t response);

private:
 
	InsteonCmdQueue*		_cmdQueue;
	InsteonPLM*	 		_plm;
	bool					_isLinking;
	
	linkCallback_t 		_callback;
	timeval				_startTime;				// used for timeouts (link Timeout)
	uint64_t  			_timeout_LINKING;		// how long to wait for Linking
 };

#endif /* InsteonLinking_hpp */
