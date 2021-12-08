//
//  DevicesViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//

import UIKit
import Toast

class DevicesViewController:  MainSubviewViewController,
										MainSubviewViewControllerDelegate,
										DeviceLinkViewControllerDelegate,
										UITableViewDelegate,
										UITableViewDataSource,
										DeviceCellDelegate{
	
	
	@IBOutlet var tableView: UITableView!
	
	private let refreshControl = UIRefreshControl()
	
	//	var deviceKeys: [String] = []
	var groupedDeviceKeys: Dictionary<String, [String]> = [:]
	var rowKeys:[String] = []
	
	var timer = Timer()
	
	static let shared: DevicesViewController! = {
		
		let storyboard = UIStoryboard(name: "DevicesView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DevicesViewController") as? DevicesViewController
		
		return vc
	}()
	
	
	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		tableView.register(DeviceCell.nib, forCellReuseIdentifier: DeviceCell.reuseIdentifier)
		
		tableView.refreshControl = refreshControl
		
		// Configure Refresh Control
		refreshControl.addTarget(self, action: #selector(refreshDeviceTable(_:)), for: .valueChanged)
	}
	
	@objc private func refreshDeviceTable(_ sender: Any) {
		DispatchQueue.main.async {
			self.stopPollng();
			self.refreshDevices(clearAll: true){
				self.startPolling();
				self.refreshControl.endRefreshing()
			}
		}
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		//	mainView?.btnAdd.isHidden = true
		
		InsteonFetcher.shared.startPolling()
		startPolling();
		refreshDevices()
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		stopPollng();
		InsteonFetcher.shared.stopPollng()
		
	}
	
	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
													if(!self.tableView.isEditing){
														
														self.refreshDevices()
														//			self.deviceKeys = InsteonFetcher.shared.sortedDeviceKeys()
														//			self.tableView.reloadData()
													}
												})
	}
	
	func stopPollng(){
		timer.invalidate()
	}
	
	let defaultKeyName = "_"
	
	func refreshDevices(clearAll: Bool = false, completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		InsteonFetcher.shared.getChangedDevices(clearAll:clearAll) {
			//			self.deviceKeys = InsteonFetcher.shared.sortedDeviceKeys()
			self.groupedDeviceKeys =  InsteonFetcher.shared.deviceKeysWithProperty( "collection",
																											defaultKey:self.defaultKeyName)
			
			self.rowKeys = self.groupedDeviceKeys.keys.sorted{ (first, second) -> Bool in
				return  first.caseInsensitiveCompare(second) == .orderedAscending
			}
			
			
			self.tableView.reloadData()
			completion()
		}
	}
	
	
	func numberOfSections(in tableView: UITableView) -> Int {
		return rowKeys.count
	}
	
	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		
		return self.groupedDeviceKeys[rowKeys[section]]?.count ?? 0
	}
	
	
	func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
		
		var title: String? = nil
		
		if rowKeys.count  > 1 {
			title = rowKeys[section]
			
			if title == defaultKeyName {
				title = "Others…"
			}
		}
		
		return title
	}
	
	func deviceID( for indexPath: IndexPath ) -> String {
		let deviceID = groupedDeviceKeys[ rowKeys[indexPath.section]]![indexPath.row]
		
		return deviceID
	}
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	DeviceCell.reuseIdentifier) as? DeviceCell{
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			cell.delegate = self;
			
			let deviceID = deviceID(for: indexPath)
			
			if let device =  InsteonFetcher.shared.devices[deviceID] {
				
				cell.deviceID = deviceID
				
				if(device.name.isEmpty){
					cell.lblName.text = "Unnamed Device: \(deviceID)"
					cell.lblName.textColor = UIColor.darkGray
				}
				else {
					cell.lblName.text = device.name
					cell.lblName.textColor = UIColor.black
				}
				
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
		
		let deviceID = deviceID(for: indexPath)
		if let detailView = DeviceDetailViewController.create(withDeviceID: deviceID) {
			
			self.show(detailView, sender: self)
		}
		
	}
	
	func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
		if editingStyle == .delete{
			
			self.verifyDelete(forRowAt: indexPath)
		}
	}
	
	func switchDidChange(deviceID: String, newState:Bool){
		
		stopPollng()
		InsteonFetcher.shared.setDeviceLevel(deviceID, toLevel: newState ? 255:0) {_ in
			self.startPolling()
		}
	}
	
	func verifyDelete(forRowAt indexPath: IndexPath) {
		
		let deviceID = deviceID(for: indexPath)
		
		if let device =  InsteonFetcher.shared.devices[deviceID] {
			
			let deviceName = device.name.isEmpty ? device.deviceID : device.name
			let warning = "Are you sure you want to delete the device: \"\(deviceName)?"
			let alert = UIAlertController(title: "Delete Device", message: warning, preferredStyle:.alert)
			let cancelAction = UIAlertAction(title: "Cancel", style: .cancel, handler: { _ in
			})
			
			let deleteAction = UIAlertAction(title: "Delete", style: .destructive, handler: { _ in
				
				InsteonFetcher.shared.deleteDevice(deviceID) { (error)  in
					
					if(error == nil){
						
						self.stopPollng();
						self.refreshDevices(clearAll: true){
							self.startPolling();
						}
						
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
	
	// MARK: - MainSubviewViewControllerDelegate
	
	func addButtonHit(_ sender: UIButton){
		
		if let linkDeviceView = DeviceLinkViewController.create() {
			linkDeviceView.delegate = self
			self.show(linkDeviceView, sender: self)
		}
		
	}
	
	// MARK: - DeviceLinkViewControllerDelegate
	func newDeviceAdded(_ sender: UIViewController, deviceID: String?){
		
		sender.dismiss(animated: true) {
			
			if let deviceID = deviceID,
				let detailView = DeviceDetailViewController.create(withDeviceID: deviceID) {
				self.show(detailView, sender: self)
			}
		}
	}
	
}
