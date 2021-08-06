//
//  main.cpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 4/9/21.
//

#include <iostream>

#include "ServerCmdQueue.hpp"
#include "InsteonAPISecretMgr.hpp"

#include <string.h>

#include <regex>
#include <string>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <unistd.h>			//Needed for sleep

#include "Utils.hpp"

#include "TCPServer.hpp"
#include "Telnet/TelnetServerConnection.hpp"
#include "REST/RESTServerConnection.hpp"
#include "ServerCmdQueue.hpp"
#include "Telnet/CmdLineRegistry.hpp"

#include "ServerCmdValidators.hpp"
#include "ServerCommands.hpp"

#include "CommonIncludes.h"

#include "Action.hpp"

#if __APPLE__
//  #define SERIAL_DEVICE "/dev/cu.usbserial-AK05Z8CQ"
//   #define SERIAL_DEVICE "/dev/cu.usbserial-AL02YD29"
#define SERIAL_DEVICE "/dev/cu.usbserial-AK05ZAZ7"
#elif __arm__
#define SERIAL_DEVICE "/dev/ttyUSB0"
#else
#warning  NO CLUE

#endif

using namespace std;

void linkKP(){
	DeviceID deviceID =  DeviceID("33.4F.F6");
	auto keypad = InsteonKeypadDevice(deviceID );
	
	START_VERBOSE;
	
	// link up the keypad button to groups.
	vector<pair<uint8_t, uint8_t> > pairs =
	{ {2, 0xA2},{3,0xA3},{4,0xA4},{5,0xA5},{6,0xA6},{7,0xA7},{8,0xA8},   };
	
	
	insteon.linkKeyPadButtonsToGroups(deviceID, pairs, [=](bool didSucceed) {
		
		insteon.addToDeviceALDB(deviceID, false,  0x02,  [=](bool didSucceed) {
			insteon.addToDeviceALDB(deviceID, false, 0x03,  [=](bool didSucceed) {
				insteon.addToDeviceALDB(deviceID, false, 0x04,  [=](bool didSucceed) {
					insteon.addToDeviceALDB(deviceID, false, 0x05,  [=](bool didSucceed) {
						insteon.addToDeviceALDB(deviceID, false, 0x06,  [=](bool didSucceed) {
							insteon.addToDeviceALDB(deviceID, false, 0x07,  [=](bool didSucceed) {
								insteon.addToDeviceALDB(deviceID, false, 0x08,  [=](bool didSucceed) {
									
									printf("success");
									
									
								});
							});
						});
					});
				});
			});
		});
	});
}

void test(){
	DeviceID deviceID =  DeviceID("33.4F.F6");
	
	//	InsteonKeypadDevice(deviceID).setNonToggleMask(00, [=]( bool didSucceed) {
	//	});
	//
	
	InsteonKeypadDevice(deviceID).getKeypadLEDState( [=](uint8_t mask, bool didSucceed) {
		
		if(didSucceed){
			mask = ~mask;
			InsteonKeypadDevice(deviceID).setKeypadLEDState(mask, [=](bool didSucceed) {
				
				test();
			});
			
		}
		else {
			test();
		}
		
		
	});
	
	
}

// MARK: - cmdline options

#ifndef IsNull
#define IsntNull( p )	( (bool) ( (p) != NULL ) )
#define IsNull( p )		( (bool) ( (p) == NULL ) )
#endif

int		gVerbose_flag	= 0;
int		gDebug_flag		= 0;
 
char*	gCacheFilePath		= NULL;

/* for command line processing */
typedef enum
{
	kArg_Invalid   = 0,
	kArg_Boolean,
	kArg_String,
	kArg_UInt,
	kArg_HexString,
	kArg_Other,
	kArg_Count,
} argType_t;


typedef struct
{
	argType_t 		type;
	void*				argument;
	const char*		shortName;
	char				charName;
	const char*		longName;
} argTable_t;
 

static argTable_t sArgTable[] =
{
		
	/* arguments/modifiers */
	{ kArg_Count,	&gVerbose_flag ,	"verbose",	'v',	"Enables verbose output" },
	{ kArg_Count,	&gDebug_flag	,		"debug",		'd',	"Enables debug output" },
	{ kArg_String,  &gCacheFilePath,	NULL,			'f',	"cacheFile path" },
  };

#define TableEntries  ((int)(sizeof(sArgTable) /  sizeof(argTable_t)))

static void sUsage()
{
	int j;
	
	printf ("\nInsteon Server \n\nusage: insteonserver [options] ..\nOptions: \n ");
		
	printf("\tOptions:\n" );
	for( j = 0; j < TableEntries; j ++)
		if( ((sArgTable[j].type == kArg_Boolean)
			 || (sArgTable[j].type == kArg_String)
			 || (sArgTable[j].type == kArg_HexString)
			 || (sArgTable[j].type == kArg_Other))
			&& sArgTable[j].longName)
			printf("\t%s%c   %2s%-10s %s\n",
					 sArgTable[j].charName?"-":"",  sArgTable[j].charName?sArgTable[j].charName:' ',
					 sArgTable[j].shortName?"--":"",  sArgTable[j].shortName?sArgTable[j].shortName:"",
					 sArgTable[j].longName);
}


static void sSetupCmdOptions (int argc, const char **argv)
{
	
	if(argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
	 			bool found = false;
				size_t	temp;

				for(int  j = 0; j < TableEntries; j ++)
					if ( (IsntNull( sArgTable[j].shortName)
						 &&  ((strncmp(argv[i], "--", 2) == 0)
						  && (strcasecmp(argv[i] + 2,  sArgTable[j].shortName) == 0)) )
					 || (( *(argv[i]) ==  '-' ) && ( *(argv[i] + 1) == sArgTable[j].charName)))
					{
						found = true;
						switch(sArgTable[j].type)
						{
									
							case kArg_Boolean:
								if(IsNull(sArgTable[j].argument)) continue;
								*((bool*)sArgTable[j].argument) = true;
								break;

							case kArg_Count:
								if(IsNull(sArgTable[j].argument)) continue;
								*((bool*)sArgTable[j].argument) = *((bool*)sArgTable[j].argument)+1;
								break;
								
							case kArg_String:
								if(IsNull(sArgTable[j].argument)) continue;
								if(IsNull(argv[++i]))  goto error;
								temp = strlen(argv[i]);
								*((char**)sArgTable[j].argument) = (char*) malloc(temp + 2);
								strcpy(*((char**)sArgTable[j].argument), argv[i]);
								break;
								
							case kArg_HexString:
								if(IsNull(sArgTable[j].argument)) continue;
								if(IsNull(argv[++i]))  goto error;
									temp = strlen(argv[i]);
								*((char**)sArgTable[j].argument) = (char*) malloc(temp + 2);
								strcpy(*((char**)sArgTable[j].argument), argv[i]);
								break;
								
							case kArg_UInt:
								if(IsNull(sArgTable[j].argument)) continue;
							{
								uint tmp;
								if( sscanf(argv[++i],"%u",&tmp) == 1)
									*((uint*)sArgTable[j].argument) = tmp;
							}
								break;
								
							case kArg_Other:
							default:;
						}
						break;
					}
				if(!found) goto error;
			}
		}
	return;
	
error:
	sUsage();
	exit(1);

}


// MARK: - MAIN
[[clang::no_destroy]]  InsteonMgr	insteon;

int main(int argc, const char **argv) {
	
	/* process Test options */
	sSetupCmdOptions(argc, argv);
 
	if(gVerbose_flag) {
		LogMgr::shared()->_logFlags = LogMgr::LogLevelVerbose;
	}else if(gDebug_flag) {
		LogMgr::shared()->_logFlags = LogMgr::LogFlagDebug;
	}
 
	
	START_INFO;

// load the database cachefile
	insteon.loadCacheFile( IsntNull(gCacheFilePath)?string(gCacheFilePath):string());
	bool remoteTelnet = insteon.getDB()->getAllowRemoteTelnet();
	int telnetPort = insteon.getDB()->getTelnetPort();
	int restPort = insteon.getDB()->getRESTPort();

//set up the api secrets
	InsteonAPISecretMgr apiSecrets(insteon.getDB());
//	apiSecrets.apiSecretCreate("foobar","12345" );
	
	// create the server command processor
	auto cmdQueue = new ServerCmdQueue(&apiSecrets);
	registerServerCommands();
	
	TCPServer telnet_server(cmdQueue);
	telnet_server.begin(telnetPort, true, [=](){
		return new TelnetServerConnection();
	});
	
	TCPServer rest_server(cmdQueue);
	rest_server.begin(restPort, remoteTelnet, [=](){
		return new RESTServerConnection();
	});
	
	// run the main loop.
	while(true) {
		sleep(60);
	}
	
	return 0;
	
}
