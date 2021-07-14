//
//  ServerInfoView.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 6/27/21.
//

import SwiftUI

struct ServerInfoView: View {
	
	@State var apiKey: String =  AppData.serverInfo.apiKey ?? ""
	@State var apiSecret: String = AppData.serverInfo.apiSecret ?? ""
	
	@State var statusStr: String = ""
	@State var systemTimeStr: String = ""
	@State var latLongStr: String = ""
	@State var sunRiseStr: String = ""
	@State var sunSetStr: String = ""
	@State var versionStr: String = ""
	@State var upTimeStr: String = ""
	
	@State var width1: CGFloat // = 100.0
	
	var solarTimeFormat: DateFormatter {
		 let formatter = DateFormatter()
		formatter.dateFormat = "hh:mm:ss a"
		formatter.timeZone = TimeZone(abbreviation: "UTC")
		return formatter
	}

	var upTimeFormatter: DateComponentsFormatter {
		let formatter = DateComponentsFormatter()
		formatter.allowedUnits = [.day, .hour, .minute, .second]
		formatter.unitsStyle = .short
		return formatter
	}
	

	func updateStatus() {
		HCServerManager.shared.RESTCall(urlPath: "status",
											  headers:nil,
											  queries: nil) { (response, json, error)  in
			
			if let obj = json as? RESTStatus {
				statusStr = obj.stateString
			}
		}
	}
	
	func updateDateInfo() {
		HCServerManager.shared.RESTCall(urlPath: "date",
											  headers:nil,
											  queries: nil) { (response, json, error)  in
			
			if let obj = json as? RESTDateInfo {
				systemTimeStr = obj.date
				latLongStr = "\(obj.latitude)\n\(obj.longitude)"
				
				upTimeStr = upTimeFormatter.string(from: TimeInterval(obj.uptime))!

				var t:TimeInterval
				var date:Date

				let lastMidnight = obj.midnight
				t = TimeInterval(lastMidnight + (obj.sunSet * 60))
				date 	= Date(timeIntervalSince1970: TimeInterval(t))
				sunSetStr = solarTimeFormat.string(from: date)
				
				t = TimeInterval(lastMidnight + (obj.civilSunSet * 60))
				date 	= Date(timeIntervalSince1970: TimeInterval(t))
				sunSetStr = sunSetStr + "\n" + solarTimeFormat.string(from: date)
				
				t = TimeInterval(lastMidnight + (obj.civilSunRise * 60))
				date 	= Date(timeIntervalSince1970: TimeInterval(t))
				sunRiseStr = solarTimeFormat.string(from: date)
				
				t = TimeInterval(lastMidnight + (obj.sunRise * 60))
				date 	= Date(timeIntervalSince1970: TimeInterval(t))
				sunRiseStr = sunRiseStr + "\n" + solarTimeFormat.string(from: date)
	 
			}
		}
	}
	
	func updateVersionInfo() {
		HCServerManager.shared.RESTCall(urlPath: "version",
											  headers:nil,
											  queries: nil) { (response, json, error)  in
			
					if let obj = json as? RESTVersion {
					versionStr = "\(obj.version)\n\(obj.timestamp)"

			}
		}
	}

 	let timer = Timer.publish(every: 5, on: .main, in: .common).autoconnect()
 
	var body: some View {
	 
		VStack(spacing: 10){
			
			Group(){
				NavigationLink(destination: PLMView()) {
					
					HStack( spacing: 8.0) {
						Text("Status:")
							.font(.headline)
							.frame(maxWidth: width1, alignment: .leading)
							.foregroundColor(Color.secondary)
						
						Text("\(statusStr)")
							.frame(maxWidth: .infinity, alignment: .leading)
							.font(.body)
							.foregroundColor(Color.secondary)
					}
				}
				
				Divider()
				
				HStack( spacing: 8.0) {
					Text("Time:")
						.font(.headline)
						.frame(maxWidth: width1, alignment: .leading)
						.foregroundColor(Color.secondary)
					
					Text("\(systemTimeStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
				}
				
				Divider()
				
				HStack( spacing: 8.0) {
					Text("Lat/Long:")
						.font(.headline)
						.frame(maxWidth: width1, alignment: .leading)
						.foregroundColor(Color.secondary)
					
					Text("\(latLongStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
						.CopyableTextStyle(latLongStr)
				}
				
				Divider()
				
				HStack( spacing: 8.0) {
					Text("Sun Rise:")
						.font(.headline)
						.frame(maxWidth: width1, alignment: .leading)
						.foregroundColor(Color.secondary)
					
					
					Text("\(sunRiseStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
						.CopyableTextStyle(sunRiseStr)
				}
				
				Divider()
				
				HStack( spacing: 8.0) {
					Text("Sun Set:")
						.font(.headline)
						.frame(maxWidth: width1, alignment: .leading)
						.foregroundColor(Color.secondary)
					
					Text("\(sunSetStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
						.CopyableTextStyle(sunSetStr)
				}
			}
			
			Divider()
			
			Group{
				
				HStack(spacing: 8.0) {
					Text("Version:")
						.font(.headline)
						.foregroundColor(Color.secondary)
						
						.frame(maxWidth: width1, alignment: .leading)
					Text("\(versionStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
						.CopyableTextStyle(versionStr)
				}
				Divider()
				
				HStack(spacing: 8.0) {
					Text("Uptime:")
						.font(.headline)
						.foregroundColor(Color.secondary)
						
						.frame(maxWidth: width1, alignment: .leading)
					Text("\(upTimeStr)")
						.frame(maxWidth: .infinity, alignment: .leading)
						.font(.body)
						.foregroundColor(Color.secondary)
						.CopyableTextStyle(upTimeStr)
				}
			}
		}
	 
		.onReceive(timer, perform: { _ in
			if(AppData.serverInfo.validated) {
				updateStatus()
				updateDateInfo()
			}
		})
		
		.onAppear(){
			if(AppData.serverInfo.validated) {
				updateStatus()
				updateDateInfo()
				updateVersionInfo()
			}
		}
	}
	
}

struct ServerInfoView_Previews: PreviewProvider {
    static var previews: some View {
		ServerInfoView(width1: 150.0)
    }
}
