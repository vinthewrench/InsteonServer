//
//  HCServerManager.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 6/27/21.
//

import Foundation
import SwiftRadix
import CoreLocation
import UIKit

enum  mgr_state_t :Int{
	case
	STATE_UNKNOWN = 0,
	STATE_INIT,
	STATE_SETUP,
	STATE_NO_PLM,
	STATE_PLM_INIT,
	STATE_PLM_ERROR,
	STATE_PLM_STOPPED,
	STATE_RESETING,
	STATE_SETUP_PLM,
	STATE_READING_PLM,
	STATE_VALIDATING,
	STATE_READY,
	STATE_LINKING,
	STATE_UPDATING
 }
 
extension mgr_state_t: Codable {
	
	enum Key: CodingKey {
		case rawValue
	}
	
	enum CodingError: Error {
		case unknownValue
	}
	
	init(from decoder: Decoder) throws {
		let container = try decoder.container(keyedBy: Key.self)
		let rawValue = try container.decode(Int.self, forKey: .rawValue)
		switch rawValue {
		case 0:
			self = .STATE_UNKNOWN
		case 1:
			self = .STATE_INIT
		case 2:
			self = .STATE_SETUP
		case 3:
			self = .STATE_NO_PLM
		case 4:
			self = .STATE_PLM_INIT
		case 5:
			self = .STATE_PLM_ERROR
		case 6:
			self = .STATE_PLM_STOPPED
		case 7:
			self = .STATE_RESETING
		case 8:
			self = .STATE_SETUP_PLM
		case 9:
			self = .STATE_READING_PLM
		case 10:
			self = .STATE_VALIDATING
		case 11:
			self = .STATE_READY
		case 12:
			self = .STATE_LINKING
		case 13:
			self = .STATE_UPDATING
			
		default:
			self = .STATE_UNKNOWN
		}
	}
	
	func encode(to encoder: Encoder) throws {
		var container = encoder.container(keyedBy: Key.self)
		switch self {
		case .STATE_UNKNOWN:
			try container.encode(0, forKey: .rawValue)
		case .STATE_INIT:
			try container.encode(1, forKey: .rawValue)
		case .STATE_SETUP:
			try container.encode(2, forKey: .rawValue)
		case .STATE_NO_PLM:
			try container.encode(3, forKey: .rawValue)
		case .STATE_PLM_INIT:
			try container.encode(4, forKey: .rawValue)
		case .STATE_PLM_ERROR:
			try container.encode(5, forKey: .rawValue)
		case .STATE_PLM_STOPPED:
			try container.encode(6, forKey: .rawValue)
		case .STATE_RESETING:
			try container.encode(7, forKey: .rawValue)
		case .STATE_SETUP_PLM:
			try container.encode(8, forKey: .rawValue)
		case .STATE_READING_PLM:
			try container.encode(9, forKey: .rawValue)
		case .STATE_VALIDATING:
			try container.encode(10, forKey: .rawValue)
		case .STATE_READY:
			try container.encode(11, forKey: .rawValue)
		case .STATE_LINKING:
			try container.encode(12, forKey: .rawValue)
		case .STATE_UPDATING:
			try container.encode(13, forKey: .rawValue)
		}
	}
}

class RESTErrorInfo: Codable {
	let code: Int
	let message: String
	let detail: String?
	let cmdDetail: String?
}

class RESTError: Codable {
	let error: RESTErrorInfo
}

struct RESTStatus: Codable {
	var state: Int
	var stateString: String
	var cpuTemp: Double?
	
	var buildtime: String
	var version: String
	let date: String
	let uptime: Int
	let os_sysname : String
	let os_nodename : String
	let os_release : String
	let os_version : String
	let os_machine : String
	
	let civilSunRise: Double?
	let civilSunSet: Double?
	let sunRise: Double?
	let sunSet: Double?
	let gmtOffset: Int?
	let latitude: Double?
	let longitude: Double?
	let midnight: Double?
	let timeZone: String?

	
	enum CodingKeys: String, CodingKey {
		case state   		= "state"
		case cpuTemp  		= "cpuTemp"
		case stateString  	= "stateString"
		case buildtime 		= "buildtime"
		case version 		= "version"
		case date 				= "date"
		case uptime 			= "uptime"
		case os_sysname 		= "os.sysname"
		case os_nodename 	= "os.nodename"
		case os_release 		= "os.release"
		case os_version 		= "os.version"
		case os_machine 		= "os.machine"
		
		case civilSunRise = "civilSunRise"
		case civilSunSet = "civilSunSet"
		case sunRise = "sunRise"
		case sunSet = "sunSet"
		case gmtOffset = "gmtOffset"
		case latitude = "latitude"
		case longitude = "longitude"
		case midnight = "midnight"
		case timeZone = "timeZone"
	}
	
}

struct RESTVersion: Codable {
	var timestamp: String
	var version: String
}


class RESTDateInfo: Codable {
	let civilSunRise: Double?
	let civilSunSet: Double?
	let sunRise: Double?
	let sunSet: Double?
	let date: String
	let gmtOffset: Int?
	let latitude: Double?
	let longitude: Double?
	let midnight: Double?
	let timeZone: String?
	let uptime: Int
}



struct RESTKeyButtons: Codable {
	var name:String?
	var level:String
 	var actions: Dictionary<String,RESTEventAction>?

	enum CodingKeys: String, CodingKey {
		case name = "name"
		case level = "level"
		case actions = "actions"
	}
}

struct RESTKeypads: Codable {
	var deviceIDs: [String]
}

struct RESTKeypad: Codable {
	var name:String?
	var buttons: Dictionary<String, RESTKeyButtons>?
	var config: Int
	var deviceID: String
	var deviceInfo: String
	var lastUpdated: String
	var valid: Bool
	var backlight: Int?


	enum CodingKeys: String, CodingKey {
		case name
		case buttons
		case config
		case deviceID
		case deviceInfo
		case lastUpdated
		case valid
		case backlight
	}

}
//
//struct RESTKeypads: Codable {
//	var details: Dictionary<String,RESTKeypad>
//
//	enum CodingKeys: String, CodingKey {
//		case details = "details"
//	}
//}


extension Int {
  func onLevelString() ->String {
	  var  str:String = ""
	  
	  if(self == 0){
		  str = "Off";
	  }
	  else if(self == 255){
		  str = "On";
	  }
	  else {
		  let level = Int(( Float(self) / 255.00) * 100)
		  str = "\(level)%"
	  }
	  return str
  }
}


struct RESTDeviceLevel: Codable {
	var deviceID: String
	var level: Int?
	var ETag: Int?
}
 
struct RESTaldbEntry: Codable {

	var aldb_address: String
	var deviceID: String
	var aldb_group: String
	var aldb_flag: String
 
	enum CodingKeys: String, CodingKey {
		case deviceID
		case aldb_group = "aldb.group"
		case aldb_flag = "aldb.flag"
 		case aldb_address = "aldb.address"
 	}
}

struct RESTDeviceDetails: Codable {
	var deviceID: String
	var deviceInfo: String
	var groupIDs: [String]?
	var isDimmer: Bool
	var isKeyPad: Bool
	var isPLM: Bool
	var lastUpdated: String
	var name: String
	var valid: Bool
	var ETag: Int?
	var level: Int?
	var backlight: Int?
 
 	var properties: Dictionary<String, String>?
	var aldb: Dictionary<String, RESTaldbEntry>?
	
	func deviceImage() ->UIImage {
		
		var image: UIImage? = nil
		
		if( !valid) {
			image = UIImage(systemName: "questionmark.circle")
			
		}
		else if( isDimmer){
			image = UIImage(systemName: "lightbulb")
			
		}
		else if(isKeyPad) {
			image = UIImage(named: "keypad")
		}
		else {
			image = UIImage(systemName: "power")
			//						.font(.system(size:20))
		}
		
		return image ?? UIImage()
	}
	
}

struct RESTLinkResponse: Codable {
	var deviceID: String
	var deviceInfo: String
	var isDimmer: Bool
	var isKeyPad: Bool
	var isPLM: Bool
	var lastUpdated: String
	var name: String
}

struct RESTGroupDetails: Codable {
	var deviceIDs: [String]
	var name: String
}

struct RESTEventAction: Codable {
	var cmd			 	:String?
	var action_group 	:String?
	var insteon_group 	:String?
	var deviceID			:String?
	var level 			:String?
 
	enum CodingKeys: String, CodingKey {
		case cmd
		case deviceID
		case level
		case action_group = "action.group"
		case insteon_group = "insteon.groups"
	}
}

struct RESTEventTrigger: Codable {
	var mins 				:Int?
	var timeBase			:Int?
	var cmd			 	:String?
	var action_group 	:String?
	var insteon_group 	:String?
	var deviceID			:String?
	var event				:String?

	enum CodingKeys: String, CodingKey {
		case mins
		case timeBase
		case cmd
		case deviceID
		case event
		case action_group = "action.group"
		case insteon_group = "insteon.group"
	}
	
	func triggerDate(_ solarTimes: ServerDateInfo ) ->Date {
		
		var solarTimeFormat: DateFormatter {
			let formatter = DateFormatter()
			formatter.dateFormat = "hh:mm:ss a"
			formatter.timeZone = TimeZone(abbreviation: "UTC")
			return formatter
		}
		
		var date: Date = Date.distantPast
		
		if let sunSet = solarTimes.sunSet,
			let civilSunSet = solarTimes.civilSunSet,
			let civilSunRise = solarTimes.civilSunRise,
			let sunRise = solarTimes.sunRise,
			let midNight = solarTimes.midNight,
			let mins = self.mins {
			
			switch  self.timeBase {
			case 1: // TOD_ABSOLUTE
				date = midNight.addingTimeInterval( Double(mins * 60))
				break
				
			case 2: // TOD_SUNRISE
				date = sunRise.addingTimeInterval( Double(mins * 60))
				break
				
			case 3: // TOD_SUNSET
				date = sunSet.addingTimeInterval( Double(mins * 60))
				break
				
			case 4: // TOD_CIVIL_SUNRISE
				date = civilSunRise.addingTimeInterval( Double(mins * 60))
				break
				
			case 5: // TOD_CIVIL_SUNSET
				date = civilSunSet.addingTimeInterval( Double(mins * 60))
				break
				
			default:
				break
			}
		}
		return date
	}
}


struct RESTEvent: Codable {
	var eventID : String?

	var name		: String
  	var action	: 	RESTEventAction
 	var trigger	: 	RESTEventTrigger
	
	func isTimedEvent() -> Bool {
		return (self.trigger.timeBase != nil) && (self.trigger.mins != nil)
	}
	
	func isDeviceEvent() -> Bool {
		return (self.trigger.deviceID != nil)
			|| (self.trigger.action_group != nil)
			||  (self.trigger.cmd != nil)
	}
	
	func isAppEvent() -> Bool {
		return (self.trigger.event != nil)
	}
 
	enum eventType: Int {
		case unknown = 0
		case device
		case timed
		case event
	}
	
	enum timedEventTimeBase: Int,CaseIterable {
		case invalid = 0
		case midnight
		case sunrise
		case sunset
		case civilSunrise
		case civilSunset
		
		func description() -> String {
			var str = "Invalid"
			
			switch self {
			case .midnight:		str = "Midnight"
			case .sunrise:  		str = "SunRise"
			case .sunset:  		str = "SunSet"
			case .civilSunrise:  str = "Civil SunRise"
			case .civilSunset:  	str = "Civil SunSet"
			default:
				break
			}
			return str
		}
		
		func image() -> UIImage {
			var image:UIImage? =  nil
			
			switch self {
			case .midnight:		image = UIImage(systemName: "clock")
			case .sunrise:  		image = UIImage(systemName: "sunrise")
			case .sunset: 		 	image = UIImage(systemName: "sunset")
			case .civilSunrise:  image = UIImage(systemName: "sunrise.fill")
			case .civilSunset:  	image = UIImage(systemName: "sunset.fill")
			default:					image =   UIImage(systemName: "questionmark")
			}
			return image ?? UIImage()
		}
	}
	
	func eventType() ->  eventType {
		
		if(self.isTimedEvent()) {
			return .timed
		}
		else 	if(self.isDeviceEvent()) {
			return .device
		}
		else 	if(self.isAppEvent()) {
			return .event
		}
		return .unknown
	}
	
	
	func stringForTrigger() ->  String {
		
		var str = "Invalid"
		
		switch self.eventType() {
		case .timed:
			if let timebase = self.trigger.timeBase,
				let timedTrigger = timedEventTimeBase(rawValue: timebase)   {
				str = timedTrigger.description()
			}
			
		case .device:
			str = "Device"
			
		case .event:
			if self.trigger.event == "startup" {
				str = "Startup"
			}
			break
			
		default:
			break
		}
		
		return str
	}
	
	
	
	func imageForTrigger() -> UIImage {
		
 		var image = UIImage(systemName: "questionmark")
	
		switch self.eventType() {
		case .timed:
			if let timebase = self.trigger.timeBase,
				let timedTrigger = timedEventTimeBase(rawValue: timebase)   {
				image = timedTrigger.image()
			}
			
		case .device:
	//		str = "Device"
			break
			
		case .event:
			if self.trigger.event == "startup" {
				image = UIImage(systemName: "power")
			}
			break
			
		default:
			break
		}

		return image ??  UIImage()
	}
}


struct RESTEventList: Codable {
	var eventIDs: 	Dictionary<String, RESTEvent>
//  	var timed: 		Dictionary<String, String>
		var future: 		[String]
	
	enum CodingKeys: String, CodingKey {
			 case eventIDs
// 			 case timed = "events.timed"
			case future = "events.future"
 		}
}

struct RESTPlmInfo: Codable {
	
	var autoStart: Bool
	var filepath: String
	var plmID:String?
	var state: Int
	var stateString: String?

	var deviceID: String?
	var deviceInfo: String?
	var count:Int?
 
	enum CodingKeys: String, CodingKey {
		case autoStart = "auto.start"
		case filepath
		case plmID
		case state
		case stateString
		case deviceID
		case deviceInfo
		case count
	}

}

struct RESTDeviceList: Codable {
	var details:  Dictionary<String, RESTDeviceDetails>
	var ETag: 	Int?
  }

struct RESTGroupList: Codable {
	var groupIDs:  Dictionary<String, RESTGroupDetails>
  }


struct DeviceCatInfo {
	let cat: Int!
	let subcat: Int!
	let sku: String
	let description: String
}


enum ServerError: Error {
	case connectFailed
	case invalidState
	case invalidURL
	case unknown
}
 
class HCServerManager: ObservableObject {

 
	@Published var lastUpdate = Date()
	
	var deviceCatagories: [DeviceCatInfo] = []
 
	static let shared: HCServerManager = {
			let instance = HCServerManager()
			// Setup code
			return instance
		}()
	
	private func loadDeviceCatagories() {
		//locate the file you want to use
		guard let filepath = Bundle.main.path(forResource: "catagories", ofType: "txt") else {
			return
		}
		
		//convert that file into one long string
		var data = ""
		do {
			data = try String(contentsOfFile: filepath)
			
			//now split that string into an array of "rows" of data.  Each row is a string.
			let rows = data.components(separatedBy: "\n")
			
			//now loop around each row, and split it into each of its columns
			for row in rows {
				let columns = row.components(separatedBy: "\t")
				
				//check that we have enough columns
				if columns.count == 4 {
					if let cat:Int =   columns[0].hex?.value,
						let subcat:Int = columns[1].hex?.value,
						let sku:String = columns[2] as String?,
						let description:String  = columns[3] as String? {
						let info = DeviceCatInfo(cat:cat, subcat: subcat, sku:sku ,description:description)
						deviceCatagories.append(info)
					}
					
				}
			}
		} catch {
		}
	}
	
	public func getDeviceCatInfo(cat:Int, subcat:Int) -> DeviceCatInfo? {
		
		if(deviceCatagories.count == 0){
			loadDeviceCatagories()
		}
		
		let filtered = deviceCatagories.filter{ $0.cat == cat && $0.subcat == subcat}
		return filtered.first
	}
	
	public func getDeviceDescriptionFrom(_ deviceInfo: String?)->String? {
		
		if let deviceInfo = deviceInfo  {
			
			let separators = CharacterSet(charactersIn: "(,)")
			let parts = deviceInfo.components(separatedBy: separators)
			if( parts.count > 3) {
				
				if let cat:Int =  parts[1].hex?.value,
					let subcat:Int = parts[2].hex?.value {
 
					if let info = getDeviceCatInfo(cat: cat, subcat: subcat) {
						return info.description
					}
				}
			}
		}
		return nil
	}
	
	public func getFirmwareVersionFrom(_ deviceInfo: String?)->String? {
		
		if let deviceInfo = deviceInfo  {
			
			let separators = CharacterSet(charactersIn: "(,)")
			let parts = deviceInfo.components(separatedBy: separators)
			if( parts.count > 3) {
				
				if let firm:Int =  Int(parts[3]),
					let version:Int = Int(parts[4]) {
					
					var versionStr:String = ""
					
					switch (version) {
					case 0: versionStr = "(i1 Engine)"
					case 1: versionStr = "(i2 Engine)"
					case 2: versionStr = "(i2CS Engine)"
					default:
						break;
					}
					return ("\(firm) \(versionStr)")
				}
			}
		}
		
		return nil
	}
	
	
	init () {
		
	}
	
	
	func calculateSignature(forRequest: URLRequest, apiSecret: String ) -> String {
		
		if let method: String =  forRequest.httpMethod,
			let urlPath = forRequest.url?.path ,
			let daytimeHeader = forRequest.value(forHTTPHeaderField: "X-auth-date"),
			let apiKey = forRequest.value(forHTTPHeaderField: "X-auth-key")
		{
			var bodyHash:String = ""
			
			if let body = forRequest.httpBody {
				bodyHash = body.sha256String()
			}
			else {
				bodyHash = Data().sha256String()
			}
			
			let stringToSign =   method +  "|" + urlPath +  "|"
				+  bodyHash +  "|" + daytimeHeader + "|" + apiKey
  
			let signatureString = stringToSign.hmac(key: apiSecret)
			
				return signatureString;
		}
	
		return "";

	}

	func SetKeypadButton(keypadID:  String, button: String, on:Bool,
								completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "keypads/\(keypadID)/\(button)"
		let newlevel = on ? "on" : "off"
		
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["level":newlevel]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
			
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
		else {
			completion(ServerError.invalidURL)
		}
	}
	
	func addToGroup(_ groupID: String, deviceID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		let urlPath = "groups/\(groupID)"
	
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			var request = URLRequest(url: urlComps.url!)
	
			let json = ["deviceID":deviceID]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	func removeFromGroup(_ groupID: String, deviceID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		let urlPath = "groups/\(groupID)/\(deviceID)"
	
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			var request = URLRequest(url: urlComps.url!)
	 
			// Specify HTTP Method to use
			request.httpMethod = "DELETE"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	public func addALDBtoDevice( _ deviceID: String,
													groupID: String,
												 isCNTL: Bool,
												 completion: @escaping (Error?) -> Void = {_ in }){

		let urlPath = "link/\(deviceID)/\(groupID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			var request = URLRequest(url: urlComps.url!)
			
			let json = ["cntrl":isCNTL]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	 
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 30
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	public func removeALDBfromDevice( _ deviceID: String, aldbAddr: String,
												 completion: @escaping (Error?) -> Void = {_ in }){
		
		let urlPath = "link/\(deviceID)/\(aldbAddr)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			var request = URLRequest(url: urlComps.url!)
			
			
			// Specify HTTP Method to use
			request.httpMethod = "DELETE"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 30
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	

	
	
	func linkDevice(deviceID: String?,
					  completion: @escaping (Any?, Error?) -> Void)  {

 		var urlPath = "link"
		if let deviceID = deviceID {
			urlPath = "link/\(deviceID)"
  		}
	 
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 60*4
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
			
				
				if urlError != nil {
					completion( nil, urlError	)
					return
				}
	
// 			print ( String(decoding: (data!), as: UTF8.self))

				if let data = data as Data? {
					
					let decoder = JSONDecoder()
					
					if let restErr = try? decoder.decode(RESTError.self, from: data){
						completion(restErr.error, nil)
					}
					else if let restLink = try? decoder.decode(RESTLinkResponse.self, from: data){
						completion(restLink, nil)
					}
					
					else {
						completion(nil,nil)
					}
				
				}
 			}
			task.resume()
		}
			else {
				completion(nil, ServerError.invalidURL)
			}
	}

		
	func renameDevice(deviceID: String, newName: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "devices/\(deviceID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["name":newName]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PATCH"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	func deleteDevice(_ deviceID: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "devices/\(deviceID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
 			// Specify HTTP Method to use
			request.httpMethod = "DELETE"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	func createGroup(name: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "groups"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["name":name]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "POST"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}

	
	func renameGroup(groupID: String, newName: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "groups/\(groupID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["name":newName]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PATCH"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	

	
	func deleteGroup(_ groupID: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "groups/\(groupID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
//			let json = ["name":newName]
//			let jsonData = try? JSONSerialization.data(withJSONObject: json)
//			request.httpBody = jsonData
//
			// Specify HTTP Method to use
			request.httpMethod = "DELETE"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	
	
	func renameEvent(eventID: String, newName: String,
					  completion: @escaping (Error?) -> Void)  {
		
		let urlPath = "events/\(eventID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["name":newName]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PATCH"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	

	
	
	func SetGroupLevel(GroupID: String, toLevel: Int,
					  completion: @escaping (Error?) -> Void)  {
	
		let urlPath = "groups/\(GroupID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["level":toLevel]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	func SetGroupBackLightLevel(GroupID: String, toLevel: Int,
					  completion: @escaping (Error?) -> Void)  {
	
		let urlPath = "groups/\(GroupID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["backlight":toLevel]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 60
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	func SetDeviceLevel(deviceID: String, toLevel: Int,
					  completion: @escaping (Error?) -> Void)  {
	
		let urlPath = "devices/\(deviceID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["level":toLevel]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	func setBackLightLevel(deviceID: String, toLevel: Int,
					  completion: @escaping (Error?) -> Void)  {
	
		let urlPath = "devices/\(deviceID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["backlight":toLevel]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
			request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	
	
	
	func BeepDevice(deviceID: String,
					  completion: @escaping (Error?) -> Void)  {
	
		let urlPath = "devices/\(deviceID)"
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
			let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			//			if let queries = queries {
			//				urlComps.queryItems = queries
			//			}
			var request = URLRequest(url: urlComps.url!)
			
			
			let json = ["beep": true]
			let jsonData = try? JSONSerialization.data(withJSONObject: json)
		request.httpBody = jsonData
	
			// Specify HTTP Method to use
			request.httpMethod = "PUT"
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			// Send HTTP Request
			request.timeoutInterval = 10
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				completion(urlError	)
			}
			task.resume()
		}
			else {
				completion(ServerError.invalidURL)
			}
	}
	func RESTCall(urlPath: String,
					  httpMethod: String? = "GET",
					  headers: [String : String]? = nil,
					  queries: [URLQueryItem]? = nil,
					  body: Any? = nil,
					  timeout:TimeInterval = 10,
					  completion: @escaping (URLResponse?,  Any?, Error?) -> Void)  {
		
		if let requestUrl: URL = AppData.serverInfo.url ,
			let apiKey = AppData.serverInfo.apiKey,
			let apiSecret = AppData.serverInfo.apiSecret {
			let unixtime = String(Int(Date().timeIntervalSince1970))
			
	 		let urlComps = NSURLComponents(string: requestUrl.appendingPathComponent(urlPath).absoluteString)!
			if let queries = queries {
				urlComps.queryItems = queries
			}
			var request = URLRequest(url: urlComps.url!)
			
			// Specify HTTP Method to use
			request.httpMethod = httpMethod
			request.setValue(apiKey,forHTTPHeaderField: "X-auth-key")
			request.setValue(String(unixtime),forHTTPHeaderField: "X-auth-date")
		
			if let body = body {
				let jsonData = try? JSONSerialization.data(withJSONObject: body)
				request.httpBody = jsonData
			}
 
			let sig =  calculateSignature(forRequest: request, apiSecret: apiSecret)
			request.setValue(sig,forHTTPHeaderField: "Authorization")
			
			headers?.forEach{
				request.setValue($1, forHTTPHeaderField: $0)
			}
				
			// Send HTTP Request
			request.timeoutInterval = timeout
			
// print(request)
			
			let session = URLSession(configuration: .ephemeral, delegate: nil, delegateQueue: .main)
			
			let task = session.dataTask(with: request) { (data, response, urlError) in
				
				if urlError != nil {
					completion(nil, nil, urlError	)
					return
				}
			
// 			print ( String(decoding: (data!), as: UTF8.self))

				if let data = data as Data? {
					
					let decoder = JSONDecoder()
					
					if let restErr = try? decoder.decode(RESTError.self, from: data){
						completion(response, restErr.error, nil)
					}
					else if let obj = try? decoder.decode(RESTStatus.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTDateInfo.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTVersion.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTDeviceDetails.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTDeviceList.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTPlmInfo.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTKeypads.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTKeypad.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTGroupList.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTEventList.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTEvent.self, from: data){
						completion(response, obj, nil)
					}
					else if let obj = try? decoder.decode(RESTDeviceLevel.self, from: data){
						completion(response, obj, nil)
					}
					else if let jsonObj = try? JSONSerialization.jsonObject(with: data, options: .allowFragments) as? Dictionary<String, Any> {
						completion(response, jsonObj, nil)
					}
					else {
						completion(response, nil, nil)
					}
				}
			}
			
			task.resume()
		}
		else {
			completion(nil,nil, ServerError.invalidURL)
		}
	}
}
