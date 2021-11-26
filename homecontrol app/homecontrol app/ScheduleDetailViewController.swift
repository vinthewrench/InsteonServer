//
//  ScheduleDetailView.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/24/21.
//

import UIKit
import Toast

class ScheduleDetailViewController :UIViewController,
												EditableUILabelDelegate {
	
	
	@IBOutlet var lblTitle	: EditableUILabel!
	
	var eventID :String = ""
	
	class func create(withEventID: String) -> ScheduleDetailViewController? {
		let storyboard = UIStoryboard(name: "ScheduleDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "ScheduleDetailViewController") as? ScheduleDetailViewController
		
		if let vc = vc {
			vc.eventID = withEventID
		}
		
		return vc
	}
	
	override func viewDidLoad() {
		super.viewDidLoad()
		lblTitle.delegate = self
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		refreshEvent()
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		
		if let firstVC = SchedulesViewController.shared {
			DispatchQueue.main.async {
				
				firstVC.refreshSchedules()
			}
		}
	}
	
	func refreshEvent() {
		
		HomeControl.shared.fetchData(.event, ID: self.eventID) { result in
			if case .success(let event as RESTEvent) = result {
			 
				self.lblTitle.text = event.name
			}
		}

	}
	
	func renameEvent(newName:String){

		InsteonFetcher.shared.renameEvent(eventID, newName: newName)
		{ (error)  in

			if(error == nil){
				self.refreshEvent()
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
		
		let alert = UIAlertController(title:  NSLocalizedString("Rename Event", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("Rename", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
			textField.placeholder =  "Event Name"
			textField.text = self.lblTitle.text
		}) { result in
			
			switch result {
			case let .ok(String:newName):
				self.renameEvent( newName: newName);
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)
		}
}
