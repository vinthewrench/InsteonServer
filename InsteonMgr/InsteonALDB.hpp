//
//  InsteonALDB.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 1/26/21.
//

#ifndef InsteonALDB_hpp
#define InsteonALDB_hpp

#include <stdlib.h>
#include <list>
#include <mutex>
#include "InsteonMgrDefs.hpp"

#include "DeviceID.hpp"
#include "InsteonPLM.hpp"


/**
 * @brief InsteonALDB is a helper class to the cmdQueue
 * it is used to process the PLR_ALDB_RESPONSE and convert them into a list the db can read.
 *
 *  *
 */

class InsteonCmdQueue;

 class InsteonALDB {
 
public:
 
	 typedef std::function<void(std::vector<insteon_aldb_t>)> aldbCallback_t;
	 typedef std::function<void(bool)> aldbWriteCallback_t;

	 InsteonALDB(InsteonCmdQueue* _cmdQueue);
	~InsteonALDB();
 
 	 bool readPLM(aldbCallback_t callback);
	 bool processPLMresponse(plm_result_t response);
 
	 bool create(DeviceID deviceID, bool isCNTL, uint8_t group, DeviceInfo info,
	 			boolCallback_t callback = NULL);

	 bool remove(DeviceID deviceID, bool isCNTL, uint8_t group,
	 		boolCallback_t callback = NULL);
	
	 typedef std::function<void(std::vector<insteon_aldb_t> db, bool didSucceed)> readALDBCallback_t;

	 bool readDeviceALDB(DeviceID deviceID, readALDBCallback_t callback);

	 bool syncDeviceALDB(DeviceID deviceID, std::vector<insteon_aldb_t> aldbIn,
								aldbWriteCallback_t callback = NULL);

	 bool addToDeviceALDB(DeviceID 		deviceID,
								 bool  		isCNTL,
								 uint8_t 		groupID,
								 u_int8_t* 	data = NULL,
								 std::function<void(const insteon_aldb_t* newAldb, bool didSucceed)> callback = NULL);
	 
//	 bool removeFromDeviceALDB(DeviceID deviceID, uint8_t groupID, boolCallback_t callback);


private:
	 mutable std::mutex _mutex;

	 typedef struct  {
		 uint8_t					id;
		 DeviceID 				deviceID;
		 uint8_t					ack;			// we got a send ACK from PLM.
		 timeval					time;			// time we sent it.. or last time we NAK
		 readALDBCallback_t 	callback;
		 
		 std::vector<insteon_aldb_t> db;
	 }readALDBreply_t;

	 
	 typedef struct  {
		 uint8_t			ack;			// we got a send ACK from PLM.
		 std::array<uint8_t,14> buffer;
	 }	 writeALDBData_t;
	 
	 typedef struct  {
		 uint8_t					id;
		 DeviceID 				deviceID;
		 timeval					time;			// time we sent it.. or last time we NAK
		 aldbWriteCallback_t 	callback;
 		 std::vector<writeALDBData_t> aldbData;
		 }writeALDBEntry_t;

	 // write remote ALDB
	 std::list<writeALDBEntry_t> _writeALDBQueue;
 	 void processNextWrite();
	 InsteonALDB::writeALDBEntry_t*  getNextAvailWriteEntry();
	 InsteonALDB::writeALDBData_t*   getNextAvailWriteBuffer(writeALDBEntry_t*);
 
	 // reading remote ALDB
	 InsteonALDB::readALDBreply_t* findReplyWithDeviceID(DeviceID deviceID);
	 InsteonALDB::readALDBreply_t* findReplyWithID(uint8_t id);
	 std::list<readALDBreply_t> 	_aldbQueue;
	 bool								_isReadingPLM;
	 bool								_isAccesingRemoteALDB;
	 timeval							_startTime;		 	// used for timeouts (IR REQ/Linkink)
	 std::vector<insteon_aldb_t> _plmDB;
	 aldbCallback_t 					_readALDBcallback;
	 
	 std::list<uint8_t>			 	getExpiredIDs();
	 void continueReadPLM();
	//-
	 
	 InsteonCmdQueue*	_cmdQueue;
 	 InsteonPLM*	 		_plm;
	 uint8_t				_entryCnt;		 // auto increment
 
	 uint64_t  			_timeout_ALDB_RESPONSE;		// how long to wait for ALDB_RESPONSE
	 uint64_t				_timeout_CMD_READ_ALDB;		// how long to wait for _CMD_READ_ALDB;
  };

#endif /* InsteonALDB_hpp */
