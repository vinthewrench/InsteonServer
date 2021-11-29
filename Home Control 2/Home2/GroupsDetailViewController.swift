//
//  GroupsDetailViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/25/21.
//
import UIKit
import Toast

 
class GroupsDetailViewController:  UIViewController,
									  UITableViewDelegate,
									  UITableViewDataSource,
										DeviceCellDelegate,
										EditableUILabelDelegate  {
	
 
	@IBOutlet var tableView: UITableView!
	@IBOutlet var lblTitle: EditableUILabel!

	var GroupID :String = ""
	
	var deviceKeys: [String] = []
	var timer = Timer()
	
	class func create(withGroupID: String) -> GroupsDetailViewController? {
		let storyboard = UIStoryboard(name: "GroupsDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "GroupsDetailViewController") as? GroupsDetailViewController
		
		if let vc = vc {
			vc.GroupID = withGroupID
		}
		
		return vc
	}
 

	override func viewDidLoad() {
		super.viewDidLoad()
		lblTitle.delegate = self
		tableView.register(DeviceCell.nib, forCellReuseIdentifier: DeviceCell.reuseIdentifier)
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		InsteonFetcher.shared.startPolling()
	
		refreshDeviceKeys()
		self.tableView.reloadData()
		startPolling();
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		stopPollng();
		InsteonFetcher.shared.stopPollng()
	}

	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
		
			self.refreshDeviceKeys()
			self.tableView.reloadData()
	 
		})
	}
	
	func stopPollng(){
		timer.invalidate()
	}


	
	func refreshDeviceKeys(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		if let group =  InsteonFetcher.shared.groups[GroupID] {
			lblTitle.text = group.name
			let sorted = InsteonFetcher.shared.sortedDeviceKeys()
			self.deviceKeys = sorted.filter{ group.deviceIDs.contains( $0)}
		}
	}

	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		return deviceKeys.count
	}
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	DeviceCell.reuseIdentifier) as? DeviceCell{
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			cell.delegate = self;
			cell.deviceID = deviceKeys[indexPath.row]
			
			if let device =  InsteonFetcher.shared.devices[deviceKeys[indexPath.row]] {
				
				cell.lblName.text = device.name
				cell.img.image = device.deviceImage()
				
				if let level = device.level {
					cell.sw.isEnabled = true
					cell.sw.isOn = level > 0
				}
				else {
					cell.sw.isEnabled = false
					cell.sw.isOn = false
				}
			}
			
			return cell
		}
		return UITableViewCell()
		
	}
 
	func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
		
		let deviceID = deviceKeys[indexPath.row]
		if let detailView = DeviceDetailViewController.create(withDeviceID: deviceID) {
			
			self.show(detailView, sender: self)
		}
		
	}
	
	
	func switchDidChange(deviceID: String, newState:Bool){
		
 		stopPollng()
		InsteonFetcher.shared.setDeviceLevel(deviceID, toLevel: newState ? 255:0) {_ in
			self.startPolling()
		}
	}

	func renameGroup(newName:String){

		InsteonFetcher.shared.renameGroup(GroupID, newName: newName)
		{ (error)  in

			if(error == nil){
				self.refreshDeviceKeys()
			}
			else {
				Toast.text(error?.localizedDescription ?? "Error",
							  config: ToastConfiguration(
								autoHide: true,
								displayTime: 1.0
								//												attachTo: self.vwError
							  )).show()

			}
		}
	}
	
	func editMenuTapped(sender: UILabel) {
		
		let alert = UIAlertController(title:  NSLocalizedString("Rename Group", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("Rename", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
			textField.placeholder =  "Group Name"
			textField.text = self.lblTitle.text
		}) { result in
			
			switch result {
			case let .ok(String:newName):
				self.renameGroup( newName: newName);
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)
		}
}
