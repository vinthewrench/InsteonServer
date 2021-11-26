//
//  @file InsteonMgr.cpp
//
//  Created by Vincent Moscaritolo on 1/15/21.
//

#include <strings.h>
#include <unistd.h>
#include "InsteonMgr.hpp"
#include "LogMgr.hpp"
#include "NotificationCenter.hpp"
#include "InsteonParser.hpp"

#include "InsteonDevice.hpp"
#include "sleep.h"

#define CHK_STATUS if(!status) goto done;


const char* 	InsteonMgr::InsteonMgr_Version = "1.0.1";

/**
 * @brief Initialize the InsteonMgr
 *
 */

InsteonMgr::InsteonMgr(){

	_isSetup = false;
	_hasPLM = false;
	_hasInfo = false;
	_state = STATE_INIT;
	_aldb = NULL;
	_linking = NULL;
	_cmdQueue = NULL;
	_validator = NULL;
	_running = false;
	_shouldRunStartupEvents = true;
	
	_nextValidationCheck = 0;			// turn it off for now
	_expired_delay	 =  0; // 8*60*60; //(60*60*24);		// pester devices each day
	
	_timeout_LINKING = (60); 			// seconds
	
	// clear database
	_db.clear();
	
	// start the thread running
	_running = true;
	
//	// cleanup from previous thread?
//	if(_thread.joinable())
//		_thread.join();
	
	_thread = std::thread(&InsteonMgr::run, this);
}


/**
 * @brief delete the InsteonMgr
 *
 * 	Free up any dynamic storage.
 *
 */

InsteonMgr::~InsteonMgr(){

	if(_aldb){
		delete _aldb;
		_aldb = NULL;
	}
	
	if(_linking) {
		delete _linking;
		_linking = NULL;
	}
	
	if(_validator) {
		delete _validator;
		_validator = NULL;
	}
  
	if(_cmdQueue) {
		delete _cmdQueue;
		_cmdQueue = NULL;
	}
 
	_isSetup = false;
	_hasPLM = false;
	
	_running = false;
	if (_thread.joinable())
		_thread.join();

}

void InsteonMgr::run(){
	
	  LOGT_INFO("InsteonMgr %s START", InsteonMgr_Version);

	try{
		
		while(_running){
			
			if( _hasPLM) {
				
				// wait for DB response
				plm_result_t result = this->handleResponse(InsteonPLM::defaultCmdTimeout);
				
				if(result == PLR_CONTINUE) continue;
				
				if(result == PLR_ERROR){
					_hasPLM = false;
					_state = STATE_PLM_ERROR;
				}
				else if(result == PLR_INVALID) {
					_state = STATE_PLM_ERROR;
				}
			}
			else { // no PLM
				sleep(1);
			}
			
		};
	}
	catch ( const PLMStreamException& e)  {
		printf("\tError %d %s\n\n", e.getErrorNumber(), e.what());
		
		if(e.getErrorNumber()	 == ENXIO){
			stopPLM();
	
			
//			if (_thread.joinable()){
//				_thread.join();
//			}

		}
	}
	
	LOGT_INFO("InsteonMgr STOP");
}


bool InsteonMgr::loadCacheFile(string filePath){
	
	bool success = false;
	_db.clear();
	success =  _db.restoreFromCacheFile(filePath);
	
	if(success) {
		_state = STATE_SETUP;
		
		double dLat, dLong;
		if(_db.getLatLong(dLat, dLong)){
			ScheduleMgr::shared()->setLatLong(dLat ,dLong);
		}
		_isSetup = true;
	}
	
	if(_db.getPLMAutoStart()){
 		startPLM("",  [=](bool didSucceed, string errorTxt) {
			if(didSucceed){
				syncPLM( [=](bool didSucceed) {
					if(didSucceed){
				// start validation process in background
							validatePLM( [](bool didSucceed) {
					// let this run in background
						});
					}
				});
			}
		});
	}	
 	return success;
}



/**
 * @brief Set the durration in seconds hat devices need to be revalidated.
 *  Typically once a day
 *
 */

 
void InsteonMgr::setExpiredDelay(time_t delayInSeconds){
	
	_expired_delay = delayInSeconds;
}
 

bool InsteonMgr::getDeviceInfo(DeviceID deviceID, insteon_dbEntry_t * info){
	bool status = true;
	status = _db.getDeviceInfo(deviceID,info);
	return status;
}


string InsteonMgr::currentStateString(){
	
	string result;
	
	switch(_state){
			
		case STATE_INIT:
			result = "Initializing";
			break;
		case STATE_SETUP:
			result = "Database Loaded";
			break;
		case STATE_NO_PLM:
			result = "No PLM Found";
			break;
		case STATE_PLM_STOPPED:
			result = "PLM Stopped";
			break;
		case STATE_PLM_INIT:
			result = "PLM Initialized";
			break;
		case STATE_PLM_ERROR:
			result = "PLM Error";
			break;
		case STATE_RESETING:
			result = "Reseting";
			break;
		case STATE_SETUP_PLM:
			result = "Setup PLM";
			break;
		case STATE_READING_PLM:
			result = "Reading PLM";
			break;
		case STATE_VALIDATING:
			result = "Validating";
			break;
		case STATE_READY:
			result = "Ready";
			break;
		case STATE_LINKING:
			result = "Linking Device";
			break;
		case STATE_UPDATING:
			result = "Updating Device Levels";
			break;
		default:
			result = "Unknown";
			break;
			
	}
	
	return result;
}

bool InsteonMgr::getSolarEvents(solarTimes_t &solar){
	return ScheduleMgr::shared()->getSolarEvents(solar);
}

bool InsteonMgr::refreshSolarEvents(){
	
	double dLat, dLong;
	
	_db.getLatLong(dLat, dLong);
	ScheduleMgr::shared()->setLatLong(dLat ,dLong);
 
	return ScheduleMgr::shared()->calculateSolarEvents();
}


// MARK: - ALDB and PLM setup


void InsteonMgr::startPLM(string plmPath,
							 std::function<void(bool didSucceed,
													  string error_text)> callback){

  int  errnum = 0;

  _state 			= STATE_NO_PLM;
  _hasPLM 			= false;
  _plmDeviceID 	=  DeviceID();
  _plmDeviceInfo 	=  DeviceInfo();
  _hasInfo			= false;
  
  if(plmPath.empty())
	  plmPath = _db.getPLMPath();
	
  if(plmPath.empty()){
	  callback(false, "No PLM path specified");
	  return;
  }

  // setup plm command queue
  if(!_cmdQueue){
	  _cmdQueue = new InsteonCmdQueue(&_plm, &_db);
  }
  
  if(!_aldb){
	  _aldb = new InsteonALDB(_cmdQueue);
  }
 
  LOGT_INFO("PLM_START: %s", plmPath.c_str());

  if(_plm.isConnected()){
	  _plm.stop();
  }

  if(! _stream.begin(plmPath.c_str(), errnum))
	  throw InsteonException(string(strerror(errnum)), errnum);
	  
  if(!_plm.begin(&_stream, InsteonPLM::defaultCmdTimeout)) {
	  _state = STATE_PLM_ERROR;
	  callback(false, "PLM begin failed");
  }

  _hasPLM 	= true;
  _state 	= STATE_SETUP_PLM;

// Step: 1 -- get the PLM info to check if its there?
  LOG_DEBUG("\tPLM_SETUP: GET IM_INFO\n");
  _cmdQueue->queueCommand(InsteonParser::IM_INFO,
								 NULL, 0, [this, callback]( auto reply, bool didSucceed) {
	  
	  if(!didSucceed){
		  _state = STATE_PLM_ERROR;
		  callback(false, "PLM didn't respond");
	  }
	  else {
		  _plmDeviceID 	=  DeviceID(reply.info.devID);
		  _plmDeviceInfo 	=  DeviceInfo(reply.info.cat,reply.info.subcat,reply.info.firmware);
		  _hasInfo	= true;
		  
		  LOG_INFO("\tPLM INFO: %s - %s %s\n",
					  _plmDeviceID.string().c_str(),
					  _plmDeviceInfo.string().c_str(),
					  _plmDeviceInfo.description_cstr());
  
		  // check if the PLM ID changed
		  DeviceID lastPLM = _db.getPlmDeviceID();
		  if(lastPLM.isNULL()){
			  // assume we never setup a PLM before.
			  // update the deviceID for the PLM database
			  _db.setPlmDeviceID(_plmDeviceID);
			  _db.saveToCacheFile();
		  }
		  else if( lastPLM != _plmDeviceID){
			  
			  _state = STATE_PLM_ERROR;
			  callback(false, "PLM deviceID changed");
			  return;
			  }

// Step:2 -- Cancel any linking in progress?
		  LOG_DEBUG("\tPLM_SETUP: IM_CANCEL_LINKING\n");
		  _cmdQueue->queueCommand(InsteonParser::IM_CANCEL_LINKING,
										 NULL, 0, [this, callback]( auto reply, bool didSucceed) {
			  
			  if(!didSucceed){
				  _state = STATE_PLM_ERROR;
				  callback(false, "PLM Cancel Linking failed");
			  }
			  else{
// Step:3 -- setup PLM IM config ?
				  uint8_t cmdArgs[]  = {  InsteonParser::IM_CONFIG_FLAG_MONITOR
				  /*	| InsteonParser::IM_CONFIG_FLAG_LED */ };
				  
				  LOG_DEBUG("\tPLM_SETUP: IM_SET_CONFIG (%02X) \n", cmdArgs[0]);
				  _cmdQueue->queueCommand(InsteonParser::IM_SET_CONFIG,
												 cmdArgs, sizeof(cmdArgs),
												 [this, callback]( auto reply, bool didSucceed) {
	  
					  if(!didSucceed){
						  _state = STATE_PLM_ERROR;
						  callback(false, "PLM set config failed");
					  }
					  else {
						  _state = STATE_PLM_INIT;
						  callback(true, string());
					  }});
			  }});
	  }});
}


/**
* @brief shutdown the Insteom Manager
*
*
*/

void InsteonMgr::stopPLM(){

  if(_hasPLM) {
	  
	  LOGT_INFO("PLM_STOP");

	  _cmdQueue->abort();

	  _plm.stop();
	  _stream.stop();
	  _plmDeviceID 	=  DeviceID();
	  _plmDeviceInfo 	=  DeviceInfo();
	  _hasInfo			= false;
	  _shouldRunStartupEvents = true;

	  _state = STATE_PLM_STOPPED;
	  _hasPLM = false;
  }
}




/**
* @brief return the PLM ddeviceID and Type
*
*
*/

bool InsteonMgr::plmInfo( DeviceID*  deviceIDp, DeviceInfo* infop){
  
  if(!_hasPLM)
	  return false;
  
  if(deviceIDp)
	  *deviceIDp = _plmDeviceID;

  if(infop)
	  *infop = _plmDeviceInfo;
  return true;
}


void InsteonMgr::initPLM(string plmPath,
								 std::function<void(bool didSucceed,
														string error_text)> callback){

	int  errnum = 0;
	
	LOGT_INFO("INIT_PLM:");
	
	// check that managers are in place.
	if(!_cmdQueue){
		_cmdQueue = new InsteonCmdQueue(&_plm, &_db);
	}
	
	if(!_aldb){
		_aldb = new InsteonALDB(_cmdQueue);
	}
 
	switch (_state) {
		case STATE_PLM_INIT:
		case STATE_PLM_ERROR:
		case STATE_PLM_STOPPED:
		case STATE_NO_PLM:
		case STATE_UNKNOWN:
		case STATE_SETUP:
			break;
			
		default:
			callback(false, "PLM in wrong state");
			return;
	}
	
	stopPLM();
	
	if(plmPath.empty())
		plmPath = _db.getPLMPath();
	
	if(plmPath.empty()){
		callback(false, "No PLM path specified");
		return;
	}
	
	if(! _stream.begin(plmPath.c_str(), errnum))
		throw InsteonException(string(strerror(errnum)), errnum);
	
	if(!_plm.begin(&_stream, InsteonPLM::defaultCmdTimeout)) {
		_state = STATE_PLM_ERROR;
		callback(false, "PLM begin failed");
	}

	_hasPLM 	= true;
	_state = STATE_RESETING;
	
	// Step: 1 -- get the PLM info to check if its there?
	LOG_DEBUG("\tPLM_INIT: GET IM_INFO\n");
	_cmdQueue->queueCommand(InsteonParser::IM_INFO,
									NULL, 0, [this, callback]( auto reply, bool didSucceed) {
		
		if(!didSucceed){
			_state = STATE_PLM_ERROR;
			callback(false, "PLM didn't respond");
		}
		else {
			_plmDeviceID 	=  DeviceID(reply.info.devID);
			_plmDeviceInfo 	=  DeviceInfo(reply.info.cat,reply.info.subcat,reply.info.firmware);
			_hasInfo	= true;
			// Step:2 -- RESET THE DEVICE
			
			_cmdQueue->queueCommand(InsteonParser::IM_RESET,
											NULL, 0,
											[this, callback]( auto reply, bool didSucceed) {
				
				if(!didSucceed){
					_state = STATE_PLM_ERROR;
					callback(false, "PLM RESET failed");
				}
				else{
					// Step:3 -- setup PLM IM config ?
					uint8_t cmdArgs[]  = {  InsteonParser::IM_CONFIG_FLAG_MONITOR
						/*	| InsteonParser::IM_CONFIG_FLAG_LED */ };
					
					LOG_DEBUG("\tPLM_SETUP: IM_SET_CONFIG (%02X) \n", cmdArgs[0]);
					_cmdQueue->queueCommand(InsteonParser::IM_SET_CONFIG,
													cmdArgs, sizeof(cmdArgs),
													[this, callback]( auto reply, bool didSucceed) {
						
						if(!didSucceed){
							_state = STATE_PLM_ERROR;
							callback(false, "PLM set config failed");
						}
						else {
							_state = STATE_PLM_INIT;
							callback(true, string());
						}});
				}});
		}
	});
}

/**
 * @brief sync the PLM database with InsteonDB
 *
 *  we start the process of reading the ALDB from the PLM.  Once we get this data we
 *  attempt to sync the ALDB with what our Insteon DB says is the truth. This might
 *  require us to add or delete some entries from the PLM.
 *
 */
void InsteonMgr::syncPLM(boolCallback_t callback){
	
	LOGT_INFO("SYNC_PLM:");

	if(_state != STATE_PLM_INIT)
		throw InsteonException("_state == STATE_PLM_INIT");
 
	if(!_aldb)
		throw InsteonException("aldb not setup");

	if(!_isSetup){
		if(!loadCacheFile()) {
			callback(false);
			return ;
		}
	}

	_state = STATE_READING_PLM;
	_aldb->readPLM([this, callback]( auto aldbEntries) {
		
		LOG_INFO("\tSYNC_PLM: READ %d entries \n", aldbEntries.size());

		// SYNC ALDB Data
		vector<insteon_aldb_t> plmRemove;
		vector<insteon_aldb_t> plmAdd;
	 
		// check if the PLM ID changed
		DeviceID lastPLM = _db.getPlmDeviceID();
		if(lastPLM.isNULL() || lastPLM != _plmDeviceID){
			// WE replaced the PLM --- fail for now.
		
			LOG_ERROR("\tPLM CHANGED: %s\n",_plmDeviceID.string().c_str());
			// reset the deviceID for the PLM database

			_state = STATE_PLM_ERROR;
			callback(false);
			return;
		};

		if(_db.syncALDB(aldbEntries, &plmRemove, &plmAdd)){
			
			LOG_INFO("\tSYNC_PLM: SYNC ALDB  add: %d remove: %d \n",
						plmAdd.size(), plmRemove.size() );

			/* set a count of how many tasks related to PLM we need to complete
			 * before readALDBContinue will actually do anything.
			 * This is a psuedo-async process, In that we are actually waiting for
			 * _aldb->remove and _aldb->create to complete their actions.
			 * what makes this psuedo-async is that these functions depend on
			 * InsteonCmdQueue::queueCommand which depends on being called by loop()
			 * - in effect the call to readALDBContinue() wont happen until the
			 * aldbTaskCount == 0..
			 */
			
			size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
			*taskCount = plmRemove.size() + plmAdd.size();
			
			for(auto e : plmRemove){
				DeviceID deviceID = e.devID;
				bool isController = (e.flag & 0x40) == 0x40;
				
				_aldb->remove(deviceID, isController, e.group,
								  [this,taskCount, callback](bool didSucceed){
					
					if(--(*taskCount) == 0) {
						LOG_INFO("SYNC_PLM: DONE\n");
						free(taskCount);
						_state = STATE_READY;
						callback(true);
					}
				});
			}
			
			for(auto e : plmAdd){
				DeviceID deviceID = e.devID;
				bool isController = (e.flag & 0x40) == 0x40;
				
				_aldb->create(deviceID, isController, e.group, DeviceInfo(e.info),
								  [this, taskCount, callback](bool didSucceed){
					if(--(*taskCount) == 0) {
						LOGT_INFO("SYNC_PLM: DONE");
						free(taskCount);
						_state = STATE_READY;
						callback(true);
					}
				});
				
			}
		}
		else {
			LOGT_INFO("SYNC_PLM: DONE");
			_state = STATE_READY;

			callback(true);
		}
	});
 }


 
void InsteonMgr::readPLM(boolCallback_t callback){
	
	LOGT_INFO("READ_PLM:\n");
	
	if(!_hasPLM)
	 throw InsteonException("PLM is not setup");
	
	if(!_aldb)
		throw InsteonException("aldb not setup");

	_state = STATE_READING_PLM;
	
	_aldb->readPLM([this, callback]( auto aldbEntries) {
		
		// reset the deviceID for the PLM database
		_db.clear();
		_db.setPlmDeviceID(_plmDeviceID);
 
		_db.addPLMEntries(aldbEntries);
		_state = STATE_READY;
		callback(true);
	});
	
}

void InsteonMgr::savePLMCacheFile(){
	_db.saveToCacheFile();
}
 
 
// MARK: - device validation

void InsteonMgr::validatePLM(boolCallback_t callback){
 
	if(!_hasPLM)
		throw InsteonException("PLM is not setup");

	// we only do this in the ready state
	if(_state != STATE_READY){
		callback(false);
		return;
	}

	if(!_validator){
		_validator = new InsteonValidator(_cmdQueue);
	}

	// create a list of devices to validate
	vector<DeviceID>  val_list = _db.devicesThatNeedValidation();

	 LOG_INFO("\tVALIDATION NEEDED for %d devices\n",  val_list.size() );

	_state = STATE_VALIDATING;
	
//	START_VERBOSE
	_validator->startValidation(val_list,
										 [=]( auto result) {
		
		for(auto e : result){
			if(e.validated){
				_db.validateDeviceInfo(e.deviceID, &e.deviceInfo);
			}
			else  {
				_db.validateDeviceInfo(e.deviceID, NULL);
			}
		}
		
		_db.saveToCacheFile();
		
		//		if(result.size() > 0){
		//			_db.saveToCacheFile();
		//		}
		
		//		// schedule next validation check in the future
		//		_nextValidationCheck = time(NULL)
		//				+ ((_db.devicesThatNeedValidation().size() > 0) ? 60 : _expired_delay);
		
		_state = STATE_READY;
		
		bool deferCompletion = false;
		
		if(_shouldRunStartupEvents) {
			
			auto eventIDs =  _db.eventsMatchingAppEvent(EventTrigger::APP_EVENT_STARTUP);
			
			if(eventIDs.size() > 0){
				size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
				*taskCount = eventIDs.size();
				deferCompletion = true;
				
				// run startup events.
				LOGT_INFO("RUN %d STARTUP EVENTS", eventIDs.size());

				for (auto eventID : eventIDs) {
					executeEvent(eventID, [=]( bool didSucceed) {
						
						if(--(*taskCount) == 0) {
							free(taskCount);
							updateLevels();
							if(callback)
								callback(true);
						}
					});
				}
			}
			
			_shouldRunStartupEvents = false;
		}
		
		if(!deferCompletion){
			updateLevels();
			if(callback)
				callback(true);
		}
	});
}

/**
 * @brief start the process of checking if the devices are actually there
 *
 *
 */

void InsteonMgr::startDeviceValidation(){
		
	// we are already busy doing this.
	if(_state == STATE_VALIDATING)
		return;
	
// we only do this in the ready state -- defer it
	if(_state != STATE_READY){
		_nextValidationCheck = time(NULL) + 60;
		return;
	}
	
	// create a list of devices to validate
	vector<DeviceID>  val_list = _db.devicesThatNeedValidation();

	 LOG_INFO("\tVALIDATION NEEDED for %d devices\n",  val_list.size() );
 
	if(val_list.size() > 0){
		
		if(!_validator){
			_validator = new InsteonValidator(_cmdQueue);
		}
		
		_state = STATE_VALIDATING;
		
	//	START_VERBOSE
		_validator->startValidation(val_list,
											 [this]( auto result) {
			
			for(auto e : result){
				if(e.validated){
					_db.validateDeviceInfo(e.deviceID, &e.deviceInfo);
				}
				else  {
					_db.validateDeviceInfo(e.deviceID, NULL);
				}
			}
			
			if(result.size() > 0){
				_db.saveToCacheFile();
			}
			
			// schedule next validation check in the future
			_nextValidationCheck = time(NULL)
					+ ((_db.devicesThatNeedValidation().size() > 0) ? 60 : _expired_delay);
		 
			_state = STATE_READY;
			
			// can we free it up from a callback?
			delete _validator;
			_validator = NULL;
			updateLevels();
			
		});
	}
	else { 	// no devices to validate
		// schedule another check in the future
		_nextValidationCheck = time(NULL) + _expired_delay;
		updateLevels();
	}
}


bool InsteonMgr::validateDevice(DeviceID deviceID,
										  boolCallback_t callback){
		
	// we only do this in the ready state
	if(_state != STATE_READY){
		return false;
	}
	
	// we are already busy doing this.
	if(_state == STATE_VALIDATING)
		return false;
	
	
	LOG_INFO("\tVALIDATION NEEDED for device %s\n",  deviceID.string().c_str() );
	
	if(!_validator){
		_validator = new InsteonValidator(_cmdQueue);
	}
	
	_state = STATE_VALIDATING;
	
	//	START_VERBOSE
	_validator->startValidation({deviceID},
										 [=]( auto result) {
		
		boolCallback_t cb = callback;
		
		_state = STATE_READY;

		auto e  = result.front();
		
		if(e.validated){
			_db.validateDeviceInfo(e.deviceID, &e.deviceInfo);
			_db.saveToCacheFile();

		}
		else  {
			_db.validateDeviceInfo(e.deviceID, NULL);
			if(cb)
				(cb)(false);
			return ;
		}
	 
		// can we free it up from a callback?
		delete _validator;
		_validator = NULL;
		
		InsteonDevice(deviceID).getOnLevel([=](uint8_t level, bool didSucceed) {
			
			if(didSucceed){
				_db.setDBOnLevel(e.deviceID, 0x01, level);
				_db.saveToCacheFile();
			}
			if(cb)
				(cb)(true);
		});
		
	});
	
	return true;
}

// MARK: - Set Device State
bool InsteonMgr::getOnLevel(DeviceID deviceID, bool forceLookup,
									 std::function<void(uint8_t level, eTag_t lastTag,  bool didSucceed)> cb){
	
	
	// state limitation
	switch(_state) {
		case STATE_READY:
		case STATE_VALIDATING:
		case STATE_UPDATING:
			if(_db.isDeviceValidated(deviceID )) break;
 // yes it's a fall through
		default:
			
			if(cb) (cb)(0,0, false);
			return false;
	}
 		 
	uint8_t group = 0x01;		// only on group 1
	uint8_t level = 0;
	eTag_t lastTag = 0;
 
	// do we have a value in our Database
	if(!forceLookup  && _db.getDBOnLevel(deviceID, group, &level, &lastTag)) {
		if(cb) (cb)(level, lastTag, true);
	}
	else {
		InsteonDevice(deviceID).getOnLevel([=](uint8_t level, bool didSucceed) {
			eTag_t lastTag = 0;

			if(didSucceed){
				_db.setDBOnLevel(deviceID, group, level, &lastTag);
			}
			if(cb) (cb)(level,lastTag, didSucceed);
		});
	}
	
 	return true;
}

bool InsteonMgr::setOnLevel(DeviceID deviceID, uint8_t onLevel,
									 std::function<void(eTag_t lastTag,  bool didSucceed)> cb ){
	
	if(_state != STATE_READY)
		return false;
	
	InsteonDevice(deviceID).setOnLevel(onLevel, [=](bool didSucceed){
		
		uint8_t group = 0x01;		// only on group 1
		eTag_t lastTag = 0;
		
		if(didSucceed){
			_db.setDBOnLevel(deviceID, group, onLevel, &lastTag);
		}
		
		if(cb) (cb)(lastTag, didSucceed);
		
	});
	
	return true;
}

 bool InsteonMgr::setLEDBrightness(DeviceID deviceID, uint8_t level,
											 boolCallback_t cb  ){
	if(_state != STATE_READY)
		return false;
	
	InsteonDevice(deviceID).setLEDBrightness(level, [=](bool didSucceed){
		if(cb) (cb)(didSucceed);
	});
	
	return true;
}

bool InsteonMgr::setKeypadLEDState(DeviceID deviceID, uint8_t mask,
											boolCallback_t cb  ){
  if(_state != STATE_READY)
	  return false;
  
	InsteonKeypadDevice(deviceID).setKeypadLEDState(mask, [=](bool didSucceed){
		if(cb) (cb)(didSucceed);
	});

   return true;
}

bool InsteonMgr::runActionForKeypad(DeviceID deviceID, uint8_t buttonID, uint8_t cmd,
												boolCallback_t cb){
	
	LOG_INFO("\tKEYPAD: %s, button: %d, cmd %02x \n",
				deviceID.string().c_str(),
				buttonID,cmd);
	
	if(_state == STATE_READY){
		
		if( _db.invokeKeyPadButton(deviceID,buttonID, cmd)) {
			
			if(auto action =  _db.actionForKeypad(deviceID, buttonID, cmd) ; action != NULL){
				runAction(*action , [=](bool didSucceed){
					if(cb) (cb)( didSucceed);
				});
			}
			// No Actions for button - just return
			else {
					if(cb) (cb)( true);
			}
		}
		return true;
		
	}
	return false;
	
}


// MARK: - Group set

bool InsteonMgr::setOnLevel(GroupID groupID, uint8_t onLevel,
									 std::function<void(bool didSucceed)> cb){
 
	if(_state != STATE_READY)
		return false;
	
	if(!_db.groupIsValid(groupID))
		return false;
	
	auto devices = _db.groupGetDevices(groupID);
	if(devices.size() >  0){
		
		size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
		*taskCount = devices.size();

		uint8_t cmd = (onLevel == 0)
				? InsteonParser::CMD_LIGHT_OFF
				:InsteonParser::CMD_LIGHT_ON;
 
		// queue up all the devices.. and then reply when we are done.
		for(auto deviceID : devices) {
	 
			_cmdQueue->queueMessage(deviceID,
											cmd, onLevel,
											NULL, 0,
											[=]( auto arg, bool didSucceed) {
				
				if(didSucceed && arg.reply.msgType == MSG_TYP_DIRECT_ACK){
					uint8_t group = 0x01;		// only on group 1
					_db.setDBOnLevel(deviceID, group, onLevel, NULL);
				}

				if(--(*taskCount) == 0) {
					free(taskCount);
					if(cb) (cb)( true);
				}
			});
		}
	}
	else {
		if(cb) (cb)( true);
	}

	return true;
}


bool InsteonMgr::setLEDBrightness(GroupID groupID, uint8_t level,
							 std::function<void(bool didSucceed)> cb){
	
	if(_state != STATE_READY)
		return false;
	
	if(!_db.groupIsValid(groupID))
		return false;
	
	// pin brightnes -- its an 7 bit value
	if(level > 127)
		level = 127;

	auto devices = _db.groupGetDevices(groupID);
	if(devices.size() >  0){
		
		size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
		*taskCount = devices.size();
		
		uint8_t buffer[] = {
			0x00, 0x07, level};
		
		// queue up all the devices.. and then reply when we are done.
		for(auto deviceID : devices) {
			
			_cmdQueue->queueMessage(deviceID,
											InsteonParser::CMD_EXT_SET_GET, 0x00,
											buffer, sizeof(buffer),
											[=]( auto arg, bool didSucceed) {
				
				if(--(*taskCount) == 0) {
					free(taskCount);
					if(cb) (cb)( true);
				}
			});
		}
	}
	else {
		if(cb) (cb)( true);
	}

	return true;
}
	

//MARK: -  action groups

bool InsteonMgr::executeActionGroup(actionGroupID_t actionGroupID,
											  std::function<void(bool didSucceed)> cb){
	
	if(_state != STATE_READY)
		return false;

	if(!_db.actionGroupIsValid(actionGroupID))
		return false;
	
	auto actions = _db.actionGroupGetActions(actionGroupID);
	if(actions.size() == 0){
		if(cb) (cb)( true);
		return true;
	}
	
	LOG_INFO("\tEXECUTE ACTION.GROUP %04x \n",actionGroupID);

	size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
	*taskCount = actions.size();
	
	for(auto ref :actions){
		Action action = ref.get();
		bool handled = false;
		
		handled = runAction(action, [=](bool didSucceed){
			if(--(*taskCount) == 0) {
				free(taskCount);
				if(cb) (cb)( true);
			}
		});
		
		if(!handled)
			if(--(*taskCount) == 0) {
				free(taskCount);
				if(cb) (cb)( false);
			}
	};
	
	return true;
}

bool InsteonMgr::runAction(Action action,
									std::function<void(bool didSucceed)> cb){
	
	uint8_t level = action.level();
	bool handled = false;
	
	switch (action.actionType()	) {
			
		case Action::ACTION_TYPE_GROUP: {
			GroupID groupID = action.groupID();
			switch (action.cmd()) {
					
				case Action::ACTION_SET_LEVEL:
					handled = setOnLevel(groupID, level, [=](bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
					break;

				case Action::ACTION_SET_LED_BRIGHTNESS:
					handled = setLEDBrightness(groupID, level, [=](bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
					break;

				default:
					break;
			}
			
		}
			break;
			
		case Action::ACTION_TYPE_KEYPAD: {
			DeviceID keypadID = action.deviceID();
			switch (action.cmd()) {
				
				case Action::ACTION_ON:
						handled = runActionForKeypad(keypadID,
															  action.buttonID(),
															  InsteonParser::CMD_LIGHT_ON,
															  [=]( bool didSucceed){
							
							uint8_t newMask = _db.LEDMaskForKeyPad( _db.findKeypadEntryWithDeviceID(keypadID));
							InsteonKeypadDevice(keypadID).setKeypadLEDState(newMask, [=](bool success){
								if(cb) (cb)( didSucceed);
							});

		//					if(cb) (cb)( didSucceed);
						});
						break;
			
				case Action::ACTION_OFF:
						handled = runActionForKeypad(keypadID,
															  action.buttonID(),
															  InsteonParser::CMD_LIGHT_OFF,
															  [=]( bool didSucceed){
							
							
							uint8_t newMask = _db.LEDMaskForKeyPad( _db.findKeypadEntryWithDeviceID(keypadID));
							InsteonKeypadDevice(keypadID).setKeypadLEDState(newMask, [=](bool success){
								if(cb) (cb)( didSucceed);
							});
//							if(cb) (cb)( didSucceed);
						});
						break;
			 
				case Action::ACTION_SET_KEYPADMASK:
					handled = setKeypadLEDState(keypadID, action.mask(), [=]( bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
 
					break;

				default:
					break;
			}
		
		}
			break;
			
		case Action::ACTION_TYPE_DEVICE: {
			DeviceID deviceID = action.deviceID();
			switch (action.cmd()) {
					
				case Action::ACTION_SET_LEVEL:
					handled = setOnLevel(deviceID, level, [=](eTag_t eTag, bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
					break;
					
				case Action::ACTION_BEEP:
					handled =  InsteonDevice(deviceID).beep([=](bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
					break;
					
				case Action::ACTION_SET_LED_BRIGHTNESS:
					handled = setLEDBrightness(deviceID, level, [=]( bool didSucceed){
						if(cb) (cb)( didSucceed);
					});
					break;

 				default:
					break;
			}
			
		}
		
			break;
			
			
		case Action::ACTION_TYPE_DEVICEGROUP: {
			{
				uint8_t devGroupID = action.deviceGroupID();
				
				handled = InsteonDeviceGroup(devGroupID).setOnLevel(level, [=](bool didSucceed){
					
					if(didSucceed) {
						
						auto devices = _db.devicesRespondingToInsteonGroup(devGroupID);
						for (auto devID : devices) {
							_db.setDBOnLevel(devID, 0x01, level);
						}
						
						if(devGroupID == 0xff && level == 0) { // wild  card shutoff
							
							for (auto keypadID : _db.allKeypads()) {
								_db.invokeKeyPadButton(keypadID, 0xff, InsteonParser::CMD_LIGHT_OFF);
							}
						}
						
					}
					if(cb) (cb)( didSucceed);
				});
			}
		}
			break;
			
		case Action::ACTION_TYPE_ACTIONGROUP: {
			
			// VINNIE  WATCH OUT FOR RECURSION!!
			
			actionGroupID_t actGrp =  action.actionGroupID();
			handled =  executeActionGroup(actGrp, [=](bool didSucceed){
				if(cb) (cb)( didSucceed);
			});
			
			break;
		}
			
		case Action::ACTION_TYPE_UNKNOWN:  
			break;
			
	}
	
	return handled;
}


//MARK: -  events

bool InsteonMgr::executeEvent(eventID_t eventID,
										std::function<void(bool didSucceed)> cb){
	
	if( !(_state == STATE_READY  || _state == STATE_UPDATING))
		return false;
	
	if(!_db.eventsIsValid(eventID))
		return false;
	
	auto ref = _db.eventsGetEvent(eventID);
	if(!ref)
		return false;
	
	Event event = ref->get();
	Action action = event.getAction();
	
	bool handled = runAction(action, [=](bool didSucceed){
		if(cb) (cb)( didSucceed);
	});
	
	return handled;
}

// MARK: - Linking

bool InsteonMgr::addToDeviceALDB(DeviceID deviceID,
											bool isCNTL, uint8_t groupID,
											boolCallback_t callback){
	bool status = false;
	
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
 
	if(!_aldb)
		throw InsteonException("aldb not setup");
	
	uint8_t linkData[3] = {
		_plmDeviceInfo.GetCat(),
		_plmDeviceInfo.GetSubcat(),
		_plmDeviceInfo.GetVersion()};
	
	status = _aldb->addToDeviceALDB(deviceID, isCNTL, groupID, linkData,
											  [=]( std::vector<insteon_aldb_t> newAldb,  bool didSucceed) {
		
		if(didSucceed){
			_db.setDeviceALDB(deviceID, newAldb);
			_db.saveToCacheFile();
		};
		
		callback(didSucceed);
	});
	
	return  status;
}


bool InsteonMgr::addToDeviceALDB(DeviceID deviceID,
							vector<pair<uint8_t,bool>> aldbGroups, // <uint8_t groupID, bool isCNTL >
							boolCallback_t cb) {
	bool status = false;
 
	if(aldbGroups.size() == 0)
		return false;

	auto gInfo = aldbGroups.back();
	aldbGroups.pop_back();
 
	status = addToDeviceALDB(deviceID,  gInfo.second , gInfo.first,
									  [=](bool didSucceed){
		if(!didSucceed) {
			if(cb) (cb)(false);
			return;
		}

		// are there more to process
		if(aldbGroups.size() > 0) {
			if(!addToDeviceALDB(deviceID,aldbGroups, cb)){
				if(cb) (cb)(false);
			}
		}
		// we are done
		else {
			if(cb) (cb)(true);
		}
	});
	
	return  status;
}


bool InsteonMgr::removeEntryFromDeviceALDB(DeviceID deviceID, uint16_t address,
														 boolCallback_t cb){
	bool status = false;
	
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
		
	if(!_aldb)
		throw InsteonException("aldb not setup");
 
	status = _aldb->removeEntryFromDeviceALDB(deviceID,address,
															[=]( std::vector<insteon_aldb_t> newAldb,  bool didSucceed) {
		
		if(didSucceed){
			_db.setDeviceALDB(deviceID, newAldb);
			_db.saveToCacheFile();
		};
		
		if(cb) (cb)(didSucceed);
		
	});

	return status;
}

bool InsteonMgr::updateALDBfromDevice(DeviceID deviceID,
												  boolCallback_t cb){
	bool status = false;
	
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
		
	if(!_aldb)
		throw InsteonException("aldb not setup");

	
	status = _aldb->readDeviceALDB(deviceID,
											 [=]( std::vector<insteon_aldb_t> newAldb,  bool didSucceed){
		if(didSucceed){
			_db.setDeviceALDB(deviceID, newAldb);
			_db.saveToCacheFile();
		};
		
		if(cb) (cb)(didSucceed);

	});
	
	return status;
}


bool InsteonMgr::linkKeyPadButtonsToGroups(DeviceID deviceID,
										 vector<pair<uint8_t,uint8_t>> buttonGroups,
														 boolCallback_t callback){
	bool status = false;
 
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
	
	if(!_aldb)
		throw InsteonException("aldb not setup");

	if(buttonGroups.size() == 0)
		return true;
	
	auto keypad = InsteonKeypadDevice(deviceID);
	status = keypad.linkKeyPadButtonsToGroups(&_db, _aldb, buttonGroups, callback);
	
	return status;
}


/**
 * @brief programatically link a new device
 *
 *  programatically linking a new device require a few steps:
 *
 *	1. We have to put our PLM in linking mode  by issuing a IM_START_LINKING command
 *	  - specifying the group number and that we will be the controller.
 *
 * 2.  We issue a Remote Enter Link Mode command to the target device .
 *
 * 3. If successful we query the device for it's Insteon Engine Version
 *
 * 4. Read the device ALDB and store it in ours Database
 *
 * -  If we wish to add ourselves as a slave mode,  use the addToDeviceALDB() API
 *  once the linking is completed.
 *
 */

bool InsteonMgr::linkDevice(DeviceID deviceID,
									 bool isCTRL,
									 uint8_t groupID,
									 string deviceName,
									 boolCallback_t callback){
	
	bool status = false;
	
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
	
	if(!_linking){
		_linking = new InsteonLinking(_cmdQueue);
	}
	
	LOG_INFO("\tLINK DEVICE %s Group: %02x\n",
				deviceID.string().c_str(),
				groupID);
	
	status = _linking->linkDevice(deviceID, isCTRL, groupID,
											[=]( auto link) {
		switch(link.status){
			case InsteonLinking::LINK_FAILED:
				LOG_INFO("\tLINKING FAILED\n");
				break;
				
			case InsteonLinking::LINK_TIMEOUT:
				LOG_INFO("\tLINKING LINK_TIMEOUT\n");
				break;
				
			case InsteonLinking::LINK_CANCELED:
				LOG_INFO("\tLINKING LINK_CANCELED\n");
				break;
				
			default:;
		}
		
		if(link.status == InsteonLinking::LINK_SUCCESS){
			
			InsteonDevice(deviceID).getEngineVersion([=]( uint8_t version, bool didSucceed) {
				
				auto info =  DeviceInfo(link.deviceInfo);
				
				if(didSucceed){
					info.SetVersion(version);
				}
				// add the new device to ur database.
				_db.linkDevice(link.deviceID,
									true,	// is Controller
									link.groupID,
									info,
									deviceName,
									true);			// is validated
				
				_aldb->readDeviceALDB(deviceID, [=]
											 ( std::vector<insteon_aldb_t> aldb,  bool didSucceed) {
					
					if(didSucceed){
						_db.updateDeviceALDB(deviceID, aldb);
					}
					
					LOG_INFO("\tLINKING COMPLETE(%02x) %s %s rev:%02X version:%02X %s\n",
								link.groupID,
								deviceID.string().c_str(),
								info.string().c_str(),
								info.GetFirmware(),
								info.GetVersion(),
								!deviceName.empty()?deviceName.c_str():"<unknown>"
								);
	 
					_db.saveToCacheFile();
					callback(true);
				});
			});
		}
		else {
			callback(false);
		}
		
	});
	
	return  status;
}

bool InsteonMgr::importDevices_Internal(vector<plmDevicesEntry_t> devices,
								 boolCallback_t cb){
	
	bool status = false;
	
	// did we complete all of them?
	if(devices.size() == 0) {
		if(cb) (cb)(true);
		return true;
	}
 
	auto dInfo = devices.back();
	devices.pop_back();
 
	status = linkDevice(dInfo.deviceID,
							  true,
							  dInfo.cntlID,
							  dInfo.name,
							  [=](bool didSucceed){
		if(didSucceed) {
			
			
			// link aldb
			addToDeviceALDB(dInfo.deviceID, dInfo.responders,
								 [=](bool didSucceed) {
				// do the next one.
				importDevices_Internal(devices, cb);
			});
		}
 		else
		{
			_importDevices_failList.push_back(dInfo.deviceID);
			
			// do the next one.
			importDevices_Internal(devices, cb);
 		}
		 
 	});
	
	return  status;
}


bool InsteonMgr::importDevices(vector<plmDevicesEntry_t> devices,
									  std::function<void(vector<DeviceID> failedDevices,  bool didSucceed)> cb){
	bool status = false;
	
	// param check
	if(devices.size() == 0)
		return false;
	
	if( (_state != STATE_READY) && (_state != STATE_PLM_INIT))
		return false;
	
	if(!_linking){
		_linking = new InsteonLinking(_cmdQueue);
	}
	
	_importDevices_failList.clear();
	
	status =  importDevices_Internal(devices, [=](bool didSucceed){
		
		if(cb)
			(cb)(_importDevices_failList, didSucceed);
		
 		_importDevices_failList.clear();
	});
	
	return  status;
}

bool InsteonMgr::startLinking(uint8_t groupID){
	bool status = false;
	
	if(_state != STATE_READY)
		return false;
		
	if(!_linking){
		_linking = new InsteonLinking(_cmdQueue);
	}
	
	LOG_INFO("\tLINKING START(%02x)\n",   groupID);

	_state = STATE_LINKING;
 	status = _linking->startLinking(true,	// is Controller
											  groupID,
											  [this]( auto link) {
		
		switch(link.status){
				
			case InsteonLinking::LINK_SUCCESS:
			{
				LOG_INFO("\tLINKING COMPLETE(%02x) %s %s rev:%02X \n",
							link.groupID,
							link.deviceID.string().c_str(),
							link.deviceInfo.string().c_str(),
							link.deviceInfo.GetFirmware());
				
				_db.linkDevice(link.deviceID,
									true,	// is Controller
									link.groupID,
									link.deviceInfo,
									"",
									true);			// is validated
				
				_db.saveToCacheFile();
				
	//			_db.printDB();
			}
				break;
				
			case InsteonLinking::LINK_FAILED:
				LOG_INFO("\tLINKING FAILED\n");
				break;
				
			case InsteonLinking::LINK_TIMEOUT:
				LOG_INFO("\tLINKING LINK_TIMEOUT\n");
				break;
				
			case InsteonLinking::LINK_CANCELED:
				LOG_INFO("\tLINKING LINK_CANCELED\n");
				break;
				
			default:;
		}
		
		_state = STATE_READY;
		
		// can we free it up from a callback?
		delete _linking;
		_linking = NULL;
		
	});
	
	if(status	){
		_state = STATE_LINKING;
	}
		 
done:
	return status;

}
  
bool InsteonMgr::cancelLinking(){
	
	bool status = true;
	
	if(_state != STATE_LINKING ||  _linking == NULL)
		return false;
	
	status = _linking->cancelLinking();
	
	_state = STATE_READY;
	
	return status;
}


// MARK:  - unlinking device

bool InsteonMgr::unlinkDevice(DeviceID deviceID,
										boolCallback_t cb){

	bool statusOK = false;
	
	if(_state != STATE_READY)
		return false;
	 
	if(!_aldb)
		throw InsteonException("aldb not setup");
	
 	// update the device's ALDB removing us from it.
	statusOK = _aldb->readDeviceALDB(deviceID, [=]
												( std::vector<insteon_aldb_t> aldb,  bool didSucceed) {
		
		bool needsALDBupdate = false;
		
		if(didSucceed){
			// walk the device ALDB looking for our _plmDeviceID
			
			for (auto e = aldb.begin(); e != aldb.end(); e++) {
				if(_plmDeviceID.isEqual(e->devID)){
					// set the flag to delete.  0
					e->flag &= 0x02;
					e->group = 0;
					e->devID[0] = 0;
					e->devID[1] = 0;
					e->devID[2] = 0;
					e->info[0] = 0;
					e->info[1] = 0;
					e->info[2] = 0;
					needsALDBupdate = true;
				}
			}
	
			if(needsALDBupdate){
	 			_aldb->syncDeviceALDB(deviceID, aldb, [=](bool didSucceed){
					
//		 			printf("Synced ALDB %s\n", didSucceed?"OK":"FAIL");
					
					_db.removeDevice(deviceID);
					if(cb) (cb)(true);
					
				}); // dont care about callback
			}
			return;
		}
		
		if(!needsALDBupdate) {
			_db.removeDevice(deviceID);
			if(cb) (cb)(true);
		}
	});
 
	return statusOK;
}


// MARK: - event manager

void InsteonMgr::registerEvent( DeviceID deviceID,
										 uint8_t groupID,
										 uint8_t cmd,
										 std::string& notification) {
	
	_eventMgr.registerEvent(deviceID, groupID, cmd, notification);
}

// MARK: -  listen for useful events

bool  InsteonMgr::processBroadcastEvents(plm_result_t response) {
	
	bool didHandle = false;
	
	insteon_msg_t msg;

	if(_plm.parseMessage(&msg)){
		
		DeviceID deviceID = DeviceID(msg.from);
	
		
		if(msg.msgType == MSG_TYP_GROUP_BROADCAST)
		{
			
			uint8_t group 	= msg.to[0];
			uint8_t cmd 		= msg.cmd[0];
			time_t now 	= time(NULL);
	 
			static	 struct {
				DeviceID deviceID;
				uint8_t group;
				uint8_t cmd;
				uint8_t cmd1;
				time_t  when;
			} last_message
			= { DeviceID(), 0, 0, 0};
			
			// Insteon often sends duplicate ON/OFF broadcast messages.
			if(deviceID == last_message.deviceID
				&& group == last_message.group
				&& cmd == last_message.cmd
				&& msg.cmd[1] == last_message.cmd1
				&& now < (last_message.when + 5) )
			{
				LOG_INFO("\tDUPLICATE MESSAGE:  - ignored \n");
				_plm._parser.reset();
				return true;
			}
	
			// ignore 06
			if((cmd == InsteonParser::CMD_INSTEON_06)
				
				// for broadcast - we expect the cmd 2 to be zero- else ignore it
			|| (msg.cmd[1] != 0))
			{
				LOG_INFO("\t MESSAGE 6:  - ignored \n");
				_plm._parser.reset();
				return true;
			}
		
			didHandle = _eventMgr.handleEvent(deviceID, group, cmd);
			
			if(!didHandle){
				switch (cmd) {
					case InsteonParser::CMD_SUCCESS_REPORT:
						// //  ignore?
						didHandle = true;
						break;
						
					case InsteonParser::CMD_LIGHT_ON:
						_db.setDBOnLevel(deviceID, group,  0xff);
						LOG_INFO("\tCMD_LIGHT_ON %s [%02X]\n", deviceID.string().c_str(),group);
						didHandle = true;
						break;
						
					case InsteonParser::CMD_LIGHT_OFF:
						_db.setDBOnLevel(deviceID, group, 0x0);
						LOG_INFO("\tCMD_LIGHT_OFF %s [%02X]\n", deviceID.string().c_str(),group);
						didHandle = true;
						break;
						
					case InsteonParser::CMD_START_MANUAL_CHANGE:
						//  ignore?
						didHandle = true;
						break;
						
					case InsteonParser::CMD_STOP_MANUAL_CHANGE:
						InsteonDevice(deviceID).getOnLevel([=](uint8_t level, bool didSucceed) {
							if(didSucceed){
								_db.setDBOnLevel(deviceID, group, level);
								LOG_INFO("\tDIM %s [%02X] %d\n", deviceID.string().c_str(),group, level);
							}
						});
						didHandle = true;
						break;
						
					default:
//						printf("-- (%02X) [%02x %02x] %s\n",
//								 msg.to[0], // group
//								 msg.cmd[0],
//								 msg.cmd[1],
//								 deviceID.string().c_str()) ;
						didHandle = true;
				}
				
				last_message = {deviceID, group,  msg.cmd[0], msg.cmd[1], now};
			}
			
			// fire off any events
			auto evt = EventTrigger(deviceID, group, cmd);
			auto eventIDs =  _db.matchingEventIDs(evt);
			if(eventIDs.size() > 0){
				// run these events.
				
				LOG_INFO("\tTRIGGER: %s \n",evt.printString().c_str());
				
				for (auto eventID : eventIDs) {
					// short delay to stablize Insteon bus
					sleep_ms(500);
					executeEvent(eventID);
				}
			}
			
			//				printf("-- (%02X) [%02x %02x] %s\n",
			//						 msg.to[0], // group
			//						 msg.cmd[0],
			//						 msg.cmd[1],
			//						 deviceID.string().c_str()) ;
			//
			// are there any keypads that match this?
			
			
			// is this a keypad button?
			if(auto keypad = _db.findKeypadEntryWithDeviceID(deviceID); keypad != NULL)
				if(auto button =  _db.findKeypadButton(keypad, group) ; button != NULL){
					
					// short delay to stablize isteon bus
					sleep_ms(500);
					
					// run button action
					runActionForKeypad(deviceID, group, cmd);
				}
		}
		
		if(didHandle){
			_plm._parser.reset();
			
		}
	}
	return didHandle;
}

// MARK: - idle processing


void InsteonMgr::idleLoop() {

	// limit this to once a minute
	
	static time_t lastRun = 0;

	time_t now = time(NULL);
	struct tm* tm = localtime(&now);
	time_t localNow  = (now + tm->tm_gmtoff);

	if(localNow > (lastRun + SECS_PER_MIN * 1))
	{
		lastRun = localNow;
		
		if(_state == STATE_READY) {
		
			static bool didReconcileEvents = false;

			// good place to check for events.
			solarTimes_t solar;
			if(ScheduleMgr::shared()->getSolarEvents(solar)){
				
				// combine any unrun events.
				if(!didReconcileEvents) {
					_db.reconcileEventGroups(solar, localNow);
				}
				
				auto eventIDs = _db.eventsThatNeedToRun(solar, localNow);
				
				for (auto eventID : eventIDs) {
					executeEvent(eventID, [=]( bool didSucceed) {
						_db.eventSetLastRunTime(eventID, localNow);
					});
				}
			}
		 
			if( _nextValidationCheck != 0 &&  (_nextValidationCheck < localNow)) {
				startDeviceValidation();
			}
			
		}
	}
 
	
};


// MARK: - PLM response
 
plm_result_t InsteonMgr::handleResponse(uint64_t timeout){
	
	plm_result_t result = PLR_NOTHING;
	
	if(!_hasPLM)
		return PLR_NOTHING;
	
		while(true) {
		
		bool didHandle = false;
		
		//	timeval now, diff;
		result = _plm.recvResponse(timeout);
		
		if(result == PLR_ERROR
			|| result == PLR_INVALID){
			return result;
		}
		
		// MARK: handle standard commands in queue
		if(_cmdQueue){
			didHandle = _cmdQueue->processPLMresponse(result);
			if(didHandle){
				break;
			}
		}
	
		// MARK: handle ALDB response
		if(_aldb) {
			didHandle = _aldb->processPLMresponse(result);
			if(didHandle){
				break;
			}
		}

		// MARK: handle device linking response
		if(_linking){
			didHandle = _linking->processPLMresponse(result);
			if(didHandle){
				break;
			}
		}
		
		// MARK: handle device validation
		if(_validator){
			didHandle = _validator->processPLMresponse(result);
			if(didHandle){
				break;
			}
		}
		// MARK: handle device broadcast events
		if(result == PLR_MSG){
			didHandle = processBroadcastEvents(result);
			if(didHandle) {
				break;
			}
		}
		
		
		// MARK: handle other stuff
		
		if(result == PLR_MSG){
			
			insteon_msg_t msg;
			
			if(_plm.parseMessage(&msg)){
				DeviceID deviceID = DeviceID(msg.from);
				
				switch(msg.msgType) {
						
					case MSG_TYP_BROADCAST:
						LOG_INFO("\tBROADCAST%s ",msg.ext?"-EXT":"");
						LOG_INFO("From %s ", deviceID.string().c_str());
						LOG_INFO("[%02X %02X %02X] ", 	msg.to[2],msg.to[1],msg.to[0]);
						LOG_INFO("CMD: %02X %02X \n", 	msg.cmd[0],msg.cmd[1]);
						break;
						
					case MSG_TYP_GROUP_BROADCAST:
						LOG_INFO("\tGROUP%s :%02x ",msg.ext?"-EXT":"", msg.to[0]);
						LOG_INFO("From %s ", deviceID.string().c_str());
						LOG_INFO("CMD: %02X %02X \n", 	msg.cmd[0],msg.cmd[1]);
						break;
						
					case MSG_TYP_DIRECT:
						LOG_INFO("\tMESSAGE%s ",msg.ext?"-EXT":"");
						LOG_INFO("From %s ", deviceID.string().c_str() );
						LOG_INFO("<%02X.%02X.%02X>  ", msg.to[2],msg.to[1],msg.to[0]);
						LOG_INFO("CMD: %02X %02X \n", 	msg.cmd[0],msg.cmd[1]);
						break;
						
					case MSG_TYP_GROUP_CLEANUP:
						LOG_INFO("\tCLEANUP%s ", 	  msg.ext?"-EXT":"");
						LOG_INFO("From %s ", deviceID.string().c_str());
						LOG_INFO("<%02X.%02X.%02X> GROUP:%02x  CMD:%02x\n",
									msg.to[2],msg.to[1],msg.to[0],
									msg.cmd[1], msg.cmd[0]) ;
						break;
						
					case MSG_TYP_DIRECT_ACK:
//						LOG_INFO("\tUNHANDLED ACK%s ", msg.ext?"-EXT":"");
//						LOG_INFO("From %s \"%s\" ", deviceID.string().c_str(), deviceID.name_cstr());
//						LOG_INFO("<%02X.%02X.%02X>  ",msg.to[2],msg.to[1],msg.to[0]);
//						LOG_INFO(" %02X %02X \n", 	msg.cmd[0],msg.cmd[1]);
 						break;
						
					default:
						
						LOG_INFO("\tOTHER%s ", msg.ext?"-EXT":"");
						LOG_INFO("From %s ", deviceID.string().c_str());
						LOG_INFO("(%d) <%02X.%02X.%02X>  ",msg.msgType,  msg.to[2],msg.to[1],msg.to[0]);
						LOG_INFO(" %02X %02X \n", 	msg.cmd[0],msg.cmd[1]);
				}
				
				_plm._parser.reset();
				break;
			}
		}
		// we got no response  -- not busy>?
		else if(result == PLR_NOTHING){
			idleLoop();
			break;
		}
		
		// waiting for more characters
		else if(result == PLR_CONTINUE){
			continue;
		}
		else
		{
			// throw it away!
			_plm._parser.reset();
			break;
			
		}
	}
	
	return result;
}
 

//bool  InsteonMgr::handleLinking(	deviceID_t devID, uint8_t groupID ){
//	
//
//	bool status = true;
//	
//	LOG_INFO("\t LINKING: Assign to Group %02X <%02X.%02X.%02X>\n",
//			 groupID, devID[2],devID[1],devID[0]);
//	
////		{
//// InsteonDeviceInfo* catDB = InsteonDeviceInfo::shared();
////			printf("Get Last Link ...");
////			insteon_linking_t lastLink;;
////			status =  _plm.getLastLink(&lastLink); CHK_STATUS;
////			printf("OK\n");
////			printf("\t LINKED (%s) : Group %02X <%02X.%02X.%02X>",
////					 (lastLink.flag & 0x40) == 0?"R":"C",
////					 lastLink.group,
////					 lastLink.dev_id[2],lastLink.dev_id[1],lastLink.dev_id[0]);
////
////			const char*	description = NULL;
////			insteon_cat_t* item = catDB->findInfoForDeviceType(lastLink.info[0],lastLink.info[1]);
////			if(item) description = item->description;
////
////			printf("  (%02X.%02X) %s rev:%02X",
////					 lastLink.info[0],lastLink.info[1],
////					 description?description:"Unknown",
////					 lastLink.info[2]);
////			printf("\n");
////
////		}
//	
//	
// // 		_state = STATE_READY;
// 
//	return status;
//}
//





//
//		if(0){
//
//			DeviceID keyPad = DeviceID("29.AA.F5");
//
// 			InsteonDevice(_cmdQueue).setLevel(keyPad, 255);
// 			InsteonDevice(_cmdQueue).setLevel(keyPad, 0);
// 			InsteonDevice(_cmdQueue).setLevel(keyPad, 255);
//
//			InsteonDevice(_cmdQueue).setLevel(keyPad, 0);
 
//			{
//				uint8_t buffer[] = {
//					0x00,0x00,0x00,0x00,};
//
//				START_VERBOSE;
// 				_cmdQueue->queueMessage(keyPad,
//												0x09, 0x01,
//												buffer, sizeof(buffer),
//												[this]( auto arg, bool didSucceed) {
//
//
//				});
//			}
//
				
		 
//			for(auto dev : _db.validDevices()){
//				InsteonDevice(_cmdQueue).setLevel(dev, 255);
//			}
	
//			for(auto dev : _db.validDevices()){
//				InsteonDevice(_cmdQueue).setLEDBrightness(dev, 11);
//			}
//			for(auto dev : _db.validDevices()){
//				InsteonDevice(_cmdQueue).setLEDBrightness(dev, 127);
//			}
			
		//	InsteonDevice(_cmdQueue).setLevel(DeviceID("33.4F.F6"), 0);
	
//			for(auto dev : _db.validDevices()){
//				InsteonDevice(_cmdQueue).setLevel(dev, 0);
//			}

//		}
		// DEBUG
		

/*
 
		if(0){
			
			bool status = false;
			
			if(!_linking){
				_linking = new InsteonLinking(_cmdQueue);
			}
			
			DeviceID device = DeviceID("57.2F.FA");

			LOG_INFO("\tLINKING %s %s\n",
						device.string().c_str(), 	device.nameString().c_str());

			status = _linking->linkDevice(device,  [this]( auto link) {
				
				switch(link.status){
						
					case InsteonLinking::LINK_SUCCESS:
					{
						LOG_INFO("\tLINKING COMPLETE(%02x) %s \"%s\" %s rev:%02X \n",
									link.groupID,
									link.deviceID.string().c_str(),
									link.deviceID.name_cstr(),
									link.deviceInfo.string().c_str(),
									link.deviceInfo.GetFirmware());
						
						_db.linkDevice(link.deviceID,
											true,	// is Controller
											link.groupID,
											link.deviceInfo,
											true);			// is validated
						
						_db.saveToCacheFile();
						
						_db.printDB();
					}
						break;
						
					case InsteonLinking::LINK_FAILED:
						LOG_INFO("\tLINKING FAILED\n");
						break;
						
					case InsteonLinking::LINK_TIMEOUT:
						LOG_INFO("\tLINKING LINK_TIMEOUT\n");
						break;
						
					case InsteonLinking::LINK_CANCELED:
						LOG_INFO("\tLINKING LINK_CANCELED\n");
						break;
						
					default:;
				}
				
			});
				

		}
		

 */

void InsteonMgr::updateLevels(){
	
	if(_state != STATE_READY)
		return;
	
	auto  devices = _db.validDevices();
	
 	if(devices.size() >  0){
		
		size_t* taskCount  = (size_t*) malloc(sizeof(size_t));
		*taskCount = devices.size();
		
		_state = STATE_UPDATING;

		LOGT_INFO("UPDATING LEVELS START");

		for(auto deviceID : devices) {
			
			InsteonDevice(deviceID).getOnLevel([=](uint8_t level, bool didSucceed) {
				
				auto devID = deviceID;
			
				if(didSucceed){
					_db.setDBOnLevel(devID, 0x01, level);
				}
			 
				if(--(*taskCount) == 0) {
					free(taskCount);
					_state = STATE_READY;

					LOGT_INFO("UPDATING LEVELS COMPLETE");

				}
			});
		}
	}

}

// debug test code


// MARK: - test code

#if 0

void InsteonMgr::test1(vector<DeviceID> devices,
							  boolCallback_t callback) {
	
	if(devices.size() == 0)
	{
		callback(true);
		return;
	}
	
	auto deviceID = devices.back();
	devices.pop_back();
//
//	printf("%2ld ALDB_READ: %s %s\n",
//			 devices.size(),
//			 deviceID.string().c_str(),
//			 deviceID.nameString().c_str());
	
	_aldb->readDeviceALDB(deviceID,
						 [this, deviceID, devices, callback]
						 ( std::vector<insteon_aldb_t> aldb,  bool didSucceed) {
		
		DeviceID	devID = deviceID;
//
//		printf("\t ALDB_READ: %s - %s\n",
//				 devID.string().c_str(),
//				 didSucceed?"OK":"FAIL");

		if(didSucceed){
			_db.updateDeviceALDB(devID, aldb);
			
	//		_db.printDeviceInfo(devID,true);
			
			_db.saveToCacheFile();
		}
	 
//		test1(devices,callback);
	});
}

void InsteonMgr::test() {
 
#if 0
	{
		auto keypad = InsteonKeypadDevice( DeviceID("33.4F.F6") );

		for(int j = 0; j < 10; j++ )
 			for(int i = 1; i < 9; i++ ){
			keypad.setKeypadLED(i,true);
			keypad.setKeypadLED(i,false);
		}
	}
#endif
#if 0
	{
 		START_VERBOSE;
		DeviceID keyPad = DeviceID("33.4F.F6");
 
		uint8_t buffer[] = {
			0x01, 0x08, 0xff, 0, 0,0, };
	
		
		_cmdQueue->queueMessage(keyPad,
										0x2E, 0x0,
										buffer, sizeof(buffer),
										[this]( auto arg, bool didSucceed) {
			
//			printf("\tCOMMAND(%02X, %02X)  %s\n",
//					 arg.reply.cmd[0],
//					 arg.reply.cmd[1],
//					 didSucceed?"OK":"FAIL");

			
		});

	}
#endif
	
#if 0
	{
		auto device = InsteonDevice(DeviceID("33.4F.F6"));
		
											 
		for(int i = 0; i < 8; i++ ){

			uint8_t level = (i&1 ? 0 : 0xff);

			device.setOnLevel(level);
			device.beep();
		}
	}
#endif
#if 0
	{
		DeviceID keyPad = DeviceID("33.4F.F6");
		
//		for(int j = 0; j < 100; j++)
		for(int i = 0; i < 8; i++ ){
			
			uint8_t mask = 1 << i;
			
			uint8_t buffer[] = {
				0x01, 0x09, mask};
			
			_cmdQueue->queueMessage(keyPad,
											0x2E, 0x00,
											buffer, sizeof(buffer),
											[=]( auto arg, bool didSucceed) {
				
				
			});
		}
		
	}
	return;
#endif
	
	if(!_aldb)
		throw InsteonException("aldb not setup");
	
	
//	START_VERBOSE;
//
//	InsteonDevice(_cmdQueue).beep(DeviceID("2E.A1.12"),
//											[this]( bool didSucceed) {
//
//
//		printf("\tBEEP %s\n", didSucceed?"":"FAIL");
//	});

//
//	return;
//	for(auto dev : _db.validDevices()){
//
//		InsteonDevice(_cmdQueue).setOnLevel(dev, 0);
//
//	};
	
	

#if  0
	
	/*
	 
	 ## Kara's Room - ICON On/Off Switch (25 max links)
	 1B.3F.74 40 FE 02 16 39 01 2021-03-02 18:34:17
	 1B.3F.74 0FFF A2 FE 52.18.42 00 00
	 1B.3F.74 0FF7 E2 01 52.18.42 03 20
	 
	 ## LampLinc 1 - LampLinc (Dual-Band)
	 2E.A1.12 40 FF 01 0E 43 02 2021-03-02 18:34:17
	 2E.A1.12 0FFF A2 FE 42.18.52 00 00
	 2E.A1.12 0FF7 A2 FE 52.18.42 00 00

	*/
	insteon_dbEntry_t  info;

	if(_db.getDeviceInfo(DeviceID("33.4F.F6"), &info)){

			_aldb->syncDeviceALDB(info.deviceID, info.deviceALDB,
										 [](bool didSucceed){

//		  printf("Sync %s\n", didSucceed?"OK":"FAIL");

		});
 	}
#endif
	
#if 1
	
 //	vector< DeviceID> devices = {DeviceID("54.F5.2D")};
  	auto devices = _db.devicesThatNeedALDB();

	LOG_INFO("\n\tALDB UPDATE NEEDED for %d devices\n",  devices.size() );

//	test1(devices, [](bool didSucceed){

		LOG_INFO("\tALDB UPDATE COMPLETE\n\n" );

	});

#endif
//
//	if(_db.getDeviceInfo(DeviceID("2E.A1.12"), &info)){
//
//			_aldb->syncDeviceALDB(info.deviceID, info.deviceALDB,
//										 [](bool didSucceed){
//
//		  printf("Sync %s\n", didSucceed?"OK":"FAIL");
//
//		});
//	}


//
// 	for(auto device :_db.validDevices()){
//		_db.clearDeviceALDB(device);
//	}
//	_db.printDB(true);
//	_db.saveToCacheFile();
	
//	START_VERBOSE;
//
//	vector<DeviceID> devices  = { DeviceID("1B.3F.74") };
//
//	LOG_INFO("\n\tALDB UPDATE NEEDED for %d devices\n",  devices.size() );
//
//	test1(devices, [](bool didSucceed){
//
//		LOG_INFO("\tALDB UPDATE COMPLETE\n\n" );
//
//	});

	

//	return;


//
// 	return;
//
//	START_VERBOSE;
//	for(auto deviceID : _db.validDevices()) {
//
//		InsteonDevice(deviceID).beep([=]( bool didSucceed) {
//
//			auto devID = deviceID;
//
//			printf("\tBEEP %s %s %s\n", didSucceed?"":"FAIL",
//					 devID.string().c_str(),
//					 devID.name_cstr() );
//		});
//
// 	}
//
//	return;
//
//	DeviceID keyPad = DeviceID("33.4F.F6");
	
	
//	for(int i = 1; i < 9; i++ ){
		
//		uint8_t mask = 1 << i;
//
//		uint8_t buffer[] = {
//			0x00, 0x00, 0};
//
//		_cmdQueue->queueMessage(keyPad,
//										0x2E, 0x00,
//										buffer, sizeof(buffer),
//										[this]( auto arg, bool didSucceed) {
//
//
//		});
////	}
	
	
}

#endif
