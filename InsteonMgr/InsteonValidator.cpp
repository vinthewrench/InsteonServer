//
//  InsteonValidator.cpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/11/21.
//

#include "InsteonValidator.hpp"
 
#include "InsteonCmdQueue.hpp"
#include "InsteonParser.hpp"
#include "InsteonPLM.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "InsteonDevice.hpp"

#include "LogMgr.hpp"

InsteonValidator::InsteonValidator(InsteonCmdQueue* cmdQueue){
	_cmdQueue 		= cmdQueue;
	_plm 				= _cmdQueue->_plm;
	_isValidating = false;
	_timeout_VALIDATE = 10;	// seconds
	_val.clear();
	_results.clear();

}

InsteonValidator::~InsteonValidator(){
	
}

// MARK: - public API

bool InsteonValidator::startValidation(std::vector<DeviceID> deviceList,
													validateCallback_t callback) {
	
	bool statusOK = false;
	
	if(_isValidating)
		return false;
	
	if(!_plm->isConnected())
		return false;
	
	_callback = callback;
	
	_val.clear();
	_results.clear();
	
	// return now if there is no work.
	if(deviceList.size() == 0){
		if(_callback)
			_callback(_results);
		return true;
	}

	_isValidating = true;

	LOG_DEBUG("\tVALIDATION START\n");
	
	// Add each device to the list
	for (DeviceID deviceID : deviceList) {
		if(!findDBEntryWithDeviceID(deviceID)){
			validation_result_t entry;
			bzero(&entry, sizeof(validation_result_t));
			entry.deviceID = deviceID;
			entry.deviceInfo = DeviceInfo();
			_val.push_back(entry);
		}
	}
	
	// fire off a lookup for each entry
	for(auto e : _val) {
		_cmdQueue->queueMessage(e.deviceID,
										InsteonParser::CMD_ID_REQUEST, 0,
										[this]( auto reply, bool didSucceed) {
			
			DeviceID deviceID = DeviceID(reply.send.to);
			validation_result_t* entry = findDBEntryWithDeviceID(deviceID);
			
			if(!entry)
				throw(std::invalid_argument("InsteonValidator::startValidation - ACK for nonexistant device Entry"));
			
			if(!didSucceed){
				_results.push_back(*entry);
				
				// remove entry from queue
				_val.remove_if ([deviceID](const  validation_result_t& valEntry){
					return  valEntry.deviceID == deviceID;
				});
			}
			else {
				entry->sendACK= true;
				gettimeofday(&entry->ackTime, NULL);	// start the clock on this.
			}
			
		});
	}
	
	return statusOK;
}

bool InsteonValidator::cancelValidation(){
 
	bool statusOK = false;
	
	if(_isValidating){
		
// stop it
		_isValidating = false;
		_val.clear();
		_results.clear();
		if(_callback)
			_callback(_results);
		 
	}
	return statusOK;
}


bool InsteonValidator::processPLMresponse(plm_result_t response){
	
	bool didHandle = false;
	
	if(_isValidating){
		
		// check for timeout
		if(response == PLR_NOTHING && _timeout_VALIDATE > 0){
			// lock durring the timeout scan
			std::lock_guard<std::mutex> lock(_mutex);
			
			timeval now, diff;
			gettimeofday(&now, NULL);
			
			// create a list of timed-out devices
			std::vector<DeviceID> _devicesTimeedOut;
			
			// scan the list of unvalidated devices and create a vector of device IDs that timed out
			for(auto e: _val){
				if(e.sendACK && !e.validated && !e.timeOut) {
					timersub(&now, &e.ackTime, &diff);
					if( diff.tv_sec >=  _timeout_VALIDATE  ) {
						e.timeOut = true;
						
						
						LOG_DEBUG("\tTIMEOUT %s %s\n",
									 e.deviceID.string().c_str(),
									 e.deviceID.nameString().c_str()
									 );
						
						// add to to the list of timeed out devices.
						_devicesTimeedOut.push_back(e.deviceID);
						// add this to the done queue
						_results.push_back(e);
					}
				}
			}
			
			// remove device IDs that timed out from the unvalidated devices list
			_val.remove_if ([_devicesTimeedOut](const  validation_result_t& valEntry){
				DeviceID deviceID = valEntry.deviceID;
				bool didFind = any_of(_devicesTimeedOut.begin(), _devicesTimeedOut.end(),
											 [&](const DeviceID& elem) { return elem == deviceID; });
				return didFind;
			});
		}
		
		else  if(response == PLR_MSG){
			insteon_msg_t msg;
			
			if(_plm->parseMessage(&msg)){
				if(msg.msgType == MSG_TYP_BROADCAST){
					
					// info about what this device is can be piocked up.
					if(msg.cmd[0] == InsteonParser::CMD_ASSIGN_TO_GROUP){
						
						// lock durring the message processing
						std::lock_guard<std::mutex> lock(_mutex);
						
						DeviceID deviceID = DeviceID(msg.from);
						validation_result_t* entry = findDBEntryWithDeviceID(deviceID);
						
						if(entry) {		// possible unhandled device sent a CMD_ASSIGN_TO_GROUP
							
							// update the deviceInfo
							entry->deviceInfo = DeviceInfo(msg.to[2],
																	 msg.to[1],
																	 msg.to[0]);
							
							entry->validated = true;
							
							LOG_INFO("\tVALIDATED %s %s Rev: %02X %s\n",
										deviceID.string().c_str(),
										entry->deviceInfo.string().c_str(),
										entry->deviceInfo.GetFirmware(),
										deviceID.nameString().c_str()
										);
							
							// Now we need to query the device for it's Insteon version
							InsteonDevice(deviceID).getEngineVersion([=]( uint8_t version, bool didSucceed) {
								
								LOG_DEBUG("\tRCV INSTEON VERSION from %s vers:%d\n",
											 deviceID.string().c_str(), version );
								
								validation_result_t* entry = findDBEntryWithDeviceID(deviceID);
								if(entry){
									
									if(didSucceed){
										entry->hasVersion = true;
										entry->deviceInfo.SetVersion(version);
									}
									
									// put the entry on the done list
									_results.push_back(*entry);
									
									// remove entry from work queue
									_val.remove_if ([deviceID](const  validation_result_t& valEntry){
										return  valEntry.deviceID == deviceID;
									});
								}
							});
							
							didHandle = true;
						}
					}
				}
			}
			if(didHandle){
				_plm->_parser.reset();
			}
			
		}
		
		// are we done here?
		if(_val.size() == 0){
			_isValidating = false;
			
			LOG_DEBUG("\tVALIDATION COMPLETE\n");
			if(_callback)
				_callback(_results);
		}
		
	}
	return didHandle;
}


//MARK: - private API

InsteonValidator::validation_result_t* InsteonValidator::findDBEntryWithDeviceID(DeviceID deviceID){
	
	for (auto e = _val.begin(); e != _val.end(); e++) {
		if(e->deviceID == deviceID)
			return &(*e);
	}
	
	return NULL;
}
