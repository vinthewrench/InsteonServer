//
//  SetupViewController.swift
//  pumphouse
//
//  Created by Vincent Moscaritolo on 9/24/21.
//

import Foundation
import UIKit
import Toast
 
class SetupViewController: UIViewController,
									UITextFieldDelegate,
									UIAdaptivePresentationControllerDelegate {

	enum loginState {
		case loading, success, failed, disconnected
	}
 	@IBOutlet var txtServerHost	: UITextField!
	@IBOutlet var txtServerPort	: UITextField!
	@IBOutlet var txtKey			: UITextField!
	@IBOutlet var txtSecret		: UITextField!
	@IBOutlet var btnConnect		: BHButton!
	@IBOutlet var btnSecured		: UIButton!
	
	@IBOutlet var lblStatus		: UILabel!
	@IBOutlet var btnInfo		: UIButton!
	
	
//	var svc : StatusViewController?
 	var timer = Timer()
	var isSecured: Bool = true
	
	private var connectState = loginState.disconnected

	class func create() -> SetupViewController? {
		let storyboard = UIStoryboard(name: "SetupView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "SetupViewController") as? SetupViewController
		
		return vc
	}
 
	
	override func viewDidLoad() {
		super.viewDidLoad()
		self.presentationController?.delegate = self
}
	
	func presentationControllerShouldDismiss(_ presentationController: UIPresentationController) -> Bool {
			 return connectState == loginState.success
		}

	
	func textFieldShouldReturn(_ textField: UITextField) -> Bool {
		 textField.resignFirstResponder()
		 return true
	}

	
	private func refreshView() {
		
		if(connectState == loginState.success){
			
			self.btnConnect.setTitle("Disconnect", for: .normal)
			
			HomeControl.shared.fetchData(.plmInfo) { result in
					
				if case .success(let plmInfo as RESTPlmInfo) = result {
				
					self.lblStatus.text = plmInfo.stateString
					self.btnInfo.isEnabled = true

				}
				else {
					self.lblStatus.text = "Failed"
					self.btnInfo.isEnabled = false
				}
			}
					
		}
		else {
			
			self.btnConnect.setTitle("Connect", for: .normal)
			
			lblStatus.text = "Not Connected"
			btnInfo.isEnabled = false
		}
		

		
		
	}
	
		
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		stopPollng();
	}
	 
	override func viewWillAppear(_ animated: Bool) {
		
		super.viewWillAppear(animated)
	
		let serverHost: String = AppData.serverInfo.url?.host ?? ""
		let port: String = String(AppData.serverInfo.url?.port ?? 8080)
		let apiKey: String =  AppData.serverInfo.apiKey ?? ""
		let apiSecret: String = AppData.serverInfo.apiSecret ?? ""
 
		txtServerHost.text = serverHost;
		txtServerPort.text = port;
		txtKey.text = apiKey;
		txtSecret.text = apiSecret;
		isSecured = true
		refreshSecureText()
	
		connectState =  AppData.serverInfo.validated ? loginState.success: loginState.disconnected

		if(connectState	 == .success) 	{
			startPolling();
		}
		else {
			stopPollng();
		}
		
		refreshView()
	}
	
	@IBAction func btnSecuredClicked(_ sender: Any) {
		
		isSecured = !isSecured
		 refreshSecureText()
	}

	
	func refreshSecureText() {
		let  image = isSecured ? UIImage(named: "Eye_Closed_Template")  :UIImage(named: "Eye_Open_Template")
		btnSecured.setImage(image, for: .normal)
		
		txtSecret.isSecureTextEntry = isSecured

	}
	
	@IBAction func btnInfoClicked(_ sender: Any) {
		if let setupView = InfoViewController.create() {
			self.show(setupView, sender: self)
		}
		
	}
 
	 
	@IBAction func btnConnectClicked(_ sender: Any) {

		self.stopPollng();

		if(connectState == loginState.success){
			self.connectState = loginState.disconnected
			AppData.serverInfo.validated = false
			refreshView()
			return
		}
		
	
///		hideKeyboard()
		
		self.view.endEditing(true)

		if let url =  URL(string:"http://\(txtServerHost.text ?? "localhost"):\(txtServerPort.text ?? "8080")") {
			AppData.serverInfo.url = url
		}

		AppData.serverInfo.apiKey  = txtKey.text
		AppData.serverInfo.apiSecret  = txtSecret.text
	
		self.connectState = loginState.loading
		self.btnConnect.isHidden = true
		
		HomeControl.shared.fetchData(.status) { result in
			
			switch result {
			case .success( _ as RESTStatus):
				
				self.connectState = loginState.success
				AppData.serverInfo.validated = true
				
				self.dismiss(animated: true)

//				self.startPolling();
//				self.refreshView()
				break
				
			case .success(let status as RESTError):
				self.connectState = loginState.failed
				AppData.serverInfo.validated = false
		 
				Toast.text(status.error.message,
											  config: ToastConfiguration(
												autoHide: true,
												displayTime: 1.0
//												attachTo: self.vwError
											  )).show()
				
 
				
				break;
				
			case .success(_):
				assert(true, "unknown condition")
				break;
				
				
			case .failure(let error):
	//			print(error.localizedDescription)
				
				self.connectState = loginState.failed
				AppData.serverInfo.validated = false
		 
				Toast.text("Failed",
							  subtitle:error.localizedDescription,
											  config: ToastConfiguration(
												autoHide: true,
												displayTime: 1.0
//			 									attachTo: self.vwError
											  )).show()

			}
			
			self.btnConnect.isHidden = false

	  }

	}
	
	public func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
												self.refreshView()
												})
	}
	
	public func stopPollng(){
		timer.invalidate()
	}
 
}
