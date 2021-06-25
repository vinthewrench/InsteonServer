//
//  InsteonALDB.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/26/21.
//

#include "InsteonALDB.hpp"

#include "InsteonCmdQueue.hpp"
#include "InsteonParser.hpp"
#include "InsteonPLM.hpp"
#include "DeviceID.hpp"
#include "LogMgr.hpp"
#include "sleep.h"



InsteonALDB::InsteonALDB(InsteonCmdQueue*  cmdQueue){
	
	_cmdQueue 		= cmdQueue;
	_plm 				= _cmdQueue->_plm;
	_isReadingPLM 	= false;
	_timeout_ALDB_RESPONSE = 4;	// seconds
	_timeout_CMD_READ_ALDB	= 8; // seconds
	
	_entryCnt = 0;
	_aldbQueue.clear();
	_writeALDBQueue.clear();
	_isAccesingRemoteALDB = false;
	_plmDB.clear();
}

InsteonALDB::~InsteonALDB(){
//	_plmDB.clear();
//	_aldbQueue.clear();

}

// MARK: - local ALDB (in PLM)

bool InsteonALDB::readPLM(aldbCallback_t callback)
{
	if(_isReadingPLM)
		return false;
	
	if(!_plm->isConnected())
		return false;

	_plmDB.clear();

	_isReadingPLM = true;
	_readALDBcallback = callback;
	
	continueReadPLM();
 
	return true;
}

void InsteonALDB::continueReadPLM() {

	// send out command to get intial value.
	// a NAK isnt a fail, it just means the DB could be empty.

	uint8_t command = _plmDB.empty()
						? InsteonParser::IM_ALDB_GETFIRST
						: InsteonParser::IM_ALDB_GETNEXT;
 
	LOG_DEBUG("\tALDB COMMAND: (%02X)\n", command);
	_cmdQueue->queueCommand(command,
								  NULL, 0, [this]( auto reply, bool didSucceed) {
		if(!didSucceed) {
			LOG_DEBUG("\tALDB_COMMAND: NAK\n") ;

			_isReadingPLM = false;
			_readALDBcallback(_plmDB);
			_readALDBcallback = NULL;
 		}
		else {
			// set the timer waiting for aldb response
			gettimeofday(&_startTime, NULL);
		}
	});
 }


bool InsteonALDB::create(DeviceID deviceID, bool isCNTL, uint8_t group,
								 DeviceInfo info,
								 boolCallback_t callback){
	
	if(_isReadingPLM)
		return false;
	
	if(!_plm->isConnected())
		return false;

	uint8_t cmdArgs[9] = {0};
 
	if(isCNTL){
		cmdArgs[0] = 0x40;
		cmdArgs[1] = 0x40;
	}else {
		cmdArgs[0] = 0x41;
		cmdArgs[1] = 0x00;
	}
	
	cmdArgs[2] = group;
	
	deviceID_t devID;
	deviceID.copyToDeviceID_t(devID);
	
	cmdArgs[3]  = devID[2];
	cmdArgs[4]  = devID[1];
	cmdArgs[5]  = devID[0];
	
	cmdArgs[6]  = info.GetCat();
	cmdArgs[7]  =  info.GetSubcat();
	cmdArgs[8]  =  info.GetFirmware();
 
	_cmdQueue->queueCommand(InsteonParser::IM_ALDB_MANAGE,
									cmdArgs, sizeof(cmdArgs), [this, callback]( auto reply, bool didSucceed) {
		
	 if(callback)
		 callback(didSucceed);
	});

	return true;
}

bool InsteonALDB::remove(DeviceID deviceID, bool isCNTL, uint8_t group,
								 boolCallback_t callback){
	if(_isReadingPLM)
		return false;
	
	if(!_plm->isConnected())
		return false;

	uint8_t cmdArgs[9] = {0};
 
	cmdArgs[0] = 0x80;
	
	if(isCNTL){
		cmdArgs[1] = 0x40;
	}else {
		cmdArgs[1] = 0x00;
	}

	cmdArgs[2] = group;

	deviceID_t devID;
	deviceID.copyToDeviceID_t(devID);
	
	cmdArgs[3]  = devID[2];
	cmdArgs[4]  = devID[1];
	cmdArgs[5]  = devID[0];
 
	_cmdQueue->queueCommand(InsteonParser::IM_ALDB_MANAGE,
									cmdArgs, sizeof(cmdArgs), [this, callback]( auto reply, bool didSucceed) {
		
	 if(callback)
		 callback(didSucceed);
	});

	return true;
}

// MARK: - remote ALDB (on device)


bool InsteonALDB::readDeviceALDB(DeviceID deviceID, readALDBCallback_t callback){
	
	// 	START_VERBOSE;
	
	if(!_cmdQueue->isConnected())
		return false;
	
	if(_isReadingPLM)
		return false;
	
	_isAccesingRemoteALDB = true;
	
	// create a queue entry
	uint8_t replyID = _entryCnt++;
	
	readALDBreply_t reply;
	reply.id = replyID;
	reply.deviceID = deviceID;
	reply.ack = false;
	reply.db.clear();
	reply.callback = callback;
	
	// queue it
	_aldbQueue.push_back(reply);
	
	uint8_t buffer[] = { 0x00, 0x00, 0x00, 0x00, 0x00};
	_cmdQueue->queueMessage(deviceID,
									InsteonParser::CMD_READ_ALDB, 0x00,
									buffer, sizeof(buffer),
									[this, replyID]( auto arg, bool didSucceed) {
		
		auto rep = findReplyWithID(replyID);
		
		if(rep){
			if(didSucceed){
				rep->ack = true;
				gettimeofday(&rep->time, NULL);		// mark time of ack
 
			} else {
				// did we fail?
	
				// save a copy for Callback
				auto result = rep->db;
				auto callback = rep->callback;
				
				// remove entry from queue
				_aldbQueue.remove_if ([&rep](const  readALDBreply_t& aldbReply){
					return  aldbReply.id == rep->id;
				});
				
				// now do callback safely
				callback(result, false);
			}
		}
	});
	return true;
}


bool InsteonALDB::syncDeviceALDB(DeviceID deviceID, std::vector<insteon_aldb_t> aldbIn,
											aldbWriteCallback_t callback) {

	if(!_cmdQueue->isConnected())
		return false;
	
	if(_isReadingPLM)
		return false;

	LOG_INFO("\tSYNC %s\n",
			deviceID.string().c_str());
 
	// create a queue entry
	uint8_t entryID = _entryCnt++;

	uint16_t addr = 0xfff;

	writeALDBEntry_t entry = {0};
	entry.id = entryID;
	entry.deviceID = deviceID;
	entry.callback = callback;
	for(auto e :aldbIn){
		uint8_t flag = e.flag;
		if((flag & 0x80) ==  0) continue;
		
		writeALDBData_t data;
		data.ack = false;
		data.buffer = _plm->_parser.makeALDBWriteRecord(e, addr);
		entry.aldbData.push_back(data);
		addr-=8;
	}
	
	// create final with high water mark
	{
		insteon_aldb_t aldb = {0};
		writeALDBData_t data;
		data.ack = false;
		data.buffer = _plm->_parser.makeALDBWriteRecord(aldb,addr );
		entry.aldbData.push_back(data);
	}
	
	// queue it
	_writeALDBQueue.push_back(entry);
	
	// possibly send it ..
 	processNextWrite();

	return true;
}

bool InsteonALDB::addToDeviceALDB(DeviceID targetDevice,
											 bool  isCNTL,
											 uint8_t groupID,
											 u_int8_t* data,
											 std::function<void(const insteon_aldb_t* newAldb,
																	  bool didSucceed)> callback){
												 
#define CHK_FAIL if(!didSucceed){ callback(NULL, false); return; }

	if(!_cmdQueue->isConnected())
		return false;
	
	if(_isReadingPLM)
		return false;

	 uint8_t info[3] = {0};
	 if(data){
		 info[0] = data[0];
		 info[1] = data[1];
 		 info[2] = data[2];
	 }
 
	// first we need to get the PLM address.
	// it's not really a lot of overhead.
	_cmdQueue->queueCommand(InsteonParser::IM_INFO,
									NULL, 0, [=]( auto reply, bool didSucceed) {
		CHK_FAIL;
			
 		readDeviceALDB(targetDevice, [=](std::vector<insteon_aldb_t> aldb,  bool didSucceed) {
			CHK_FAIL;
			
			DeviceID plmDevice =  DeviceID(reply.info.devID);
			uint16_t		lastAddress = 0xfff;
			
			for(auto e :aldb){
				lastAddress = e.address;
	
				// is it already there?
				if( cmpDevID(e.devID, reply.info.devID)
					&& e.group == groupID)
				{
					if((e.flag & 0x40) == (isCNTL?0x40:0))  // is it a controller
					{
						callback(NULL, true);
						return ;
					}
				}
			}

			// create an aldb record
			// create the new ALDB record to append
			insteon_aldb_t newAldb = {0};
			copyDevID(reply.info.devID, newAldb.devID);
			newAldb.group = groupID;
			newAldb.flag = isCNTL?0xC2:0xAA; // = Record is in use, Controller/rsponder of Device ID,
													  // Record has been used before
			memcpy(newAldb.info, info, 3);
 
			newAldb.address = lastAddress-8;
			
 			auto buffer = _plm->_parser.makeALDBWriteRecord(newAldb, newAldb.address);
	
			printf("INSERT %04x %s group:%02x\n",  lastAddress-8, plmDevice.string().c_str(), groupID);
	
			START_VERBOSE;
			_cmdQueue->queueMessage(targetDevice,
											InsteonParser::CMD_READ_ALDB, 0x00,
											buffer.data(), buffer.size(),
											[=]( auto arg, bool didSucceed) {
				CHK_FAIL;
	  			callback(&newAldb,
							didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK );
			});
		});
		
	});
	return true;
												 
#undef CHK_FAIL
}

bool InsteonALDB::removeFromDeviceALDB(DeviceID deviceID, uint8_t groupID, boolCallback_t callback){
	
	
	if(callback)
		(callback)(false);
	
	return false;
}


// MARK: - private API remote ALDB


InsteonALDB::writeALDBData_t*  InsteonALDB::getNextAvailWriteBuffer(writeALDBEntry_t* entry){
	
	for (auto e = entry->aldbData.begin(); e != entry->aldbData.end(); e++) {
 		if(!e->ack) {
			return &(*e);
		}
	}

		return NULL;
}
	

InsteonALDB::writeALDBEntry_t*  InsteonALDB::getNextAvailWriteEntry(){
	std::lock_guard<std::mutex> lock(_mutex);
	
	for (auto e = _writeALDBQueue.begin(); e != _writeALDBQueue.end(); e++) {
		
		writeALDBEntry_t* entry = &(*e);
		if(getNextAvailWriteBuffer(entry) != NULL)
 				return entry;
		}
 
	return NULL;
}



void InsteonALDB::processNextWrite(){
 
	if(auto entry = getNextAvailWriteEntry(); entry != NULL) {
		if(auto aldbData = getNextAvailWriteBuffer(entry); aldbData != NULL) {
	 
 #if 1
			uint8_t* buffer = aldbData->buffer.data();
			size_t buflen = aldbData->buffer.size() ;
	
			_cmdQueue->queueMessage(entry->deviceID,
											InsteonParser::CMD_READ_ALDB, 0x00,
											buffer, buflen,
											[this, entry, aldbData ]( auto arg, bool didSucceed) {
				
				aldbData->ack = true;
				
				// are there any more aldb entries?
				if(getNextAvailWriteBuffer(entry) == NULL){
					
					aldbWriteCallback_t  callback = entry->callback;
					
					// remove entry from queue
					_writeALDBQueue.remove_if ([&entry](const  writeALDBEntry_t& aldbEntry){
						return  aldbEntry.id == entry->id;
					});
					
					if(callback)
						callback(true);
				}
				else {
					// give the PLM a short break before sending the next one.
					sleep_ms(500);

				}
				processNextWrite();
			});
#else
			deviceID_t devID;
			entry->deviceID.copyToDeviceID_t(devID);
			
			printf("02 62 %02X %02X %02X 1F %02X %02X ",
					 devID[2], devID[1], devID[0],
					 InsteonParser::CMD_READ_ALDB, 0x00);
			
			u_int8_t sum = 0;
			sum  += InsteonParser::CMD_READ_ALDB;
			
			
			for(auto val : aldbData->buffer) {
				sum +=val;
				printf("%02X ", val);
			}
			
			sum = (~sum) + 1 ;
			printf(" - %02X \n", sum);
			
			aldbData->ack = true;
			
			
			// call during completion
			{
				// are there any more aldb entries?
				if(getNextAvailWriteBuffer(entry) == NULL){
					
					aldbWriteCallback_t  callback = entry->callback;
					
					// remove entry from queue
					_writeALDBQueue.remove_if ([&entry](const  writeALDBEntry_t& aldbEntry){
						return  aldbEntry.id == entry->id;
					});
					
					callback(true);
					
				}
				
				// do the next one
				processNextWrite();
			}

#endif
				
			
		}
	}
 }

// MARK: - private API - local ALDB

InsteonALDB::readALDBreply_t* InsteonALDB::findReplyWithDeviceID(DeviceID deviceID){
	
	for (auto e = _aldbQueue.begin(); e != _aldbQueue.end(); e++) {
		if(e->deviceID == deviceID)
			return &(*e);
	}
	return NULL;
}

InsteonALDB::readALDBreply_t* InsteonALDB::findReplyWithID(uint8_t id){
	
	for (auto e = _aldbQueue.begin(); e != _aldbQueue.end(); e++) {
		if(e->id == id)
			return &(*e);
	}
	return NULL;
}


std::list<uint8_t> InsteonALDB::getExpiredIDs(){
	
	std::lock_guard<std::mutex> lock(_mutex);
	std::list<uint8_t> expiredIDs;
	
	timeval now;
	gettimeofday(&now, NULL);

	for(auto e :_aldbQueue) {
		if(e.ack) {
			
			timeval diff;
			timersub(&now, &e.time, &diff);
	 
			if(_timeout_CMD_READ_ALDB
				&& diff.tv_sec > _timeout_CMD_READ_ALDB) {
				expiredIDs.push_back(e.id);
			}
		}
	}
	
	return expiredIDs;
}



// MARK: - process PLM response


bool InsteonALDB::processPLMresponse(plm_result_t response){
	
	bool didHandle = false;
	
	if(_isAccesingRemoteALDB) {
		
		if(response == PLR_NOTHING){
			
			timeval now, diff;
			gettimeofday(&now, NULL);
			timersub(&now, &_startTime, &diff);
			
			// complete all values that timed out.
			if(auto expiredID = getExpiredIDs(); expiredID.size() > 0) {
				for(uint8_t replyID : expiredID) {
					
					// these CMD_READ_ALDB timed out
					auto rep = findReplyWithID(replyID);
					if(rep){
						
						// save a copy for Callback
						auto result = rep->db;
						auto callback = rep->callback;
						
						// remove entry from queue
						_aldbQueue.remove_if ([&rep](const  readALDBreply_t& aldbReply){
							return  aldbReply.id == rep->id;
						});
						
						// now do callback safely
						if(callback)
							callback(result, false);
					}
				}
			}
			
			if(_aldbQueue.size() == 0){
				_isAccesingRemoteALDB = false;
			}
		}
		else if(response == PLR_MSG){
			
			insteon_msg_t msg;
			
			if(_plm->parseMessage(&msg)
				&& msg.ext
				&& msg.cmd[0] == InsteonParser::CMD_READ_ALDB){
				
				DeviceID deviceID = DeviceID(msg.from);
 
				insteon_aldb_t aldb;
				if(_plm->parseRemoteALDB(&aldb)) {
					
					auto reply = findReplyWithDeviceID(deviceID);
					if(reply){
						
						// check bit 1 of flag to see if we are at high-water mark.
						if((aldb.flag & 0x02) == 0) {
							
							// save a copy for Callback
							auto result = reply->db;
							auto callback = reply->callback;
							
							// remove entry from queue
							_aldbQueue.remove_if ([&reply](const  readALDBreply_t& aldbReply){
								return  aldbReply.id == reply->id;
							});
							
							// now callback safely
							callback(result, true);
						}
						else {
							reply->db.push_back(aldb);				// store result
							gettimeofday(&reply->time, NULL);		// reset timer
						}
					}
					
					_plm->_parser.reset();
					
					if(_aldbQueue.size() == 0){
						_isAccesingRemoteALDB = false;
					}
					
					didHandle = true;
				}
			}
		}
	}
	else if(_isReadingPLM){
		
		if(response == PLR_NOTHING){
			
			timeval now, diff;
			gettimeofday(&now, NULL);
			timersub(&now, &_startTime, &diff);
			
			if(_timeout_ALDB_RESPONSE > 0
				&& diff.tv_sec >=  _timeout_ALDB_RESPONSE  ) {
				
				// Timeout occured
				LOG_DEBUG("\tREQ ID TIMEOUT for PLR_ALDB_RESPONSE \n") ;
				didHandle = true;
				
			}
		}
		else if(response == PLR_ALDB_RESPONSE){
			insteon_aldb_t aldb;
			if(_plm->parseALDB(&aldb)){
				
				_plmDB.push_back(aldb);
				
				DeviceID deviceID = DeviceID(aldb.devID);
				
				LOG_DEBUG("\tALDB_RESPONSE <%s>\n",
							deviceID.string().c_str() ) ;
			}
			_plm->_parser.reset();
			this->continueReadPLM();
			didHandle = true;
		}
	}
	
	return didHandle;
}

