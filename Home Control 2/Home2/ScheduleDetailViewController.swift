//
//  ScheduleDetailView.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/24/21.
//

import UIKit
import Toast
import PureLayout




// MARK:- TriggerViewController

public protocol TriggerViewControllerDelegate  {
	func triggerViewChanged()
}

class TriggerViewController: UIViewController {
	
	var event: RESTEvent? = nil
	var delegate:TriggerViewControllerDelegate? = nil

}

class TriggerDeviceViewController: TriggerViewController {
	
	
	class func create(withEvent: RESTEvent) -> TriggerDeviceViewController? {
		let storyboard = UIStoryboard(name: "ScheduleDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "TriggerDeviceViewController") as? TriggerDeviceViewController
		
		if let vc = vc {
			vc.event = withEvent
		}
		
		return vc
	}

}

class TriggerTimeViewController: TriggerViewController, 												UITextFieldDelegate

{
	
	@IBOutlet var btnTimeBase	: UIButton!
	@IBOutlet var txtOffset	: UITextField!

	class func create(withEvent: RESTEvent) -> TriggerTimeViewController? {
		let storyboard = UIStoryboard(name: "ScheduleDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "TriggerTimeViewController") as? TriggerTimeViewController
		
		if let vc = vc {
			vc.event = withEvent
		}
		
		return vc
	}

	
	override func viewDidLoad() {
		super.viewDidLoad()
		
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		btnTimeBase.setTitle(event?.stringForTrigger(), for: .normal	)
		btnTimeBase.setImage(event?.imageForTrigger(), for: .normal)

		var actions: [UIAction] = []
		for timebase  in RESTEvent.timedEventTimeBase.allCases {
			
			if(timebase == .invalid){
				continue
			}
			
			let item = UIAction(title: timebase.description(),
									  image: timebase.image(),
									  identifier: UIAction.Identifier( "\(timebase.rawValue)")
			
			) { (action) in
				
				print("action: \(action.identifier)")
				
				if let newTimeBase = Int(action.identifier.rawValue){
					if (newTimeBase != self.event?.trigger.timeBase ){
						
						self.event?.trigger.timeBase = Int(action.identifier.rawValue)
						self.refreshView()
						
						if let delegate = self.delegate {
							delegate.triggerViewChanged()
						}
			
					}
				}

				self.event?.trigger.timeBase = Int(action.identifier.rawValue)
				self.refreshView()
			}

			actions.append(item)
			
		}

		let menu = UIMenu(title: "Timebase for Event",
								 options: .displayInline, children: actions)
		
		btnTimeBase.menu = menu
		btnTimeBase.showsMenuAsPrimaryAction = true
		
 	}
	
	func refreshView() {
		btnTimeBase.setTitle(event?.stringForTrigger(), for: .normal	)
		btnTimeBase.setImage(event?.imageForTrigger(), for: .normal)

	}
	
	var dateFormate = false
	
	func textField(_ textField: UITextField, shouldChangeCharactersIn range: NSRange, replacementString string: String) -> Bool  {

		 //1. To make sure that this is applicable to only particular textfield add tag.
		 if textField.tag == 1 {
			  //2. this one helps to make sure that user enters only numeric characters and '-' in fields
			  let numbersOnly = CharacterSet(charactersIn: "1234567890:")

			  let Validate = string.rangeOfCharacter(from: numbersOnly.inverted) == nil ? true : false
			  if !Validate {
					return false;
			  }
			if range.length + range.location > (textField.text?.count)! {
					return false
			  }
			  let newLength = (textField.text?.count)! + string.count - range.length
			  if newLength == 3 {
				let  char = string.cString(using: String.Encoding.utf8)!
					let isBackSpace = strcmp(char, "\\b")

					if (isBackSpace == -92) {
						 dateFormate = false;
					}else{
						 dateFormate = true;
					}

					if dateFormate {
						if let newtext = textField.text {
							textField.text  =	"\(newtext):"
							dateFormate = false
						}
//						 let textContent:String!
//						 textContent = textField.text
//
//						 //3.Here we add '-' on overself.
//
//						let textWithHifen = "\(String(describing: textContent)):"
//					//	let textWithHifen:NSString = "\(textContent)-" as NSString
//						 textField.text = textWithHifen
//						 dateFormate = false
					}
			  }
			  //4. this one helps to make sure only 8 character is added in textfield .(ie: dd-mm-yy)
			  return newLength <= 5;

		 }
		 return true
	}
	
}

class TriggerEventViewController: TriggerViewController {
	
	class func create(withEvent: RESTEvent) -> TriggerEventViewController? {
		let storyboard = UIStoryboard(name: "ScheduleDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "TriggerEventViewController") as? TriggerEventViewController
		
		if let vc = vc {
			vc.event = withEvent
		}
		
		return vc
	}

}

// MARK:- ScheduleDetailViewController


class ScheduleDetailViewController :UIViewController,
												EditableUILabelDelegate,
												TriggerViewControllerDelegate{
	
	
	@IBOutlet var lblTitle	: EditableUILabel!

	@IBOutlet var segEvent	: UISegmentedControl!
	
	@IBOutlet var vwEvent	: UIView!
	@IBOutlet var btnSave	: UIButton!

	var currentEventVC : TriggerViewController? = nil
	
	var event: RESTEvent? = nil
	var eventType:RESTEvent.eventType = .unknown
	var didChangeEvent:Bool = false
	
	
	var eventID :String = ""
	
	class func create(withEventID: String) -> ScheduleDetailViewController? {
		let storyboard = UIStoryboard(name: "ScheduleDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "ScheduleDetailViewController") as? ScheduleDetailViewController
		
		if let vc = vc {
			vc.eventID = withEventID
		}
		
		return vc
	}
	

	
	// MARK:-  view
	
	override func viewDidLoad() {
		super.viewDidLoad()
		lblTitle.delegate = self
		
		// Add function to handle Value Changed events
		segEvent.addTarget(self, action: #selector(self.segmentedValueChanged(_:)), for: .valueChanged)
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
	
	
	func setupSubviewsForEvent(){
		
		var newVC:TriggerViewController? = nil
	 
		if let event = event {
			self.lblTitle.text = event.name

			switch(eventType){
			
			case .device:
				segEvent.selectedSegmentIndex = 0
				newVC = TriggerDeviceViewController.create(withEvent: event)
				
			case .timed:
				segEvent.selectedSegmentIndex = 1
				newVC = TriggerTimeViewController.create(withEvent: event)
				
				
			case .event:
				segEvent.selectedSegmentIndex = 2
				newVC = TriggerEventViewController.create(withEvent: event)
				
			default:
				break
			}
			
			if(newVC == currentEventVC){
				return
			}
			
			if let cvc = currentEventVC {
				cvc.willMove(toParent: nil)
				cvc.view.removeFromSuperview()
				cvc.removeFromParent()
				currentEventVC = nil
			}
			
			if let newVC = newVC {
				currentEventVC = newVC
				newVC.view.frame = self.vwEvent.bounds
				newVC.willMove(toParent: self)
				self.vwEvent.addSubview(newVC.view)
				self.addChild(newVC)
				newVC.didMove(toParent: self)
				newVC.delegate = self
			}
		}
	
		
	}
	
	
	
	func refreshEvent() {
		
		if(event == nil){
			HomeControl.shared.fetchData(.event, ID: self.eventID) { result in
				if case .success(let evt as RESTEvent) = result {
					
					DispatchQueue.main.async{
						
						self.btnSave.isEnabled = false

						self.event = evt
						self.eventType = evt.eventType()
						self.setupSubviewsForEvent()
						}
				}
				
			}
		}
		
	}
	
	// MARK:- segment control
	
	@objc func segmentedValueChanged(_ sender:UISegmentedControl!) {
		switch( sender.selectedSegmentIndex) {
		case 0:
			eventType = .device
			
		case 1:
			eventType = .timed
			
		case 2:
			eventType = .event
			
		default: break
			
		}
		
		didChangeEvent = true
		btnSave.isEnabled = true
		setupSubviewsForEvent()
	}

	// MARK: -


	
	@IBAction func btnSaveClicked(_ sender: Any) {

 	}
	
	func triggerViewChanged(){
		btnSave.isEnabled = true
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
