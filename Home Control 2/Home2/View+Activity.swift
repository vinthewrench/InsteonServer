//
//  View+Activity.swift
//  Home2
//
//  Created by Vincent Moscaritolo on 12/1/21.
//

import UIKit
//import With

fileprivate let overlayViewTag = 999
fileprivate let activityIndicatorViewTag = 1000

extension UIView {
	private var activityIndicatorView: UIActivityIndicatorView {
		let view = UIActivityIndicatorView(style: .large)
			view.translatesAutoresizingMaskIntoConstraints = false
			view.tag = activityIndicatorViewTag
			view.color = UIColor.systemBlue
		return view
	}
	//
	private var overlayView: UIView {
		let view = UIView()
			view.translatesAutoresizingMaskIntoConstraints = false
			view.backgroundColor = UIColor.black
			view.alpha = 0.5
			view.tag = overlayViewTag
		return view
	}
	//
	public func displayActivityIndicator(shouldDisplay: Bool) {
		shouldDisplay ? setActivityIndicatorView() : removeActivityIndicatorView()
	}
	//
	private func setActivityIndicatorView() {
		guard !isDisplayingActivityIndicatorOverlay() else { return }
		let overlayView = self.overlayView
		let activityIndicatorView = self.activityIndicatorView
		//add subviews
		overlayView.addSubview(activityIndicatorView)
		addSubview(overlayView)
		//add overlay constraints
		overlayView.heightAnchor.constraint(equalTo: self.heightAnchor).isActive = true
		overlayView.widthAnchor.constraint(equalTo: self.widthAnchor).isActive = true
		//add indicator constraints
		activityIndicatorView.centerXAnchor.constraint(equalTo: overlayView.centerXAnchor).isActive = true
		activityIndicatorView.centerYAnchor.constraint(equalTo: overlayView.centerYAnchor).isActive = true
		//animate indicator
		activityIndicatorView.startAnimating()
	}
	//
	private func removeActivityIndicatorView() {
		let activityIndicator = getActivityIndicatorView()
		guard let overlayView = getOverlayView() else { return }
		UIView.animate(withDuration: 0.2, animations: {
			overlayView.alpha = 0.0
			activityIndicator?.stopAnimating()
		}) { (finished) in
			activityIndicator?.removeFromSuperview()
			overlayView.removeFromSuperview()
		}
	}
	//
	private func isDisplayingActivityIndicatorOverlay() -> Bool {
		getActivityIndicatorView() != nil
			&& getOverlayView() != nil
	}
	//
	private func getActivityIndicatorView() -> UIActivityIndicatorView? {
		viewWithTag(activityIndicatorViewTag) as? UIActivityIndicatorView
	}
	//
	private func getOverlayView() -> UIView? {
		viewWithTag(overlayViewTag)
	}
}
