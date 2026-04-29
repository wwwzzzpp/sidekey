import Cocoa
import FlutterMacOS
import ApplicationServices
import ServiceManagement

private struct NativeKeyBinding {
  let strokes: [NativeKeyStroke]

  var isValid: Bool {
    !strokes.isEmpty && strokes.allSatisfy { $0.isValid }
  }

  var isHoldable: Bool {
    strokes.count == 1
  }

  static let invalid = NativeKeyBinding(strokes: [])
  static let enter = NativeKeyBinding(
    strokes: [NativeKeyStroke(macosKeyCode: 36, modifiers: [])]
  )
}

private struct NativeKeyStroke {
  let macosKeyCode: Int
  let modifiers: [String]

  var isValid: Bool {
    macosKeyCode >= 0
  }
}

final class SidekeyController {
  private var channel: FlutterMethodChannel?
  private var eventTap: CFMachPort?
  private var runLoopSource: CFRunLoopSource?

  private let backButtonNumber = 3
  private let forwardButtonNumber = 4

  private var enabled = true
  private var interceptOriginal = true
  private var voiceHoldMode = false
  private var voicePressed = false
  private var voiceBinding = NativeKeyBinding.invalid
  private var backBinding = NativeKeyBinding.enter

  func attach(to messenger: FlutterBinaryMessenger) {
    channel = FlutterMethodChannel(name: "sidekey/native", binaryMessenger: messenger)
    channel?.setMethodCallHandler { [weak self] call, result in
      self?.handle(call: call, result: result)
    }
  }

  private func handle(call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "applyConfig":
      result(applyConfig(call.arguments as? [String: Any]))
    case "getPermissionStatus":
      result(permissionStatus())
    case "openPermissionSettings":
      openPermissionSettings()
      result(nil)
    case "setLaunchAtLogin":
      let args = call.arguments as? [String: Any]
      let enabled = args?["enabled"] as? Bool ?? false
      result(setLaunchAtLogin(enabled))
    case "testAction":
      let args = call.arguments as? [String: Any]
      let action = args?["action"] as? String ?? ""
      result(trigger(action: action))
    case "shutdown":
      shutdown()
      result(nil)
    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private func applyConfig(_ args: [String: Any]?) -> [String: Any] {
    guard let args else {
      return status(message: "Invalid config.")
    }

    if voicePressed {
      release(binding: voiceBinding)
      voicePressed = false
    }

    enabled = args["enabled"] as? Bool ?? true
    interceptOriginal = args["interceptOriginal"] as? Bool ?? true
    voiceHoldMode = (args["voiceMode"] as? String ?? "tap") == "hold"
    voiceBinding = parseBinding(args["voiceBinding"] as? [String: Any])
    backBinding = parseBinding(args["backBinding"] as? [String: Any])
    if !backBinding.isValid {
      backBinding = NativeKeyBinding.enter
    }

    if enabled {
      startEventTap()
    } else {
      stopEventTap()
    }

    if !enabled {
      return status(message: "Mapping paused.")
    }
    let permission = permissionStatus()["permission"] as? String
    if permission != "granted" {
      return status(message: "macOS needs Accessibility and Input Monitoring permission.")
    }
    if eventTap == nil {
      return status(message: "Event tap failed. Check macOS privacy permissions.")
    }
    if !voiceBinding.isValid {
      return status(message: "Back button maps to Enter. Record a voice hotkey for the forward button.")
    }
    return status(message: "macOS event tap running. Forward maps to voice hotkey; back maps to Enter.")
  }

  private func permissionStatus() -> [String: Any] {
    let accessibilityAllowed = AXIsProcessTrusted()
    var inputAllowed = true
    if #available(macOS 10.15, *) {
      inputAllowed = CGPreflightListenEventAccess()
    }
    let granted = accessibilityAllowed && inputAllowed
    return [
      "running": enabled && eventTap != nil,
      "permission": granted ? "granted" : "needsPermission",
      "message": granted
        ? "macOS permissions granted."
        : "Enable SideKey in Accessibility and Input Monitoring."
    ]
  }

  private func status(message: String) -> [String: Any] {
    let permission = permissionStatus()["permission"] as? String ?? "unknown"
    return [
      "running": enabled && eventTap != nil,
      "permission": permission,
      "message": message
    ]
  }

  private func openPermissionSettings() {
    let accessibilityURL = URL(
      string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
    )
    let inputURL = URL(
      string: "x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent"
    )
    if let accessibilityURL = accessibilityURL {
      NSWorkspace.shared.open(accessibilityURL)
    }
    if let inputURL = inputURL {
      DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) {
        NSWorkspace.shared.open(inputURL)
      }
    }
  }

  private func setLaunchAtLogin(_ enabled: Bool) -> Bool {
    if #available(macOS 13.0, *) {
      do {
        if enabled {
          try SMAppService.mainApp.register()
        } else {
          try SMAppService.mainApp.unregister()
        }
        return true
      } catch {
        return false
      }
    }
    return false
  }

  private func shutdown() {
    if voicePressed {
      release(binding: voiceBinding)
      voicePressed = false
    }
    stopEventTap()
  }

  private func startEventTap() {
    if eventTap != nil {
      return
    }

    let mask = (CGEventMask(1) << CGEventType.otherMouseDown.rawValue)
      | (CGEventMask(1) << CGEventType.otherMouseUp.rawValue)
      | (CGEventMask(1) << CGEventType.tapDisabledByTimeout.rawValue)
      | (CGEventMask(1) << CGEventType.tapDisabledByUserInput.rawValue)

    let refcon = UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
    eventTap = CGEvent.tapCreate(
      tap: .cgSessionEventTap,
      place: .headInsertEventTap,
      options: .defaultTap,
      eventsOfInterest: mask,
      callback: sidekeyEventTapCallback,
      userInfo: refcon
    )

    guard let eventTap else {
      return
    }
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0)
    if let runLoopSource {
      CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, .commonModes)
    }
    CGEvent.tapEnable(tap: eventTap, enable: true)
  }

  private func stopEventTap() {
    if let eventTap {
      CGEvent.tapEnable(tap: eventTap, enable: false)
    }
    if let runLoopSource {
      CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, .commonModes)
    }
    runLoopSource = nil
    eventTap = nil
  }

  fileprivate func handleEvent(
    proxy: CGEventTapProxy,
    type: CGEventType,
    event: CGEvent
  ) -> Unmanaged<CGEvent>? {
    if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
      if let eventTap {
        CGEvent.tapEnable(tap: eventTap, enable: true)
      }
      return Unmanaged.passUnretained(event)
    }

    guard enabled, type == .otherMouseDown || type == .otherMouseUp else {
      return Unmanaged.passUnretained(event)
    }

    let buttonNumber = Int(event.getIntegerValueField(.mouseEventButtonNumber))
    let isDown = type == .otherMouseDown
    var handled = false

    if buttonNumber == backButtonNumber {
      if isDown {
        tap(binding: backBinding)
      }
      handled = true
    } else if buttonNumber == forwardButtonNumber, voiceBinding.isValid {
      if voiceHoldMode && voiceBinding.isHoldable {
        if isDown && !voicePressed {
          press(binding: voiceBinding)
          voicePressed = true
        } else if !isDown && voicePressed {
          release(binding: voiceBinding)
          voicePressed = false
        }
      } else if isDown {
        tap(binding: voiceBinding)
      }
      handled = true
    }

    if handled && interceptOriginal {
      return nil
    }
    return Unmanaged.passUnretained(event)
  }

  private func trigger(action: String) -> Bool {
    if action == "voice" {
      guard voiceBinding.isValid else {
        return false
      }
      tap(binding: voiceBinding)
      return true
    }
    if action == "back" {
      tap(binding: backBinding)
      return true
    }
    return false
  }

  private func parseBinding(_ data: [String: Any]?) -> NativeKeyBinding {
    guard let data else {
      return NativeKeyBinding.invalid
    }
    if let strokesData = data["strokes"] as? [[String: Any]] {
      let strokes = strokesData.compactMap(parseStroke)
      if !strokes.isEmpty {
        return NativeKeyBinding(strokes: strokes)
      }
    }
    if let stroke = parseStroke(data) {
      return NativeKeyBinding(strokes: [stroke])
    }
    return NativeKeyBinding.invalid
  }

  private func parseStroke(_ data: [String: Any]) -> NativeKeyStroke? {
    let keyCode: Int
    if let number = data["macosKeyCode"] as? NSNumber {
      keyCode = number.intValue
    } else {
      keyCode = data["macosKeyCode"] as? Int ?? -1
    }
    if keyCode < 0 {
      return nil
    }
    let modifiers = data["modifiers"] as? [String] ?? []
    return NativeKeyStroke(macosKeyCode: keyCode, modifiers: modifiers)
  }

  private func press(binding: NativeKeyBinding) {
    guard binding.isValid, let stroke = binding.strokes.first else {
      return
    }
    press(stroke: stroke)
  }

  private func release(binding: NativeKeyBinding) {
    guard binding.isValid, let stroke = binding.strokes.first else {
      return
    }
    release(stroke: stroke)
  }

  private func tap(binding: NativeKeyBinding) {
    guard binding.isValid else {
      return
    }
    for (index, stroke) in binding.strokes.enumerated() {
      tap(stroke: stroke)
      if index < binding.strokes.count - 1 {
        Thread.sleep(forTimeInterval: 0.06)
      }
    }
  }

  private func press(stroke: NativeKeyStroke) {
    guard stroke.isValid else {
      return
    }
    var flags: CGEventFlags = []
    for modifier in stroke.modifiers {
      flags.insert(modifierFlag(for: modifier))
      sendModifier(modifier, keyDown: true, flags: flags)
    }
    sendKey(
      code: stroke.macosKeyCode,
      keyDown: true,
      flags: flags.union(modifierFlag(forKeyCode: stroke.macosKeyCode))
    )
  }

  private func release(stroke: NativeKeyStroke) {
    guard stroke.isValid else {
      return
    }
    var flags = eventFlags(for: stroke.modifiers)
    sendKey(
      code: stroke.macosKeyCode,
      keyDown: false,
      flags: flags
    )
    for modifier in stroke.modifiers.reversed() {
      flags.remove(modifierFlag(for: modifier))
      sendModifier(modifier, keyDown: false, flags: flags)
    }
  }

  private func tap(stroke: NativeKeyStroke) {
    press(stroke: stroke)
    Thread.sleep(forTimeInterval: 0.045)
    release(stroke: stroke)
  }

  private func sendModifier(_ modifier: String, keyDown: Bool, flags: CGEventFlags) {
    guard let code = modifierKeyCode(modifier) else {
      return
    }
    sendKey(code: code, keyDown: keyDown, flags: flags)
  }

  private func sendKey(code: Int, keyDown: Bool, flags: CGEventFlags) {
    guard let source = CGEventSource(stateID: .hidSystemState),
      let event = CGEvent(
        keyboardEventSource: source,
        virtualKey: CGKeyCode(code),
        keyDown: keyDown
      )
    else {
      return
    }
    event.flags = event.flags.union(flags)
    event.post(tap: .cghidEventTap)
  }

  private func eventFlags(for modifiers: [String]) -> CGEventFlags {
    var flags: CGEventFlags = []
    for modifier in modifiers {
      switch modifier {
      case "ctrl", "ctrlLeft", "ctrlRight":
        flags.insert(.maskControl)
      case "shift", "shiftLeft", "shiftRight":
        flags.insert(.maskShift)
      case "alt", "altLeft", "altRight":
        flags.insert(.maskAlternate)
      case "meta", "metaLeft", "metaRight":
        flags.insert(.maskCommand)
      default:
        break
      }
    }
    return flags
  }

  private func modifierKeyCode(_ modifier: String) -> Int? {
    switch modifier {
    case "ctrl", "ctrlLeft":
      return 59
    case "ctrlRight":
      return 62
    case "shift", "shiftLeft":
      return 56
    case "shiftRight":
      return 60
    case "alt", "altLeft":
      return 58
    case "altRight":
      return 61
    case "meta", "metaLeft":
      return 55
    case "metaRight":
      return 54
    default:
      return nil
    }
  }

  private func modifierFlag(for modifier: String) -> CGEventFlags {
    switch modifier {
    case "ctrl", "ctrlLeft", "ctrlRight":
      return .maskControl
    case "shift", "shiftLeft", "shiftRight":
      return .maskShift
    case "alt", "altLeft", "altRight":
      return .maskAlternate
    case "meta", "metaLeft", "metaRight":
      return .maskCommand
    default:
      return []
    }
  }

  private func modifierFlag(forKeyCode keyCode: Int) -> CGEventFlags {
    switch keyCode {
    case 59, 62:
      return .maskControl
    case 56, 60:
      return .maskShift
    case 58, 61:
      return .maskAlternate
    case 55, 54:
      return .maskCommand
    case 63:
      return .maskSecondaryFn
    default:
      return []
    }
  }
}

private let sidekeyEventTapCallback: CGEventTapCallBack = {
  proxy, type, event, refcon in
  guard let refcon else {
    return Unmanaged.passUnretained(event)
  }
  let controller = Unmanaged<SidekeyController>
    .fromOpaque(refcon)
    .takeUnretainedValue()
  return controller.handleEvent(proxy: proxy, type: type, event: event)
}
