//
//  SchedulesViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//


import UIKit

final class SchedulesTriggeredCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "SchedulesTriggeredCell"
	
	@IBOutlet var lblName	: UILabel!
	@IBOutlet var img	: UIImageView!
	
	override func awakeFromNib() {
		super.awakeFromNib()
	}
}


final class SchedulesTimedCell: UITableViewCell {

	public static let cellReuseIdentifier = "SchedulesTimedCell"

  @IBOutlet var lblName	: UILabel!
  @IBOutlet var lblTime	: UILabel!
  @IBOutlet var img		: UIImageView!
	
  override func awakeFromNib() {
	  super.awakeFromNib()
  }
}


class SchedulesViewController: UIViewController,
										 UITableViewDelegate,
										 UITableViewDataSource  {
	
	@IBOutlet var tableView: UITableView!
	
	
	var sortedTimedKeys:[String] = []
	var sortedTriggerKeys:[String] = []
	var events: Dictionary<String, RESTEvent> = [:]
	var solarTimes : ServerDateInfo? = nil

	
	var solarTimeFormat: DateFormatter {
		let formatter = DateFormatter()
		formatter.dateFormat = "hh:mm a"
		formatter.timeZone = TimeZone(abbreviation: "UTC")
		return formatter
	}

	static let shared: SchedulesViewController! = {
		
		let storyboard = UIStoryboard(name: "SchedulesView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "SchedulesViewController") as? SchedulesViewController
		
		return vc
	}()
	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		self.tableView.tableFooterView = UIView()
		//		self.tableView.separatorStyle = .none
		
		tableView.delegate = self
		tableView.dataSource = self
		
		tableView.register(
			ScheduleTableHeaderView.nib,
			forHeaderFooterViewReuseIdentifier:
				ScheduleTableHeaderView.reuseIdentifier)
		
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		self.refreshSchedules()
	}
	
	
	
	func refreshSchedules() {
		
		guard AppData.serverInfo.validated  else {
			return
		}
		
		let dp = DispatchGroup()
		
		dp.enter()
		HomeControl.shared.fetchData(.date) { result in
			dp.leave()
			if case .success(let solarTimes as ServerDateInfo) = result {
				self.solarTimes = solarTimes
			}
			else {
				self.solarTimes = nil
			}
		}
		
		
		dp.enter()
		HomeControl.shared.fetchData(.events) { result in
			dp.leave()
			if case .success(let events as RESTEventList) = result {
				
				// split the keys into timed events and Trigger events
				
				let timedEventIDs = events.eventIDs.filter({  $1.isTimedEvent() })
				let sortedTimedEvents = timedEventIDs.sorted { (first, second) -> Bool in
					return  first.value.name.caseInsensitiveCompare(second.value.name) == .orderedAscending
				}
				self.sortedTimedKeys  = sortedTimedEvents.map{$0.key}
				
				let trigerEventIDs = events.eventIDs.filter({  !$1.isTimedEvent() })
				let sortedTriggerEvents = trigerEventIDs.sorted { (first, second) -> Bool in
					return  first.value.name.caseInsensitiveCompare(second.value.name) == .orderedAscending
				}
				self.sortedTriggerKeys = sortedTriggerEvents.map{$0.key}
				
				self.events = events.eventIDs
				
			}
			else
			{
				self.sortedTimedKeys = []
				self.sortedTriggerKeys = []
				self.events = [:]
			}
			
			dp.notify(queue: .main) {
				self.tableView.reloadData()
			}
		}
	}
	
 
	// MARK: - table view
	
	func numberOfSections(in tableView: UITableView) -> Int {
		return 2
	}
	
	
	func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
		
		guard let headerView = tableView.dequeueReusableHeaderFooterView(
			withIdentifier: ScheduleTableHeaderView.reuseIdentifier)
					as? ScheduleTableHeaderView
		else {
			return nil
		}
		
//		
//		let backgroundView = UIView(frame: headerView.bounds)
//		backgroundView.backgroundColor = UIColor(white: 0.5, alpha: 0.5)
//		headerView.backgroundView = backgroundView
//		
		var text = ""
		
		switch(section){
		case 0:
			text = "SCHEDULED"
			
		case 1:
			text = "TRIGGERS"
			
		default:
			break;
		}
		
		headerView.title?.text = text
		return headerView
	}
	
	func tableView(_ tableView: UITableView,
						heightForHeaderInSection section: Int) -> CGFloat {
		return UITableView.automaticDimension
	}
	
	func tableView(_ tableView: UITableView,
						estimatedHeightForHeaderInSection section: Int) -> CGFloat {
		return 50.0
	}
	
	
	//	func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
	//		return 60.0
	//	}
	//
	//
	// number of rows in table view
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		
		var count = 0;
		
		switch(section){
		case 0:
			count = sortedTimedKeys.count
			
		case 1:
			count = sortedTriggerKeys.count
			
		default:
			count = 0;
			
		}
		
		return count
	}
	
	func deviceEventString(_ trig: RESTEventTrigger)  ->String? {
 		var result: String?
	
		// NOTE: -  VINNIE WRITE ME
		
//		if  let ig = trig.insteon_group,
//			 let cmd = trig.cmd,
//			 let deviceID = trig.deviceID,
//				let device = fetcher.devices[deviceID],
//				(device.isKeyPad) {
//
//			if let ig = ig.hex?.value {
//				let keyName = 	String(UnicodeScalar(ig + 64)!)
//				var command: String = ""
//				switch cmd {
//				case "0x11":
//					command = "On"
//
//				case "0x13":
//					command = "Off"
//
//				default:
//					break
//				}
//				result = "\(device.name) [\(keyName)] \(command)"
//			}
//		}
		return result
	}

	func eventTriggerString(_ trig: RESTEventTrigger) ->String? {
		var result: String?
		
		
		if( trig.timeBase != nil) {
			if let st = solarTimes {
				let date = trig.triggerDate(st)
				result = solarTimeFormat.string(from: date)
			}
 		}
		else if( trig.event != nil) {
	//		result =  trig.event
		}
		else {
 			result =  deviceEventString(trig)
		}
		
		return result
	}
	
	func imageForEvent(_ event: RESTEvent) -> UIImage {
		
		var image:UIImage?
		
		if let timeBase = event.trigger.timeBase {
			
			if timeBase == 1 {
				image = UIImage(systemName: "clock")
			}
			else if timeBase == 2 {
				image = UIImage(systemName: "sunrise")
			}
			else if timeBase == 3 {
				image = UIImage(systemName: "sunset")
			}
			else if timeBase == 4 {
				image = UIImage(systemName: "sunrise.fill")
			}
			else if timeBase == 5 {
				image = UIImage(systemName: "sunset.fill")
			}
			else {
				image = UIImage(systemName: "questionmark")
			}
		}
		else if( event.trigger.event == "startup")
		{
			image = UIImage(systemName: "power")
		}
		else {
			image = UIImage(systemName: "questionmark")
		}
		
		return image ??  UIImage()
	}
	
	
	func cellForTimedEvent(_ event: RESTEvent) -> UITableViewCell{
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	SchedulesTimedCell.cellReuseIdentifier) as? SchedulesTimedCell{
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			
			cell.lblName?.text = event.name
			
			if let str = eventTriggerString(event.trigger) {
				cell.lblTime?.text = str
			}

			
			cell.img.image = imageForEvent(event)
			
			return cell
			
		}
		
		return UITableViewCell()
		
	}
	
	func cellForTriggerEvent(_ event: RESTEvent) -> UITableViewCell{
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	SchedulesTriggeredCell.cellReuseIdentifier) as? SchedulesTriggeredCell{
			
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			
			cell.lblName?.text = event.name
			
			cell.img.image = imageForEvent(event)
			
			return cell
			
		}
		
		return UITableViewCell()
		
	}
	
	
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		switch(indexPath.section){
			
		case 0:
			if indexPath.row <= sortedTimedKeys.count,
				let event =  events[sortedTimedKeys[indexPath.row]] {
				return cellForTimedEvent( event)
			}
			
		case 1:
			if indexPath.row <= sortedTriggerKeys.count,
				let event =  events[sortedTriggerKeys[indexPath.row]] {
				return cellForTriggerEvent( event)
			}
			
			
		default:
			break
		}
		
		return UITableViewCell()
		
	}
	
	
	func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
		
	 	var eventKey:String? = nil
		
		switch(indexPath.section){
			
		case 0:
			if indexPath.row <= sortedTimedKeys.count{
				eventKey = sortedTimedKeys[indexPath.row];
 			}
			
		case 1:
			if indexPath.row <= sortedTriggerKeys.count {
				eventKey = sortedTriggerKeys[indexPath.row];
			}
			
		default:
			break
		}
		
		if let eventKey = eventKey  {
			if let detailView = ScheduleDetailViewController.create(withEventID: eventKey) {
				
				self.show(detailView, sender: self)
				}
 
		}
	}
}
