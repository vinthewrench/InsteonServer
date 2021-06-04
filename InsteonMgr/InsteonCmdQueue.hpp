//
//  InsteonCmdQueue.hpp
//  plmtest
//
//  Created by Vincent Moscaritolo on 2/2/21.
//

#ifndef InsteonCmdQueue_hpp
#define InsteonCmdQueue_hpp

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <list>
#include <mutex>

#include "InsteonMgrDefs.hpp"
#include "InsteonParser.hpp"
#include "InsteonPLM.hpp"
#include "DeviceID.hpp"
#include "DeviceInfo.hpp"
#include "InsteonException.hpp"

class InsteonDB;
class InsteonALDB;
class InsteonLinking;
class InsteonValidator;

class InsteonCmdQueue{
 
	friend InsteonALDB;			// needs access to plm -
	friend InsteonLinking;		// needs to access PLM
	friend InsteonValidator;
	
public:
		static InsteonCmdQueue *shared() {
		if(!sharedInstance)
			throw InsteonException("InsteonCmdQueue not setup");
 
		return sharedInstance;
	}

	typedef struct  {
		insteon_msg_t send;
		insteon_msg_t reply;		// reply only valid if succeed.
	} msgReply_t;
	
	typedef struct  {
		uint8_t 		cmd;
		uint8_t		ack;

		union {
			struct  {
				uint8_t		params[32];
				size_t			paramLen;
			}	raw;
			struct  {
				deviceID_t	devID;
				uint8_t		cat;
				uint8_t		subcat;
				uint8_t		firmware;
			}	info;
	
		};
	}cmdReply_t;
 
	typedef std::function<void(msgReply_t replyBlock, bool didSucceed)> msgCallback_t;
	typedef std::function<void(cmdReply_t replyBlock, bool didSucceed)> cmdCallback_t;
	
	InsteonCmdQueue(InsteonPLM* plm, InsteonDB* db);
	~InsteonCmdQueue();
	
	void abort();		// abprt all queued commands
	
	bool isConnected() { return _plm->isConnected();};
	
	bool processPLMresponse(plm_result_t response);
	
	void queueMessage(DeviceID deviceID,
							uint8_t command,
							uint8_t arg,
							uint8_t* data = NULL,
							size_t 	datLen = 0,
							msgCallback_t callback = NULL
							);

	void queueMessage(DeviceID deviceID,
							uint8_t command,
							uint8_t arg,
							msgCallback_t callback ) {
							return queueMessage(deviceID,command, arg, NULL, 0,callback);
	}
	
	void queueMessageToGroup(	uint8_t command,
									 uint8_t arg,
									 uint8_t group,
									 msgCallback_t callback = NULL );

	void queueCommand(uint8_t command,
							const uint8_t* paramBuf,
							size_t paramBufLen,
							cmdCallback_t callback
							);

	
 private:
	
	static InsteonCmdQueue *sharedInstance;
	
	mutable std::mutex _mutex;

	typedef enum {
		CMD_ENTRY_INVALID = 0,
		CMD_ENTRY_CMD,
		CMD_ENTRY_MSG,
		CMD_ENTRY_MSG_GROUP,

	}cmd_entry_typ;
	
	typedef struct  {
		cmd_entry_typ 		typ;
		uint8_t				id;
		
		uint8_t				sendCount;		// for tracking how many times we sent
		bool					sendACK;			// we got a send ACK from PLM.
		timeval				time;				// time we sent it.. or last time we NAK

		// I would rather make these a union, but C++ throws up on them.
		// used for messages
		insteon_msg_t 	send;
		uint32_t			hash;				// caclulated XXHash32 with makeMsgHash
		msgCallback_t 	msgCallback;
		
 		// used for commands
 		uint8_t 			command;
 		uint8_t 			params[32];
 		size_t 			paramLen;
		cmdCallback_t 	cmdCallback;
	} cmd_entry_t;
	
	std::list<cmd_entry_t> _cmdQueue;
	cmd_entry_t* 			getEntryWithHash(uint32_t hash);
	cmd_entry_t* 			getEntryWithDevice(DeviceID device);
	cmd_entry_t* 			getEntryWithCommand(uint8_t command);

	cmd_entry_t* 			getNextAvailEntry();
	std::list<uint32_t> 	getExpiredHashs();
	
	void performCompletion(cmd_entry_t* entry,
								  bool didSucceed,
								  void* reply = NULL);

	bool processEntry( cmd_entry_t* entry);
	
	bool processNext(bool withDelay);

	void processTimeout() ;
	
	uint8_t				_entryCnt;
	uint8_t				_maxSendRetry;
	uint64_t				_timeout_Send;
	
	InsteonPLM*	 		_plm;
	InsteonDB* 	 		_db;
	
};

#endif /* InsteonCmdQueue_hpp */
