//
//  KeypadsViewController.swift
//  homecontrol app
//
//  Created by Vincent Moscaritolo on 11/18/21.
//
 
import UIKit
import Toast

class KeyCapButton: UIButton {

	override init(frame: CGRect) {
			 super.init(frame: frame)
		setupButton()
		  }

		  required init?(coder aDecoder: NSCoder) {
			 super.init(coder: aDecoder)
			  setupButton()
		  }
		  
		  override func prepareForInterfaceBuilder() {
			 super.prepareForInterfaceBuilder()
			  setupButton()
		  }
	 

	override var isSelected: Bool {
			willSet(newValue) {
				 super.isSelected = newValue;
		 
				updateBackgroundColor()
				
			}
	  }



	func updateBackgroundColor() {
		
		if (isSelected) {
			self.backgroundColor = UIColor(named: "keyCapColor")!
		}
		else {
			self.backgroundColor = .clear
			
		}
	}
	
	func setupButton() {
		
		titleLabel?.textAlignment = .center
		titleLabel?.numberOfLines = 0
		titleLabel?.lineBreakMode = .byWordWrapping
	//	titleLabel?.font =  UIFont.preferredFont(forTextStyle: .subheadline)
		
		titleLabel?.font = UIFont.systemFont(ofSize: 17, weight: .regular)
	//	titleLabel?.font = UIFont(name: "Helvetica Neue", size: 24)
		
		titleEdgeInsets = UIEdgeInsets(top: 0, left: 10, bottom: 0, right: 10)
	
		sizeToFit()
		setTitleColor(.black, for: .normal)
		setTitleColor(.black, for: .selected)
		
		titleLabel?.backgroundColor = .clear
		tintColor = .clear
	//	setTitleColor(.white, for: .focused)
	
		
		borderWidth = 2
		cornerRadius = 8
		borderColor = .systemGray2
		
	 	shadowColor = .darkGray
		
		shadowRadius	 = 8
		shadowOpacity = 0.9
		shadowOffset	= CGSize(width: 8, height: 8)
		
		backgroundColor = .systemGray5
		

	}
	
}

class KeypadViewController: UIViewController, EditableUILabelDelegate {
	
	@IBOutlet var lblID : EditableUILabel!
	
	@IBOutlet var btn0 : KeyCapButton!
	@IBOutlet var btn1 : KeyCapButton!
	@IBOutlet var btn2 : KeyCapButton!
	@IBOutlet var btn3 : KeyCapButton!
	@IBOutlet var btn4 : KeyCapButton!
	@IBOutlet var btn5 : KeyCapButton!
	@IBOutlet var btn6 : KeyCapButton!
	@IBOutlet var btn7 : KeyCapButton!
	
	var keyPadID : String? = nil
	var keypad : RESTKeypad? = nil
	
	var timer = Timer()
	
	class func create() -> KeypadViewController? {
		let storyboard = UIStoryboard(name: "KeypadsView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "KeypadViewController") as? KeypadViewController
		
		return vc
	}
	
	private func button(key : String) ->UIButton?{
		
		var item :UIButton?  = nil
		
		if let i = Int(key){
			switch(i){
			case 1 : item = self.btn0
			case 2 : item = self.btn1
			case 3 : item = self.btn2
			case 4 : item = self.btn3
			case 5 : item = self.btn4
			case 6 : item = self.btn5
			case 7 : item = self.btn6
			case 8 : item = self.btn7
			default:
				item = nil
			}
		}
		
		return item
	}
	
	private func keyForButton(button : UIButton) -> String?{
		
		var key :String?  = nil
		
		switch(button){
		case self.btn0: 	key = "1"
		case self.btn1: 	key = "2"
		case self.btn2: 	key = "3"
		case self.btn3: 	key = "4"
		case self.btn4: 	key = "5"
		case self.btn5: 	key = "6"
		case self.btn6: 	key = "7"
		case self.btn7: 	key = "8"
			
		default:
			key = nil
		}
		return key
	}
	
	
	func setKeyPadID(deviceID : String?){
		
		keyPadID = deviceID;
		refreshView();
		
	}
	
	override func viewDidLoad() {
		super.viewDidLoad()
		lblID.delegate = self
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		if(AppData.serverInfo.validated){
			startPolling();
		}
		else {
			stopPollng();
		}
		
		
		refreshView();
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		stopPollng();
		
	}
	
	func startPolling() {
		timer =  Timer.scheduledTimer(withTimeInterval: 1.0,
												repeats: true,
												block: { timer in
			
			guard self.keyPadID != nil else {
				return
			}
			
			HomeControl.shared.fetchData(.keypad, ID: self.keyPadID) { result in
				if case .success(let keypad as RESTKeypad) = result {
					self.keypad = keypad
					self.refreshKeyCaps()
				}
			}
			
		})
	}
	
	func stopPollng(){
		timer.invalidate()
	}
	
	
	@IBAction func btnClicked(_ sender: UIButton) -> Void {
		
		if let key = self.keyForButton(button: sender){
			
			if let but = keypad?.buttons![key] {
				var newLevel: Bool = true
				
				if( but.level == "on") {
					newLevel = false
					keypad?.buttons![key]?.level = "off"
				}
				else {
					keypad?.buttons![key]?.level = "on"
				}
				
				sender.isSelected	 = newLevel
				
				HCServerManager.shared.SetKeypadButton(keypadID: keyPadID!,
																	button: key,
																	on: newLevel)
				{ (error)  in
					
					//					DispatchQueue.main.async{
					//						self.refreshView()
					//					}
				}
				
			}
			
			
		}
		
	}
	
	
	private func refreshView() {
		
		guard self.isViewLoaded  else {
			return
		}
		
		guard keyPadID != nil else {
			lblID.text = "<nil>"
			return
		}
		
		lblID.text = keyPadID
		
		HomeControl.shared.fetchData(.keypad, ID: keyPadID) { result in
			if case .success(let keypad as RESTKeypad) = result {
				
				self.keypad = keypad
			}
			else {
				self.keypad = nil
			}
			
			//		DispatchQueue.main.async{
			self.refreshKeyPad()
			//		}
		}
	}
	
	private func refreshKeyPad() {
		
		guard let kp = self.keypad else {
			self.lblID.text = "unknown <\(self.keyPadID ?? "nil")>"
			return
		}
		
		self.lblID.text = kp.name
		
		refreshKeyCaps()
	}
	
	private func refreshKeyCaps() {
		
		guard let buttons =  self.keypad?.buttons else {
			return
		}
		
		for (key, but) in buttons {
			
			if let button = self.button(key:key ) {
				button.setTitle(but.name, for: .normal)
				
				if but.level == "on" {
					button.isSelected = true
				}
				else {
					button.isSelected = false
				}
			}
		}
	}
	
	func renameKeyboard(newName:String){
		
		InsteonFetcher.shared.renameDevice(keyPadID!, newName: newName)
		{ (error)  in
			
			if(error == nil){
				self.refreshView()
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
		
		let alert = UIAlertController(title:  NSLocalizedString("Rename Keypad", comment: ""),
												message: nil,
												cancelButtonTitle: NSLocalizedString("Cancel", comment: ""),
												okButtonTitle:  NSLocalizedString("Rename", comment: ""),
												validate: .nonEmpty,
												textFieldConfiguration: { textField in
			textField.placeholder =  "Keypad Name"
			textField.text = self.lblID.text
		}) { result in
			
			switch result {
			case let .ok(String:newName):
				self.renameKeyboard( newName: newName);
				break
				
			case .cancel:
				break
			}
		}
		
		// Present the alert to the user
		self.present(alert, animated: true, completion: nil)
		}
}
 

class KeypadsViewController: UIViewController {
	
	private var pageController: UIPageViewController?
	
	private var keypadIDs: [String] = []
	private var lastKeyPadID: String?
 
	private var kpVCs:[KeypadViewController] = []
	
	static let shared: KeypadsViewController! = {
		
		let storyboard = UIStoryboard(name: "KeypadsView", bundle: nil)
		let vc = storyboard.instantiateViewController(withIdentifier: "KeypadsViewController") as? KeypadsViewController
		
		return vc
	}()
	
	override func viewDidLoad() {
		super.viewDidLoad()
		self.setupPageController()
	}
	
	private func setupPageController() {
		
		self.pageController = UIPageViewController(transitionStyle: .scroll,
																 navigationOrientation: .horizontal,
																 options: nil)
		
		guard let pg = self.pageController else {
			return
		}
		
		pg.dataSource = self
		pg.delegate = self
		pg.view.backgroundColor = .clear
		pg.view.frame = CGRect(x: 0,y: 0,
									  width: self.view.frame.width,
									  height: self.view.frame.height)
		
		self.addChild(pg)
		self.view.addSubview(pg.view)
		pg.didMove(toParent: self)
		
		for item in pg.view?.subviews ?? []{
			if item is UIPageControl{
				(item as! UIPageControl).pageIndicatorTintColor = #colorLiteral(red: 0.6666666865, green: 0.6666666865, blue: 0.6666666865, alpha: 1)
				(item as! UIPageControl).currentPageIndicatorTintColor = #colorLiteral(red: 0.3333333433, green: 0.3333333433, blue: 0.3333333433, alpha: 1)
				break
			}
		}
	}
	
	override func viewWillAppear(_ animated: Bool) {
		super.viewWillAppear(animated)
		
		guard AppData.serverInfo.validated  else {
			return
		}
		
		HomeControl.shared.fetchData(.keypads) { result in
				if case .success(let keypads as RESTKeypads) = result {
				
	 		self.keypadIDs = keypads.deviceIDs
//	 	 self.keypadIDs = [ keypads.deviceIDs[0], keypads.deviceIDs[0], "keypad 2"]
				
				self.kpVCs.removeAll()
				for keypadID in self.keypadIDs  {
					if let vc = KeypadViewController.create() {
						vc.setKeyPadID(deviceID: keypadID)
						self.kpVCs.append(vc)
					}
				}
		 
				guard let pg = self.pageController else {
					return
				}
					
				for item in pg.view?.subviews ?? []{
					if item is UIPageControl{
						(item as! UIPageControl).isHidden =  self.keypadIDs.count < 2
						break
					}
				}
				
				if(self.kpVCs.count > 0) {
					
					var index = 0
					if let lastID = self.lastKeyPadID,
						let newIndex = self.keypadIDs.firstIndex(of: lastID)  {
						index = newIndex
					}
			 
					pg.setViewControllers([self.kpVCs[index]],
												 direction: .forward,
												 animated: false,
												 completion: nil)
				}
				
				
			}
		}
		
	}
	
	override func viewWillDisappear(_ animated: Bool) {
		super.viewWillDisappear(animated)
		
		guard let pg = self.pageController else {
			return
		}
	
		let index = self.presentationIndex(for: pg)
		if(keypadIDs.count > 0) {
			lastKeyPadID = keypadIDs[index]
		}
	}
}
	

extension KeypadsViewController: UIPageViewControllerDataSource, UIPageViewControllerDelegate {
	 
	func pageViewController(_ pageViewController: UIPageViewController,
									viewControllerBefore viewController: UIViewController) -> UIViewController? {
		
		guard let vc = viewController as? KeypadViewController else {
	 				return nil
		}
	 
		if let index = self.kpVCs.firstIndex(of: vc)  {
			
			if index == 0 {
				return nil
			}
			
			let newIndex = index - 1
			return self.kpVCs[newIndex]
		}
		
		return nil
	}

	 
	 func pageViewController(_ pageViewController: UIPageViewController, viewControllerAfter viewController: UIViewController) -> UIViewController? {

	 		 guard let vc = viewController as? KeypadViewController else {
					 return nil
		 }
	  
		 if let index = self.kpVCs.firstIndex(of: vc)  {
			 
			 if index == keypadIDs.count - 1 {
					return nil
				}
			 
			 let newIndex = index + 1
			 return self.kpVCs[newIndex]
		 }
		 
		 return nil
	 }
	 
	 func presentationCount(for pageViewController: UIPageViewController) -> Int {
	
		 return self.keypadIDs.count
	 }
	 
	 func presentationIndex(for pageViewController: UIPageViewController) -> Int {
		 
		 if let vcs = pageViewController.viewControllers,
			 let vc = vcs.first as? KeypadViewController,
				let index = self.kpVCs.firstIndex(of: vc)  {
				
			 return index
		}
		
		 return 0
	 }
	
	
}

