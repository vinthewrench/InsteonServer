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



class SchedulesViewController: MainSubviewViewController,
										 MainSubviewViewControllerDelegate,
										 UITableViewDelegate,
										 UITableViewDataSource  {
	
		
	@IBOutlet var tableView: UITableView!
	
	var sortedTimedKeys:[String] = []
	var sortedTriggerKeys:[String] = []
	var events: Dictionary<String, RESTEvent> = [:]
	var futureEvents: [String] = []
	var solarTimes : ServerDateInfo? = nil
	
	private let refreshControl = UIRefreshControl()

	
	var solarTimeFormat: DateFormatter {
		let formatter = DateFormatter()
		formatter.dateFormat = "h:mm a"
		formatter.timeZone = TimeZone(abbreviation: "UTC")
		return formatter
	}
	
	static let shared: SchedulesViewController! = {
		
		let storyboard = UIStoryboard(name: "SchedulesView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "SchedulesViewController") as? SchedulesViewController
		
		return vc
	}()
	
	
	
	// MARK: - view lifetime
	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		self.tableView.tableFooterView = UIView()
		//		self.tableView.separatorStyle = .none
		
		tableView.delegate = self
		tableView.dataSource = self
		
		tableView.refreshControl = refreshControl
		
		// Configure Refresh Control
		refreshControl.addTarget(self, action: #selector(refreshTable(_:)), for: .valueChanged)

		tableView.register(
			ScheduleTableHeaderView.nib,
			forHeaderFooterViewReuseIdentifier:
				ScheduleTableHeaderView.reuseIdentifier)

	}
	
	@objc private func refreshTable(_ sender: Any) {
		DispatchQueue.main.async {
			self.refreshSchedules(){
				self.refreshControl.endRefreshing()
			}
		}
	}

	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
	//	mainView?.btnAdd.isHidden = false

		self.refreshSchedules()
	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
	}
	
	func refreshSchedules(completion: @escaping () -> Void = {}) {
		
//		guard AppData.serverInfo.validated  else {
//			completion()
//			return
//		}
		
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
				
				self.events = events.eventIDs
				self.sortedTimedKeys = []
				self.sortedTriggerKeys = []
				self.futureEvents = events.future ?? []
	
				// split the keys into timed events and Trigger events
					
			}
			else
			{
				self.events = [:]
			}
			
			dp.notify(queue: .main) {
				
				let timedEventIDs = self.events.filter({  $1.isTimedEvent() })
				if let st = self.solarTimes {
				
					let sortedTimedEvents = timedEventIDs.sorted { (first, second) -> Bool in
						let t1 = first.value.trigger.triggerDate(st)
						let t2 = second.value.trigger.triggerDate(st)
						return t1 < t2
					}
					self.sortedTimedKeys  = sortedTimedEvents.map{$0.key}
				}
				
				let trigerEventIDs = self.events.filter({  !$1.isTimedEvent() })
				let sortedTriggerEvents = trigerEventIDs.sorted { (first, second) -> Bool in
					return  first.value.name.caseInsensitiveCompare(second.value.name) == .orderedAscending
				}
				self.sortedTriggerKeys = sortedTriggerEvents.map{$0.key}
				

				
				self.tableView.reloadData()
				completion()
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
	
	func tableView(_ tableView: UITableView, editingStyleForRowAt indexPath: IndexPath) -> UITableViewCell.EditingStyle {
		return .delete
	}
	
	func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
		if editingStyle == .delete{
			print("Deleted Row")
			//				days.remove(at: indexPath.row)
			//					tableView.deleteRows(at: [indexPath], with: .left)
		}
	}
	
	
	func cellForTimedEvent(eventID: String, event: RESTEvent) -> UITableViewCell{
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	SchedulesTimedCell.cellReuseIdentifier) as? SchedulesTimedCell{
			
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			
			cell.lblName?.text = event.name
			
			let  isFutureEvent =  self.futureEvents.contains(eventID)
			
			if let str = eventTriggerString(event.trigger) {
				cell.lblTime?.text = str
			}
			
	 		cell.img.image =  event.imageForTrigger()
			cell.img.tintColor = isFutureEvent ? UIColor.systemOrange : UIColor.systemBlue
				 
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
			
			cell.img.image = event.imageForTrigger()
			
			return cell
			
		}
		
		return UITableViewCell()
		
	}
	
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		switch(indexPath.section){
		
		case 0:
			if indexPath.row <= sortedTimedKeys.count{
				let eventID = sortedTimedKeys[indexPath.row]
				if let event =  events[eventID] {
					return cellForTimedEvent(eventID: eventID, event: 	event)
				}
			}
			
		case 1:
			if indexPath.row <= sortedTimedKeys.count{
				let eventID = sortedTimedKeys[indexPath.row]
				if let event =  events[eventID] {
					return cellForTriggerEvent( event)
		
				}
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
	// MARK: -
	
	
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
	
	// MARK: - MainSubviewViewControllerDelegate

	func addButtonHit(_ sender: UIButton){
		createNewEvent()
	}

	func createNewEvent() {
		if(tableView.isEditing) {
			return
		}
		
		// create the alert
				 let alert = UIAlertController(title: "Lazy Programmer",
														 message: "code not written yet..  send money.",
														 preferredStyle: UIAlertController.Style.alert)

				 // add an action (button)
				 alert.addAction(UIAlertAction(title: "OK", style: UIAlertAction.Style.default, handler: nil))

				 // show the alert
				 self.present(alert, animated: true, completion: nil)
		
		}
	 
	
}
