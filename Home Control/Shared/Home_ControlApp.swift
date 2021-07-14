//
//  Home_ControlApp.swift
//  Shared
//
//  Created by Vincent Moscaritolo on 6/27/21.
//

import SwiftUI

@main


struct Home_ControlApp: App {
	
	init() {
		if(AppData.serverInfo.validated) {
			InsteonFetcher.shared.startPolling()
		}
}
	 
	var body: some Scene {
		WindowGroup {
			ContentView()
				
				.onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { (_) in
					if(AppData.serverInfo.validated) {
						InsteonFetcher.shared.startPolling()
					}
				}
				
				.onReceive(NotificationCenter.default.publisher(for: UIApplication.willResignActiveNotification)) { (_) in
					InsteonFetcher.shared.stopPollng()
					
				}
			
		}
		
	}
	
}

enum AssetsColor: String {
	 case backgroundGray
	 case deviceIconColor
	 case colorAccent
	 case colorPrimary
 	 case warningColor
//	 case yellow
}

extension Color {
	 static func appColor(_ name: AssetsColor) -> Color? {
		return Color(UIColor(named: name.rawValue)!)
	 }
}

extension Binding {
	func onChange(_ handler: @escaping (Value) -> Void) -> Binding<Value> {
		Binding(
			get: { self.wrappedValue },
			set: { newValue in
				self.wrappedValue = newValue
				handler(newValue)
			}
		)
	}
}
