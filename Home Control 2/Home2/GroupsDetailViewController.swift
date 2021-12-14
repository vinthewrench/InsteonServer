//
//  GroupsDetailViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/25/21.
//
import UIKit
import Toast

public protocol GroupsDetailViewControllerDelegate  {
	func groupDetailChanged(GroupID: String)
}


class GroupsDetailViewController:  UIViewController,
									  UITableViewDelegate,
									  UITableViewDataSource,
										DeviceCellDelegate,
										DevicePickerControllerDelegate,
										EditableUILabelDelegate,
										FloatingButtonDelegate{
	

	@IBOutlet var tableView: UITableView!
	@IBOutlet var lblTitle: EditableUILabel!
	
	@IBOutlet var btnOff: 	BHButton!
	@IBOutlet var btnOn: 	BHButton!

	var btnFLoat: FloatingButton = FloatingButton()

	var GroupID :String = ""
	var delegate:GroupsDetailViewControllerDelegate? = nil

	var deviceKeys: [String] = []
	var timer = Timer()
	private let refreshControl = UIRefreshControl()

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
		btnFLoat.delegate = self

		tableView.register(DeviceCell.nib, forCellReuseIdentifier: DeviceCell.reuseIdentifier)
		
		tableView.refreshControl = refreshControl
		
		// Configure Refresh Control
		refreshControl.addTarget(self, action: #selector(refreshDeviceTable(_:)), for: .valueChanged)
	}
	
	@objc private func refreshDeviceTable(_ sender: Any) {
		DispatchQueue.main.async {
			self.refreshDeviceKeys(){
				self.refreshControl.endRefreshing()
			}
		}
	}

	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		InsteonFetcher.shared.startPolling()
	
		refreshDeviceKeys()
		self.tableView.reloadData()
		startPolling();
	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		btnFLoat.setup(toView: view)
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		stopPollng();
		InsteonFetcher.shared.stopPollng()
		btnFLoat.remove()
	}
	
 
	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
		
													if(!self.tableView.isEditing){
														self.refreshDeviceKeys()
														self.tableView.reloadData()
														
													}
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
			let sorted = InsteonFetcher.shared.sortedDeviceKeys(allKeys: true)
			self.deviceKeys = sorted.filter{ group.deviceIDs.contains( $0)}
			completion()
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
	
	func tableView(_ tableView: UITableView, editingStyleForRowAt indexPath: IndexPath) -> UITableViewCell.EditingStyle {
		return .delete
	}

	func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
 
		if editingStyle == .delete{
			self.verifyDelete(forRowAt: indexPath)
		}
	}
	
	func verifyDelete(forRowAt indexPath: IndexPath) {
		
		if let device =  InsteonFetcher.shared.devices[deviceKeys[indexPath.row]] {
	 
			let warning = "Are you sure you want to remove: \"\(device.name)?"
			let alert = UIAlertController(title: "Remove Device", message: warning, preferredStyle:.alert)
			let cancelAction = UIAlertAction(title: "Cancel", style: .cancel, handler: { _ in
			
			})
			
			let deleteAction = UIAlertAction(title: "Delete", style: .destructive, handler: { _ in
			
				InsteonFetcher.shared.removeFromGroup(self.GroupID,
																  deviceID:device.deviceID)
				{ (error)  in
					if(error == nil){
						self.refreshDeviceKeys()
						self.tableView.reloadData()
						self.delegate?.groupDetailChanged(GroupID: self.GroupID)
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
			})
			
			alert.addAction(cancelAction)
			alert.addAction(deleteAction)
			self.present(alert, animated: true, completion: nil)
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
				self.delegate?.groupDetailChanged(GroupID: self.GroupID)
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
	
	// MARK: - floating button

	func floatingButtonHit(sender: Any) {
		if(tableView.isEditing) {
			return
		}
			
		if let pickerView = DevicePickerController.create(withDelgate: self,
																		  excludeDeviceIDs: self.deviceKeys) {
			self.show(pickerView, sender: self)
		}
		
	}
	// MARK: - DevicePickerControllerDelegate

	func devicePicked(DeviceID: String?){
		
		if let newDeviceID = DeviceID {
			print(" devicePicked: \(newDeviceID)")
		
			InsteonFetcher.shared.addToGroup(self.GroupID,
															  deviceID:newDeviceID)
			{ (error)  in
				if(error == nil){
					self.refreshDeviceKeys()
					self.tableView.reloadData()
					self.delegate?.groupDetailChanged(GroupID: self.GroupID)
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
	 
	}

	// MARK: - Buttons

	@IBAction func btnOnClicked(_ sender: BHButton) {
	
 		sender.isEnabled = false
		InsteonFetcher.shared.setGroupLevel(GroupID, toLevel: 255) { (error)  in
  			sender.isEnabled = true

		}
	 
	}

	@IBAction func btnOffClicked(_ sender: BHButton) {
		sender.isEnabled = false
		InsteonFetcher.shared.setGroupLevel(GroupID, toLevel: 0) { (error)  in
			sender.isEnabled = true

		}
	}
}
