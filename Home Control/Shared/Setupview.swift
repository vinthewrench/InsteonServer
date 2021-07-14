//
//  Setupview.swift
//  Home Control
//
//  Created by Vincent Moscaritolo on 6/27/21.
//

import SwiftUI
import AlertToast

struct EyeImage: View {
	 
	 // 1
	 private var imageName: String
	 init(name: String) {
		  self.imageName = name
	 }
	 
	 // 2
	 var body: some View {
		  Image(imageName)
				.resizable()
			.renderingMode(/*@START_MENU_TOKEN@*/.template/*@END_MENU_TOKEN@*/)
				.foregroundColor(.blue)
				.frame(width: 32, height: 32, alignment: .trailing)
	 }
}


struct BlueButtonStyle: ButtonStyle {
	
	@Environment(\.isEnabled) var isEnabled

	 func makeBody(configuration: Configuration) -> some View {
		  configuration.label
				.font(.system(size: 20, weight:.bold, design: .rounded))
				.foregroundColor(.white)
				.padding(.horizontal)
				.padding(5)
				.background( isEnabled ? Color.blue.opacity(0.8) :  Color.gray.opacity(0.5) )
				.cornerRadius(20)
				.opacity(configuration.isPressed ? 0.6 : 1.0)
				.scaleEffect(configuration.isPressed ? 0.8 : 1.0)
	 }
}

struct Setupview: View {
	
	@State var serverHost: String = AppData.serverInfo.url?.host ?? ""
	@State var port: String = String(AppData.serverInfo.url?.port ?? 8080)
	@State var apiKey: String =  AppData.serverInfo.apiKey ?? ""
	@State var apiSecret: String = AppData.serverInfo.apiSecret ?? ""
	
	@State private var isPresentingToast = false
	@State private var isPresentingProgress = false
	
	enum loginState {
		case loading, success, failed, disconnected
	}
	
	@State private var connectState = loginState.disconnected
 
	@State private var errorString:String = ""
	@State private var secured: Bool = true
	
	var body: some View {
		
		GeometryReader { formView in
			let width1 = formView.size.width/4.0
			
			Form {
				Section(header: Text("Server")) {
					HStack( spacing: 8.0) {
						Text("IP Address:")
							.font(.headline)
							.frame(maxWidth: width1,
									 alignment: .leading)
						TextField("IP Address", text: $serverHost)
							.frame(maxWidth: .infinity, alignment: .leading)
							.font(.body)
							.disableAutocorrection(true)
							.autocapitalization(.none)
						
					}
					HStack( spacing: 8.0) {
						Text("Port:")
							.font(.headline)
							.frame(maxWidth: width1, alignment: .leading)
						TextField("Port", text: $port)
							.frame(maxWidth: .infinity, alignment: .leading)
							.font(.body)
							.keyboardType(.decimalPad)
					}
					
					HStack( spacing: 8.0) {
						Text("Key:")
							.font(.headline)
							.frame(maxWidth: width1, alignment: .leading)
						TextField("Key", text: $apiKey)
							.frame(maxWidth: .infinity, alignment: .leading)
							.font(.body)
							.autocapitalization(.none)
							.disableAutocorrection(true)
							.keyboardType(.asciiCapable)
					}
					
					HStack( spacing: 8.0) {
						
						Text("Secret:")
							.font(.headline)
							.frame(maxWidth: width1, alignment: .leading)
						
						HStack {
							
							if secured {
								SecureField("Secret", text: $apiSecret)
									.padding(4)
									.font(.body)
									.disableAutocorrection(true)
									.keyboardType(.asciiCapable)
								
							} else {
								
								TextField("Secret", text: $apiSecret)
									.padding(4)
									.font(.body)
									.disableAutocorrection(true)
									.keyboardType(.asciiCapable)
							}
							
							Button(action: {
								hideKeyboard()
								self.secured.toggle()
								
							}) {
								
								if secured {
									EyeImage(name: "Eye_Closed")
								} else {
									EyeImage(name: "Eye_Open")
								}
							}
							
						}
					}
					
					Group() {
						HStack(){
							Spacer()
								if(connectState == loginState.success) {
								
								Button(action: {
									
									DispatchQueue.main.async{
										AppData.serverInfo.validated = false
										connectState = loginState.disconnected
									}
									
								}) {
									Text("Disconnect")
								}
								.buttonStyle(BlueButtonStyle())

							}
							else {
								Button(action: {
									hideKeyboard()
									AppData.serverInfo.apiKey  = apiKey
									AppData.serverInfo.apiSecret  = apiSecret
									
									if let url =  URL(string:"http://\(serverHost):\(port)") {
										AppData.serverInfo.url = url
									}
									
									connectState = loginState.loading
									isPresentingProgress = true;
									
									HCServerManager.shared.RESTCall(urlPath: "status",
																		  headers:nil,
																		  queries: nil) { (response, json, error)  in
										
										isPresentingProgress = false;
										if(error == nil){
											
											switch json {
											
											case is RESTErrorInfo:
												let restErr = json as! RESTErrorInfo
													connectState = loginState.failed
													errorString = restErr.message;
													isPresentingToast = true;
													AppData.serverInfo.validated = false
													return
		
			 
											case is RESTStatus:
 													connectState = loginState.success
													AppData.serverInfo.validated = true
												return
		
											default:
												break
											}
										}
										
										self.isPresentingToast = true
										
										if let error = error {
											errorString = error.localizedDescription
										}
										else
										{
											errorString = response?.httpStatusCodeString() ?? "Unknown"
										}
												
										connectState = loginState.failed
									}
									
								})
								{
									Text("Login")
								}
								.disabled(connectState == loginState.failed
												|| connectState == loginState.loading)
								.buttonStyle(BlueButtonStyle())
							}
							Spacer()
						}
						
					}
				}
				
				Section(header: Text("Info")) {
					if(connectState == loginState.success){
						ServerInfoView(width1: width1)
							.padding(4)
					}
				}
			}
			
		}
		.toast(isPresenting: $isPresentingProgress,
				 duration: 0,
				 tapToDismiss: false) {
			
			AlertToast(type: .loading, title: "Connecting…", subTitle: serverHost)
		}
		
		.toast(isPresenting: $isPresentingToast,
				 duration: 2,
				 tapToDismiss: true,
				 alert: {
					AlertToast( type: .error(.red),
										title: "Error!",
										subTitle: errorString)
			
 
		}, onTap: {
			
			//onTap would call either if `tapToDismis` is true/false
			//If tapToDismiss is true, onTap would call and then dismis the alert
		}, completion: {
			
			connectState = loginState.disconnected
			
			//Completion block after dismiss
		})
//		.toast(isPresenting: $isPresentingToast){
//
//			// `.alert` is the default displayMode
//			//		 AlertToast(type: .regular, title: "Message Sent!")
//
//			AlertToast( type: .error(.red),
//							title: "Error!",
//							subTitle: errorString
//
//
//			//Choose .hud to toast alert from the top of the screen
//			//AlertToast(displayMode: .hud, type: .regular, title: "Message Sent!")
//		}
//
		.onAppear(){
 			connectState =  AppData.serverInfo.validated ? loginState.success: loginState.disconnected
		}
		
	}
	
	
}

struct Setupview_Previews: PreviewProvider {
	static var previews: some View {
		Group {
			Setupview()
		}
	}
}
