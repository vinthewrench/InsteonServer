//
//  InsteonPLM.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/13/21.
//

#include "InsteonPLM.hpp"
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdexcept>      // std::invalid_argument

#include <sys/errno.h>
#include "LogMgr.hpp"

#define CHK_STATUS if(!status) goto done;

#define DEBUG_STREAM 0
 
InsteonPLM::InsteonPLM(){
		
}

InsteonPLM::~InsteonPLM(){
	
}

bool InsteonPLM::begin(PLMStream *stream,
							  uint64_t timeout){
 	_cmdTimeout = timeout;
	_stream = stream;
 	return true;
}

bool InsteonPLM::isConnected(){
	
	return _stream->isOpen();
}

void InsteonPLM::stop(){
	
}

void InsteonPLM::dumpBuf(){
	_parser.dumpBuf();
}

// MARK: - local PLM commands

bool InsteonPLM::resetPLM(){
	
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
 
	static const uint8_t packet[]
			= {InsteonParser::STX,
				InsteonParser::IM_RESET };
	
	_stream->write( packet, sizeof(packet));
 
	while(true) {
		plm_result_t result = recvResponse(10000);   // this could take a while
		if(result == PLR_CMD){
			statusOk = true;
		}
		break;
	}
 
	if(statusOk){
		insteon_cmd_t cmd;
		statusOk =  _parser.get_cmd(&cmd)
		&& cmd.cmd == InsteonParser::IM_RESET
		&& cmd.ack == InsteonParser::ACK;
	}
 
	return statusOk;
}


bool InsteonPLM::setLED(bool ledState){
 
	bool statusOk = false;
	
	if(!_stream->isOpen())
		return false;

	uint8_t packet[]
				= {InsteonParser::STX, 00};
 	packet[1] =  ledState ?InsteonParser::IM_LED_ON:InsteonParser::IM_LED_OFF;

	_stream->write( packet, sizeof(packet));

	while(true) {
		plm_result_t result = recvResponse(_cmdTimeout);
		if(result == PLR_CMD){
			statusOk = true;
		}
		break;
	}

	if(statusOk){
		insteon_cmd_t cmd;
		
		statusOk =  _parser.get_cmd(&cmd)
		&& cmd.ack == InsteonParser::ACK;
	}

	return statusOk;

}
 
bool InsteonPLM::setConfiguration(u_int8_t config){
	
	bool statusOk = false;
	
	if(!_stream->isOpen())
		return false;

	uint8_t packet[]
				= {InsteonParser::STX,
					InsteonParser::IM_SET_CONFIG ,
					config
				};

	_stream->write( packet, sizeof(packet));

	while(true) {
		plm_result_t result = recvResponse(_cmdTimeout);
		if(result == PLR_CMD){
			statusOk = true;
		}
		break;
	}

	if(statusOk){
		insteon_cmd_t cmd;
		
		statusOk =  _parser.get_cmd(&cmd)
		&& cmd.cmd == InsteonParser::IM_SET_CONFIG
		&& cmd.ack == InsteonParser::ACK;
	}

	return statusOk;
}

bool InsteonPLM::getConfiguration(u_int8_t *config){
	
	bool statusOk = false;
	
	if(!_stream->isOpen())
		return false;

 	static const uint8_t packet[]
				= {InsteonParser::STX,
					InsteonParser::IM_GET_CONFIG };

	_stream->write( packet, sizeof(packet));

	while(true) {
		plm_result_t result = recvResponse(_cmdTimeout);
		if(result == PLR_CMD){
			statusOk = true;
		}
		break;
	}
 
	if(statusOk){
		insteon_cmd_t cmd;
		
		statusOk =  _parser.get_cmd(&cmd)
		&& cmd.cmd == InsteonParser::IM_GET_CONFIG
	 	&& cmd.ack == InsteonParser::ACK;
		
		if(statusOk && config){
			*config = cmd.params[0];
		}
 	}
 
	return statusOk;
}
 
bool InsteonPLM::startLinking(uint8_t link_code, uint8_t groupID){
  
  bool statusOk = false;
  
  if(!_stream->isOpen())
	  return false;

  uint8_t packet[]
			  = {InsteonParser::STX,
				  InsteonParser::IM_START_LINKING ,
				  link_code, groupID
			  };

	_stream->write( packet, sizeof(packet));
	
	statusOk = true;

  return statusOk;
}



bool InsteonPLM::cancelLinking(){
  
  bool statusOk = false;
  
  if(!_stream->isOpen())
	  return false;

  uint8_t packet[]
			  = {InsteonParser::STX,
				  InsteonParser::IM_CANCEL_LINKING
			  };

	_stream->write( packet, sizeof(packet));

  while(true) {
	  plm_result_t result = recvResponse(_cmdTimeout);
	  if(result == PLR_CMD){
		  statusOk = true;
	  }
	  break;
  }

  if(statusOk){
	  insteon_cmd_t cmd;
	  
	  statusOk =  _parser.get_cmd(&cmd)
	  && cmd.cmd == InsteonParser::IM_CANCEL_LINKING
	  && cmd.ack == InsteonParser::ACK;
  }

  return statusOk;
}

// MARK: - commands and messages
 
bool InsteonPLM::doCmd(uint8_t command,
							  const uint8_t* paramBuf,
							  size_t paramBufLen){
	bool statusOk = false;
 
	if(!_stream->isOpen())
		return false;
	
	uint8_t packet[34];
 
 if (paramBufLen >  sizeof(packet) - 2) // need form for STX and CMD
 	throw(std::invalid_argument("InsteonPLM::doCmd - paramBufLen"));
 
	packet[0] = InsteonParser::STX;
	packet[1] = command;
 
	if(paramBufLen) {
		memcpy(&packet[2], paramBuf, paramBufLen);
	}

	size_t len = (size_t)(paramBufLen + 2);
	
	statusOk =  (_stream->write( packet, len) == len);

  return statusOk;
}

bool InsteonPLM::sendMsg(const deviceID_t target,
								const uint8_t flags,
								const uint8_t cmd[2],
								const uint8_t *data ){
  bool statusOk = false;
	
	if(!_stream->isOpen())
		return false;

	uint8_t packet[22] = {0};
	uint8_t *p = packet;
	
	*p++ = InsteonParser::STX;
	*p++ = InsteonParser::MSG_OUT;
	*p++ = target[2];
	*p++ = target[1];
	*p++ = target[0];
	*p++ = flags;
	*p++ = cmd[0];
	*p++ = cmd[1];
	
	if( (flags & 0x10) == 0x10){
		for(int i = 0; i < 14; i++) {
			*p++ = data[i];
		}
	}
	
	size_t len = (size_t)(p - packet);
	
	statusOk =  (_stream->write( packet, len) == len);
	
  return statusOk;
}

// MARK: - local all link data base (ALDB)


bool InsteonPLM::getFirstDB(){
  
  bool statusOk = false;
  
  if(!_stream->isOpen())
	  return false;

  uint8_t packet[]
			  = {InsteonParser::STX,
				  InsteonParser::IM_ALDB_GETFIRST
			  };

	_stream->write( packet, sizeof(packet));

  while(true) {
	  plm_result_t result = recvResponse(_cmdTimeout);
	  if(result == PLR_CMD){
		  statusOk = true;
	  }
	  break;
  }

  if(statusOk){
	  insteon_cmd_t cmd;
	  
	  statusOk =  _parser.get_cmd(&cmd)
	  && cmd.cmd == InsteonParser::IM_ALDB_GETFIRST
	  && cmd.ack == InsteonParser::ACK;
  }

  return statusOk;
}


bool InsteonPLM::getNextDB(){
  
  bool statusOk = false;
  
  if(!_stream->isOpen())
	  return false;

  uint8_t packet[]
			  = {InsteonParser::STX,
				  InsteonParser::IM_ALDB_GETNEXT
			  };

	_stream->write( packet, sizeof(packet));

  while(true) {
	  plm_result_t result = recvResponse(_cmdTimeout);
	  if(result == PLR_CMD){
		  statusOk = true;
	  }
	  break;
  }

  if(statusOk){
	  insteon_cmd_t cmd;
	  
	  statusOk =  _parser.get_cmd(&cmd)
	  && cmd.cmd == InsteonParser::IM_ALDB_GETNEXT
	  && cmd.ack == InsteonParser::ACK;
  }

  return statusOk;
}


//bool InsteonPLM::getLastLink(insteon_linking_t *data) {
//
//	bool statusOk = false;
//
//	if(!_stream->isOpen())
//		return false;
//
//	uint8_t packet[]
//				= {InsteonParser::STX,
//					InsteonParser::IM_GET_LAST_LINK
//				};
//
//	_stream->write( packet, sizeof(packet));
//
//	while(true) {
//		plm_result_t result = recvResponse(_cmdTimeout);
//		if(result == PLR_CMD){
//			statusOk = true;
//		}
//		break;
//	}
//
//	if(statusOk){
//		insteon_cmd_t cmd;
//
//		statusOk =  _parser.get_cmd(&cmd)
//		&& cmd.cmd == InsteonParser::IM_GET_LAST_LINK
//		&& cmd.ack == InsteonParser::ACK;
//	}
//
//	if(statusOk) {
//		while(true) {
//			plm_result_t result = recvResponse(_cmdTimeout);
//			if(result == PLR_ALDB_RESPONSE){
//				statusOk = _parser.get_linking(data);
// 			}
//			break;
//		}
//	}
//
//	return statusOk;
//}
 




// MARK: -

bool InsteonPLM::resetParser(){
		
	if(!_stream->isOpen())
		return false;
	
	_parser.reset();
	
	return true;
	
}


bool InsteonPLM::parseALDB(insteon_aldb_t *aldb){
 
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
  
	if(_parser.getState() != InsteonParser::INS_ALDB_RESPONSE)
		return false;
 
	statusOk = _parser.parse_aldb(aldb);
		
	return statusOk;
}


bool InsteonPLM::parseRemoteALDB(insteon_aldb_t *aldb){
	
	  bool statusOk = false;

	  if(!_stream->isOpen())
		  return false;
	 
	  if(_parser.getState() != InsteonParser::INS_MSGEXT)
		  return false;
	
	statusOk = _parser.parse_aldb_remote(aldb);
		  
	  return statusOk;

}

bool InsteonPLM::parseCMD(insteon_cmd_t *cmd){
 
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
  
	if(!((_parser.getState() == InsteonParser::INS_CMD)
			|| (_parser.getState() == InsteonParser::INS_INFO)))
		return false;
 
	statusOk = _parser.parse_cmd(cmd);
		
	return statusOk;
}

bool InsteonPLM::parseLinking(insteon_linking_t *link){
 
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
  
	if(_parser.getState() != InsteonParser::INS_LINKING)
		return false;
 
	statusOk = _parser.parse_linking(link);
		
	return statusOk;
}

bool InsteonPLM::parseMessage(insteon_msg_t *msg){
 
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
	
	if(auto state = _parser.getState();
		( state == InsteonParser::INS_MSG ||  state == InsteonParser::INS_MSGEXT)) {
		  statusOk = _parser.parse_msg(msg);
 	}
		
	return statusOk;
}


bool InsteonPLM::parseMessageOut(insteon_msg_t *msg){
 
	bool statusOk = false;

	if(!_stream->isOpen())
		return false;
  
	if(_parser.getState() != InsteonParser::INS_MSG_OUT)
		return false;
 
	statusOk = _parser.parse_msg_out(msg);
		
	return statusOk;
}



plm_result_t InsteonPLM::recvResponse(uint64_t uSecs){
	
#if DEBUG_STREAM
	uint8_t buffer[1024] ={0};
	size_t  count = 0;
#endif
	
	plm_result_t result = PLR_NOTHING;
	
	if(!_stream->isOpen()) {
		return PLR_ERROR;
	}
	
	//	/* Initialize the timeout data structure. */
	struct timeval timeout;
	
	timeout.tv_sec = uSecs / 1000;
	timeout.tv_usec = (uSecs % 1000);
	
	// we wait for input for a max of timeout ..
	
	while (_stream->sleep(timeout) > 0 ) {
		
		uint8_t ch = _stream->read();
		result = _parser.process_char(ch);
		
#if DEBUG_STREAM
		buffer[count++] = ch;
		printf( "%02x  = %u\n", ch, result);
#endif
		
		if(result != PLR_CONTINUE)
			goto done;
	}
	
done:
	
	if(result == PLR_CONTINUE)
		return result;
	
	if(result == PLR_INVALID){
		uint8_t sav =  LogMgr::shared()->_logFlags;
		START_VERBOSE;
		LogMgr::shared()->writeToLog((const uint8_t* )"INVALID: ", 9);
		_parser.dumpBuf();
		LogMgr::shared()->_logFlags = sav;
		return result;
	}
	
	if(result != PLR_NOTHING){
		_parser.dumpBuf();
	}
	
	return result;
}

