//
//  GroupsViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//


import UIKit
import Toast

final class GroupsCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "GroupsCell"
	
	@IBOutlet var lblName	: UILabel!
	@IBOutlet var lblCount	: UILabel!

 
	override func awakeFromNib() {
		super.awakeFromNib()
	}
	
}

class GroupsViewController:  MainSubviewViewController,
									  MainSubviewViewControllerDelegate,
									  GroupsDetailViewControllerDelegate,
									  UITableViewDelegate,
									  UITableViewDataSource  {
	
		
	@IBOutlet var tableView: UITableView!

	var groupKeys: [String] = []

	private let refreshControl = UIRefreshControl()

	static let shared: GroupsViewController! = {
		
		let storyboard = UIStoryboard(name: "GroupsView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "GroupsViewController") as? GroupsViewController
		
			return vc
	}()

 	override func viewDidLoad() {
		super.viewDidLoad()

		tableView.refreshControl = refreshControl
		self.tableView.tableFooterView = UIView()
	
		// Configure Refresh Control
		refreshControl.addTarget(self, action: #selector(refreshGroupsTable(_:)), for: .valueChanged)

	}
	
	@objc private func refreshGroupsTable(_ sender: Any) {
		DispatchQueue.main.async {
			self.refreshGroups(){
				self.refreshControl.endRefreshing()
			}
		}
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
//		mainView?.btnAdd.isHidden = false

		refreshGroups()
		}
	
	override func viewDidAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
	}
	
	func refreshGroups(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		InsteonFetcher.shared.getGroups(){
			self.groupKeys = InsteonFetcher.shared.sortedGroupKeys()
			self.tableView.reloadData()
			completion()
		}
}

	// MARK: - table view

	func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
		return groupKeys.count
	}
	
	func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
		if let cell = tableView.dequeueReusableCell(withIdentifier:
																	GroupsCell.cellReuseIdentifier) as? GroupsCell{
			
			cell.accessoryType = .disclosureIndicator
			cell.selectionStyle = .none
			
			if let group =  InsteonFetcher.shared.groups[groupKeys[indexPath.row]] {
			
				cell.lblName.text = group.name
				cell.lblCount.text = String( group.deviceIDs.count)
				
		 
				cell.lblCount.layer.cornerRadius = cell.lblCount.frame.height / 2
				cell.lblCount.layer.masksToBounds = true
				
		//		let size:CGFloat =   28.0 // chosen arbitrarily
//				cell.lblCount.bounds = CGRect(x: 0.0, y: 0.0, width: size, height: size)
//				cell.lblCount.layer.cornerRadius = size / 2
//				cell.lblCount.layer.borderWidth = 1.0
//				cell.lblCount.layer.backgroundColor = UIColor.clear.cgColor
//				cell.lblCount.layer.borderColor = UIColor.systemBlue.cgColor
//				cell.lblCount.layer.masksToBounds = true
//
			}
			
				
	
			return cell
			
		}
		
		return UITableViewCell()
	}
	
	
	func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
		
	 
		let groupID = groupKeys[indexPath.row]
		if let detailView = GroupsDetailViewController.create(withGroupID: groupID) {
			
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
		
		if let group =  InsteonFetcher.shared.groups[groupKeys[indexPath.row]] {
	
			let warning = "Are you sure you want to delete the group: \"\(group.name)?"
			let alert = UIAlertController(title: "Delete Group", message: warning, preferredStyle:.alert)
			let cancelAction = UIAlertAction(title: "Cancel", style: .cancel, handler: { _ in
			})
			
			let deleteAction = UIAlertAction(title: "Delete", style: .destructive, handler: { _ in
				
				let key = self.groupKeys[indexPath.row]
				InsteonFetcher.shared.deleteGroup(key) { (error)  in
					
					if(error == nil){
						self.refreshGroups()
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
 
 
	

//	func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration?
//		{
//			 let deleteAction = UIContextualAction(style: .destructive, title: "Delete") { (action, view, handler) in
//				  print("Delete Action Tapped")
//			 }
//			 deleteAction.backgroundColor = .red
//			 let configuration = UISwipeActionsConfiguration(actions: [deleteAction])
//			configuration.performsFirstActionWithFullSwipe = false //HERE..
//			 return configuration
//		}

	
	// MARK: - GroupsDetailViewControllerDelegate
	func groupDetailChanged(GroupID: String) {
		self.refreshGroups()
	}
	
	
	// MARK: - MainSubviewViewControllerDelegate

	func addButtonHit(_ sender: UIButton){
		createNewGroup()
	}
	 

	func createNewGroup() {
		
		if(tableView.isEditing) {
			return
		}

		let alert = UIAlertController(title:  NSLocalizedString("Create Group", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("OK", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
			textField.placeholder =  "Group Name"
		}) { result in
			
			switch result {
			case let .ok(String:newName):
			 
				InsteonFetcher.shared.createGroup(newName) { (error)  in
					if(error == nil){
						self.refreshGroups()
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
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)

	}
}
