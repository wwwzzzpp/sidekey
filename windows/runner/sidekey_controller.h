#ifndef RUNNER_SIDEKEY_CONTROLLER_H_
#define RUNNER_SIDEKEY_CONTROLLER_H_

#include <windows.h>

#include <flutter/binary_messenger.h>
#include <flutter/encodable_value.h>
#include <flutter/method_channel.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

class SidekeyController {
 public:
  explicit SidekeyController(flutter::BinaryMessenger* messenger);
  ~SidekeyController();

 private:
  struct KeyBinding {
    int windows_vk = 0;
    std::vector<WORD> modifiers;

    bool IsValid() const { return windows_vk > 0; }
  };

  static LRESULT CALLBACK LowLevelMouseProc(int n_code, WPARAM w_param,
                                            LPARAM l_param);
  static SidekeyController* instance_;

  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  flutter::EncodableValue ApplyConfig(const flutter::EncodableValue* args);
  flutter::EncodableValue PermissionStatus() const;
  flutter::EncodableValue Status(const std::string& message) const;
  bool SetLaunchAtLogin(bool enabled);
  void Shutdown();

  void StartHook();
  void StopHook();
  LRESULT HandleMouseEvent(int n_code, WPARAM w_param, LPARAM l_param);

  void PressBinding(const KeyBinding& binding);
  void ReleaseBinding(const KeyBinding& binding);
  void TapBinding(const KeyBinding& binding);
  void SendVirtualKey(WORD vk, bool key_down);
  bool TriggerAction(const std::string& action);

  KeyBinding ParseBinding(const flutter::EncodableMap* map) const;

  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
  HHOOK mouse_hook_ = nullptr;

  mutable std::mutex mutex_;
  bool enabled_ = true;
  bool intercept_original_ = true;
  bool voice_hold_mode_ = false;
  bool voice_pressed_ = false;
  KeyBinding voice_binding_;
  KeyBinding back_binding_;
};

#endif  // RUNNER_SIDEKEY_CONTROLLER_H_
