//
//  SelfSizedTableView.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/1/21.
//

import UIKit

class SelfSizedTableView: UITableView {
  var maxHeight: CGFloat = UIScreen.main.bounds.size.height
  
  override func reloadData() {
	 super.reloadData()
	 self.invalidateIntrinsicContentSize()
	 self.layoutIfNeeded()
  }
  
	override func layoutSubviews() {
	super.layoutSubviews()
	invalidateIntrinsicContentSize()
	}
  override var intrinsicContentSize: CGSize {
	 let height = min(contentSize.height, maxHeight)
	 return CGSize(width: contentSize.width, height: height)
  }
}
