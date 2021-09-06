//
//  InsteonCmdQueue.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/2/21.
//

#include <algorithm>
#include "InsteonCmdQueue.hpp"
#include "InsteonDB.hpp"
#include "sleep.h"

#include "LogMgr.hpp"

InsteonCmdQueue *InsteonCmdQueue::sharedInstance = NULL;


InsteonCmdQueue::InsteonCmdQueue(InsteonPLM* plm, InsteonDB* db){
	_plm = plm;
	_db = db;
	_maxSendRetry = 4;
	_timeout_Send = 12; 		// 8 // seconds
	_cmdQueue.clear();
	_entryCnt = 0;
	
	sharedInstance = this;
}

InsteonCmdQueue::~InsteonCmdQueue(){
	
}
// MARK: -

InsteonCmdQueue::cmd_entry_t* InsteonCmdQueue::getEntryWithHash(uint32_t hash){
	
	std::lock_guard<std::mutex> lock(_mutex);

	for (auto e = _cmdQueue.begin(); e != _cmdQueue.end(); e++) {
		if(e->typ	 == CMD_ENTRY_MSG && e->hash == hash)
			return  &(*e);
	}
	return NULL;
}

InsteonCmdQueue::cmd_entry_t* InsteonCmdQueue::getNextAvailEntry(){
	std::lock_guard<std::mutex> lock(_mutex);

	for (auto e = _cmdQueue.begin(); e != _cmdQueue.end(); e++) {
		
		// if there is one in progress ahead of us, punt sending.
		if((e->sendCount > 0)  && (e->sendACK	 == false))
			return NULL;
 
		// else find the next available one.
		if(e->sendCount == 0)
			return  &(*e);
		
	}
	return NULL;
}


InsteonCmdQueue::cmd_entry_t* InsteonCmdQueue::getEntryWithCommand(uint8_t command){
	std::lock_guard<std::mutex> lock(_mutex);

	for (auto e = _cmdQueue.begin(); e != _cmdQueue.end(); e++) {
		if(e->typ	 == CMD_ENTRY_CMD && e->command == command)
			return  &(*e);
	}
	return NULL;
}

InsteonCmdQueue::cmd_entry_t* InsteonCmdQueue::getEntryWithDevice(DeviceID deviceID ){
	std::lock_guard<std::mutex> lock(_mutex);

	for (auto e = _cmdQueue.begin(); e != _cmdQueue.end(); e++) {
		if( (e->typ == CMD_ENTRY_MSG || e->typ == CMD_ENTRY_MSG_GROUP)
			&& deviceID.isEqual(e->send.to) )
			return  &(*e);
	}
	return NULL;
}

std::list<uint32_t> InsteonCmdQueue::getExpiredHashs(){

	std::lock_guard<std::mutex> lock(_mutex);
	std::list<uint32_t> expired_hashs;
	timeval now;
	gettimeofday(&now, NULL);

//	for(auto e : _cmdQueue) {
	for (auto e = _cmdQueue.begin(); e != _cmdQueue.end(); e++) {

		if(e->typ	!= CMD_ENTRY_MSG) continue;
		
		// count only those we have attempted to send
		if(e->sendCount == 0) continue;
		
		timeval diff;
		timersub(&now, &e->time, &diff);
		
		if(_timeout_Send
			&& diff.tv_sec > _timeout_Send) {
			expired_hashs.push_back(e->hash);
		}
	}
	
	return expired_hashs;
}


void InsteonCmdQueue::processTimeout() {
 
	// complete all values that timed out.
	if(auto expired_hashs = getExpiredHashs(); expired_hashs.size() > 0) {
		for(uint32_t hash : expired_hashs) {
			cmd_entry_t* entry = getEntryWithHash(hash);
			
			switch (entry->typ) {
				case CMD_ENTRY_MSG:
				{
					DeviceID deviceID = DeviceID(entry->send.to);
					LOG_DEBUG("\tTIMEOUT-MSG %s [%02x, %02x]\n",
								deviceID.string().c_str(),
								entry->send.cmd[0], entry->send.cmd[0] );
		
				}
					break;
				case CMD_ENTRY_CMD:
				{
	 				LOG_DEBUG("\tTIMEOUT-CMD (%02X) \n",
								entry->command );
 				}
					break;
					
				default:
					break;
			}
 
			performCompletion(entry, false);
		}
	}
}

/*
 The IM buffers IM Commands as it receives them, so you can send a complete IM Command without pause. To maintain compatibility with earlier IM versions, the IM will echo each byte that it receives (earlier versions of the IM used byte echoing for flow control). You can now ignore the byte echos, but in order to avoid overrunning the IM’s receive buffer, you must wait for the IM to send its response to your current IM Command before sending a new one.
*/

bool InsteonCmdQueue::processNext(bool withDelay){
	bool status = false;
	
	if(withDelay){
		// give the PLM a short break before sending the next one.
		sleep_ms(500);
	}

 	if(auto cmdEntry = getNextAvailEntry(); cmdEntry != NULL) {
		status = processEntry(cmdEntry);
	}
	
	return status;
}


bool InsteonCmdQueue::processEntry( cmd_entry_t* entry) {
	
	bool status = false;
	
	switch (entry->typ) {
		case CMD_ENTRY_MSG_GROUP: {
			
			status = _plm->sendMsg(entry->send.to,
										  entry->send.flag,
										  entry->send.cmd,
										  entry->send.data);

			LOG_DEBUG("\tSEND-MSG_GROUP (%d) %s %02X [%02x, %02x]\n",
						 entry->sendCount,
						 status?"":"FAIL ",
						 entry->send.to[0],
						 entry->send.cmd[0], entry->send.cmd[1] );
	
			if(status) {
				entry->sendCount++;
				gettimeofday(&entry->time, NULL);
			}

		} break;

		case CMD_ENTRY_MSG: {
			DeviceID deviceID = DeviceID(entry->send.to);
			
			LOG_DEBUG("\tSEND-MSG (%d) %s [%02x, %02x]\n",
						 entry->sendCount,
						 deviceID.string().c_str(),
						 entry->send.cmd[0], entry->send.cmd[1] );
			
			status = _plm->sendMsg(entry->send.to,
										  entry->send.flag,
										  entry->send.cmd,
										  entry->send.data);
			
			if(!status){
				LOG_DEBUG("\t  FAILED \n");
			}
			
			if(status) {
				entry->sendCount++;
				gettimeofday(&entry->time, NULL);
			}
		}
			break;

		case CMD_ENTRY_CMD: {
		
			status = _plm->doCmd(entry->command,
										entry->params,
										entry->paramLen); ;
	
			LOG_DEBUG("\tDO-CMD (%02X)\n", entry->command);
		  
			if(status) {
				entry->sendCount++;
				gettimeofday(&entry->time, NULL);
			}
		}
		break;

		default:
			throw(std::invalid_argument("InsteonCmdQueue::processEntry - entry.typ"));
			break;
	}
 
	return status;
}

//MARK:- completions

void InsteonCmdQueue::performCompletion(cmd_entry_t* entry,
													  bool didSucceed,
													  void* reply){
	if(entry){
		switch (entry->typ) {
			case CMD_ENTRY_MSG_GROUP:
			{
				msgReply_t arg;
				bzero(&arg, sizeof(arg));
				arg.send = entry->send;
	
				if(entry->msgCallback){
					entry->msgCallback(arg, didSucceed);
				}

			}
				break;
			case CMD_ENTRY_MSG:
			{
				msgReply_t arg;
				bzero(&arg, sizeof(arg));
				
				arg.send = entry->send;
				if(didSucceed && reply) {
					memcpy(&arg.reply, reply, sizeof(arg.reply));
				}
				
				if(entry->msgCallback){
					entry->msgCallback(arg, didSucceed);
				}
			}
				break;
			case CMD_ENTRY_CMD:
			{
				cmdReply_t	arg;
	  			bzero(&arg, sizeof(arg));
		
				arg.cmd = entry->command;

				if(didSucceed && reply) {
	 				insteon_cmd_t* rc = (insteon_cmd_t*)reply;
					arg.ack = rc->ack;
		
					if(rc->cmd == InsteonParser::IM_INFO){
 						if(rc->paramLen < 6)
							throw std::runtime_error("performCompletion wrong length for InsteonParser::IM_INFO");
						
						uint8_t *p =  rc->params;
						arg.info.devID[2] = *p++;
						arg.info.devID[1] = *p++;
						arg.info.devID[0] = *p++;
						arg.info.cat 	= *p++;
						arg.info.subcat	= *p++;
						arg.info.firmware	= *p++;
					}
					else {
						insteon_cmd_t* rc = (insteon_cmd_t*)reply;
						arg.ack = rc->ack;
						arg.raw.paramLen = rc->paramLen;
						memcpy(&arg.raw.params, rc->params, rc->paramLen);
					}
				}
				else
				{
					arg.cmd = entry->command;
				}
				
				if(entry->cmdCallback){
					entry->cmdCallback(arg, didSucceed);
				}
			}
				break;
				
			default:
				throw(std::invalid_argument("InsteonCmdQueue::performCompletion - entry.typ"));
				
		}
		
		// remove entry from queue
		_cmdQueue.remove_if ([&entry](const  cmd_entry_t& cmdEntry){
			return  cmdEntry.id == entry->id;
		});

		// send the next one?
		processNext(true);
	}
}

// MARK: - public API

void InsteonCmdQueue:: abort(){
 	_cmdQueue.clear();
	
}


void InsteonCmdQueue::queueCommand(uint8_t command,
												const uint8_t* paramBuf,
												size_t paramBufLen,
												cmdCallback_t callback ) {
	cmd_entry_t entry;
	bzero(&entry, sizeof(entry));

	if (paramBufLen >  size(entry.params))
		throw(std::invalid_argument("InsteonCmdQueue::queueCommand - paramBufLen"));
  	
	entry.typ = CMD_ENTRY_CMD;
	entry.id = _entryCnt++;		// every entry id unique
	entry.command = command;
	
	if(paramBuf) {
		memcpy(entry.params, paramBuf, paramBufLen);
		entry.paramLen = paramBufLen;
	}
	entry.sendCount = 0;
 
	entry.cmdCallback = callback;
	LOG_DEBUG("\tQUEUE-CMD (%02X)\n", entry.command);
 	
	// queue it
	_cmdQueue.push_back(entry);
	
	// possibly send it ..
	processNext(false);
	
}

void InsteonCmdQueue::queueMessageToGroup( uint8_t command,
														uint8_t arg,
														uint8_t group,
														msgCallback_t callback ){
	cmd_entry_t entry;
	bzero(&entry, sizeof(entry));
	
	entry.typ = CMD_ENTRY_MSG_GROUP;
	entry.id = _entryCnt++;		// every entry id unique
	entry.send.to[0] = group;
	entry.send.cmd[0] = command;
	entry.send.cmd[1] = arg;
	entry.send.flag = 0xCF;  // Group message, max hops
	entry.send.msgType =  InsteonParser::msgTypForFlag(entry.send.flag);
 
	entry.hash = makeMsgHash(&entry.send);
	entry.sendCount = 0;
	
 	entry.msgCallback = callback;
 
	LOG_DEBUG("\tQUEUE-MSG_GROUP %02X  [%02x, %02x]\n",
	 			group,
				command,arg );
	
	// queue it
	_cmdQueue.push_back(entry);
	
	// possibly send it ..
	processNext(false);
};


void InsteonCmdQueue::queueMessage(DeviceID deviceID,
											  uint8_t command,
											  uint8_t arg,
											  uint8_t* data,
											  size_t 	dataLen,
											  msgCallback_t callback ) {
	cmd_entry_t entry;
	
	bzero(&entry, sizeof(entry));
	
	entry.typ = CMD_ENTRY_MSG;
	entry.id = _entryCnt++;		// every entry id unique
	deviceID.copyToDeviceID_t(entry.send.to);
	entry.send.cmd[0] = command;
	entry.send.cmd[1] = arg;
	entry.send.flag = 0x0F;  // direct message, max hops
	entry.send.msgType =  InsteonParser::msgTypForFlag(entry.send.flag);
 
	if(data && dataLen > 0){
		if(dataLen > 14)
			throw(std::invalid_argument("InsteonCmdQueue::queueMessage - dataLen > 14"));

		entry.send.flag |= 0x10;
		memcpy(&entry.send.data, data, dataLen);
		makeMsgChecksum(&entry.send);
	}
	 
	entry.hash = makeMsgHash(&entry.send);
	entry.sendCount = 0;
	
	entry.msgCallback = callback;
	
	LOG_DEBUG("\tQUEUE-MSG %s [%02x, %02x]\n",
				deviceID.string().c_str(),
				command,arg );
	
	// queue it
	_cmdQueue.push_back(entry);
	
	// possibly send it ..
	processNext(false);
}

bool InsteonCmdQueue::processPLMresponse(plm_result_t response) {
	
	bool didHandle = false;
	
	if(response == PLR_NOTHING){
		processTimeout();
		didHandle = false;   // allow others to do timeout procssing too
	}
	else if(response == PLR_MSG_OUT){
		
		insteon_msg_t msgOut;
		
		if(_plm->parseMessageOut(&msgOut)){
			
			DeviceID deviceID = DeviceID(msgOut.to);
			uint32_t hash = makeMsgHash(&msgOut);
			cmd_entry_t* cmdEntry = getEntryWithHash(hash);
			
			if (!cmdEntry)
				cmdEntry = getEntryWithDevice(deviceID);
			
			if(cmdEntry){
				
				if(cmdEntry->typ == CMD_ENTRY_MSG_GROUP){
					LOG_DEBUG("\t%-8s GROUP(%02X)\n",
								msgOut.ack == InsteonParser::ACK?"SEND-ACK":"SEND-NAK",
								 msgOut.to[0]);
				}
				else {
					LOG_DEBUG("\t%-8s %s \n",
								msgOut.ack == InsteonParser::ACK?"SEND-ACK":"SEND-NAK",
								deviceID.string().c_str());
				}
				
				// if it was ACK -  We succeded
				if(msgOut.ack == InsteonParser::ACK){
					cmdEntry->sendACK = true;
					
					// if it was a group message - we dont expect a reply
					if(cmdEntry->typ == CMD_ENTRY_MSG_GROUP){
						performCompletion(cmdEntry, true);
 					}
				}
				else { // we need to retry
					if(cmdEntry->sendCount < _maxSendRetry) {
						// retry message send
						sleep_ms(500); // too soon. let PLM stablize
						
						bool didResend = processEntry(cmdEntry);
						if(!didResend)
							performCompletion(cmdEntry, false);
					}
					else  // just punt
					{
						LOG_DEBUG("\tSEND FAIL %s \n",
									deviceID.string().c_str() );
						
						performCompletion(cmdEntry, false);
					}
				}
			}
		}
		_plm->_parser.reset();
		didHandle = true;
	}
	else if(response == PLR_MSG){
		
		insteon_msg_t reply;
		
		if(_plm->parseMessage(&reply)){
			
			if(reply.msgType == MSG_TYP_DIRECT_ACK
				||reply.msgType == MSG_TYP_DIRECT_NAK) {
				
				DeviceID deviceID = DeviceID(reply.from);
				
				cmd_entry_t* cmdEntry = getEntryWithDevice(deviceID);
				
				if(cmdEntry && (cmdEntry->sendCount > 0)){
					
					LOG_DEBUG("\tRECV-%s %s \n",
								reply.msgType == MSG_TYP_DIRECT_ACK?"ACK":"NAK",
								deviceID.string().c_str() );
					
					performCompletion(cmdEntry, true, &reply);
					_plm->_parser.reset();
					didHandle = true;
					
				}
			}
		}
	}
	else if(response == PLR_CMD || response == PLR_INFO){
		
		insteon_cmd_t cmdIn;
		
		if(_plm->parseCMD(&cmdIn)){
			
			cmd_entry_t* cmdEntry = getEntryWithCommand(cmdIn.cmd);
			
			if(cmdEntry && (cmdEntry->sendCount > 0)){
				
				LOG_DEBUG("\tCMD-%s (%02X)\n",
							 cmdIn.ack == InsteonParser::ACK?"ACK":"NAK",
							 cmdIn.cmd);
				
				cmdEntry->sendACK = true;
				
				performCompletion(cmdEntry,
										cmdIn.ack == InsteonParser::ACK,
										&cmdIn);
				
				_plm->_parser.reset();
				didHandle = true;
			}
		}
	}
 
	return didHandle;
}
