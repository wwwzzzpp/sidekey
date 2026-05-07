#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cwchar>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "resource.h"

namespace {

constexpr UINT kTrayMessage = WM_APP + 10;
constexpr UINT_PTR kFinishRecordingTimer = 1;
constexpr int kTapHoldMs = 45;
constexpr int kSequenceGapMs = 60;
constexpr int kRecordQuietMs = 650;

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HHOOK g_mouse_hook = nullptr;
HHOOK g_keyboard_hook = nullptr;
NOTIFYICONDATAW g_tray = {};

HWND g_enable_box = nullptr;
HWND g_intercept_box = nullptr;
HWND g_launch_box = nullptr;
HWND g_binding_text = nullptr;
HWND g_status_text = nullptr;
HWND g_mode_tap = nullptr;
HWND g_mode_hold = nullptr;

struct Stroke {
  WORD primary = 0;
  std::vector<WORD> modifiers;

  bool IsValid() const { return primary != 0; }
};

struct Binding {
  std::vector<Stroke> strokes;

  bool IsValid() const { return !strokes.empty(); }
  bool IsHoldable() const { return strokes.size() == 1; }
};

struct Config {
  bool enabled = true;
  bool intercept_original = true;
  bool launch_at_login = false;
  bool hold_mode = false;
  Binding voice;
};

Config g_config;
bool g_voice_pressed = false;
bool g_recording = false;
std::vector<Stroke> g_recorded_strokes;
std::set<WORD> g_modifiers_used_in_chord;

std::wstring Quote(const std::wstring& text) {
  return L"\"" + text + L"\"";
}

std::wstring ExePath() {
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

std::wstring AppDataDir() {
  DWORD needed = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
  std::wstring root;
  if (needed > 0) {
    root.resize(needed - 1);
    GetEnvironmentVariableW(L"APPDATA", root.data(), needed);
  } else {
    root = L".";
  }
  std::wstring dir = root + L"\\SideKey";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

std::wstring ConfigPath() {
  return AppDataDir() + L"\\config.ini";
}

bool IsModifier(WORD vk) {
  switch (vk) {
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
      return true;
    default:
      return false;
  }
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
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_RWIN:
      return true;
    default:
      return false;
  }
}

WORD NormalizeVk(const KBDLLHOOKSTRUCT* info) {
  WORD vk = static_cast<WORD>(info->vkCode);
  const bool extended = (info->flags & LLKHF_EXTENDED) != 0;

  if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
    return extended ? VK_RCONTROL : VK_LCONTROL;
  }
  if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
    return extended ? VK_RMENU : VK_LMENU;
  }
  if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
    UINT mapped = MapVirtualKeyW(info->scanCode, MAPVK_VSC_TO_VK_EX);
    return mapped == VK_RSHIFT ? VK_RSHIFT : VK_LSHIFT;
  }
  return vk;
}

std::wstring VkLabel(WORD vk) {
  if (vk >= L'A' && vk <= L'Z') {
    return std::wstring(1, static_cast<wchar_t>(vk));
  }
  if (vk >= L'0' && vk <= L'9') {
    return std::wstring(1, static_cast<wchar_t>(vk));
  }
  if (vk >= VK_F1 && vk <= VK_F24) {
    return L"F" + std::to_wstring(vk - VK_F1 + 1);
  }

  switch (vk) {
    case VK_LCONTROL:
      return L"Left Ctrl";
    case VK_RCONTROL:
      return L"Right Ctrl";
    case VK_LSHIFT:
      return L"Left Shift";
    case VK_RSHIFT:
      return L"Right Shift";
    case VK_LMENU:
      return L"Left Alt";
    case VK_RMENU:
      return L"Right Alt";
    case VK_LWIN:
      return L"Left Win";
    case VK_RWIN:
      return L"Right Win";
    case VK_RETURN:
      return L"Enter";
    case VK_ESCAPE:
      return L"Esc";
    case VK_SPACE:
      return L"Space";
    case VK_TAB:
      return L"Tab";
    case VK_BACK:
      return L"Backspace";
    case VK_DELETE:
      return L"Delete";
    case VK_INSERT:
      return L"Insert";
    case VK_HOME:
      return L"Home";
    case VK_END:
      return L"End";
    case VK_PRIOR:
      return L"PageUp";
    case VK_NEXT:
      return L"PageDown";
    case VK_LEFT:
      return L"Left";
    case VK_RIGHT:
      return L"Right";
    case VK_UP:
      return L"Up";
    case VK_DOWN:
      return L"Down";
    case VK_OEM_MINUS:
      return L"-";
    case VK_OEM_PLUS:
      return L"=";
    case VK_OEM_4:
      return L"[";
    case VK_OEM_6:
      return L"]";
    case VK_OEM_5:
      return L"\\";
    case VK_OEM_1:
      return L";";
    case VK_OEM_7:
      return L"'";
    case VK_OEM_COMMA:
      return L",";
    case VK_OEM_PERIOD:
      return L".";
    case VK_OEM_2:
      return L"/";
    case VK_OEM_3:
      return L"`";
    default:
      return L"VK " + std::to_wstring(vk);
  }
}

std::wstring JoinLabels(const std::vector<std::wstring>& labels,
                        const wchar_t* separator) {
  std::wstring result;
  for (size_t i = 0; i < labels.size(); ++i) {
    if (i > 0) {
      result += separator;
    }
    result += labels[i];
  }
  return result;
}

std::wstring StrokeLabel(const Stroke& stroke) {
  std::vector<std::wstring> labels;
  for (WORD modifier : stroke.modifiers) {
    labels.push_back(VkLabel(modifier));
  }
  labels.push_back(VkLabel(stroke.primary));
  return JoinLabels(labels, L" + ");
}

std::wstring BindingLabel(const Binding& binding) {
  if (!binding.IsValid()) {
    return L"Not recorded";
  }

  std::vector<std::wstring> labels;
  for (const Stroke& stroke : binding.strokes) {
    labels.push_back(StrokeLabel(stroke));
  }

  std::vector<std::wstring> compact;
  for (size_t i = 0; i < labels.size();) {
    size_t count = 1;
    while (i + count < labels.size() && labels[i + count] == labels[i]) {
      ++count;
    }
    compact.push_back(count == 1 ? labels[i]
                                 : labels[i] + L" x" + std::to_wstring(count));
    i += count;
  }
  return JoinLabels(compact, L" -> ");
}

std::vector<WORD> ActiveModifiers() {
  const WORD order[] = {VK_LCONTROL, VK_RCONTROL, VK_LSHIFT, VK_RSHIFT,
                        VK_LMENU,    VK_RMENU,    VK_LWIN,   VK_RWIN};
  std::vector<WORD> result;
  for (WORD vk : order) {
    if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
      result.push_back(vk);
    }
  }
  return result;
}

void SendVirtualKey(WORD vk, bool down) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  if (IsExtendedKey(vk)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  SendInput(1, &input, sizeof(input));
}

void PressStroke(const Stroke& stroke) {
  if (!stroke.IsValid()) {
    return;
  }
  for (WORD modifier : stroke.modifiers) {
    SendVirtualKey(modifier, true);
  }
  if (!stroke.modifiers.empty()) {
    Sleep(10);
  }
  SendVirtualKey(stroke.primary, true);
}

void ReleaseStroke(const Stroke& stroke) {
  if (!stroke.IsValid()) {
    return;
  }
  SendVirtualKey(stroke.primary, false);
  for (auto it = stroke.modifiers.rbegin(); it != stroke.modifiers.rend();
       ++it) {
    SendVirtualKey(*it, false);
  }
}

void TapStroke(const Stroke& stroke) {
  PressStroke(stroke);
  Sleep(kTapHoldMs);
  ReleaseStroke(stroke);
}

void TapBinding(const Binding& binding) {
  for (size_t i = 0; i < binding.strokes.size(); ++i) {
    TapStroke(binding.strokes[i]);
    if (i + 1 < binding.strokes.size()) {
      Sleep(kSequenceGapMs);
    }
  }
}

void PressBinding(const Binding& binding) {
  if (binding.IsValid()) {
    PressStroke(binding.strokes.front());
  }
}

void ReleaseBinding(const Binding& binding) {
  if (binding.IsValid()) {
    ReleaseStroke(binding.strokes.front());
  }
}

std::wstring EncodeStroke(const Stroke& stroke) {
  std::wstringstream stream;
  stream << stroke.primary << L":";
  for (size_t i = 0; i < stroke.modifiers.size(); ++i) {
    if (i > 0) {
      stream << L",";
    }
    stream << stroke.modifiers[i];
  }
  return stream.str();
}

std::wstring EncodeBinding(const Binding& binding) {
  std::wstring result;
  for (size_t i = 0; i < binding.strokes.size(); ++i) {
    if (i > 0) {
      result += L";";
    }
    result += EncodeStroke(binding.strokes[i]);
  }
  return result;
}

Stroke DecodeStroke(const std::wstring& text) {
  Stroke stroke;
  size_t colon = text.find(L':');
  std::wstring primary = colon == std::wstring::npos ? text : text.substr(0, colon);
  try {
    stroke.primary = static_cast<WORD>(std::stoul(primary));
  } catch (...) {
    stroke.primary = 0;
  }

  if (colon == std::wstring::npos) {
    return stroke;
  }

  std::wstring rest = text.substr(colon + 1);
  size_t start = 0;
  while (start < rest.size()) {
    size_t comma = rest.find(L',', start);
    std::wstring part = rest.substr(start, comma - start);
    if (!part.empty()) {
      try {
        stroke.modifiers.push_back(static_cast<WORD>(std::stoul(part)));
      } catch (...) {
      }
    }
    if (comma == std::wstring::npos) {
      break;
    }
    start = comma + 1;
  }
  return stroke;
}

Binding DecodeBinding(const std::wstring& text) {
  Binding binding;
  size_t start = 0;
  while (start < text.size()) {
    size_t semi = text.find(L';', start);
    Stroke stroke = DecodeStroke(text.substr(start, semi - start));
    if (stroke.IsValid()) {
      binding.strokes.push_back(stroke);
    }
    if (semi == std::wstring::npos) {
      break;
    }
    start = semi + 1;
  }
  return binding;
}

void SaveConfig() {
  std::wstring path = ConfigPath();
  WritePrivateProfileStringW(L"sidekey", L"enabled",
                             g_config.enabled ? L"1" : L"0", path.c_str());
  WritePrivateProfileStringW(L"sidekey", L"intercept",
                             g_config.intercept_original ? L"1" : L"0",
                             path.c_str());
  WritePrivateProfileStringW(L"sidekey", L"launch",
                             g_config.launch_at_login ? L"1" : L"0",
                             path.c_str());
  WritePrivateProfileStringW(L"sidekey", L"hold",
                             g_config.hold_mode ? L"1" : L"0", path.c_str());
  WritePrivateProfileStringW(L"sidekey", L"voice",
                             EncodeBinding(g_config.voice).c_str(),
                             path.c_str());
}

std::wstring ReadIniString(const wchar_t* key, const wchar_t* fallback) {
  wchar_t buffer[2048] = {};
  GetPrivateProfileStringW(L"sidekey", key, fallback, buffer,
                           static_cast<DWORD>(std::size(buffer)), ConfigPath().c_str());
  return buffer;
}

bool ReadIniBool(const wchar_t* key, bool fallback) {
  return ReadIniString(key, fallback ? L"1" : L"0") == L"1";
}

void LoadConfig() {
  g_config.enabled = ReadIniBool(L"enabled", true);
  g_config.intercept_original = ReadIniBool(L"intercept", true);
  g_config.launch_at_login = ReadIniBool(L"launch", false);
  g_config.hold_mode = ReadIniBool(L"hold", false);
  g_config.voice = DecodeBinding(ReadIniString(L"voice", L""));
}

void SetLaunchAtLogin(bool enabled) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    return;
  }

  if (enabled) {
    std::wstring value = Quote(ExePath());
    RegSetValueExW(key, L"SideKey", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, L"SideKey");
  }
  RegCloseKey(key);
}

void UpdateUi() {
  if (!g_window) {
    return;
  }
  SendMessageW(g_enable_box, BM_SETCHECK,
               g_config.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g_intercept_box, BM_SETCHECK,
               g_config.intercept_original ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g_launch_box, BM_SETCHECK,
               g_config.launch_at_login ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g_mode_tap, BM_SETCHECK,
               g_config.hold_mode ? BST_UNCHECKED : BST_CHECKED, 0);
  SendMessageW(g_mode_hold, BM_SETCHECK,
               g_config.hold_mode ? BST_CHECKED : BST_UNCHECKED, 0);

  SetWindowTextW(g_binding_text, BindingLabel(g_config.voice).c_str());
  std::wstring status = g_config.enabled
                            ? L"Running. Forward maps to recorded hotkey; back maps to Enter."
                            : L"Paused.";
  SetWindowTextW(g_status_text, status.c_str());

  if (g_tray.cbSize != 0) {
    wcscpy_s(g_tray.szTip, g_config.enabled ? L"SideKey running"
                                            : L"SideKey paused");
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
  }
}

LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code < 0) {
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }
  if (wparam != WM_XBUTTONDOWN && wparam != WM_XBUTTONUP) {
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
  }

  const MSLLHOOKSTRUCT* info =
      reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
  int button = HIWORD(info->mouseData);
  bool down = wparam == WM_XBUTTONDOWN;
  bool handled = false;

  if (g_config.enabled && button == XBUTTON1) {
    if (down) {
      Stroke enter;
      enter.primary = VK_RETURN;
      TapStroke(enter);
    }
    handled = true;
  } else if (g_config.enabled && button == XBUTTON2 &&
             g_config.voice.IsValid()) {
    if (g_config.hold_mode && g_config.voice.IsHoldable()) {
      if (down && !g_voice_pressed) {
        PressBinding(g_config.voice);
        g_voice_pressed = true;
      } else if (!down && g_voice_pressed) {
        ReleaseBinding(g_config.voice);
        g_voice_pressed = false;
      }
    } else if (down) {
      TapBinding(g_config.voice);
    }
    handled = true;
  }

  if (handled && g_config.intercept_original) {
    return 1;
  }
  return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
}

void StartMouseHook() {
  if (!g_mouse_hook) {
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, g_instance, 0);
  }
}

void StopMouseHook() {
  if (g_mouse_hook) {
    UnhookWindowsHookEx(g_mouse_hook);
    g_mouse_hook = nullptr;
  }
}

void StopKeyboardHook();

void FinishRecording() {
  KillTimer(g_window, kFinishRecordingTimer);
  if (g_recorded_strokes.empty()) {
    return;
  }
  g_config.voice.strokes = g_recorded_strokes;
  g_recorded_strokes.clear();
  g_modifiers_used_in_chord.clear();
  g_recording = false;
  StopKeyboardHook();
  SaveConfig();
  UpdateUi();
}

void RecordStroke(const Stroke& stroke) {
  if (!stroke.IsValid()) {
    return;
  }
  g_recorded_strokes.push_back(stroke);
  SetWindowTextW(g_binding_text, BindingLabel(Binding{g_recorded_strokes}).c_str());
  SetTimer(g_window, kFinishRecordingTimer, kRecordQuietMs, nullptr);
}

void RecordStandalone(WORD vk) {
  Stroke stroke;
  stroke.primary = vk;
  RecordStroke(stroke);
}

void RecordModifierChordOrStandalone(WORD released_vk) {
  std::vector<WORD> active = ActiveModifiers();
  active.erase(std::remove(active.begin(), active.end(), released_vk), active.end());
  if (active.empty()) {
    RecordStandalone(released_vk);
    return;
  }

  for (WORD modifier : active) {
    g_modifiers_used_in_chord.insert(modifier);
  }

  Stroke stroke;
  stroke.primary = released_vk;
  stroke.modifiers = active;
  RecordStroke(stroke);
}

void RecordChord(WORD primary) {
  Stroke stroke;
  stroke.primary = primary;
  stroke.modifiers = ActiveModifiers();
  for (WORD modifier : stroke.modifiers) {
    g_modifiers_used_in_chord.insert(modifier);
  }
  RecordStroke(stroke);
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code < 0 || !g_recording) {
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
  }
  const KBDLLHOOKSTRUCT* info =
      reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
  WORD vk = NormalizeVk(info);
  bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
  bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;

  if (down) {
    if (!IsModifier(vk)) {
      RecordChord(vk);
    }
    return 1;
  }
  if (up) {
    if (IsModifier(vk)) {
      auto used = g_modifiers_used_in_chord.find(vk);
      if (used != g_modifiers_used_in_chord.end()) {
        g_modifiers_used_in_chord.erase(used);
      } else {
        RecordModifierChordOrStandalone(vk);
      }
    }
    return 1;
  }
  return 1;
}

void StartKeyboardHook() {
  if (g_keyboard_hook) {
    return;
  }
  g_keyboard_hook =
      SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, g_instance, 0);
}

void StopKeyboardHook() {
  if (g_keyboard_hook) {
    UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
  }
}

void StartRecording() {
  g_recording = true;
  g_recorded_strokes.clear();
  g_modifiers_used_in_chord.clear();
  SetWindowTextW(g_binding_text, L"Recording... release keys, then wait");
  StartKeyboardHook();
}

void AddTrayIcon() {
  g_tray = {};
  g_tray.cbSize = sizeof(g_tray);
  g_tray.hWnd = g_window;
  g_tray.uID = 1;
  g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  g_tray.uCallbackMessage = kTrayMessage;
  g_tray.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP));
  wcscpy_s(g_tray.szTip, L"SideKey");
  Shell_NotifyIconW(NIM_ADD, &g_tray);
}

void RemoveTrayIcon() {
  if (g_tray.cbSize != 0) {
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
  }
}

void ShowTrayMenu() {
  POINT point;
  GetCursorPos(&point);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Open settings");
  AppendMenuW(menu, MF_STRING, ID_TRAY_TOGGLE,
              g_config.enabled ? L"Pause mapping" : L"Enable mapping");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Quit");
  SetForegroundWindow(g_window);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, g_window, nullptr);
  DestroyMenu(menu);
}

HWND AddControl(const wchar_t* cls, const wchar_t* text, DWORD style, int x,
                int y, int w, int h, int id) {
  HWND control = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w,
                               h, g_window, reinterpret_cast<HMENU>(id),
                               g_instance, nullptr);
  SendMessageW(control, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
  return control;
}

void CreateControls() {
  AddControl(L"STATIC", L"SideKey for Windows 7", 0, 20, 18, 360, 24, 0);
  AddControl(L"STATIC", L"Mouse forward button sends the recorded hotkey.", 0,
             20, 44, 430, 20, 0);

  g_enable_box = AddControl(L"BUTTON", L"Enable mapping", BS_AUTOCHECKBOX, 20,
                            80, 180, 24, IDC_ENABLE);
  g_intercept_box =
      AddControl(L"BUTTON", L"Block original browser forward/back", BS_AUTOCHECKBOX,
                 20, 108, 280, 24, IDC_INTERCEPT);
  g_launch_box = AddControl(L"BUTTON", L"Launch at login", BS_AUTOCHECKBOX, 20,
                            136, 180, 24, IDC_LAUNCH);

  AddControl(L"STATIC", L"Forward button hotkey:", 0, 20, 178, 180, 20, 0);
  g_binding_text = AddControl(L"STATIC", L"Not recorded", SS_SUNKEN, 20, 204,
                              420, 28, IDC_BINDING);
  AddControl(L"BUTTON", L"Record", 0, 20, 244, 86, 28, IDC_RECORD);
  AddControl(L"BUTTON", L"Clear", 0, 116, 244, 86, 28, IDC_CLEAR);
  AddControl(L"BUTTON", L"Test", 0, 212, 244, 86, 28, IDC_TEST);

  AddControl(L"STATIC", L"Trigger mode:", 0, 20, 292, 120, 20, 0);
  g_mode_tap =
      AddControl(L"BUTTON", L"Tap once", BS_AUTORADIOBUTTON, 20, 318, 120, 24,
                 IDC_MODE_TAP);
  g_mode_hold =
      AddControl(L"BUTTON", L"Hold while side button is held",
                 BS_AUTORADIOBUTTON, 150, 318, 260, 24, IDC_MODE_HOLD);

  g_status_text = AddControl(L"STATIC", L"", SS_SUNKEN, 20, 368, 520, 42,
                             IDC_STATUS);
}

void ApplyEnabledState() {
  if (g_config.enabled) {
    StartMouseHook();
  }
  UpdateUi();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      g_window = hwnd;
      CreateControls();
      AddTrayIcon();
      ApplyEnabledState();
      return 0;

    case WM_COMMAND: {
      int id = LOWORD(wparam);
      if (id == IDC_ENABLE) {
        g_config.enabled =
            SendMessageW(g_enable_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (g_config.enabled) {
          StartMouseHook();
        } else {
          if (g_voice_pressed) {
            ReleaseBinding(g_config.voice);
            g_voice_pressed = false;
          }
        }
        SaveConfig();
        UpdateUi();
      } else if (id == IDC_INTERCEPT) {
        g_config.intercept_original =
            SendMessageW(g_intercept_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SaveConfig();
      } else if (id == IDC_LAUNCH) {
        g_config.launch_at_login =
            SendMessageW(g_launch_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SetLaunchAtLogin(g_config.launch_at_login);
        SaveConfig();
      } else if (id == IDC_MODE_TAP || id == IDC_MODE_HOLD) {
        g_config.hold_mode = id == IDC_MODE_HOLD;
        SaveConfig();
        UpdateUi();
      } else if (id == IDC_RECORD) {
        StartRecording();
      } else if (id == IDC_CLEAR) {
        g_config.voice.strokes.clear();
        SaveConfig();
        UpdateUi();
      } else if (id == IDC_TEST) {
        TapBinding(g_config.voice);
      } else if (id == ID_TRAY_SHOW) {
        ShowWindow(g_window, SW_SHOW);
        SetForegroundWindow(g_window);
      } else if (id == ID_TRAY_TOGGLE) {
        g_config.enabled = !g_config.enabled;
        SaveConfig();
        ApplyEnabledState();
      } else if (id == ID_TRAY_QUIT) {
        DestroyWindow(g_window);
      }
      return 0;
    }

    case WM_TIMER:
      if (wparam == kFinishRecordingTimer) {
        FinishRecording();
      }
      return 0;

    case kTrayMessage:
      if (lparam == WM_LBUTTONDBLCLK) {
        ShowWindow(g_window, SW_SHOW);
        SetForegroundWindow(g_window);
      } else if (lparam == WM_RBUTTONUP) {
        ShowTrayMenu();
      }
      return 0;

    case WM_CLOSE:
      ShowWindow(g_window, SW_HIDE);
      return 0;

    case WM_DESTROY:
      StopKeyboardHook();
      if (g_voice_pressed) {
        ReleaseBinding(g_config.voice);
        g_voice_pressed = false;
      }
      StopMouseHook();
      RemoveTrayIcon();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
  g_instance = instance;

  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"SideKeyWin7SingleInstance");
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(nullptr, L"SideKey is already running.", L"SideKey", MB_OK);
    return 0;
  }

  LoadConfig();

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"SideKeyWin7Window";
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"SideKey",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                  WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 580, 470, nullptr,
                              nullptr, instance, nullptr);
  if (!hwnd) {
    return 1;
  }

  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (mutex) {
    CloseHandle(mutex);
  }
  return static_cast<int>(msg.wParam);
}
