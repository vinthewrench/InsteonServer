//
//  Keyboard.swift
//  HexadecimalKeyboard-Xib
//
//  Created by Marcy Vernon on 8/18/20.
//

import UIKit

class Keyboard: UIView, NibLoadable {
    
    weak var target: (UIKeyInput & UIResponder)?
    
    @IBOutlet var buttons: [UIButton]!
    
    init(target: (UIKeyInput & UIResponder)) {
        super.init(frame: .zero)
        self.target = target
        configure()
    }
    
    @IBAction func didTapButton(_ sender: UIButton) {
        target?.insertText("\(sender.currentTitle!)")
    }
    
    @IBAction func didTapDeleteButton(_ sender: UIButton) {
        target?.deleteBackward()
    }
    
    @IBAction func didTapOKButton(_ sender: UIButton) {
        target?.resignFirstResponder()
    }
    
    func configure() {
        autoresizingMask = [.flexibleWidth, .flexibleHeight]
        setupFromNib()
        _ = buttons.map({$0.commonFormat()})
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    
}  // end of class
