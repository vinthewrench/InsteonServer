//
//  ViewController.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 11/18/21.
//

import UIKit
 

@objc protocol MainSubviewViewControllerDelegate  {
	func addButtonHit(_ sender: UIButton)
}



class MainSubviewViewController: UIViewController {
 
	var mainView : MainViewController?
	
}
 
class MainViewController: UIViewController, UITabBarDelegate {
	
	@IBOutlet var tabBar	: UITabBar!
	@IBOutlet var lbTitle 	: UILabel!
	@IBOutlet var vwContainer : UIView!

	@IBOutlet var btnAdd 	: UIButton!

	var containViewController : UIViewController? = nil

	var subViewDelegate: MainSubviewViewControllerDelegate? = nil

	override var title: String? {
		 get {
			 return lbTitle.text ?? ""
		 }
		 set(name) {
			 lbTitle.text = name
		 }
	}
	
	

	override func viewDidLoad() {
		super.viewDidLoad()
		
		
	//	let index = AppData.serverInfo.tabSelection
		
		tabBar.selectedItem = tabBar.items![AppData.serverInfo.tabSelection]
 		self.tabBar(tabBar, didSelect: tabBar.selectedItem!)
		
 	}

	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		
	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewDidAppear(animated)
		
		if(!AppData.serverInfo.validated){
			self.displaySetupView();
		}
	
	}
	
	
	func tabBar(_ tabBar: UITabBar, didSelect item: UITabBarItem) {
		self.title = item.title
		AppData.serverInfo.tabSelection = item.tag
		
		var newVC:MainSubviewViewController? = nil;
 
		switch(item.tag){
		case 0:
			newVC = KeypadsViewController.shared
			
		case 1:
			newVC = DevicesViewController.shared
			
		case 2:
			newVC = SchedulesViewController.shared
			
	 	case 3:
			newVC = GroupsViewController.shared
	 
		default:
			break;
		}
		
		if(newVC == containViewController){
			return
		}
		
		newVC?.mainView = self
		subViewDelegate = newVC as? MainSubviewViewControllerDelegate
		
		btnAdd.isHidden = subViewDelegate == nil
		
		
		if let cvc = containViewController {
			cvc.willMove(toParent: nil)
			cvc.view.removeFromSuperview()
			cvc.removeFromParent()
			containViewController = nil
		}
	
		if let newVC = newVC {
			containViewController = newVC
			newVC.view.frame = self.vwContainer.bounds
			newVC.willMove(toParent: self)
			self.vwContainer.addSubview(newVC.view)
			self.addChild(newVC)
			newVC.didMove(toParent: self)
		}
	 
	}
 
 
	@IBAction func SetupUpClicked(_ sender: UIButton) -> Void {

		self.displaySetupView()
	}

	func displaySetupView(){

		if let setupView = SetupViewController.create() {
			self.show(setupView, sender: self)
		}
	}
	
	@IBAction func addBtnUpClicked(_ sender: UIButton) -> Void {
		subViewDelegate?.addButtonHit(sender)

	}

}
