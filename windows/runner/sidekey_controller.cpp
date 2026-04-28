#include "sidekey_controller.h"

#include <flutter/standard_method_codec.h>

#include <algorithm>
#include <sstream>
#include <variant>

namespace {

constexpr int kXButtonBack = XBUTTON1;
constexpr int kXButtonForward = XBUTTON2;

flutter::EncodableValue Value(const char* key) {
  return flutter::EncodableValue(std::string(key));
}

const flutter::EncodableValue* FindValue(const flutter::EncodableMap& map,
                                         const char* key) {
  auto it = map.find(Value(key));
  if (it == map.end()) {
    return nullptr;
  }
  return &it->second;
}

bool GetBool(const flutter::EncodableMap& map, const char* key,
             bool fallback) {
  const auto* value = FindValue(map, key);
  if (!value) {
    return fallback;
  }
  if (const auto* typed = std::get_if<bool>(value)) {
    return *typed;
  }
  return fallback;
}

int GetInt(const flutter::EncodableMap& map, const char* key, int fallback) {
  const auto* value = FindValue(map, key);
  if (!value) {
    return fallback;
  }
  if (const auto* typed = std::get_if<int32_t>(value)) {
    return *typed;
  }
  if (const auto* typed = std::get_if<int64_t>(value)) {
    return static_cast<int>(*typed);
  }
  if (const auto* typed = std::get_if<double>(value)) {
    return static_cast<int>(*typed);
  }
  return fallback;
}

std::string GetString(const flutter::EncodableMap& map, const char* key,
                      const std::string& fallback = "") {
  const auto* value = FindValue(map, key);
  if (!value) {
    return fallback;
  }
  if (const auto* typed = std::get_if<std::string>(value)) {
    return *typed;
  }
  return fallback;
}

const flutter::EncodableMap* GetMap(const flutter::EncodableMap& map,
                                    const char* key) {
  const auto* value = FindValue(map, key);
  if (!value) {
    return nullptr;
  }
  return std::get_if<flutter::EncodableMap>(value);
}

std::vector<std::string> GetStringList(const flutter::EncodableMap& map,
                                       const char* key) {
  std::vector<std::string> result;
  const auto* value = FindValue(map, key);
  if (!value) {
    return result;
  }
  const auto* list = std::get_if<flutter::EncodableList>(value);
  if (!list) {
    return result;
  }
  for (const auto& item : *list) {
    if (const auto* text = std::get_if<std::string>(&item)) {
      result.push_back(*text);
    }
  }
  return result;
}

WORD ModifierToVk(const std::string& modifier) {
  if (modifier == "ctrl") {
    return VK_CONTROL;
  }
  if (modifier == "shift") {
    return VK_SHIFT;
  }
  if (modifier == "alt") {
    return VK_MENU;
  }
  if (modifier == "meta") {
    return VK_LWIN;
  }
  return 0;
}

std::wstring CurrentExecutablePath() {
  std::vector<wchar_t> buffer(MAX_PATH);
  DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                    static_cast<DWORD>(buffer.size()));
  while (length == buffer.size()) {
    buffer.resize(buffer.size() * 2);
    length = GetModuleFileNameW(nullptr, buffer.data(),
                                static_cast<DWORD>(buffer.size()));
  }
  if (length == 0) {
    return L"";
  }
  return std::wstring(buffer.data(), length);
}

bool IsExtendedKey(WORD vk) {
  switch (vk) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
      return true;
    default:
      return false;
  }
}

}  // namespace

SidekeyController* SidekeyController::instance_ = nullptr;

SidekeyController::SidekeyController(flutter::BinaryMessenger* messenger) {
  back_binding_.windows_vk = VK_RETURN;
  channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      messenger, "sidekey/native", &flutter::StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler(
      [this](const auto& call, auto result) {
        HandleMethodCall(call, std::move(result));
      });
  instance_ = this;
}

SidekeyController::~SidekeyController() {
  Shutdown();
  if (instance_ == this) {
    instance_ = nullptr;
  }
}

void SidekeyController::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const std::string& method = method_call.method_name();

  if (method == "applyConfig") {
    result->Success(ApplyConfig(method_call.arguments()));
    return;
  }
  if (method == "getPermissionStatus") {
    result->Success(PermissionStatus());
    return;
  }
  if (method == "openPermissionSettings") {
    result->Success();
    return;
  }
  if (method == "setLaunchAtLogin") {
    const auto* args =
        method_call.arguments()
            ? std::get_if<flutter::EncodableMap>(method_call.arguments())
            : nullptr;
    const bool enabled = args ? GetBool(*args, "enabled", false) : false;
    result->Success(flutter::EncodableValue(SetLaunchAtLogin(enabled)));
    return;
  }
  if (method == "testAction") {
    const auto* args =
        method_call.arguments()
            ? std::get_if<flutter::EncodableMap>(method_call.arguments())
            : nullptr;
    const std::string action = args ? GetString(*args, "action") : "";
    result->Success(flutter::EncodableValue(TriggerAction(action)));
    return;
  }
  if (method == "shutdown") {
    Shutdown();
    result->Success();
    return;
  }

  result->NotImplemented();
}

flutter::EncodableValue SidekeyController::ApplyConfig(
    const flutter::EncodableValue* args) {
  const auto* map = args ? std::get_if<flutter::EncodableMap>(args) : nullptr;
  if (!map) {
    return Status("Invalid config.");
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (voice_pressed_) {
      ReleaseBinding(voice_binding_);
      voice_pressed_ = false;
    }
    enabled_ = GetBool(*map, "enabled", true);
    intercept_original_ = GetBool(*map, "interceptOriginal", true);
    voice_hold_mode_ = GetString(*map, "voiceMode", "tap") == "hold";
    voice_binding_ = ParseBinding(GetMap(*map, "voiceBinding"));
    back_binding_ = ParseBinding(GetMap(*map, "backBinding"));
    if (!back_binding_.IsValid()) {
      back_binding_.windows_vk = VK_RETURN;
    }
  }

  if (enabled_) {
    StartHook();
  } else {
    StopHook();
  }

  if (!enabled_) {
    return Status("Mapping paused.");
  }
  if (!mouse_hook_) {
    std::ostringstream stream;
    stream << "Windows hook failed. LastError=" << GetLastError();
    return Status(stream.str());
  }
  if (!voice_binding_.IsValid()) {
    return Status("Back button maps to Enter. Record a voice hotkey for the forward button.");
  }
  return Status("Windows hook running. Forward maps to voice hotkey; back maps to Enter.");
}

flutter::EncodableValue SidekeyController::PermissionStatus() const {
  return Status("Windows does not require extra input-monitoring permission. Elevated apps may ignore injected keys.");
}

flutter::EncodableValue SidekeyController::Status(
    const std::string& message) const {
  flutter::EncodableMap data;
  data[Value("running")] =
      flutter::EncodableValue(enabled_ && mouse_hook_ != nullptr);
  data[Value("permission")] = flutter::EncodableValue("granted");
  data[Value("message")] = flutter::EncodableValue(message);
  return flutter::EncodableValue(data);
}

bool SidekeyController::SetLaunchAtLogin(bool enabled) {
  HKEY run_key = nullptr;
  const wchar_t* run_path =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  LONG open_result = RegOpenKeyExW(HKEY_CURRENT_USER, run_path, 0, KEY_SET_VALUE,
                                   &run_key);
  if (open_result != ERROR_SUCCESS) {
    return false;
  }

  constexpr const wchar_t* value_name = L"SideKey";
  LONG result = ERROR_SUCCESS;
  if (enabled) {
    const std::wstring path = CurrentExecutablePath();
    if (path.empty()) {
      RegCloseKey(run_key);
      return false;
    }
    result = RegSetValueExW(
        run_key, value_name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(path.c_str()),
        static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
  } else {
    result = RegDeleteValueW(run_key, value_name);
    if (result == ERROR_FILE_NOT_FOUND) {
      result = ERROR_SUCCESS;
    }
  }

  RegCloseKey(run_key);
  return result == ERROR_SUCCESS;
}

void SidekeyController::Shutdown() {
  StopHook();
}

void SidekeyController::StartHook() {
  if (mouse_hook_) {
    return;
  }
  mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc,
                                  GetModuleHandleW(nullptr), 0);
}

void SidekeyController::StopHook() {
  if (mouse_hook_) {
    UnhookWindowsHookEx(mouse_hook_);
    mouse_hook_ = nullptr;
  }
}

LRESULT CALLBACK SidekeyController::LowLevelMouseProc(int n_code,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
  if (instance_) {
    return instance_->HandleMouseEvent(n_code, w_param, l_param);
  }
  return CallNextHookEx(nullptr, n_code, w_param, l_param);
}

LRESULT SidekeyController::HandleMouseEvent(int n_code, WPARAM w_param,
                                            LPARAM l_param) {
  if (n_code < 0) {
    return CallNextHookEx(mouse_hook_, n_code, w_param, l_param);
  }
  if (w_param != WM_XBUTTONDOWN && w_param != WM_XBUTTONUP) {
    return CallNextHookEx(mouse_hook_, n_code, w_param, l_param);
  }

  const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(l_param);
  const int button = HIWORD(info->mouseData);

  std::lock_guard<std::mutex> lock(mutex_);
  if (!enabled_) {
    return CallNextHookEx(mouse_hook_, n_code, w_param, l_param);
  }

  bool handled = false;
  if (button == kXButtonBack) {
    if (w_param == WM_XBUTTONDOWN) {
      TapBinding(back_binding_);
    }
    handled = true;
  } else if (button == kXButtonForward && voice_binding_.IsValid()) {
    if (voice_hold_mode_) {
      if (w_param == WM_XBUTTONDOWN && !voice_pressed_) {
        PressBinding(voice_binding_);
        voice_pressed_ = true;
      } else if (w_param == WM_XBUTTONUP && voice_pressed_) {
        ReleaseBinding(voice_binding_);
        voice_pressed_ = false;
      }
    } else if (w_param == WM_XBUTTONDOWN) {
      TapBinding(voice_binding_);
    }
    handled = true;
  }

  if (handled && intercept_original_) {
    return 1;
  }
  return CallNextHookEx(mouse_hook_, n_code, w_param, l_param);
}

void SidekeyController::PressBinding(const KeyBinding& binding) {
  if (!binding.IsValid()) {
    return;
  }
  for (WORD modifier : binding.modifiers) {
    SendVirtualKey(modifier, true);
  }
  SendVirtualKey(static_cast<WORD>(binding.windows_vk), true);
}

void SidekeyController::ReleaseBinding(const KeyBinding& binding) {
  if (!binding.IsValid()) {
    return;
  }
  SendVirtualKey(static_cast<WORD>(binding.windows_vk), false);
  for (auto it = binding.modifiers.rbegin(); it != binding.modifiers.rend();
       ++it) {
    SendVirtualKey(*it, false);
  }
}

void SidekeyController::TapBinding(const KeyBinding& binding) {
  PressBinding(binding);
  ReleaseBinding(binding);
}

void SidekeyController::SendVirtualKey(WORD vk, bool key_down) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.dwFlags = key_down ? 0 : KEYEVENTF_KEYUP;
  if (IsExtendedKey(vk)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  SendInput(1, &input, sizeof(INPUT));
}

bool SidekeyController::TriggerAction(const std::string& action) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (action == "voice") {
    if (!voice_binding_.IsValid()) {
      return false;
    }
    TapBinding(voice_binding_);
    return true;
  }
  if (action == "back") {
    TapBinding(back_binding_);
    return true;
  }
  return false;
}

SidekeyController::KeyBinding SidekeyController::ParseBinding(
    const flutter::EncodableMap* map) const {
  KeyBinding binding;
  if (!map) {
    return binding;
  }
  binding.windows_vk = GetInt(*map, "windowsVk", 0);
  const auto modifiers = GetStringList(*map, "modifiers");
  for (const auto& modifier : modifiers) {
    const WORD vk = ModifierToVk(modifier);
    if (vk != 0) {
      binding.modifiers.push_back(vk);
    }
  }
  return binding;
}
