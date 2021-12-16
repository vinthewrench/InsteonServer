//
//  DeviceGroupViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/15/21.
//

import UIKit




class DeviceGroupViewController :MainSubviewViewController,
											MainSubviewViewControllerDelegate
{
	@IBOutlet var segDevGroup	: UISegmentedControl!
	@IBOutlet var vwSubView	: UIView!
 
	var currentVC : MainSubviewViewController? = nil
	var currentTab: GroupDeviceTab = .device
	
	static let shared: DeviceGroupViewController! = {
		
		let storyboard = UIStoryboard(name: "DeviceGroupView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "DeviceGroupViewController") as? DeviceGroupViewController
		
			return vc
	}()

	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		// Add function to handle Value Changed events
		segDevGroup.addTarget(self, action: #selector(self.segmentedValueChanged(_:)), for: .valueChanged)

	 
}

	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		setupSubviews()
 	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
	}
	

	// MARK:- segment control
	
	@objc func segmentedValueChanged(_ sender:UISegmentedControl!) {
		switch( sender.selectedSegmentIndex) {
		case 0:
			currentTab = .device
			
		case 1:
			currentTab = .group
			
	 
		default: break
			
		}
		
		setupSubviews()
	}

	func setupSubviews(){
		
		var newVC:MainSubviewViewController? = nil
		
		switch currentTab {
		case .device:
			segDevGroup.selectedSegmentIndex = 0
			newVC = DevicesViewController.shared
			
		case .group:
			segDevGroup.selectedSegmentIndex = 1
			newVC = GroupsViewController.shared
			
		default:
			break
		}
		
		
		if(newVC == currentVC){
			return
		}
		
		if let cvc = currentVC {
			cvc.willMove(toParent: nil)
			cvc.view.removeFromSuperview()
			cvc.removeFromParent()
			currentVC = nil
		}
		
		if let newVC = newVC {
			currentVC = newVC
			newVC.view.frame = self.vwSubView.bounds
			newVC.willMove(toParent: self)
			self.vwSubView.addSubview(newVC.view)
			self.addChild(newVC)
			newVC.didMove(toParent: self)
		}
	}
	
	
	// MARK: - MainSubviewViewControllerDelegate

	func addButtonHit(_ sender: UIButton){
		
		
		if let subViewDelegate = currentVC as? MainSubviewViewControllerDelegate {
			subViewDelegate.addButtonHit(sender)
		}
	 
	}
	
}

