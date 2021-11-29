//
//  EditableUILabel.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/26/21.
//
 
import UIKit

public protocol EditableUILabelDelegate  {
	func editMenuTapped(sender: UILabel)
}


class EditableUILabel: UILabel {

	var delegate:EditableUILabelDelegate? = nil

	 override public var canBecomeFirstResponder: Bool {
		  get {
				return true
		  }
	 }

	 override init(frame: CGRect) {
		  super.init(frame: frame)
		  sharedInit()
	 }

	 required init?(coder aDecoder: NSCoder) {
		  super.init(coder: aDecoder)
		  sharedInit()
	 }

	 func sharedInit() {
		 
		 let menuItem1: UIMenuItem = UIMenuItem(title: "Edit",
															 action: #selector(editMenu(sender:)))
		 
		 // Store MenuItem in array.
		 UIMenuController.shared.menuItems = [menuItem1]
		 
		  isUserInteractionEnabled = true
		  addGestureRecognizer(UILongPressGestureRecognizer(
				target: self,
				action: #selector(showMenu(sender:))
		  ))
	 }

	@objc func editMenu(sender: UIMenuItem) {
		if let delegate = self.delegate {
			delegate.editMenuTapped(sender: self)
		}
	  }
  

	 override func copy(_ sender: Any?) {
		  UIPasteboard.general.string = text
		 UIMenuController.shared.showMenu(from: self, rect: bounds)
	 }

	@objc func showMenu(sender: Any?) {
		  becomeFirstResponder()
		  let menu = UIMenuController.shared
		  if !menu.isMenuVisible {
			  menu.showMenu(from: self, rect: bounds)
		  }
	 }

	 override func canPerformAction(_ action: Selector, withSender sender: Any?) -> Bool {
		  return ( ((action == #selector(editMenu(sender:))) && self.delegate != nil)
					 || (action == #selector(copy(_:)))
		  			)
		 
	 }
}
