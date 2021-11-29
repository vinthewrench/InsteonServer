//
//  GroupsViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//


import UIKit


final class GroupsCell: UITableViewCell {
	
	public static let cellReuseIdentifier = "GroupsCell"
	
	@IBOutlet var lblName	: UILabel!
	@IBOutlet var lblCount	: UILabel!

 
	override func awakeFromNib() {
		super.awakeFromNib()
	}
	
}

class GroupsViewController:  UIViewController,
									  UITableViewDelegate,
									  UITableViewDataSource  {
	
 
	@IBOutlet var tableView: UITableView!
	
	var groupKeys: [String] = []

	static let shared: GroupsViewController! = {
		
		let storyboard = UIStoryboard(name: "GroupsView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "GroupsViewController") as? GroupsViewController
		
			return vc
	}()

 	override func viewDidLoad() {
		super.viewDidLoad()
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		refreshGroups()
		}
	
	func refreshGroups(completion: @escaping () -> Void = {}) {
		
		guard AppData.serverInfo.validated  else {
			completion()
			return
		}
		
		InsteonFetcher.shared.getGroups(){
			self.groupKeys = InsteonFetcher.shared.sortedGroupKeys()
			self.tableView.reloadData()
	
		}
}

	
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
			
			self.show(detailView, sender: self)
		}
		
	}


}
