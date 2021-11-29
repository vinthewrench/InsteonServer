
//  UIButton+Extensions.swift
//  pumphouse
//
//  Created by Vincent Moscaritolo on 11/22/21.
//

import UIKit

@IBDesignable
class GradientButton: UIButton {
	 let gradientLayer = CAGradientLayer()
	 
	 @IBInspectable
	 var topGradientColor: UIColor? {
		  didSet {
				setGradient(topGradientColor: topGradientColor, bottomGradientColor: bottomGradientColor)
		  }
	 }
	 
	 @IBInspectable
	 var bottomGradientColor: UIColor? {
		  didSet {
				setGradient(topGradientColor: topGradientColor, bottomGradientColor: bottomGradientColor)
		  }
	 }
	 
	 private func setGradient(topGradientColor: UIColor?, bottomGradientColor: UIColor?) {
		  if let topGradientColor = topGradientColor, let bottomGradientColor = bottomGradientColor {
				gradientLayer.frame = bounds
				gradientLayer.colors = [topGradientColor.cgColor, bottomGradientColor.cgColor]
				gradientLayer.borderColor = layer.borderColor
				gradientLayer.borderWidth = layer.borderWidth
				gradientLayer.cornerRadius = layer.cornerRadius
				layer.insertSublayer(gradientLayer, at: 0)
		  } else {
				gradientLayer.removeFromSuperlayer()
		  }
	 }
}


final class ExtendedHitButton: UIButton {
	 
	 override func point( inside point: CGPoint, with event: UIEvent? ) -> Bool {
		  let relativeFrame = self.bounds
		  let hitTestEdgeInsets = UIEdgeInsets(top: -44, left: -44, bottom: -44, right: -44) // Apple recommended hit target
		  let hitFrame = relativeFrame.inset(by: hitTestEdgeInsets)
		  return hitFrame.contains( point );
	 }
}
