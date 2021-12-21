//
//  ActionDetailViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/15/21.
//

import UIKit
import Toast

final class ActionCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "ActionCell"
	
	@IBOutlet var lblID	: UILabel!
	@IBOutlet var lblNoun	: UILabel!
	@IBOutlet var img		: UIImageView!
	@IBOutlet var lblVerb	: UILabel!

	override func awakeFromNib() {
		super.awakeFromNib()
	}
}

public protocol ActionDetailViewControllerDelegate  {
	func actionDetailChanged(actionID: String)
}

class ActionDetailViewController:  UIViewController,
									  UITableViewDelegate,
									  UITableViewDataSource,
										EditableUILabelDelegate,
										FloatingButtonDelegate{
	
	
	@IBOutlet var lblTitle: EditableUILabel!
	@IBOutlet var lblActionID: UILabel!
	@IBOutlet var btnRun: 	BHButton!
	@IBOutlet var tableView: UITableView!
 
	var btnFLoat: FloatingButton = FloatingButton()
	
	var ActionID :String = ""
	var action: RESTActionDetail? = nil
	var actionKeys: [String] = []
	
	var delegate:ActionDetailViewControllerDelegate? = nil
	
	
	private let refreshControl = UIRefreshControl()
	
	class func create(withActionID: String) -> ActionDetailViewController? {
		let storyboard = UIStoryboard(name: "ActionDetailView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "ActionDetailViewController") as? ActionDetailViewController
		
		if let vc = vc {
			vc.ActionID = withActionID
		}
		
		return vc
	}
 
	override func viewDidLoad() {
		super.viewDidLoad()
		lblTitle.delegate = self
		btnFLoat.delegate = self
		self.tableView.tableFooterView = UIView()
	
		//		tableView.register(DeviceCell.nib, forCellReuseIdentifier: DeviceCell.reuseIdentifier)
		//
		//		tableView.refreshControl = refreshControl
		//
		//		// Configure Refresh Control
		//		refreshControl.addTarget(self, action: #selector(refreshDeviceTable(_:)), for: .valueChanged)
		
	}
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		refreshView()
	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		btnFLoat.setup(toView: view)
		
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		btnFLoat.remove()
	}
	
	
	
	func floatingButtonHit(sender: Any) {
		
	}
	
	func refreshView(){
		
		self.lblActionID.text = ActionID
		HomeControl.shared.fetchData(.action, ID: ActionID) { result in
			
			self.actionKeys = []
			self.action = nil

			if case .success(let action as RESTActionDetail) = result {
				
				self.action = action
				self.lblTitle.text = action.name
				
				if let actions = action.actions {
					self.actionKeys =  Array(actions.keys).sorted()
				}
				
	 
			}
			else {
				self.lblTitle.text = self.ActionID
			}
			
			self.tableView.reloadData()
		}
	}
	
	func renameAction(newName:String){
		
		InsteonFetcher.shared.renameAction(ActionID, newName: newName)
		{ (error)  in
			
			if(error == nil){
				self.refreshView()
				self.delegate?.actionDetailChanged(actionID: self.ActionID)
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
		
		let alert = UIAlertController(title:  NSLocalizedString("Rename Action", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("Rename", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
													textField.placeholder =  "Action Name"
													textField.text = self.lblTitle.text
												}) { result in
			
			switch result {
			case let .ok(String:newName):
				self.renameAction( newName: newName);
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)
	}
	
	
	// MARK: - tableView
	
	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		
		return actionKeys.count
 	}
	
	private let cellReuseIdentifier: String = "yourCellReuseIdentifier"

	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		
		
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	ActionCell.cellReuseIdentifier) as? ActionCell{
			
			
			let key = actionKeys[indexPath.row]
			
			if let action  = self.action?.actions?[key] {
				cell.lblID.text = key
				
				cell.img.image = action.image()
				cell.lblNoun.text =  action.noun()
				
				switch action.nounClass() {
				case .deviceID,
					  .keypadID:
					
					if let device =  InsteonFetcher.shared.devices[action.noun() ] {
						cell.lblNoun.text = device.name
						cell.img.image = device.deviceImage()
					}
					break
					
				case .groupID:
					if let group =  InsteonFetcher.shared.groups[action.noun() ] {
						cell.lblNoun.text = group.name
					}
					break
					
				case .insteonGroup:
					cell.lblNoun.text =  String("ALL-Link Group: \(action.noun())")
					cell.img.image = action.image()
					
				case .actionGroup:
					
					HomeControl.shared.fetchData(.action, ID: action.action_group) { result in
						if case .success(let act1 as RESTActionDetail) = result {
							cell.lblNoun.text =  act1.name
						}
					}
					break;
					
				default:
					break;
				}
				
				cell.lblVerb.text = action.verb()
			}
			
			
			
			return cell
			
		}
		
		return UITableViewCell()
		
	}
	
 
	// MARK: - Buttons
	
	@IBAction func btnRunClicked(_ sender: BHButton) {
		
		sender.isEnabled = false
		InsteonFetcher.shared.runAction(ActionID) { (error)  in
			sender.isEnabled = true
		}
		
	}
	
}

