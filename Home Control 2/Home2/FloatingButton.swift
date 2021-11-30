//
//  FloatingButton.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 11/29/21.
//


import UIKit


public protocol FloatingButtonDelegate  {
	func floatingButtonHit(sender: Any)
}


class FloatingButton: UIButton {
	
	private var button = UIButton(frame: .zero)
	
	var delegate: FloatingButtonDelegate? = nil
	
	public init() {
		super.init(frame: .zero)
		
		button.translatesAutoresizingMaskIntoConstraints = false
		button.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.6)
		
		button.setImage(
			UIImage(systemName: "plus")?.withTintColor(.white, renderingMode:.alwaysOriginal), for: .normal)
		
		button.setPreferredSymbolConfiguration(UIImage.SymbolConfiguration(pointSize: 36), forImageIn: .normal)
		
		button.addTarget(self, action: #selector(btnNewTapped(_:)), for: .touchUpInside)
		
	}
 
	@objc func btnNewTapped(_ button: UIButton) {
		delegate?.floatingButtonHit(sender: self)
	}
		required init(coder: NSCoder) {
			 fatalError("init(coder:) has not been implemented")
		}
	
	private func layoutUI(toView: UIView)  {
		NSLayoutConstraint.activate([
			button.trailingAnchor.constraint(equalTo: toView.trailingAnchor, constant: -20),
			button.bottomAnchor.constraint(equalTo: toView.bottomAnchor, constant: -20),
			button.heightAnchor.constraint(equalToConstant: 60),
			button.widthAnchor.constraint(equalToConstant: 60)
		])
		button.layer.cornerRadius = 30
		button.layer.masksToBounds = true
		button.layer.borderColor = UIColor.lightGray.cgColor
		button.layer.borderWidth = 2
	}
	
	func setup(toView: UIView) {
		if let view = UIApplication.shared.windows.first(where: { $0.isKeyWindow })  {
			view.addSubview(button)
			layoutUI(toView: toView)
		}
	}
	
	func remove() {
		if let view = UIApplication.shared.windows.first(where: { $0.isKeyWindow }),
			button.isDescendant(of: view) {
			button.removeFromSuperview()
		}
	}
	
 }
