//
//  DevicePickerController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 11/30/21.
//

import UIKit

public protocol DevicePickerControllerDelegate  {
	func devicePicked(DeviceID: String?)
}


class DevicePickerController:  	UIViewController,
									  		UITableViewDelegate,
									  		UITableViewDataSource  {

	@IBOutlet var tableView: UITableView!
	var delegate:DevicePickerControllerDelegate? = nil

	var excludedDeviceIDs: [String]? = []
	
	var deviceKeys: [String] = []

	
	class func create(withDelgate: DevicePickerControllerDelegate? = nil,
							excludeDeviceIDs: [String]? = []
							) -> DevicePickerController? {
		let storyboard = UIStoryboard(name: "DevicePicker", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DevicePickerController") as? DevicePickerController
		
		if let vc = vc {
			vc.delegate = withDelgate
			vc.excludedDeviceIDs = excludeDeviceIDs
		}
	
		return vc
	}
 
	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		tableView.register(DeviceCell.nib, forCellReuseIdentifier: DeviceCell.reuseIdentifier)
	
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		refreshDeviceKeys()
	}
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
	
	}
	

	func refreshDeviceKeys(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
 
		InsteonFetcher.shared.getChangedDevices() {
			var  keys = InsteonFetcher.shared.sortedDeviceKeys()
			if let excluded = self.excludedDeviceIDs {
				keys = keys.filter{ !excluded.contains($0) }
			}
	 		self.deviceKeys = keys
 
			self.tableView.reloadData()
			completion()
		}
		
	}

	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		return deviceKeys.count
	}

	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	DeviceCell.reuseIdentifier) as? DeviceCell{
			
			cell.accessoryType = .none
			cell.selectionStyle = .none
			cell.deviceID = self.deviceKeys[indexPath.row]
			
			if let device =  InsteonFetcher.shared.devices[deviceKeys[indexPath.row]] {
				
				cell.lblName.text = device.name
				cell.img.image = device.deviceImage()
				cell.sw.isHidden = true
	
			}
			
			return cell
		}
		return UITableViewCell()
		
	}


	func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
		
		let deviceID = deviceKeys[indexPath.row]
		
		self.delegate?.devicePicked(DeviceID: deviceID)
		
		self.dismiss(animated: true)
	}

	@IBAction func btnCancelClicked(_ sender: Any) {
		
		self.delegate?.devicePicked(DeviceID: nil)
		self.dismiss(animated: true)

	}


}
 
