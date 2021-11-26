//
//  DeviceDetailViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/24/21.
//

import UIKit
import Toast

class DeviceDetailViewController :UIViewController, EditableUILabelDelegate {
	
	@IBOutlet var lblName	: EditableUILabel!
	@IBOutlet var img	: UIImageView!
	@IBOutlet var lblLevel	: UILabel!

	@IBOutlet var sw	: UISwitch!
	@IBOutlet var slider	: UISlider!

	@IBOutlet var lblDeviceID	: UILabel!
	@IBOutlet var lblModel	: UILabel!
	@IBOutlet var lblFirmware	: UILabel!
	
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
		refreshDevice();
	}
	
	
	override func viewWillDisappear(_ animated: Bool) {
		stopPollng();
	}
	
	
	func refreshDevice() {
	 
		HomeControl.shared.fetchData(.device, ID: self.deviceID) { result in
			if case .success(let device as RESTDeviceDetails) = result {
			 
				self.lblName.text = device.name
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
					
				self.lblDeviceID.text = device.deviceID
				
				if let deviceModelStr = HCServerManager.shared.getDeviceDescriptionFrom(device.deviceInfo) {
					self.lblModel.text = deviceModelStr
				}
				 
				if let versStr = HCServerManager.shared.getFirmwareVersionFrom(device.deviceInfo) {
					self.lblFirmware.text = versStr
				}
				
			}
		}
	}
	
	
	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
		
			self.refreshDevice()
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
}
 
