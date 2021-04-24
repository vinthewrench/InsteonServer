//
//  InsteonPLM.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/13/21.
//

#ifndef InsteonPLM_hpp
#define InsteonPLM_hpp
#include <unistd.h>

#include "DeviceID.hpp"
#include "DeviceInfo.hpp"

#include "PLMStream.hpp"
#include "InsteonParser.hpp"
 
class InsteonPLM  {
 
friend class InsteonMgr;
	
 public:
	
	static const uint64_t  defaultCmdTimeout	 = 1000;  //mSec

	InsteonPLM();
	~InsteonPLM();
	
	bool isConnected();

	bool begin(PLMStream *stream, uint64_t timeout = defaultCmdTimeout);
	
	void stop();
 
	bool getConfiguration(u_int8_t *config);
	bool setConfiguration(u_int8_t config);

	bool resetParser();
	bool parseMessage(insteon_msg_t *msg);
	bool parseMessageOut(insteon_msg_t *msg);
	bool parseALDB(insteon_aldb_t *aldb);
	bool parseRemoteALDB(insteon_aldb_t *aldb);

	bool parseLinking(insteon_linking_t *link);
	bool parseCMD(insteon_cmd_t *cmd);
 
	bool setLED(bool ledState);
	
	bool startLinking(uint8_t link_code, uint8_t groupID);
	bool cancelLinking();

	bool sendMsg(const deviceID_t target,
					 const uint8_t flags,
					 const uint8_t cmd[2],
					 const  uint8_t *data );

	bool doCmd(uint8_t command,
				  const uint8_t* paramBuf,
				  size_t paramBufLen);

	bool resetPLM();
	
	bool getFirstDB();
	bool getNextDB();

	plm_result_t recvResponse(uint64_t timeout);

	// debug
	void dumpBuf();

// fix this!!
	InsteonParser _parser;


private:

	PLMStream* 	_stream;
	uint64_t		_cmdTimeout;
};



#endif /* InsteonPLM_hpp */
