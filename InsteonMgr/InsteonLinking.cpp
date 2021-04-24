//
//  InsteonLinking.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/29/21.
//

#include "InsteonLinking.hpp"

#include "InsteonCmdQueue.hpp"
#include "InsteonParser.hpp"
#include "InsteonPLM.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"

#include "LogMgr.hpp"

InsteonLinking::InsteonLinking(InsteonCmdQueue* cmdQueue){
	_cmdQueue 		= cmdQueue;
	_plm 				= _cmdQueue->_plm;
	_isLinking 		= false;
	_timeout_LINKING = 4 * 60;	// seconds
}

InsteonLinking::~InsteonLinking(){
	
}


bool InsteonLinking::startLinking(uint8_t link_code, uint8_t groupID,
											 linkCallback_t callback){
	
	bool statusOK = false;
	
	if(_isLinking)
		return false;
	
	if(!_plm->isConnected())
		return false;
	
	_callback = callback;
	_isLinking = true;

//	LOG_DEBUG("\tLINKING START (%02x %02x)\n", link_code, groupID);

	uint8_t cmdArgs[]  = {link_code, groupID };
 	_cmdQueue->queueCommand(InsteonParser::IM_START_LINKING,
								  cmdArgs, sizeof(cmdArgs),
								  [this]( auto reply, bool didSucceed) {
		// start linking
		
		if(didSucceed){
			// set the timer waiting for a PLR_LINKING_COMPLETED
			gettimeofday(&_startTime, NULL);
		}
		else
		{
			_isLinking = false;

			link_result_t link_result ={0};
	 		link_result.status = LINK_FAILED;
			
			if(_callback)
				_callback(link_result);
		}
	
	});
	
	
	return statusOK;
}

bool InsteonLinking::cancelLinking(){
	bool statusOK = false;
	
	if(_isLinking){
		_isLinking = false;
		
		//Cancel any linking in progress?
		_cmdQueue->queueCommand(InsteonParser::IM_CANCEL_LINKING,
										NULL, 0, [this]( auto reply, bool didSucceed) {
			
			link_result_t link_result ={0};
			link_result.status = LINK_CANCELED;
	
			if(_callback)
				_callback(link_result);
		});
		
	}
	return statusOK;
}


bool InsteonLinking::linkDevice(DeviceID deviceID,
										  bool isCTRL,
										  uint8_t groupID,
										  linkCallback_t callback){
	
	bool statusOK = false;

	if(_isLinking)
		return false;
	
	if(!_plm->isConnected())
		return false;
	
	_callback = callback;
	_isLinking = true;
	
	//get into linking mode.  as controller
 	uint8_t cmdArgs[]  = {0, groupID };
	cmdArgs[0] = isCTRL? 0x01:0x00;

	_cmdQueue->queueCommand(InsteonParser::IM_START_LINKING,
								  cmdArgs, sizeof(cmdArgs),
								  [=]( auto reply, bool didSucceed) {

		if(!didSucceed){
			
			_isLinking = false;

			link_result_t link_result ={0};
			link_result.status = LINK_FAILED;

			if(_callback)
				_callback(link_result);
			return;
 		}
		
		// Issue a Remote Enter Link Mode command to the target device .
		// make Cmd 2  Always enter group 0x01 linking?
		// needs to be an extended command - data is all zeros.
			uint8_t buffer[] = {
				0x00,0x00,0x00,0x00,};

			_cmdQueue->queueMessage(deviceID,
											InsteonParser::CMD_ENTER_LINK_MODE, groupID,
											buffer, sizeof(buffer),
											[=]( auto arg, bool didSucceed) {
				
				
				if(didSucceed){
					// set the timer waiting for a PLR_LINKING_COMPLETED
					gettimeofday(&_startTime, NULL);
				}
				else
				{
					_isLinking = false;

					link_result_t link_result ={0};
					link_result.status = LINK_FAILED;
					
					if(_callback)
						_callback(link_result);
				}

			});

	});
	
	
	return statusOK;
}

bool InsteonLinking::processPLMresponse(plm_result_t response){
	
	bool didHandle = false;
	
	if(_isLinking){
		if(response == PLR_NOTHING){
			
			timeval now, diff;
			
			gettimeofday(&now, NULL);
			timersub(&now, &_startTime, &diff);
			
			if(_timeout_LINKING > 0
				&& diff.tv_sec >=  _timeout_LINKING  ) {
				
				// Timeout occured
//				LOG_DEBUG("\t LINKING TIMEOUT\n") ;
				_isLinking = false;
				didHandle = true;
				
				//Cancel any linking in progress?
				_cmdQueue->queueCommand(InsteonParser::IM_CANCEL_LINKING,
												NULL, 0, [this]( auto reply, bool didSucceed) {
					
					link_result_t link_result ={0};
					link_result.status = LINK_TIMEOUT;
			
					if(_callback)
						_callback(link_result);
				});
			}
		}
		
		else if( response == PLR_LINKING_COMPLETED){
			
			insteon_linking_t data;
			bool parseOK = _plm->parseLinking(&data);
			
			// we do this here because the callback might delete the class;
			_plm->_parser.reset();
			_isLinking = false;
			didHandle = true;
			
			if(parseOK){
				
				link_result_t link_result;
				auto deviceID = DeviceID(data.dev_id);
				link_result.deviceID = deviceID;
				link_result.deviceInfo = DeviceInfo(data.info);
				link_result.linkCode = data.flag;
				link_result.groupID = data.group;;
				link_result.status = LINK_SUCCESS;

				LOG_DEBUG("\tLINKING COMPLETE (%02x %02x) %s \"%s\"\n",
							 data.flag, data.group,
							 deviceID.string().c_str(),
							 deviceID.name_cstr());

				if(_callback)
					_callback(link_result);
			}
		}
		
		// A device responding to our IM_START_LINKING will reply with a direct message
		// of type command [CMD_ASSIGN_TO_GROUP and cmd[1] matching our group]
		// -- ignore it
		
		else if(response == PLR_MSG){
			
			insteon_msg_t msg;
			
			if(_plm->parseMessage(&msg)){
				if( msg.msgType == MSG_TYP_DIRECT_ACK
					&& msg.cmd[0] == InsteonParser::CMD_ASSIGN_TO_GROUP) {
					
					// ignore this as noise - we get nothing from this.
					
					_plm->_parser.reset();
					didHandle = true;
				}
			}
		}
	}
	
	return didHandle;
}

