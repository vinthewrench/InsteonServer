//
//  DeviceLinkViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/3/21.
//

import UIKit
import Toast
import AnyFormatKit


extension String {
	 static func ~= (lhs: String, rhs: String) -> Bool {
		  guard let regex = try? NSRegularExpression(pattern: rhs) else { return false }
		  let range = NSRange(location: 0, length: lhs.utf16.count)
		  return regex.firstMatch(in: lhs, options: [], range: range) != nil
	 }
}



public protocol DeviceLinkViewControllerDelegate  {
	func newDeviceAdded(_ sender: UIViewController, deviceID: String?)
}

class  DeviceLinkViewController : UIViewController,
											 UITextFieldDelegate,
											 UIAdaptivePresentationControllerDelegate {
	
	
	@IBOutlet var txtAddr: UITextField!
	@IBOutlet var btnStart: UIButton!
	
	@IBOutlet var img1 : UIImageView!
	@IBOutlet var lbl2 : UILabel!
	@IBOutlet var img2 : UIImageView!
	@IBOutlet var act2 : UIActivityIndicatorView!
	
	@IBOutlet var lbl3 : UILabel!
	@IBOutlet var img3 : UIImageView!
	
	@IBOutlet var vwLinked : UIView!
	@IBOutlet var lblDeviceID : UILabel!
	@IBOutlet var lblDeviceType : UILabel!
	@IBOutlet var lblVersion : UILabel!
	@IBOutlet var btnDetails: UIButton!
	
	var newDeviceID: String? = nil

	enum linkStage: Int {
		case initial = 0
		case input
		case started
		case linked
		case error
	}
	
	var currentStage:linkStage = .initial
	
	let phoneFormatter = DefaultTextInputFormatter(textPattern: "##.##.##")
	
	var delegate:DeviceLinkViewControllerDelegate? = nil
	
	class func create() -> DeviceLinkViewController? {
		let storyboard = UIStoryboard(name: "DeviceLinkView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DeviceLinkViewController") as? DeviceLinkViewController
		
		return vc
	}
	
	override func viewDidLoad() {
		super.viewDidLoad()
		self.presentationController?.delegate = self
		
		createKeyboard(txtAddr)
		currentStage = .initial
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		switch currentStage {
		case .linked,
			  .started,
			  .input:
			break
			
		case .error,
			  .initial:
				setLinkStage(.initial)
		}
	}
	
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		
	}
	
	func presentationControllerShouldDismiss(_ presentationController: UIPresentationController) -> Bool {
		return currentStage != .started
	}

	
	func createKeyboard(_ textField: UITextField) {
		textField.backgroundColor = .systemGroupedBackground
		textField.clearButtonMode = .never
		textField.inputView = Keyboard(target: textField)
		
	}
	
	@IBAction func clickTextField(_ sender: UITextField) {
		sender.reloadInputViews()
		self.textFieldChanged(sender)
	}
	
	func filterHex(_ textIn: String?)-> String {
		
		if let text = textIn?.uppercased() {
			let digitSet = CharacterSet(charactersIn: "1234567890ABCDEF")
			let addr = String(text.unicodeScalars.filter { digitSet.contains($0) })
			return phoneFormatter.format(addr) ?? ""
		}
		
		return ""
		
	}
	
	@IBAction func textFieldEnded(_ sender: UITextField) {
		sender.reloadInputViews()
		sender.text =  filterHex(sender.text)
	}
	
	@IBAction func textFieldChanged(_ sender: UITextField) {
		
		sender.text =  filterHex(sender.text)
		
		setLinkStage(.input)
	}
	
	
	func setLinkStage(_ stage :linkStage, errorMsg: String? = nil){
		
		currentStage = stage
		
		lbl2.textColor = .lightGray
		img2.isHidden = true
		act2.isHidden = true
		
		lbl3.isHidden = true
		img3.isHidden = true
		
		vwLinked.isHidden = true
		
		switch stage {
		
		case .initial:
			txtAddr.text = ""
			btnStart.isEnabled = true
			newDeviceID = nil
		
			
		case .input:
			newDeviceID = nil
		
			if let text = txtAddr.text {
				if text.lengthOfBytes(using: .utf8) == 0 {
					btnStart.isEnabled = true
				}
				else {
					btnStart.isEnabled =  text ~=  "^[A-Fa-f0-9]{2}.[A-Fa-f0-9]{2}.[A-Fa-f0-9]{2}$"
				}
			}
			
		case .started:
			newDeviceID = nil
			btnStart.isEnabled = false
	 
			lbl2.textColor = .black
			img2.isHidden = true
			act2.isHidden = false
			break
			
		case .linked:
			
			lbl2.textColor = .black
			act2.isHidden = true
			
			img2.image = UIImage(systemName: "checkmark.circle")
			img2.tintColor = UIColor.systemGreen
			img2.isHidden = false
	
			btnStart.isEnabled = false
			img3.image = UIImage(systemName: "checkmark.circle")
			img3.tintColor = UIColor.systemGreen
			img3.isHidden = false
			lbl3.text = "Device successfully linked."
			lbl3.isHidden	= false
			vwLinked.isHidden = false;
			
			break
			
		case .error:
			newDeviceID = nil
		
			lbl2.textColor = .black

			act2.isHidden = true
			img2.image = UIImage(systemName: "checkmark.circle")
			img2.tintColor = UIColor.systemGreen
			img2.isHidden = false
	
			img3.image = UIImage(systemName: "xmark")
			img3.tintColor = UIColor.systemRed
			img3.isHidden = false
			lbl3.text = errorMsg ?? "Error"
			lbl3.isHidden	= false
		
			break
		}
		
		if btnStart.isEnabled{
			btnStart.backgroundColor = UIColor.systemBlue
		}
		else {
			btnStart.backgroundColor = UIColor.lightGray
		}
		
	}
	
	
	
	@IBAction func startClicked(_ sender: UIButton) {
		txtAddr.resignFirstResponder()
		
		var addr:String? = nil
		
		if txtAddr.text!.lengthOfBytes(using: .utf8) == 0 {
			addr = nil
  		}
		else {
			addr = txtAddr.text
 		}
	
		setLinkStage(.started)
	 
		HCServerManager.shared.linkDevice(deviceID: addr )
		{ (json,error)  in
			
			if let err = json  as? RESTErrorInfo {
						
				var message = err.message
				if let detail =  err.detail{
					if !detail.isEmpty {
						message = detail
					}
	 			}
				
				self.setLinkStage(.error, errorMsg: message)
			}
			else 	if let linkInfo = json as? RESTLinkResponse {  // whatever this i
				
				if let deviceModelStr = HCServerManager.shared.getDeviceDescriptionFrom(linkInfo.deviceInfo) {
					self.lblDeviceType.text = deviceModelStr
				}
				
				if let versStr = HCServerManager.shared.getFirmwareVersionFrom(linkInfo.deviceInfo) {
					self.lblVersion.text = versStr
				}
				
				self.lblDeviceID.text = linkInfo.deviceID
				self.newDeviceID = linkInfo.deviceID
				self.setLinkStage(.linked)
			}
 			else {
				self.setLinkStage(.error, errorMsg: "Undefined response")
			}
			
		}
	}
	
	
	@IBAction func showDetailsClicked(_ sender: UIButton) {
		txtAddr.resignFirstResponder()

		self.delegate?.newDeviceAdded(self, deviceID: newDeviceID)
	}
	
}

