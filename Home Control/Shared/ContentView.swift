//
//  ContentView.swift
//  Shared
//
//  Created by Vincent Moscaritolo on 6/27/21.
//

import SwiftUI


//
//extension TabSelect: Equatable {
//	 static func == (lhs: TabSelect, rhs: TabSelect) -> Bool {
//		return lhs.selection == rhs.selection
//	 }
//}


//class TabSelect: ObservableObject {
//
//	static let shared: TabSelect = {
//			let instance = TabSelect()
//			return instance
//		}()
//
//	@Published var selection = TabName(rawValue: AppData.serverInfo.tabSelection) ?? TabName.settings {
//		willSet {
//			objectWillChange.send()
//		}
//		didSet	{
//			AppData.serverInfo.tabSelection = Int(self.selection.rawValue)
//			}
//	}
//}

enum TabName: Int {
	case keypads  = 0
	case devices
	case schedules
	case groups
	case settings
}

extension TabName: Equatable {
	public static func == (lhs: TabName, rhs: TabName) -> Bool {
		return lhs.rawValue == rhs.rawValue
	 }
}
//
//extension TabSelection: Equatable {
//	public static func == (lhs: TabSelection, rhs: TabSelection) -> Bool {
//		return lhs.selection == rhs.selection
//	 }
//}

public class TabSelection: ObservableObject {
	//	@Published var selection:Int  = 0
	
	static let shared: TabSelection = {
		let instance = TabSelection()
		// Setup code
		return instance
	}()
 
	@Published var selection =  TabName(rawValue: AppData.serverInfo.tabSelection) ?? TabName.settings
	{
		willSet {
			objectWillChange.send()
		}
		didSet	{
			AppData.serverInfo.tabSelection = Int(self.selection.rawValue)
		}
		
	}
}
 
struct ContentView: View {
	
	@ObservedObject var TabSelect = TabSelection.shared
	
	var body: some View {
		TabView(selection: $TabSelect.selection){
			
			NavigationView {
				KeypadsView()
					.navigationBarTitle("", displayMode: .inline)
					.navigationBarItems(leading:
												Text("Keypads").font(Font.custom("Quicksand-Bold", size: 24)))
			}
			
			.tabItem {
				Image(systemName: "keyboard")
				Text("Keypads")
			}
			.tag(TabName.keypads)
			
			NavigationView {
				DevicesView()
					.navigationBarTitle("", displayMode: .inline)
					.navigationBarItems(leading:
												Text("Devices").font(Font.custom("Quicksand-Bold", size: 24)))
			}
//			.onAppear()	{
//				InsteonFetcher.shared.startPolling()
//			}
//			.onDisappear(){
//				InsteonFetcher.shared.stopPollng()
//	
//			}
			.tabItem {
				Image(systemName: "lightbulb")
				Text("devices")
			}
			.tag(TabName.devices)
			
			NavigationView {
				EventsView()
					
					.navigationBarTitle("", displayMode: .inline)
					.navigationBarItems(leading:
												Text("Schedules").font(Font.custom("Quicksand-Bold", size: 24)))
				
				
			}
			.tabItem {
				Image(systemName: "clock")
				Text("Schedules")
			}
			.tag(TabName.schedules)
			
			NavigationView {
				GroupsView()
					.navigationBarTitle("", displayMode: .inline)
					.navigationBarItems(leading:
												Text("Groups").font(Font.custom("Quicksand-Bold", size: 24)))
			}
			.tabItem {
				Image(systemName: "square.grid.3x1.folder.badge.plus")
				Text("Groups")
				
			}
			.tag(TabName.groups)
			
			NavigationView {
				
				Setupview()
					.navigationBarTitle("", displayMode: .inline)
					.navigationBarItems(leading:
												Text("Settings").font(Font.custom("Quicksand-Bold", size: 24)))
			}
			.tabItem {
				Image(systemName: "gear")
				Text("Settings")
			}
			.tag(TabName.settings)
			
		}
	}
	
}
 
