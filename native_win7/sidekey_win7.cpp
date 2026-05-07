#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#define IDI_APP_ICON 101

namespace {

constexpr wchar_t kAppName[] = L"SideKey";
constexpr wchar_t kRunValueName[] = L"SideKey";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT_PTR kTrayId = 1001;
constexpr int kXButtonBack = XBUTTON1;
constexpr int kXButtonForward = XBUTTON2;

enum ControlId {
  kEnabledCheck = 2001,
  kInterceptCheck = 2002,
  kHoldModeCheck = 2003,
  kLaunchAtLoginCheck = 2004,
  kRecordButton = 2005,
  kTestVoiceButton = 2006,
  kTestEnterButton = 2007,
  kSaveButton = 2008,
  kHideButton = 2009,
  kExitButton = 2010,
  kStatusText = 2011,
  kVoiceText = 2012,
};

enum MenuId {
  kMenuShow = 3001,
  kMenuToggle = 3002,
  kMenuExit = 3003,
};

struct KeyStroke {
  WORD vk = 0;
  std::vector<WORD> modifiers;

  bool IsValid() const { return vk != 0; }
};

struct AppConfig {
  bool enabled = true;
  bool intercept_original = true;
  bool voice_hold_mode = false;
  bool launch_at_login = false;
  KeyStroke voice_binding;
};

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HHOOK g_mouse_hook = nullptr;
AppConfig g_config;
bool g_voice_pressed = false;
bool g_recording = false;

std::wstring GetExePath() {
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

std::wstring GetConfigDir() {
  wchar_t path[MAX_PATH] = {};
  if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
                       SHGFP_TYPE_CURRENT, path) != S_OK) {
    return L".";
  }
  std::wstring dir = std::wstring(path) + L"\\SideKey";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

std::wstring GetConfigPath() { return GetConfigDir() + L"\\sidekey.ini"; }

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

void SendVirtualKey(WORD vk, bool key_down) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.dwFlags = key_down ? 0 : KEYEVENTF_KEYUP;
  if (IsExtendedKey(vk)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  SendInput(1, &input, sizeof(INPUT));
}

void PressStroke(const KeyStroke& stroke) {
  if (!stroke.IsValid()) {
    return;
  }
  for (WORD modifier : stroke.modifiers) {
    SendVirtualKey(modifier, true);
  }
  SendVirtualKey(stroke.vk, true);
}

void ReleaseStroke(const KeyStroke& stroke) {
  if (!stroke.IsValid()) {
    return;
  }
  SendVirtualKey(stroke.vk, false);
  for (auto it = stroke.modifiers.rbegin(); it != stroke.modifiers.rend();
       ++it) {
    SendVirtualKey(*it, false);
  }
}

void TapStroke(const KeyStroke& stroke) {
  PressStroke(stroke);
  Sleep(45);
  ReleaseStroke(stroke);
}

std::wstring VkName(WORD vk) {
  switch (vk) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
      return L"Ctrl";
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
      return L"Shift";
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
      return L"Alt";
    case VK_LWIN:
    case VK_RWIN:
      return L"Win";
    case VK_RETURN:
      return L"Enter";
    case VK_SPACE:
      return L"Space";
    case VK_TAB:
      return L"Tab";
    case VK_ESCAPE:
      return L"Esc";
    case VK_BACK:
      return L"Backspace";
    case VK_DELETE:
      return L"Delete";
    case VK_LEFT:
      return L"Left";
    case VK_RIGHT:
      return L"Right";
    case VK_UP:
      return L"Up";
    case VK_DOWN:
      return L"Down";
    default:
      break;
  }

  UINT scan_code = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
  wchar_t name[128] = {};
  if (GetKeyNameTextW(static_cast<LONG>(scan_code), name, 128) > 0) {
    return name;
  }

  std::wstringstream stream;
  stream << L"VK " << vk;
  return stream.str();
}

std::wstring BindingText(const KeyStroke& stroke) {
  if (!stroke.IsValid()) {
    return L"未录制";
  }
  std::wstring text;
  for (WORD modifier : stroke.modifiers) {
    if (!text.empty()) {
      text += L" + ";
    }
    text += VkName(modifier);
  }
  if (!text.empty()) {
    text += L" + ";
  }
  text += VkName(stroke.vk);
  return text;
}

std::wstring JoinModifiers(const std::vector<WORD>& modifiers) {
  std::wstringstream stream;
  for (size_t i = 0; i < modifiers.size(); ++i) {
    if (i != 0) {
      stream << L",";
    }
    stream << modifiers[i];
  }
  return stream.str();
}

std::vector<WORD> ParseModifiers(const std::wstring& text) {
  std::vector<WORD> modifiers;
  std::wstringstream stream(text);
  std::wstring token;
  while (std::getline(stream, token, L',')) {
    const int value = _wtoi(token.c_str());
    if (value > 0) {
      modifiers.push_back(static_cast<WORD>(value));
    }
  }
  return modifiers;
}

bool IsLaunchAtLoginEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  wchar_t value[MAX_PATH * 2] = {};
  DWORD size = sizeof(value);
  LONG result = RegQueryValueExW(key, kRunValueName, nullptr, nullptr,
                                 reinterpret_cast<BYTE*>(value), &size);
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

bool SetLaunchAtLogin(bool enabled) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }
  LONG result = ERROR_SUCCESS;
  if (enabled) {
    std::wstring path = L"\"" + GetExePath() + L"\"";
    result = RegSetValueExW(
        key, kRunValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(path.c_str()),
        static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
  } else {
    result = RegDeleteValueW(key, kRunValueName);
    if (result == ERROR_FILE_NOT_FOUND) {
      result = ERROR_SUCCESS;
    }
  }
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

void LoadConfig() {
  const std::wstring path = GetConfigPath();
  g_config.enabled =
      GetPrivateProfileIntW(L"SideKey", L"Enabled", 1, path.c_str()) != 0;
  g_config.intercept_original =
      GetPrivateProfileIntW(L"SideKey", L"InterceptOriginal", 1,
                            path.c_str()) != 0;
  g_config.voice_hold_mode =
      GetPrivateProfileIntW(L"SideKey", L"VoiceHoldMode", 0, path.c_str()) != 0;
  g_config.voice_binding.vk =
      static_cast<WORD>(GetPrivateProfileIntW(L"SideKey", L"VoiceVk", 0,
                                              path.c_str()));

  wchar_t modifiers[256] = {};
  GetPrivateProfileStringW(L"SideKey", L"VoiceModifiers", L"", modifiers, 256,
                           path.c_str());
  g_config.voice_binding.modifiers = ParseModifiers(modifiers);
  g_config.launch_at_login = IsLaunchAtLoginEnabled();
}

void SaveConfig() {
  const std::wstring path = GetConfigPath();
  WritePrivateProfileStringW(L"SideKey", L"Enabled",
                             g_config.enabled ? L"1" : L"0", path.c_str());
  WritePrivateProfileStringW(L"SideKey", L"InterceptOriginal",
                             g_config.intercept_original ? L"1" : L"0",
                             path.c_str());
  WritePrivateProfileStringW(L"SideKey", L"VoiceHoldMode",
                             g_config.voice_hold_mode ? L"1" : L"0",
                             path.c_str());

  wchar_t vk_text[32] = {};
  wsprintfW(vk_text, L"%u", g_config.voice_binding.vk);
  WritePrivateProfileStringW(L"SideKey", L"VoiceVk", vk_text, path.c_str());
  const std::wstring modifiers = JoinModifiers(g_config.voice_binding.modifiers);
  WritePrivateProfileStringW(L"SideKey", L"VoiceModifiers", modifiers.c_str(),
                             path.c_str());
  SetLaunchAtLogin(g_config.launch_at_login);
}

void StopHook() {
  if (g_mouse_hook) {
    UnhookWindowsHookEx(g_mouse_hook);
    g_mouse_hook = nullptr;
  }
  if (g_voice_pressed) {
    ReleaseStroke(g_config.voice_binding);
    g_voice_pressed = false;
  }
}

LRESULT CALLBACK MouseProc(int n_code, WPARAM w_param, LPARAM l_param) {
  if (n_code < 0 || !g_config.enabled) {
    return CallNextHookEx(g_mouse_hook, n_code, w_param, l_param);
  }
  if (w_param != WM_XBUTTONDOWN && w_param != WM_XBUTTONUP) {
    return CallNextHookEx(g_mouse_hook, n_code, w_param, l_param);
  }

  const MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(l_param);
  const int button = HIWORD(info->mouseData);
  bool handled = false;

  if (button == kXButtonBack) {
    if (w_param == WM_XBUTTONDOWN) {
      KeyStroke enter;
      enter.vk = VK_RETURN;
      TapStroke(enter);
    }
    handled = true;
  } else if (button == kXButtonForward &&
             g_config.voice_binding.IsValid()) {
    if (g_config.voice_hold_mode) {
      if (w_param == WM_XBUTTONDOWN && !g_voice_pressed) {
        PressStroke(g_config.voice_binding);
        g_voice_pressed = true;
      } else if (w_param == WM_XBUTTONUP && g_voice_pressed) {
        ReleaseStroke(g_config.voice_binding);
        g_voice_pressed = false;
      }
    } else if (w_param == WM_XBUTTONDOWN) {
      TapStroke(g_config.voice_binding);
    }
    handled = true;
  }

  if (handled && g_config.intercept_original) {
    return 1;
  }
  return CallNextHookEx(g_mouse_hook, n_code, w_param, l_param);
}

bool StartHook() {
  if (g_mouse_hook) {
    return true;
  }
  g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc,
                                   GetModuleHandleW(nullptr), 0);
  return g_mouse_hook != nullptr;
}

void SetCheckbox(HWND parent, int id, bool checked) {
  SendDlgItemMessageW(parent, id, BM_SETCHECK,
                      checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool GetCheckbox(HWND parent, int id) {
  return SendDlgItemMessageW(parent, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void UpdateStatusText() {
  if (!g_window) {
    return;
  }
  std::wstring status;
  if (!g_config.enabled) {
    status = L"状态：已暂停。";
  } else if (!g_mouse_hook) {
    status = L"状态：监听未启动，请点击“保存并应用”。";
  } else if (!g_config.voice_binding.IsValid()) {
    status = L"状态：后退键=Enter；请录制前进键语音快捷键。";
  } else {
    status = L"状态：运行中。前进键=语音快捷键，后退键=Enter。";
  }
  SetDlgItemTextW(g_window, kStatusText, status.c_str());

  const std::wstring voice = L"语音快捷键：" + BindingText(g_config.voice_binding);
  SetDlgItemTextW(g_window, kVoiceText, voice.c_str());
  SetDlgItemTextW(g_window, kRecordButton,
                  g_recording ? L"请按快捷键..." : L"录制语音快捷键");
  SetCheckbox(g_window, kEnabledCheck, g_config.enabled);
  SetCheckbox(g_window, kInterceptCheck, g_config.intercept_original);
  SetCheckbox(g_window, kHoldModeCheck, g_config.voice_hold_mode);
  SetCheckbox(g_window, kLaunchAtLoginCheck, g_config.launch_at_login);
}

void ApplyConfigFromWindow() {
  g_config.enabled = GetCheckbox(g_window, kEnabledCheck);
  g_config.intercept_original = GetCheckbox(g_window, kInterceptCheck);
  g_config.voice_hold_mode = GetCheckbox(g_window, kHoldModeCheck);
  g_config.launch_at_login = GetCheckbox(g_window, kLaunchAtLoginCheck);
  SaveConfig();
  if (g_config.enabled) {
    StartHook();
  } else {
    StopHook();
  }
  UpdateStatusText();
}

void AddTrayIcon(HWND window) {
  NOTIFYICONDATAW data = {};
  data.cbSize = sizeof(data);
  data.hWnd = window;
  data.uID = kTrayId;
  data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  data.uCallbackMessage = kTrayMessage;
  data.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
  lstrcpynW(data.szTip, L"SideKey", ARRAYSIZE(data.szTip));
  Shell_NotifyIconW(NIM_ADD, &data);
}

void RemoveTrayIcon(HWND window) {
  NOTIFYICONDATAW data = {};
  data.cbSize = sizeof(data);
  data.hWnd = window;
  data.uID = kTrayId;
  Shell_NotifyIconW(NIM_DELETE, &data);
}

void ShowMainWindow() {
  ShowWindow(g_window, SW_SHOWNORMAL);
  SetForegroundWindow(g_window);
}

void ShowTrayMenu() {
  POINT point = {};
  GetCursorPos(&point);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, kMenuShow, L"打开设置");
  AppendMenuW(menu, MF_STRING, kMenuToggle,
              g_config.enabled ? L"暂停映射" : L"启用映射");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuExit, L"退出");
  SetForegroundWindow(g_window);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, g_window, nullptr);
  DestroyMenu(menu);
}

void CreateControls(HWND window) {
  CreateWindowW(L"STATIC", L"SideKey Win7 版", WS_CHILD | WS_VISIBLE, 18, 16,
                360, 24, window, nullptr, g_instance, nullptr);
  CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 18, 46, 520, 24, window,
                reinterpret_cast<HMENU>(kStatusText), g_instance, nullptr);

  CreateWindowW(L"BUTTON", L"启用映射", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                18, 84, 180, 24, window,
                reinterpret_cast<HMENU>(kEnabledCheck), g_instance, nullptr);
  CreateWindowW(L"BUTTON", L"拦截原始前进/后退行为",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 116, 260, 24,
                window, reinterpret_cast<HMENU>(kInterceptCheck), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"前进键按住时持续按住语音快捷键",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 148, 320, 24,
                window, reinterpret_cast<HMENU>(kHoldModeCheck), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"开机自动启动",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 180, 180, 24,
                window, reinterpret_cast<HMENU>(kLaunchAtLoginCheck), g_instance,
                nullptr);

  CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 18, 222, 420, 24,
                window, reinterpret_cast<HMENU>(kVoiceText), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"录制语音快捷键", WS_CHILD | WS_VISIBLE, 18, 254,
                150, 30, window, reinterpret_cast<HMENU>(kRecordButton),
                g_instance, nullptr);
  CreateWindowW(L"BUTTON", L"测试语音", WS_CHILD | WS_VISIBLE, 178, 254, 100, 30,
                window, reinterpret_cast<HMENU>(kTestVoiceButton), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"测试 Enter", WS_CHILD | WS_VISIBLE, 288, 254, 110,
                30, window, reinterpret_cast<HMENU>(kTestEnterButton),
                g_instance, nullptr);

  CreateWindowW(L"BUTTON", L"保存并应用", WS_CHILD | WS_VISIBLE, 18, 310, 120,
                32, window, reinterpret_cast<HMENU>(kSaveButton), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"隐藏到托盘", WS_CHILD | WS_VISIBLE, 148, 310, 120,
                32, window, reinterpret_cast<HMENU>(kHideButton), g_instance,
                nullptr);
  CreateWindowW(L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE, 278, 310, 80, 32,
                window, reinterpret_cast<HMENU>(kExitButton), g_instance,
                nullptr);
}

bool IsModifierKey(WPARAM vk) {
  return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
         vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
         vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
         vk == VK_LWIN || vk == VK_RWIN;
}

void RecordHotkey(WPARAM vk) {
  if (!g_recording || IsModifierKey(vk)) {
    return;
  }
  KeyStroke stroke;
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
    stroke.modifiers.push_back(VK_CONTROL);
  }
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
    stroke.modifiers.push_back(VK_SHIFT);
  }
  if (GetAsyncKeyState(VK_MENU) & 0x8000) {
    stroke.modifiers.push_back(VK_MENU);
  }
  if ((GetAsyncKeyState(VK_LWIN) & 0x8000) ||
      (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
    stroke.modifiers.push_back(VK_LWIN);
  }
  stroke.vk = static_cast<WORD>(vk);
  g_config.voice_binding = stroke;
  g_recording = false;
  ApplyConfigFromWindow();
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param,
                            LPARAM l_param) {
  switch (message) {
    case WM_CREATE:
      CreateControls(window);
      AddTrayIcon(window);
      UpdateStatusText();
      return 0;
    case WM_COMMAND: {
      const int id = LOWORD(w_param);
      switch (id) {
        case kRecordButton:
          g_recording = true;
          SetFocus(window);
          UpdateStatusText();
          return 0;
        case kTestVoiceButton:
          TapStroke(g_config.voice_binding);
          return 0;
        case kTestEnterButton: {
          KeyStroke enter;
          enter.vk = VK_RETURN;
          TapStroke(enter);
          return 0;
        }
        case kSaveButton:
          ApplyConfigFromWindow();
          return 0;
        case kHideButton:
          ShowWindow(window, SW_HIDE);
          return 0;
        case kExitButton:
        case kMenuExit:
          DestroyWindow(window);
          return 0;
        case kMenuShow:
          ShowMainWindow();
          return 0;
        case kMenuToggle:
          g_config.enabled = !g_config.enabled;
          SetCheckbox(window, kEnabledCheck, g_config.enabled);
          ApplyConfigFromWindow();
          return 0;
        default:
          break;
      }
      break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      RecordHotkey(w_param);
      return 0;
    case kTrayMessage:
      if (l_param == WM_LBUTTONDBLCLK) {
        ShowMainWindow();
      } else if (l_param == WM_RBUTTONUP) {
        ShowTrayMenu();
      }
      return 0;
    case WM_CLOSE:
      ShowWindow(window, SW_HIDE);
      return 0;
    case WM_DESTROY:
      RemoveTrayIcon(window);
      StopHook();
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(window, message, w_param, l_param);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command) {
  g_instance = instance;
  LoadConfig();

  INITCOMMONCONTROLSEX controls = {};
  controls.dwSize = sizeof(controls);
  controls.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&controls);

  WNDCLASSW window_class = {};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.lpszClassName = L"SideKeyWin7Window";
  window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&window_class);

  g_window = CreateWindowExW(
      0, window_class.lpszClassName, L"SideKey", WS_OVERLAPPED | WS_CAPTION |
                                                 WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 560, 410, nullptr, nullptr, instance,
      nullptr);
  if (!g_window) {
    return 1;
  }

  if (g_config.enabled) {
    StartHook();
  }
  UpdateStatusText();

  ShowWindow(g_window, show_command);
  UpdateWindow(g_window);

  MSG message = {};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}
