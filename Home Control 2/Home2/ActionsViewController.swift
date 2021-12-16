//
//  ActionsViewController.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/15/21.
//
 
import UIKit
import Toast



final class ActionsCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "ActionsCell"
	
	@IBOutlet var lblName	: UILabel!
	@IBOutlet var lblCount	: UILabel!
  
 
	override func awakeFromNib() {
		super.awakeFromNib()
	}
	
}

class ActionsViewController :MainSubviewViewController,
											MainSubviewViewControllerDelegate,
											UITableViewDelegate,
											UITableViewDataSource, ActionDetailViewControllerDelegate
{
	
	
	@IBOutlet var tableView: UITableView!

	var actionsKeys: [String] = []
	var actions: Dictionary<String,RESTActionDetail> = [:]

	private let refreshControl = UIRefreshControl()

	
 	static let shared: ActionsViewController! = {
		
		let storyboard = UIStoryboard(name: "ActionsView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "ActionsViewController") as? ActionsViewController
		
			return vc
	}()

	
	override func viewDidLoad() {
		super.viewDidLoad()
		
		tableView.refreshControl = refreshControl
		self.tableView.tableFooterView = UIView()

		// Configure Refresh Control
		refreshControl.addTarget(self, action: #selector(refreshActionsTable(_:)), for: .valueChanged)
	}
	
	@objc private func refreshActionsTable(_ sender: Any) {
		DispatchQueue.main.async {
			self.refreshActions(){
				self.refreshControl.endRefreshing()
			}
		}
	}


	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		refreshActions()

	 	}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
	}
	

	func refreshActions(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		HomeControl.shared.fetchData(.actions) { result in
			
			self.actionsKeys = []
			if case .success(let actions as RESTActionList) = result {
				
				if let groupIDs = actions.groupIDs {
					
					self.actions = groupIDs
					
					let sorted = groupIDs.sorted { (first, second) -> Bool in
						return  first.value.name?.caseInsensitiveCompare(second.value.name ?? "") == .orderedAscending
					}
					
					self.actionsKeys =  sorted.map{$0.key}
	 				}
				self.tableView.reloadData()
				completion()
			}
			
		}
	}
 
	
	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		return actionsKeys.count
	}
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	ActionsCell.cellReuseIdentifier) as? ActionsCell{
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			
			let actionID = self.actionsKeys[indexPath.row]
			if let action = self.actions[actionID] {
				
				cell.lblName.text = action.name
				if let actions = action.actions {
					cell.lblCount.text = String(  actions.count)
					cell.lblCount.layer.cornerRadius = cell.lblCount.frame.height / 2
					cell.lblCount.layer.masksToBounds = true
					cell.lblCount.isHidden = false
					
				}
				else {
					cell.lblCount.isHidden = true
				}
				
			}
			
			return cell
			
		}
		
		return UITableViewCell()
	}
	
	
	func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
		
		let actionID = self.actionsKeys[indexPath.row]
		if let detailView = ActionDetailViewController.create(withActionID: actionID) {

			detailView.delegate = self
			self.show(detailView, sender: self)
		}
		
	}
	
	func tableView(_ tableView: UITableView, editingStyleForRowAt indexPath: IndexPath) -> UITableViewCell.EditingStyle {
		return .delete
	}

	func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
		if editingStyle == .delete{
			
			self.verifyDelete(forRowAt: indexPath)
	//		print("Deleted Row")
			//				days.remove(at: indexPath.row)
			//					tableView.deleteRows(at: [indexPath], with: .left)
		}
	}
	
	func verifyDelete(forRowAt indexPath: IndexPath) {
		
		let actionID = self.actionsKeys[indexPath.row]
		if let action = self.actions[actionID] {
			
			let warning = "Are you sure you want to delete the action: \"\(action.name ?? actionID)?"
			let alert = UIAlertController(title: "Delete Action", message: warning, preferredStyle:.alert)
			let cancelAction = UIAlertAction(title: "Cancel", style: .cancel, handler: { _ in
			})
			
			let deleteAction = UIAlertAction(title: "Delete", style: .destructive, handler: { _ in
				
				let actionID = self.actionsKeys[indexPath.row]
				InsteonFetcher.shared.deleteAction(actionID) { (error)  in
					
					if(error == nil){
						self.refreshActions()
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
			})
			
			alert.addAction(cancelAction)
			alert.addAction(deleteAction)
			self.present(alert, animated: true, completion: nil)
		}
	 
	}
 
	
	// MARK: - MainSubviewViewControllerDelegate

	func addButtonHit(_ sender: UIButton){
		
 
	}
	
	// MARK: - ActionDetailViewControllerDelegate
	func actionDetailChanged(actionID: String) {
		self.refreshActions()
	}
 

}
