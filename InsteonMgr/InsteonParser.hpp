//
//  InsteonParser.hpppp
//  test9
//
//  Created by Vincent Moscaritolo on 1/6/21.
//

#ifndef InsteonParser_hpp
#define InsteonParser_hpp
 
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <array>

typedef uint8_t deviceID_t[3];
typedef  unsigned short groupID_t;

typedef enum {
	PLR_INVALID = 0,
	PLR_OK,
	PLR_ERROR,
	PLR_FAIL,
	 
	PLR_INFO,
	PLR_MSG,
	PLR_CMD,

	PLR_MSG_OUT,

 	PLR_ALDB_RESPONSE,
 
	PLR_LINKING_COMPLETED,
 
	PLR_CONTINUE,
	
	PLR_NOTHING,		// we never return this.  use it for Stream->available() == 0

}plm_result_t;

typedef enum {
	MSG_TYP_BROADCAST	 	= 4,
	MSG_TYP_DIRECT 		= 0,
	MSG_TYP_DIRECT_ACK 	= 1,
	MSG_TYP_DIRECT_NAK 	= 5,
	MSG_TYP_GROUP_BROADCAST 	= 6,
	MSG_TYP_GROUP_CLEANUP	 	= 2,
	MSG_TYP_GROUP_CLEANUP_ACK 	= 3,
	MSG_TYP_GROUP_CLEANUP_NAK 	= 7,

}plm_msgType_t;


typedef struct  {
	bool			ext;			// is extended
	deviceID_t 	from;		// for incomming only
	deviceID_t 	to;
	uint8_t		flag;
plm_msgType_t	msgType;		// message type (flag bits 7-5)

	uint8_t		cmd[2];
	uint8_t		data[14];
	uint8_t		ack;		// for outgoing message
}insteon_msg_t;

typedef struct  {
 	uint8_t 		cmd;
	uint8_t		params[32];
	size_t			paramLen;
	uint8_t		ack;
}insteon_cmd_t;
 
typedef struct  {
	deviceID_t 	devID;
	uint8_t		cat;
	uint8_t		subcat;
	uint8_t		firmware;
	uint8_t		ack;
}insteon_info_t;

typedef struct  {
	uint8_t 		flag;
	uint8_t		group;
	deviceID_t	dev_id;
	uint8_t		info[3];
}insteon_linking_t;

typedef struct  {
	uint8_t		flag;
	uint8_t		group;
	deviceID_t 	devID;
	uint8_t		info[3];
	
 	uint16_t		address; // for remote devices
}insteon_aldb_t;


extern "C" {


	constexpr void copyDevID( const deviceID_t fromDev, deviceID_t toDev ) {
		toDev[2] = fromDev[2];
		toDev[1] = fromDev[1];
		toDev[0] = fromDev[0];
	}

	constexpr bool cmpDevID( const deviceID_t dev1, const deviceID_t dev2 ) {
		return( dev1[2] == dev2[2] && dev1[1] == dev2[1]  && dev1[0] == dev2[0]  );
	}

	bool str_to_deviceID(const char* str, deviceID_t devID);
	bool str_to_GroupID(const char* str, groupID_t *groupIDOut);
	bool validateDeviceID( const std::string arg);

	// caclulated XXHash32(to,flag,cmd,[(optional)data])
	uint32_t makeMsgHash(insteon_msg_t *msg);

	void makeMsgChecksum(insteon_msg_t* msg);
 
}


class InsteonParser {

 public:

	static const uint8_t  STX				= 0x02;
	static const uint8_t  ACK	 			= 0x06;
	static const uint8_t  NAK	 			= 0x15;

	static const uint8_t 	IM_INFO 						= 0x60;
	static const uint8_t 	IM_GET_CONFIG 				= 0x73;
	static const uint8_t 	IM_SET_CONFIG 				= 0x6B;

	static const uint8_t 	IM_CONFIG_FLAG_AUTOLINK  	= 0x80;
	static const uint8_t 	IM_CONFIG_FLAG_MONITOR   	= 0x40;
	static const uint8_t 	IM_CONFIG_FLAG_LED  	  	= 0x20;
	static const uint8_t 	IM_CONFIG_FLAG_DEADMAN	  	= 0x10;

	static const uint8_t 	IM_LED_ON 					= 0x6D;
	static const uint8_t 	IM_LED_OFF 					= 0x6E;
 
	static const uint8_t 	IM_RESET						= 0x67;
 
	static const uint8_t 	IM_START_LINKING 			= 0x64;
	static const uint8_t 	IM_CANCEL_LINKING 			= 0x65;
	static const uint8_t 	IM_LINK_FLAG_RESPONDER	  	= 0x00;
	static const uint8_t 	IM_LINK_FLAG_CONTROLLER	= 0x01;
	static const uint8_t 	IM_LINK_FLAG_ALL				= 0x03;
	static const uint8_t 	IM_LINK_FLAG_DELETE			= 0xFF;

	static const uint8_t 	IM_LINKING_COMPLETED 		= 0x53;
 
	static const uint8_t 	IM_GET_LAST_LINK 			= 0x6C;
 
	
	static const uint8_t  MSG_IN_STD	 				= 0x50;
	static const uint8_t  MSG_IN_EXT	 				= 0x51;
 
	static const uint8_t  MSG_OUT		 			= 0x62;

	 
	static const uint8_t	IM_ALDB_GETFIRST			= 0x69;
	static const uint8_t	IM_ALDB_GETNEXT			= 0x6A;
	static const uint8_t	IM_ALDB_RESPONSE 		= 0x57;
	static const uint8_t	IM_ALDB_MANAGE	 		= 0x6F;
 
	
	// commands found in message
	static const uint8_t 	CMD_ASSIGN_TO_GROUP 	= 0x01;
	static const uint8_t 	CMD_ID_REQUEST		 	= 0x10;
	static const uint8_t 	CMD_PING			 		= 0x0f;

	static const uint8_t 	CMD_EXT_SET_GET		= 0x2E;
	static const uint8_t 	CMD_LIGHT_ON				= 0x11;
	static const uint8_t 	CMD_FAST_ON				= 0x12;
	static const uint8_t 	CMD_LIGHT_OFF			= 0x13;
	static const uint8_t 	CMD_FAST_OFF				= 0x14;
	static const uint8_t 	CMD_GET_INSTEON_VERSION = 0x0D;
	static const uint8_t 	CMD_GET_ON_LEVEL 		= 0x19;
	static const uint8_t 	CMD_BEEP			 		= 0x30;
	static const uint8_t	CMD_ENTER_LINK_MODE		= 0x09;
	static const uint8_t	CMD_GET_OPERATING_FLAGS	= 0x1F;
	static const uint8_t	CMD_SET_OPERATING_FLAGS	= 0x20;

	static const uint8_t	CMD_INSTEON_06				= 0x06;  // I dont know what this ism, but ignore
	
	static const uint8_t 	CMD_START_MANUAL_CHANGE	= 0x17;
	static const uint8_t 	CMD_STOP_MANUAL_CHANGE		= 0x18;
	static const uint8_t 	CMD_READ_ALDB				= 0x2F;

	static const uint8_t 	CMD_SUCCESS_REPORT			= 0x06; //Sent immediately following a group broadcast

	static plm_msgType_t msgTypForFlag(uint8_t flag) { return (plm_msgType_t)((flag >> 5)& 7); };


	typedef enum  {
		INS_UNKNOWN = 0,
		
		INS_READY ,
		INS_STX,
		
		INS_MSG,
		INS_MSGEXT,
		INS_INFO,
		
		INS_CMD,
		INS_LINKING,
	
		INS_ALDB_RESPONSE,
		INS_RESET,

		INS_INVALID,
		INS_MSG_OUT,
	 
	}in_state_t;


	InsteonParser();
	~InsteonParser();
	
	void  reset();
	
	plm_result_t process_char(uint8_t ch);

	bool parse_msg(insteon_msg_t *msg);
	bool parse_msg_out(insteon_msg_t *msg);

	bool parse_aldb(insteon_aldb_t *aldb);
	
	bool parse_aldb_remote(insteon_aldb_t *aldb);

	bool parse_linking(insteon_linking_t *data);
	bool parse_cmd(insteon_cmd_t *cmd);
	
	bool get_cmd(insteon_cmd_t *cmd);
//	bool get_linking(insteon_linking_t *data);
	bool get_info(insteon_info_t *info);
 
	void dumpBuf();
	
	std::array<uint8_t,14> makeALDBWriteRecord(insteon_aldb_t aldb, uint16_t addr);
	
	in_state_t getState() {return _state ;};
	
private:
	
	void dbuff_init();
	bool dbuff_cmp(const char* str);
	bool dbuff_cmp_end(const char* str);
	
	bool append_data(void* data, size_t len);
	bool append_char(uint8_t c);
	size_t data_size();

	struct  {
		size_t  	used;			// actual end of buffer
		size_t 	expect;
		size_t  	dataLen;		// data len
		size_t  	alloc;
		uint8_t*  data;
		
	} dbuf;
 
	in_state_t 	_state;

};


#endif /* InsteonParser_hpp */
