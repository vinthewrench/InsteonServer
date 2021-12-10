//
//  DeviceALDBViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/1/21.
//

import Foundation

import UIKit
import Toast


final class DeviceALDBCell1: UITableViewCell {
	
	public static let cellReuseIdentifier = "DeviceALDBCell1"
	
	@IBOutlet var lblAddr	: UILabel!
	@IBOutlet var lblGroup	: UILabel!
	@IBOutlet var lblDeviceID	: UILabel!
	@IBOutlet var imgDot			: UIImageView!
	
	override func awakeFromNib() {
		super.awakeFromNib()
	}
}

final class DeviceALDBCell2: UITableViewCell {
	
	public static let cellReuseIdentifier = "DeviceALDBCell2"
	
	@IBOutlet var lblAddr	: UILabel!
	@IBOutlet var lblGroup	: UILabel!
	@IBOutlet var lblDeviceName	: UILabel!
	@IBOutlet var lblDeviceID	: UILabel!
	@IBOutlet var imgDot			: UIImageView!
	
	override func awakeFromNib() {
		super.awakeFromNib()
	}
}


class DeviceALDBViewController :UIViewController,
										  UITableViewDelegate,
										  UITableViewDataSource,
										  ALDBAddViewControllerDelegate{
	
	
	
	@IBOutlet var lblName		: UILabel!
	@IBOutlet var lblDeviceID	: UILabel!
	@IBOutlet var lblDeviceType	: UILabel!
	
	@IBOutlet var tableView: SelfSizedTableView!
	
	var deviceID :String = ""
	var aldb: Dictionary<String, RESTaldbEntry> = [:]
	var aldbKeys: [String] = []
	
	var plmID:String?
	
	class func create(withDeviceID: String) -> DeviceALDBViewController? {
		let storyboard = UIStoryboard(name: "DeviceALDBView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DeviceALDBViewController") as? DeviceALDBViewController
		
		if let vc = vc {
			vc.deviceID = withDeviceID
		}
		
		return vc
	}
	
	override func viewDidLoad() {
		super.viewDidLoad()
		tableView.maxHeight = 500
		
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		let dp = DispatchGroup()
		dp.enter()
		
		HomeControl.shared.fetchData(.plmInfo) { result in
			if case .success(let plmInfo as RESTPlmInfo) = result {
				self.plmID = plmInfo.plmID
			}
			dp.leave()
			
		}
		
		dp.enter()
		refreshDevice(){
			dp.leave()
		}
		
		dp.notify(queue: .main) {
			self.tableView.reloadData()
		}
	}
	
	
	override func viewWillDisappear(_ animated: Bool) {
		
		self.view.displayActivityIndicator(shouldDisplay: false)

	}
	
	
	func refreshDevice(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		
		HomeControl.shared.fetchData(.device_aldb, ID: self.deviceID) { result in
			if case .success(let device as RESTDeviceDetails) = result {
				
				self.lblName.text = device.name
				self.lblDeviceID.text = device.deviceID
				
				if let deviceModelStr = HCServerManager.shared.getDeviceDescriptionFrom(device.deviceInfo) {
					self.lblDeviceType.text = 	deviceModelStr
				}
				
				
				if let dict = device.aldb {
					self.aldb = dict
					
					let f = dict.filter { entry in
						
						if let flag = UInt8(entry.value.aldb_flag, radix: 16) {
							return (flag  & 0x80) != 0
						}
						return false
					}
					
					self.aldbKeys = Array(f.keys).sorted().reversed()
					//			self.aldbKeys = dict.keys
				}
				
				
				completion()
				
			}
			
		}
	}
	
	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		
		return aldbKeys.count
	}
	
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		let  key = aldbKeys[indexPath.row]
		if let entry = aldb[key] {
			
			let font =	UIFont.monospacedSystemFont(ofSize: UIFont.systemFontSize,
																  weight: .regular)
			
			
			let thisPLM = entry.deviceID == self.plmID
			let flag = UInt8(entry.aldb_flag, radix: 16)
			let isCTNL = (flag! & 0x40) == 0x40
			
			let img = UIImage(systemName: isCTNL ? "c.circle.fill" : "r.circle")
			//		let group = UInt8(entry.aldb_group, radix: 16)
			let device =  InsteonFetcher.shared.devices[entry.deviceID]
			
				if(thisPLM || device == nil){
				if let cell = tableView.dequeueReusableCell(withIdentifier:
																			DeviceALDBCell1.cellReuseIdentifier) as? DeviceALDBCell1{
					
					cell.lblAddr.font = font
					cell.lblGroup.font = font
					cell.lblDeviceID.font = font.withSize(UIFont.smallSystemFontSize)
					cell.imgDot.image = img
					cell.imgDot.tintColor = isCTNL ? UIColor.systemBlue : UIColor.systemGray
					
					cell.lblAddr.text = entry.aldb_address.uppercased()
					cell.lblGroup.text = entry.aldb_group.uppercased()
					
					if(thisPLM){
						cell.lblDeviceID.text = "PLM"
					}
					else {
						cell.lblDeviceID.text = entry.deviceID
					}
					return cell
				}
			}
			else {
				if let cell = tableView.dequeueReusableCell(withIdentifier:
																			DeviceALDBCell2.cellReuseIdentifier) as? DeviceALDBCell2{
					
					cell.lblAddr.font = font
					cell.lblGroup.font = font
					cell.lblDeviceID.font = font.withSize(UIFont.smallSystemFontSize)
					cell.imgDot.image = img
					cell.imgDot.tintColor = isCTNL ? UIColor.systemBlue : UIColor.systemGray
					
					
					cell.lblAddr.text = entry.aldb_address.uppercased()
					cell.lblGroup.text = entry.aldb_group.uppercased()
					
					cell.lblDeviceID.text = entry.deviceID
					cell.lblDeviceName.text = device?.name
					
					//					if let device =  InsteonFetcher.shared.devices[entry.deviceID] {
					//
					//				  }
					//				  else {
					//					  cell.lblDeviceName.text = "Unknown"
					//				  }
					return cell
				}
				
			}
			
			
		}
		
		return UITableViewCell()
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
		
		let  key = aldbKeys[indexPath.row]
		if let entry = aldb[key] {
			
			let warning = "Are you sure you want to remove ALDB entry: \"\(entry.aldb_address)?"
			let alert = UIAlertController(title: "Remove ALDB Entry", message: warning, preferredStyle:.alert)
			let cancelAction = UIAlertAction(title: "Cancel", style: .cancel, handler: { _ in
				
			})
			
			let deleteAction = UIAlertAction(title: "Delete", style: .destructive, handler: { _ in
				
				self.view.displayActivityIndicator(shouldDisplay: true)
				InsteonFetcher.shared.removeALDBfromDevice(self.deviceID, aldbAddr: entry.aldb_address)
				{ (error)  in
					if(error == nil){
						
						self.refreshDevice(){
							self.view.displayActivityIndicator(shouldDisplay: false)
		
							self.tableView.reloadData()
						}
					}
					else {
						self.view.displayActivityIndicator(shouldDisplay: false)
	
		
						Toast.text(error?.localizedDescription ?? "Error",
									  config: ToastConfiguration(
										autoHide: true,
										displayTime: 1.0,
										attachTo: self.view
									  )).show()
						
					}
				}
			})
			
			alert.addAction(cancelAction)
			alert.addAction(deleteAction)
			self.present(alert, animated: true, completion: nil)
		}
	}
	
	@IBAction func btnALDBHit(sender: UIButton) {
 
		if let aldbAddView = ALDBAddViewController.create() {
			aldbAddView.delegate = self
			self.present(aldbAddView, animated: true, completion: nil);
  
	//		self.show(adlbAddView, sender: self)
		}

	}
	
	// MARK: - ALDBAddViewControllerDelegate
	
	func addALDBGroup(_ sender: UIViewController,
							groupID: String,
							isCNTL: Bool) {
		
		self.view.displayActivityIndicator(shouldDisplay: true)
		
		InsteonFetcher.shared.addALDBtoDevice(self.deviceID,
														  groupID: groupID,
														  isCNTL: isCNTL )
		{ (error)  in
			if(error == nil){
				
				self.refreshDevice(){
					self.view.displayActivityIndicator(shouldDisplay: false)
					
					self.tableView.reloadData()
				}
			}
			else {
				self.view.displayActivityIndicator(shouldDisplay: false)
				
				
				Toast.text(error?.localizedDescription ?? "Error",
							  config: ToastConfiguration(
								autoHide: true,
								displayTime: 1.0,
								attachTo: self.tableView
							  )).show()
				
			}
		}
	}

}


