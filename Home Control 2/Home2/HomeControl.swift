//
//  HomeControl.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 11/18/21.
//

import Foundation
import CoreLocation



class ServerDateInfo {

	func getCLLocationCoordinate2D() -> CLLocationCoordinate2D {
		return  CLLocationCoordinate2DMake(latitude, longitude)
	}

	let latitude : CLLocationDegrees
	let longitude : CLLocationDegrees
	let uptime 	: TimeInterval

	let gmtOffset: 		Int
	let timeZone: 		String
	let midNight: 		Date
	let sunSet:			Date
	let civilSunSet: 	Date
	let civilSunRise: 	Date
	let sunRise: 		Date

	init(from :RESTDateInfo){
		self.latitude 	= from.latitude
		self.longitude 	= from.longitude
		self.uptime 		= TimeInterval(from.uptime)
		self.gmtOffset	 = from.gmtOffset
		self.timeZone	 = from.timeZone

		var t:TimeInterval
		self.midNight 	 = Date(timeIntervalSince1970: from.midnight)

		t = TimeInterval(from.midnight + (from.sunSet * 60))
		self.sunSet 	= Date(timeIntervalSince1970: TimeInterval(t))

		t = TimeInterval(from.midnight + (from.civilSunSet * 60))
		self.civilSunSet 	= Date(timeIntervalSince1970: TimeInterval(t))

		t = TimeInterval(from.midnight + (from.civilSunRise * 60))
		self.civilSunRise 	= Date(timeIntervalSince1970: TimeInterval(t))

		t = TimeInterval(from.midnight + (from.sunRise * 60))
		self.sunRise 	= Date(timeIntervalSince1970: TimeInterval(t))

	}
}


enum HomeControlError: Error {
	case connectFailed
	case invalidState
	case invalidURL
	case restError
	case internalError
	
	case unknown
}

enum HomeControlRequest: Error {
	case status
	case date
	case plmInfo
	case keypads
	case keypad
	case events
	case event
	case device
	case device_aldb

	case unknown
}
 
enum DeviceState: Int {
	case unknown = 0
	case disconnected
	case connected
	case error
	case timeout
}

public class HomeControl {
	
	var isValid: Bool = false;
	
	static let shared: HomeControl = {
		let instance = HomeControl()
		// Setup code
		return instance
	}()
	
	private init(){
		
	}
	
	
	func fetchData(_ requestType: HomeControlRequest, ID : String? = nil,
						completionHandler: @escaping (Result<Any?, Error>) -> Void)  {
		
	 	var urlPath :String = ""
		var queries: [URLQueryItem]? = nil
		
		switch(requestType){
		case .status:
			urlPath = "state"
			
		case .date:
			urlPath = "date"
			
		case .plmInfo:
			urlPath = "plm/info"
			
		case .events:
			urlPath = "events"
			
		case .keypads:
			urlPath = "keypads"
			
		case .event:
			if let evtID = ID {
				urlPath = "events/" + evtID
			}
			
		case .keypad:
			if let kpID = ID {
				urlPath = "keypads/" + kpID
			}
			
		case .device:
			if let devID = ID {
				urlPath = "devices/" + devID
			}
			
 		case .device_aldb:
			if let devID = ID {
				urlPath = "devices/" + devID
				queries = [URLQueryItem(name: "aldb", value: "1")]
			}

	
			
		default:
			break;
		}
		
		HCServerManager.shared.RESTCall(urlPath: urlPath,
												  headers:nil,
												  queries: queries) { (response, json, error)  in
			
			if (json == nil) {
				completionHandler(.failure(HomeControlError.connectFailed))
			}
			else 	if let status = json as? RESTStatus {
				completionHandler(.success(status))
			}
			else 	if let status = json as? RESTPlmInfo {
				completionHandler(.success(status))
			}
			else 	if let keypads = json as? RESTKeypads {
				completionHandler(.success(keypads))
			}
			else 	if let keypad = json as? RESTKeypad {
				completionHandler(.success(keypad))
			}
			else 	if let events = json as? RESTEventList {
				completionHandler(.success(events))
			}
			else if let obj = json as? RESTDateInfo {
				let date =  ServerDateInfo(from: obj)
				completionHandler(.success(date))
			}
			else 	if let event = json as? RESTEvent {
				completionHandler(.success(event))
			}
			else 	if let device = json as? RESTDeviceDetails {
				completionHandler(.success(device))
			}
				else if let restErr = json as? RESTError {
				completionHandler(.success(restErr))
			}
			else if let error = error{
				completionHandler(.failure(error))
			}
		}
	}
	
}
