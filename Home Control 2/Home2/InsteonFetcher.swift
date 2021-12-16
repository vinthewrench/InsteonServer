//
//  InsteonFetcher.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 7/5/21.
//

import Foundation

public class InsteonFetcher: ObservableObject {
	
	var devices: 	Dictionary<String, RESTDeviceDetails> = [:]
	var groups: 	Dictionary<String, RESTGroupDetails> = [:]
	
	var lastUpdate = Date()
	var lastEtag:Int = 0
	var isLoaded:Bool = false

	var timer = Timer()
	
	static let shared: InsteonFetcher = {
		let instance = InsteonFetcher()
		// Setup code
		return instance
	}()
	
	private init(){
		
		reload()
		
	}
	
	public func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
													self.getChangedDevices()
	//												self.getKeypads()
												})
	}
	
	public func stopPollng(){
		timer.invalidate()
	}
	
	public func reload(){
		self.isLoaded = false
		
		guard AppData.serverInfo.validated  else {
			return
		}
		
		let dp = DispatchGroup()
		
		dp.enter()
		getDevices(){
			dp.leave()
		}
		
		
		dp.enter()
		getGroups(){
			dp.leave()
		}
		
		 
		dp.notify(queue: .main) {
			self.isLoaded = true
		}
		
	}
	
	
	public func getDevices(completion: @escaping () -> Void = {}) {
		self.getChangedDevices(clearAll: true){
			completion()
		}
	}
	

	public func getChangedDevices(clearAll: Bool = false,
											completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			return
		}
		
		if(clearAll) {
			self.lastEtag = 0
		}
		
		HCServerManager.shared.RESTCall(urlPath: "devices",
												  headers: ["If-None-Match" : String( self.lastEtag + 1 )],
												  queries: [URLQueryItem(name: "details", value: "1")])
		{ (response, json, error)  in
			
			if let obj = json  as? RESTDeviceList {
				if let etag = obj.ETag {
					self.lastEtag = etag
				}
				
				if(clearAll){
					self.devices = obj.details
				}
				else
				{
					self.devices = self.devices.merging(obj.details) { $1 }

				}
					
			}
			completion()

		}
	}
	
	var devicesToCheck:[String] =  []
	
	private func continueDeviceUpdate() {
		
		if let deviceID = devicesToCheck.first {
			updateDevice(deviceID){ (success) in
				DispatchQueue.main.async{
					self.devicesToCheck = self.devicesToCheck.filter { $0 != deviceID }
					if(self.devicesToCheck.count > 0){
						self.continueDeviceUpdate()
					}
					else {
						print("update done")
					}
					
				}
			}
		}
		
	}
	
	public func startDeviceUpdate() {
		
		// is a update already progress ?
		if(self.devicesToCheck.count > 0){
			return;
		}
		
		devicesToCheck = self.devices.map({$0.key})
		continueDeviceUpdate()
	}
	
	

	
	public func renameDevice(_ deviceID: String, newName: String,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.renameDevice(deviceID: deviceID,
														newName: newName)
		{ (error)  in
			
			if(error == nil){
				
				self.devices[deviceID]?.name = newName
			}
			
			completion(error)
		}
	}
	
	public func deleteDevice(_ deviceID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		
		devices.removeValue(forKey: deviceID)
		
		HCServerManager.shared.deleteDevice(deviceID)
		{ (error)  in
			
			if(error == nil){
				
				self.getChangedDevices(clearAll: true){
					completion(nil)
					return
	
				}
				
			}
			
			completion(error)
		}
	}


	public func renameGroup(_ groupID: String, newName: String,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.renameGroup(groupID: groupID,
														newName: newName)
		{ (error)  in
			
			if(error == nil){
				
				self.groups[groupID]?.name = newName
				
			}
			
			completion(error)
		}
	}
	
	
		
	public func createGroup(_ newName: String,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.createGroup(name: newName)
		{ (error)  in
			
			if(error == nil){
				
				self.getGroups(){
					completion(nil)
					return
	
				}
			}
			else {
				completion(error)

			}
		}
	}

	public func deleteGroup(_ groupID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.deleteGroup(groupID)
		{ (error)  in
			
			if(error == nil){
				
				self.getGroups(){
					completion(nil)
					return
	
				}
				
			}
			
			completion(error)
		}
	}

	public func addToGroup(_ groupID: String, deviceID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.addToGroup(groupID, deviceID:deviceID)
		{ (error)  in

			if(error == nil){

				self.getGroups(){
					completion(nil)
					return
	
				}

			}

			completion(error)
		}
	}

	
	public func removeFromGroup(_ groupID: String, deviceID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.removeFromGroup(groupID, deviceID:deviceID)
		{ (error)  in

			if(error == nil){

				self.getGroups(){
					completion(nil)
					return
				}

			}

			completion(error)
		}
	}

	public func setGroupLevel(_ groupID: String, toLevel: Int,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.SetGroupLevel(GroupID: groupID,
														  toLevel: toLevel)
		{ (error)  in
			
			if(error == nil){
				completion(nil)

				self.getChangedDevices( )
				return
		}
			
			completion(error)
	
		}
	}

	public func setGroupBackLightLevel(_ groupID: String, toLevel: Int,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.SetGroupBackLightLevel(GroupID: groupID,
														  toLevel: toLevel)
		{ (error)  in
			
			if(error == nil){
				completion(nil)

				self.getChangedDevices( )
				return
		}
			
			completion(error)
	
		}
	}


	public func deleteAction(_ actionID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		
		
//		HCServerManager.shared.deleteGroup(groupID)
//		{ (error)  in
//
//			if(error == nil){
//
//				self.getGroups(){
 					completion(nil)
//					return
//
//				}
//
//			}
//
//			completion(error)
//		}
	}
	
	public func renameAction(_ actionID: String, newName: String,
										completion: @escaping (Error?) -> Void = {_ in }){
	
		completion(nil)

//
//		HCServerManager.shared.renameGroup(groupID: groupID,
//														newName: newName)
//		{ (error)  in
//
//			if(error == nil){
//
//			}
//
//			completion(error)
//		}
	}
 

	public func runAction(_ actionID: String,
									completion: @escaping (Error?) -> Void = {_ in }){
		

		HCServerManager.shared.runAction( actionID)
		{ (error)  in
 		completion(error)
		}

	}


	public func addALDBtoDevice( _ deviceID: String,
										  			groupID: String = "01",
										 		 isCNTL: Bool = false,
												  completion: @escaping (Error?) -> Void = {_ in }){
		
		HCServerManager.shared.addALDBtoDevice(deviceID, groupID: groupID, isCNTL: isCNTL)
		{ (error)  in
			
			if(error == nil){
				
				self.getChangedDevices(){
					completion(nil)
				
				}
				return
				
			}
			
			completion(error)
		}
	}

	public func removeALDBfromDevice( _ deviceID: String, aldbAddr: String,
												 completion: @escaping (Error?) -> Void = {_ in }){
			 
			 
		HCServerManager.shared.removeALDBfromDevice(deviceID, aldbAddr:aldbAddr)
			 { (error)  in

				 if(error == nil){

					 self.getChangedDevices(){
					 completion(nil)
						 return
					 }

				 }

				 completion(error)
			 }
}
	
	

	public func renameEvent(_ eventID: String, newName: String,
										completion: @escaping (Error?) -> Void = {_ in }){
		
		
		HCServerManager.shared.renameEvent(eventID: eventID,
														newName: newName)
		{ (error)  in
			
//			if(error == nil){
//				
//			}
			
			completion(error)
		}
	}

	 
	public func setDeviceLevel(_ deviceID: String, toLevel: Int,
										completion: @escaping (Bool) -> Void = {_ in }){
		
		
		HCServerManager.shared.SetDeviceLevel(deviceID: deviceID,
														  toLevel: toLevel)
		{ (error)  in
			
			if(error == nil){
				self.devices[deviceID]?.level = toLevel
				completion(true)
			}
			else {
				completion(false)
			}
			
		}
	}
	
	

	public func setBackLightLevel(_ deviceID: String, toLevel: Int,
										completion: @escaping (Bool) -> Void = {_ in }){
		
		
		HCServerManager.shared.setBackLightLevel(deviceID: deviceID,
														  toLevel: toLevel)
		{ (error)  in
			
			if(error == nil){
				self.devices[deviceID]?.level = toLevel
				completion(true)
			}
			else {
				completion(false)
			}
			
		}
	}


	public func updateDevice(_ deviceID: String,
									 completion: @escaping (Bool) -> Void = {_ in })  {
		
		guard AppData.serverInfo.validated  else {
			return
		}
		
		
		HCServerManager.shared.RESTCall(urlPath: "devices/\(deviceID)",
												  queries: [URLQueryItem(name: "force", value: "1")])
		{ (response, json, error)  in
			
			if let obj = json  as? RESTDeviceDetails {
				
				print("updated: \(deviceID)")
				
				self.devices[deviceID] = obj
				completion(true)
				
			}
			else{
				completion(false)
			}
			
		}
	}
	
	
	public func getGroups( completion: @escaping () -> Void = {}) {
		guard AppData.serverInfo.validated  else {
			return
		}
		
		HCServerManager.shared.RESTCall(urlPath: "groups")
		{ (response, json, error)  in
			
			if let obj = json  as? RESTGroupList {
					self.groups = obj.groupIDs
			}
			
			completion()
			//				self.updateDeviceLevels()
		}
	}
	
	func deviceKeysWithProperty(_ propertyName: String, defaultKey:String = "") -> Dictionary<String, [String]>{
		
		var collections: 	Dictionary<String, [String]> = [:]
		
		for key in sortedDeviceKeys() {
			if let device =  devices[key] {
				
				let name = device.properties?[propertyName] ?? defaultKey
				
				if var tmp = collections[name] {
					tmp.append(key)
					collections[name] = tmp
				}
				else {
					collections[name] = [key]
					
				}
			}
		}
		
		return collections
	}
	
	
	func sortedDeviceKeys(allKeys: Bool = false) -> [String]{

		
		let filtered =  allKeys
			? devices
			: devices.filter({ !($1.isKeyPad || $1.isPLM) })
		
		let sorted = filtered.sorted { (first, second) -> Bool in
			return  first.value.name.caseInsensitiveCompare(second.value.name) == .orderedAscending
		}
		
		return sorted.map{$0.key}
	}

	
	func sortedGroupKeys() -> [String]{

	 		let sorted = groups.sorted { (first, second) -> Bool in
			return  first.value.name.caseInsensitiveCompare(second.value.name) == .orderedAscending
		}

		return sorted.map{$0.key}
	}

}
