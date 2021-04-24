//
//  InsteonParser.cpp
//  test9
//
//  Created by Vincent Moscaritolo on 1/6/21.
//

#include "InsteonParser.hpp"

#define ALLOC_QUANTUM 32
#include "LogMgr.hpp"

#define XXH_STATIC_LINKING_ONLY   /* *_state_t */

#include "xxhash.h"

 
InsteonParser::InsteonParser() {
	dbuff_init();
}
 
InsteonParser::~InsteonParser(){
	
	if(dbuf.data)
		free(dbuf.data);
	
	dbuf.used = 0;
	dbuf.alloc = 0;
	dbuf.dataLen = 0;
}


//MARK: - buffer management

void InsteonParser::dbuff_init(){
	
	_state = INS_READY;
	dbuf.used = 0;
	dbuf.expect = 0;

	dbuf.dataLen = 0;
	dbuf.alloc = ALLOC_QUANTUM;
	dbuf.data =  (uint8_t*) malloc(ALLOC_QUANTUM);
}
 

bool InsteonParser::dbuff_cmp(const char* str){
	if(!str) return
		false;
	
	size_t len =  strlen(str);
	
	if(dbuf.used < len)
		return false;
	
	return (memcmp(dbuf.data, str, len) == 0);
};


bool InsteonParser::dbuff_cmp_end(const char* str){
	if(!str) return
		false;
	
	size_t len =  strlen(str);
	
	if(dbuf.used < len)
		return false;
	
	return (memcmp(dbuf.data+dbuf.used-len, str, len) == 0);
};


void  InsteonParser::reset(){
	_state = INS_READY;
	dbuf.expect = 0;
	dbuf.used = 0;
	dbuf.dataLen = 0;
}


size_t InsteonParser::data_size(){
  return dbuf.used;
};


bool InsteonParser::append_data(void* data, size_t len){
  
  if(len + dbuf.used >  dbuf.alloc){
	  size_t newSize = len + dbuf.used + ALLOC_QUANTUM;
	  
	  if( (dbuf.data = (uint8_t*) realloc(dbuf.data,newSize)) == NULL)
		  return false;
	  
	  dbuf.alloc = newSize;
  }
  
  memcpy((void*) (dbuf.data + dbuf.used), data, len);
  dbuf.used += len;
  dbuf.dataLen = dbuf.used;

  return true;
}

bool InsteonParser::append_char(uint8_t c){
  return append_data(&c, 1);
}



void InsteonParser::dumpBuf()
{
	int length = 	(int)dbuf.used;
	const uint8_t	  *bufferPtr = dbuf.data;
 
	TRACE_DATA_IN(bufferPtr, length);
}
 
//MARK: - state machine


plm_result_t InsteonParser::process_char( uint8_t ch){
  
	plm_result_t retval = PLR_CONTINUE;

//	{
//		static int count = 0;
//		
//// 		printf("%3i _state %u %02x \n",count++, _state,  ch );
//
//	}
  
  if(!dbuf.data  || dbuf.alloc== 0 ){
	  dbuff_init();
  }
 
	switch (_state) {

		case INS_READY:
			if(ch == STX){
				_state = INS_STX;
			} else {
				_state = INS_INVALID;
				retval = PLR_INVALID;
 			}
			break;
			
		case INS_STX:
			append_char(ch);

			if(ch == MSG_IN_STD){
				_state = INS_MSG;
				dbuf.expect = 10;
			}
			else if(ch == MSG_OUT){
				_state = INS_MSG_OUT;
				dbuf.expect = 8;
			}
			else if(ch == MSG_IN_EXT){
				_state = INS_MSGEXT;
				dbuf.expect = 24;
			}
			else if(ch == IM_INFO){
				_state = INS_INFO;
				dbuf.expect = 8;
			}
			else if(ch == IM_ALDB_GETFIRST){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_ALDB_GETNEXT){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_ALDB_RESPONSE){
				_state = INS_ALDB_RESPONSE;
				dbuf.expect = 9;
			}
			else if(ch == IM_RESET){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_GET_CONFIG){
				_state = INS_CMD;
				dbuf.expect = 5;
			}
			else if(ch == IM_SET_CONFIG){
				_state = INS_CMD;
				dbuf.expect = 3;
			}
			else if((ch == IM_LED_ON) || (ch == IM_LED_OFF)){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_START_LINKING){
				_state = INS_CMD;
				dbuf.expect = 4;
			}
			else if(ch == IM_CANCEL_LINKING){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_GET_LAST_LINK){
				_state = INS_CMD;
				dbuf.expect = 2;
			}
			else if(ch == IM_LINKING_COMPLETED){
				_state = INS_LINKING;
				dbuf.expect = 9;
			}
			else if(ch == IM_ALDB_MANAGE){
				_state = INS_CMD;
				dbuf.expect = 11;
			}
			else
			{
				_state = INS_INVALID;
				retval = PLR_INVALID;
			}
			break;

		default:
			if(dbuf.used >=  dbuf.expect){   /// overflow
				this->reset();
				_state = INS_INVALID;
				retval = PLR_INVALID;
			}
			else { 	// append byte
				append_char( ch);
				
				if(_state == INS_MSG_OUT && dbuf.used == 5) {
					if((ch & 0x10) == 0x10) {
						dbuf.expect += 14;
					}
				}
			}

			// are we done?
			if( dbuf.used == dbuf.expect){
				
				switch(_state){
					case INS_MSG_OUT:
						retval = PLR_MSG_OUT;
						break;
						
					case INS_CMD:
						retval = PLR_CMD;
						break;
						
					case INS_LINKING:
						retval = PLR_LINKING_COMPLETED;
						break;
	 
					case INS_INFO:
						retval = PLR_INFO;
						break;
						
					case INS_MSG:
					case INS_MSGEXT:
						retval = PLR_MSG;
						break;
						
					case INS_ALDB_RESPONSE:
						retval = PLR_ALDB_RESPONSE;
						break;
	 
					default:
						_state = INS_INVALID;
						retval = PLR_INVALID;
						break;
				}
				
			}
			break;
	}
 
	return retval;
}

// MARK: - Format buffer

 
std::array<uint8_t,14> InsteonParser::makeALDBWriteRecord(insteon_aldb_t aldb, uint16_t address){
	std::array<uint8_t,14> buffer;

	//ALDB Write Record:
	  // D1 unused,
	  // D2 0x02,
	  // D3-D4 address,
	  // D5 number of bytes (0x01-0x08),
	  // D6-D13 data to write

	uint8_t *p = buffer.data();
	 	
	*p++ = 0;
	*p++ = 0x02;
	*p++ =  address >> 8;
	*p++ =  address &0xFF;
	
	*p++ = 8;		// length
	
	if(aldb.flag == 0) {
		*p++ = 0;			// high water mark
	}
	else {
		bool isCNTL = (aldb.flag & 0x40) == 0x40;
		*p++ = isCNTL? 0xC2:0xA2;
	}
	
	*p++ = aldb.group;
	
	*p++ = aldb.devID[2];
	*p++ = aldb.devID[1];
	*p++ = aldb.devID[0];

	*p++ = aldb.info[0];
	*p++ = aldb.info[1];
	*p++ = aldb.info[2];
	
	return buffer;
}


//MARK: - Get Data from buffer


bool InsteonParser::get_info(insteon_info_t *info){
	
	if( !(_state == INS_INFO )
		||  dbuf.used != dbuf.expect)
		return false;

	if(info){
		uint8_t *p = dbuf.data;
		p++;  // skip the command.
		info->devID[2] = *p++;
		info->devID[1] = *p++;
		info->devID[0] = *p++;
		info->cat 	= *p++;
		info->subcat	= *p++;
		info->firmware	= *p++;
		info->ack = *p++;
	}
	
	this->reset();
	return true;
}

bool InsteonParser::get_cmd(insteon_cmd_t *cmd){
	
	if( !(_state == INS_CMD )
		||  dbuf.used != dbuf.expect)
		return false;

	if(cmd){
		uint8_t *p = dbuf.data;
		cmd->cmd = *p++;
		
		for(size_t i = 0; i < dbuf.used -2; i++){
			cmd->params[i] 	= *p++;
		}
		cmd->ack = *p++;
	}
	
	this->reset();
	return true;
}

bool InsteonParser::parse_cmd(insteon_cmd_t *cmd){
	if( !(_state == INS_CMD || _state == INS_INFO )
		||  dbuf.used != dbuf.expect)
		return false;

	if(cmd){
		uint8_t *p = dbuf.data;
		cmd->cmd = *p++;
		
		cmd->paramLen = dbuf.used -2;
		for(size_t i = 0; i < cmd->paramLen; i++){
			cmd->params[i] 	= *p++;
		}
	
		cmd->ack = *p++;
	}
	
 	return true;
}

bool InsteonParser::parse_linking(insteon_linking_t *data){
  
  if( !(_state == INS_LINKING)
	  ||  dbuf.used != dbuf.expect)
	  return false;
  
	if(data){
		uint8_t *p = dbuf.data;
		
		if(_state == INS_LINKING){
			if(*p++ != IM_LINKING_COMPLETED)
				return false;
		}
		
		data->flag = *p++;
		data->group = *p++;

		data->dev_id[2] = *p++;
		data->dev_id[1] = *p++;
		data->dev_id[0] = *p++;

		data->info[0] = *p++;
		data->info[1] = *p++;
		data->info[2] = *p++;
	}
	
	return true;
}

bool InsteonParser::parse_aldb(insteon_aldb_t *data){
	
	if( (_state != INS_ALDB_RESPONSE)
		||  dbuf.used != dbuf.expect)
		return false;
	
	 if(data){
		 uint8_t *p = dbuf.data;
		 
		 p++; // skip command reply
	 
		 data->flag = *p++;
		 data->group = *p++;

		 data->devID[2] = *p++;
		 data->devID[1] = *p++;
		 data->devID[0] = *p++;

		 data->info[0] = *p++;
		 data->info[1] = *p++;
		 data->info[2] = *p++;
		 
 		 data->address = 0;	// not used
	 }
 
	 return true;
}

// used for parsing respose from extended message  CMD_READ_ALDB

bool InsteonParser::parse_aldb_remote(insteon_aldb_t *aldb){
	
	 	if( (_state != INS_MSGEXT)
		||  dbuf.used != dbuf.expect)
		return false;

	uint8_t *p = dbuf.data;
	p+=8;  // skip to command.

	if(*p++ != CMD_READ_ALDB)
		return false;

	p++; // skip cmd2
	p++; // skip unused
	
	if(*p++ != 0x01)	// expect a 1
		return false;

	aldb->address = (*p++ << 8) ;
	aldb->address |= (*p++);
	
	p++; // skip unused

	aldb->flag = *p++;
	aldb->group = *p++;

	aldb->devID[2] = *p++;
	aldb->devID[1] = *p++;
	aldb->devID[0] = *p++;
 
	aldb->info[0] = *p++;
	aldb->info[1] = *p++;
	aldb->info[2] = *p++;
	
	return true;
}


bool InsteonParser::parse_msg(insteon_msg_t *msg){
	
	if( !(_state == INS_MSG || _state== INS_MSGEXT)
		||  dbuf.used != dbuf.expect)
		return false;
	if(msg){
		
		msg->ext =  _state== INS_MSGEXT;
		uint8_t *p = dbuf.data;
		p++;  // skip te command.
		
		msg->from[2] = *p++;
		msg->from[1] = *p++;
		msg->from[0] = *p++;
		msg->to[2] 	= *p++;
		msg->to[1] 	= *p++;
		msg->to[0] 	= *p++;
		msg->flag 	= *p++;
		msg->msgType = msgTypForFlag(msg->flag);
	 
		msg->cmd[0] 	= *p++;
		msg->cmd[1] 	= *p++;

		if( _state == INS_MSGEXT ){
			for(int i = 0; i < 13; i++){
				msg->data[i] 	= *p++;
			}
		}
		
	}
 	return true;
}


bool InsteonParser::parse_msg_out(insteon_msg_t *msg){
	
	if(  _state != INS_MSG_OUT
		||  dbuf.used != dbuf.expect)
		return false;
	if(msg){
		
		bzero(msg, sizeof(insteon_msg_t));
	 	uint8_t *p = dbuf.data;
		p++;
		
		msg->from[2] = 0;
		msg->from[1] = 0;
		msg->from[0] = 0;
		
		msg->to[2] 	= *p++;
		msg->to[1] 	= *p++;
		msg->to[0] 	= *p++;
		
		msg->flag 	= *p++;
		msg->msgType = (plm_msgType_t)((msg->flag >> 5)& 7);
		msg->ext = ((msg->flag & 0x10) == 0x10);
		
		msg->cmd[0] 	= *p++;
		msg->cmd[1] 	= *p++;

		if( msg->ext ){
			for(int i = 0; i < 14; i++){
				msg->data[i] 	= *p++;
			}
		}
		
		msg->ack = *p++;
		makeMsgHash(msg);
	}
	return true;
}

// MARK: utilities

extern "C" {

/*
 
 uint8_t buffer[] = {
 	 0x2E, 0x00, 0x00, 0x07, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
 
 uint8_t checksum =  makeCheckSum( buffer, sizeof(buffer));
 
 checksum == 0x4C
 
 */


void makeMsgChecksum(insteon_msg_t *msg){
 
	// only do this for extended messages
	if((msg->flag & 0x10) == 0)
		return ;
 
	u_int8_t sum = 0;
	sum  += (msg->cmd[0] + msg->cmd[1]);
	
	for(int i = 0; i < 13; ++i)
 		 sum += msg->data[i];
	
	sum = (~sum) + 1 ;
	
	msg->data[13] = sum;
}

uint32_t makeMsgHash(insteon_msg_t* msg){
	
	XXH32_state_t hashState;
	XXH32_reset(&hashState, 2654435761U);
	
	XXH32_update(&hashState, &msg->to, sizeof(msg->to) );
//	XXH32_update(&hashState, &msg->from, sizeof(msg->from) );
	XXH32_update(&hashState, &msg->flag, sizeof(msg->flag) );
	XXH32_update(&hashState, &msg->cmd, sizeof(msg->cmd) );
	if( ((msg->flag & 0x10) == 0x10)){
		XXH32_update(&hashState, &msg->data, sizeof(msg->data) );
	}
	return XXH32_digest(&hashState);
}

bool str_to_deviceID(const char* str, deviceID_t devIDOut){
	
	bool status = false;
	
	deviceID_t val = {0,0,0};
 
	status = sscanf(str, "%hhx.%hhx.%hhx", &val[2],&val[1],&val[0]) == 3;
	
	if(devIDOut)  {
		copyDevID(val,devIDOut);
	}
	
	return status;
}
 
} /* Extern c*/
 
