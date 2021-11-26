//
//  DevicesViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//

import UIKit


class DevicesViewController:  UIViewController,
										UITableViewDelegate,
										UITableViewDataSource,
										DeviceCellDelegate  {
	
	@IBOutlet var tableView: UITableView!
	
	private let refreshControl = UIRefreshControl()
	
	var deviceKeys: [String] = []
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
			self.refreshDevices(){
				self.refreshControl.endRefreshing()
			}
		}
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
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
		
			self.deviceKeys = InsteonFetcher.shared.sortedDeviceKeys()
			self.tableView.reloadData()
	 
		})
	}
	
	func stopPollng(){
		timer.invalidate()
	}

	
	
	func refreshDevices(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		InsteonFetcher.shared.getChangedDevices() {
			self.deviceKeys = InsteonFetcher.shared.sortedDeviceKeys()
			self.tableView.reloadData()
			completion()
		}
	}

	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {

		return self.deviceKeys.count
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


}
