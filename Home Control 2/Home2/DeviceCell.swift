//
//  DeviceCell.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/26/21.
//

import UIKit

public protocol DeviceCellDelegate  {
	func switchDidChange(deviceID: String, newState:Bool)
}

final class DeviceCell: UITableViewCell {
	
//	static let reuseIdentifier: String = String(describing: self)

	static var nib: UINib {
		 return UINib(nibName: String(describing: self), bundle: nil)
	}

	
	public static let reuseIdentifier = "DeviceCell"
	
	@IBOutlet var lblName	: UILabel!
	@IBOutlet var img	: UIImageView!
	@IBOutlet var sw	: UISwitch!

	var delegate:DeviceCellDelegate? = nil
	var deviceID:String? = nil

	override func awakeFromNib() {
		super.awakeFromNib()
	}
	
	@IBAction func switchChanged(sender: UISwitch) {
		
		
		if let delegate = self.delegate,
			let deviceID = self.deviceID
		{
			delegate.switchDidChange(deviceID: deviceID, newState: sender.isOn)
		}
	}
	
}
