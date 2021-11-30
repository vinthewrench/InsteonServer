//
//  DeviceDetailViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/24/21.
//

import UIKit
import Toast

final class DeviceDetailPropertyCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "DeviceDetailPropertyCell"
	
	@IBOutlet var lblPropName	: UILabel!
	@IBOutlet var lblValue	: EditableUILabel!
	@IBOutlet var img			: UIImageView!
	
	override func awakeFromNib() {
		super.awakeFromNib()
	}
}

class DeviceDetailViewController :UIViewController, EditableUILabelDelegate,
											 UITableViewDelegate,
											 UITableViewDataSource {
	
	@IBOutlet var lblName	: EditableUILabel!
	@IBOutlet var img	: UIImageView!
	@IBOutlet var lblLevel	: UILabel!
	
	@IBOutlet var sw	: UISwitch!
	@IBOutlet var slider	: UISlider!
	
	struct PropTableEntry  {
		var propName: String =  ""
		var propValue: String = ""
		var imageName: String = ""
		
	}
	var properties:[PropTableEntry] = []
	
	@IBOutlet var tableView: UITableView!
	//	@IBOutlet public var cnstTableHeight : NSLayoutConstraint!
	
	var timer = Timer()
	
	
	var deviceID :String = ""
	
	class func create(withDeviceID: String) -> DeviceDetailViewController? {
		let storyboard = UIStoryboard(name: "DeviceDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DeviceDetailViewController") as? DeviceDetailViewController
		
		if let vc = vc {
			vc.deviceID = withDeviceID
		}
		
		return vc
	}
	
	override func viewDidLoad() {
		super.viewDidLoad()
		lblName.delegate = self
		
		slider.addTarget(self, action: #selector(DeviceDetailViewController.sliderBeganTracking(_:)),
							  for: .touchDown)
		
		slider.addTarget(self, action: #selector(DeviceDetailViewController.sliderEndedTracking(_:)),
							  for: .touchUpInside)
		
		slider.addTarget(self, action: #selector(DeviceDetailViewController.sliderEndedTracking(_:)),
							  for: .touchUpOutside)
		
		slider.addTarget(self, action: #selector(DeviceDetailViewController.sliderValueChanged(_:)),
							  for: .valueChanged)
		
		slider.minimumTrackTintColor = UIColor.systemGreen
		
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		startPolling();
		refreshDevice(updateProps:true);
	}
	
	
	override func viewWillDisappear(_ animated: Bool) {
		stopPollng();
	}
	
	//	override func updateViewConstraints() {
	//		super.updateViewConstraints()
	//		self.cnstTableHeight.constant = tableView.contentSize.height
	//	}
	//
	//	override  func viewDidLayoutSubviews() {
	//		super.viewDidLayoutSubviews()
	//		self.cnstTableHeight.constant = tableView.contentSize.height
	//
	//	}
	
	func refreshDevice(updateProps: Bool = false) {
		
		HomeControl.shared.fetchData(.device, ID: self.deviceID) { result in
			if case .success(let device as RESTDeviceDetails) = result {
				
				self.lblLevel.text = device.level?.onLevelString()
				self.img.image = device.deviceImage()
				
				if(device.isDimmer){
					self.sw.isHidden = true
					self.slider.isHidden = false
					self.slider.value =  Float(device.level ?? 0)
					
				} else
				{
					self.sw.isHidden = false
					self.sw.isOn = device.level ?? 0 > 0
					
					self.slider.isHidden = true
				}
				
				if(updateProps){
					self.properties = []
					
					self.properties.append( PropTableEntry(propName: "DeviceID",
																		propValue: device.deviceID,
																		imageName: "dot.radiowaves.right"))
					
					if let deviceModelStr = HCServerManager.shared.getDeviceDescriptionFrom(device.deviceInfo) {
						
						self.properties.append( PropTableEntry(propName: "Model Type",
																			propValue: deviceModelStr,
																			imageName: "m.circle"))
					}
					
					if let versStr = HCServerManager.shared.getFirmwareVersionFrom(device.deviceInfo) {
						
						self.properties.append( PropTableEntry(propName: "Firmware Version",
																			propValue: versStr,
																			imageName: "f.circle"))
					}
					
					if let props:Dictionary<String, String>  = device.properties {
						
						
						let keys = props.keys.sorted{ (first, second) -> Bool in
							return  first.caseInsensitiveCompare(second) == .orderedAscending
						}
						
						for key  in keys {
							
							self.properties.append( PropTableEntry(propName: key,
																				propValue: props[key]!,
																				imageName: "p.circle"))
						}
					}
					
					self.tableView.reloadData()
					self.tableView.updateConstraints()
					
				}
				
			}
		}
	}
	
	
	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
													
													if(!self.tableView.isEditing){
														self.refreshDevice(updateProps:false);
													}
												})
	}
	
	func stopPollng(){
		timer.invalidate()
	}
	
	@objc func sliderBeganTracking(_ slider: UISlider!) {
		//	print("sliderBeganTracking")
		stopPollng()
	}
	
	@objc func sliderEndedTracking(_ slider: UISlider!) {
		InsteonFetcher.shared.setDeviceLevel(deviceID, toLevel:  Int(slider.value)) {_ in
			self.startPolling()
		}
	}
	
	@objc func sliderValueChanged(_ slider: UISlider!) {
		
		self.lblLevel.text =  Int(slider.value).onLevelString()
	}
	
	@IBAction func switchChanged(sender: UISwitch) {
		
		stopPollng()
		InsteonFetcher.shared.setDeviceLevel(deviceID, toLevel: sender.isOn ? 255:0) {_ in
			self.startPolling()
		}
	}
	
	func renameDevice(newName:String){
		
		InsteonFetcher.shared.renameDevice(deviceID, newName: newName)
		{ (error)  in
			
			if(error == nil){
				self.refreshDevice()
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
		
		let alert = UIAlertController(title:  NSLocalizedString("Rename Device", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("Rename", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
			textField.placeholder =  "Device Name"
			textField.text = self.lblName.text
		}) { result in
			
			switch result {
			case let .ok(String:newName):
				self.renameDevice( newName: newName);
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)
	}
	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		
		return properties.count
	}
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	DeviceDetailPropertyCell.cellReuseIdentifier) as? DeviceDetailPropertyCell{
			
			cell.accessoryType = .none
			cell.selectionStyle = .none
			
			let property = properties[indexPath.row]
			cell.lblPropName.text = property.propName
			cell.lblValue.text = property.propValue
			
			cell.img.image =  UIImage(systemName: property.imageName)
			??  UIImage(systemName: "questionmark")
			
			return cell
			
		}
		
		return UITableViewCell()
		
	}
	
}

