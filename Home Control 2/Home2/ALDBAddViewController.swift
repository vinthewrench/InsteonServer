//
//  ALDBAddViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/9/21.
//

import Foundation
import UIKit


public protocol ALDBAddViewControllerDelegate  {
	func addALDBGroup(_ sender: UIViewController,
							groupID: String,
							isCNTL: Bool)
}


class  ALDBAddViewController : UIViewController ,
										 UITextFieldDelegate,
										 UIAdaptivePresentationControllerDelegate{

	var delegate:ALDBAddViewControllerDelegate? = nil

	
	@IBOutlet var txtGroup: UITextField!
	@IBOutlet var segCNTL: UISegmentedControl!

	@IBOutlet var btnAdd: UIButton!
	@IBOutlet var btnCancel: UIButton!
 
	class func create() -> ALDBAddViewController? {
		let storyboard = UIStoryboard(name: "ALDBAddView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "ALDBAddViewController") as? ALDBAddViewController
				
		return vc
	}
 
	override func viewDidLoad() {
		super.viewDidLoad()
		self.presentationController?.delegate = self
		
		createKeyboard(txtGroup)
		btnAdd.isEnabled = false
	

 	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		segCNTL.selectedSegmentIndex = 1
		
		
	}

	func presentationControllerShouldDismiss(_ presentationController: UIPresentationController) -> Bool {
		return false
	}

	@IBAction func btnCancelClicked(_ sender: UIButton) {
		
		self.dismiss(animated: true)

	}
	@IBAction func btnAddClicked(_ sender: UIButton) {
		
		self.dismiss(animated: true)
		
		if let group = txtGroup.text {
			delegate?.addALDBGroup(self,
										  groupID: group,
										  isCNTL: segCNTL.selectedSegmentIndex == 0)
			
		}
	}


	// MARK: - UITextFieldDelegate
	
 
	func createKeyboard(_ textField: UITextField) {
		textField.backgroundColor = .systemGroupedBackground
		textField.clearButtonMode = .never
		textField.inputView = Keyboard(target: textField)
		
	}
	
	@IBAction func clickTextField(_ sender: UITextField) {
		sender.reloadInputViews()
		self.textFieldChanged(sender)
	}
	
	func filter2Hex(_ textIn: String?)-> String {
		
		if let text = textIn?.uppercased() {
			let digitSet = CharacterSet(charactersIn: "1234567890ABCDEF")
			let addr = String(text.unicodeScalars.filter { digitSet.contains($0) })
			return String(addr.prefix(2))
			}
		
		return ""
		
	}
	
	@IBAction func textFieldEnded(_ sender: UITextField) {
		sender.reloadInputViews()
		let group = filter2Hex(sender.text)
		btnAdd.isEnabled = group.count == 2
		sender.text =  group
	}
	
	@IBAction func textFieldChanged(_ sender: UITextField) {
		let group = filter2Hex(sender.text)
		btnAdd.isEnabled = group.count == 2
		sender.text =  group
	}
	
	
 
	
}
	
