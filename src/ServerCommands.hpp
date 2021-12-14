//
//  ServerCommands.hpp
//  InsteonServer
//
//  Created by Vincent Moscaritolo on 4/9/21.
//

#ifndef ServerCommands_hpp
#define ServerCommands_hpp

#include <stdio.h>

// MARK: - SERVER DEFINES

constexpr string_view NOUN_VERSION		 		= "version";
constexpr string_view NOUN_DATE		 			= "date";
constexpr string_view NOUN_DEVICES	 			= "devices";
constexpr string_view NOUN_STATUS		 		= "status";
constexpr string_view NOUN_STATE		 		= "state";

constexpr string_view NOUN_LOG		 			= "log";
constexpr string_view NOUN_PLM			 		= "plm";
constexpr string_view NOUN_GROUPS			 	= "groups";
constexpr string_view NOUN_INSTEON_GROUPS	= "insteon.groups";
constexpr string_view NOUN_ACTION_GROUPS		= "action.groups";
constexpr string_view NOUN_EVENTS				= "events";
constexpr string_view NOUN_EVENTS_GROUPS		= "event.groups";
constexpr string_view NOUN_KEYPADS				= "keypads";

constexpr string_view NOUN_CONFIG		 		= "config";

constexpr string_view NOUN_LINK	 				= "link";
 
constexpr string_view SUBPATH_INFO			 	= "info";
constexpr string_view SUBPATH_DATABASE		= "database";
constexpr string_view SUBPATH_RUN_ACTION		= "run.actions";
constexpr string_view SUBPATH_PORT			 	= "port";
constexpr string_view SUBPATH_STATE		 	= "state";
constexpr string_view SUBPATH_FILE	 			= "file";
constexpr string_view SUBPATH_EXPORT			= "export";

constexpr string_view JSON_ARG_DEVICEID 		= "deviceID";
constexpr string_view JSON_ARG_GROUPID 		= "groupID";
constexpr string_view JSON_ARG_ACTIONID 		= "actionID";
constexpr string_view JSON_ARG_EVENTID 		= "eventID";
constexpr string_view JSON_ARG_ACTION			= "action";
constexpr string_view JSON_ARG_TRIGGER		= "trigger";

constexpr string_view JSON_ARG_CONFIG		= "config";

constexpr string_view JSON_ARG_BEEP 			= "beep";
constexpr string_view JSON_ARG_LEVEL 			= "level";
constexpr string_view JSON_ARG_BACKLIGHT		= "backlight";
constexpr string_view JSON_ARG_KP_MASK 		= "keyMask"; 	// Keypad LED mask
constexpr string_view JSON_ARG_BUTTONS		= "buttons";
constexpr string_view JSON_ARG_BUTTON			= "button";

constexpr string_view JSON_ARG_VALIDATE		= "validate";
constexpr string_view JSON_ARG_DUMP		 	= "dump";			// for debug datat
constexpr string_view JSON_ARG_MESSAGE		= "message";			// for logfile
 
constexpr string_view JSON_ARG_LOAD 			= "load";
constexpr string_view JSON_ARG_SAVE 			= "save";

constexpr string_view JSON_ARG_DATE			= "date";
constexpr string_view JSON_ARG_VERSION		= "version";
constexpr string_view JSON_ARG_BUILD_TIME	= "buildtime";
constexpr string_view JSON_ARG_UPTIME		= "uptime";


constexpr string_view JSON_ARG_TIMESTAMP		= "timestamp";
constexpr string_view JSON_ARG_DEVICEIDS	= 	"deviceIDs";

constexpr string_view JSON_ARG_IS_KEYPAD		= "isKeyPad";
constexpr string_view JSON_ARG_IS_DIMMER		= "isDimmer";
constexpr string_view JSON_ARG_IS_PLM			= "isPLM";

constexpr string_view JSON_ARG_DEVICEINFO 	= "deviceInfo";
constexpr string_view JSON_ARG_DETAILS 		= "details";
constexpr string_view JSON_ARG_LEVELS 		= "levels";
constexpr string_view JSON_ARG_ACTIONS		= "actions";
constexpr string_view JSON_ARG_ALDB 			= "aldb";

constexpr string_view JSON_ARG_STATE			= "state";
constexpr string_view JSON_ARG_STATESTR		= "stateString";
constexpr string_view JSON_ARG_CPU_TEMP		= "cpuTemp";

constexpr string_view JSON_ARG_ETAG			= "ETag";
constexpr string_view JSON_ARG_FORCE			= "force";

constexpr string_view JSON_ARG_EXTINFO			= "extinfo";

constexpr string_view JSON_ARG_PROPERTIES	= "properties";
constexpr string_view JSON_ARG_NAME			= "name";
constexpr string_view JSON_ARG_GROUPIDS		= "groupIDs";
constexpr string_view JSON_ARG_EVENTIDS		= "eventIDs";
constexpr string_view JSON_ARG_FILEPATH		= "filepath";
constexpr string_view JSON_ARG_LOGFLAGS		= "logflags";
constexpr string_view JSON_ARG_COUNT			= "count";
constexpr string_view JSON_ARG_ISCNTRL		= "cntrl";  // used for linking devices
constexpr string_view JSON_ARG_AUTOSTART		= "auto.start";
constexpr string_view JSON_ARG_REMOTETELNET	= "allow.remote.telnet";
constexpr string_view JSON_ARG_PLMID			= "plmID";

constexpr string_view JSON_ARG_TIMED_EVENTS	= "events.timed";
constexpr string_view JSON_ARG_FUTURE_EVENTS	= "events.future";

constexpr string_view JSON_ARG_LATITUDE		= "latitude";
constexpr string_view JSON_ARG_LONGITUDE		= "longitude";
 
constexpr string_view JSON_VAL_ALL				= "all";
constexpr string_view JSON_VAL_VALID			= "valid";
constexpr string_view JSON_VAL_DETAILS		= "details";
constexpr string_view JSON_VAL_LEVELS			= "levels";
constexpr string_view JSON_VAL_ALDB			= "aldb";

constexpr string_view JSON_VAL_ALDB_FLAG		= "aldb.flag";
constexpr string_view JSON_VAL_ALDB_ADDR		= "aldb.address";
constexpr string_view JSON_VAL_ALDB_GROUP	= "aldb.group";

constexpr string_view JSON_VAL_START			= "start";
constexpr string_view JSON_VAL_STOP			= "stop";
constexpr string_view JSON_VAL_ERASE			= "erase";
constexpr string_view JSON_VAL_IMPORT			= "import";

constexpr string_view JSON_ARG_OS_SYSNAME	= "os.sysname";
constexpr string_view JSON_ARG_OS_NODENAME	= "os.nodename";
constexpr string_view JSON_ARG_OS_RELEASE	= "os.release";
constexpr string_view JSON_ARG_OS_MACHINE	= "os.machine";
constexpr string_view JSON_ARG_OS_VERSION	= "os.version";

void registerCommandsLineFunctions();
void registerServerNouns();

#endif /* ServerCommands_hpp */
