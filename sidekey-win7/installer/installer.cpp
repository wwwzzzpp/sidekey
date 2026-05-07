#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <string>
#include <vector>

#include "../src/resource.h"

namespace {

constexpr int IDR_SIDEKEY_EXE = 201;

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
  if (left.empty()) {
    return right;
  }
  if (left.back() == L'\\') {
    return left + right;
  }
  return left + L"\\" + right;
}

std::wstring ModulePath() {
  std::vector<wchar_t> buffer(MAX_PATH);
  DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                    static_cast<DWORD>(buffer.size()));
  while (length == buffer.size()) {
    buffer.resize(buffer.size() * 2);
    length = GetModuleFileNameW(nullptr, buffer.data(),
                                static_cast<DWORD>(buffer.size()));
  }
  return length == 0 ? L"" : std::wstring(buffer.data(), length);
}

std::wstring KnownFolder(int csidl) {
  wchar_t path[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT,
                                 path))) {
    return path;
  }
  return L"";
}

bool EnsureDirectory(const std::wstring& path) {
  if (CreateDirectoryW(path.c_str(), nullptr) ||
      GetLastError() == ERROR_ALREADY_EXISTS) {
    return true;
  }
  return false;
}

bool WriteResourceToFile(HINSTANCE instance, int resource_id,
                         const std::wstring& path) {
  HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resource_id),
                                 RT_RCDATA);
  if (!resource) {
    return false;
  }
  HGLOBAL loaded = LoadResource(instance, resource);
  if (!loaded) {
    return false;
  }
  DWORD size = SizeofResource(instance, resource);
  const void* data = LockResource(loaded);
  if (!data || size == 0) {
    return false;
  }

  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD written = 0;
  BOOL ok = WriteFile(file, data, size, &written, nullptr);
  CloseHandle(file);
  return ok && written == size;
}

void WriteUninstallRegistry(const std::wstring& install_dir) {
  HKEY key = nullptr;
  const wchar_t* reg_path =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SideKey";
  if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, reg_path, 0, nullptr, 0,
                      KEY_SET_VALUE, nullptr, &key, nullptr) !=
      ERROR_SUCCESS) {
    return;
  }

  std::wstring uninstall = JoinPath(install_dir, L"Uninstall.exe");
  std::wstring app = JoinPath(install_dir, L"sidekey.exe");
  DWORD one = 1;
  const auto set_string = [&](const wchar_t* name, const std::wstring& value) {
    RegSetValueExW(key, name, 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  };
  set_string(L"DisplayName", L"SideKey");
  set_string(L"DisplayVersion", L"1.0.0");
  set_string(L"Publisher", L"SideKey");
  set_string(L"InstallLocation", install_dir);
  set_string(L"DisplayIcon", app);
  set_string(L"UninstallString", L"\"" + uninstall + L"\" /uninstall");
  RegSetValueExW(key, L"NoModify", 0, REG_DWORD,
                 reinterpret_cast<const BYTE*>(&one), sizeof(one));
  RegSetValueExW(key, L"NoRepair", 0, REG_DWORD,
                 reinterpret_cast<const BYTE*>(&one), sizeof(one));
  RegCloseKey(key);
}

void DeleteUninstallRegistry() {
  RegDeleteKeyW(HKEY_LOCAL_MACHINE,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SideKey");
}

void DeleteLaunchAtLogin() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
    RegDeleteValueW(key, L"SideKey");
    RegCloseKey(key);
  }
}

bool CreateShortcut(const std::wstring& link_path, const std::wstring& target,
                    const std::wstring& working_dir,
                    const std::wstring& description) {
  IShellLinkW* link = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW,
                                reinterpret_cast<void**>(&link));
  if (FAILED(hr) || !link) {
    return false;
  }
  link->SetPath(target.c_str());
  link->SetWorkingDirectory(working_dir.c_str());
  link->SetDescription(description.c_str());

  IPersistFile* file = nullptr;
  hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file));
  if (SUCCEEDED(hr) && file) {
    hr = file->Save(link_path.c_str(), TRUE);
    file->Release();
  }
  link->Release();
  return SUCCEEDED(hr);
}

void CreateShortcuts(const std::wstring& install_dir) {
  CoInitialize(nullptr);
  std::wstring programs = KnownFolder(CSIDL_COMMON_PROGRAMS);
  std::wstring desktop = KnownFolder(CSIDL_COMMON_DESKTOPDIRECTORY);
  std::wstring group = JoinPath(programs, L"SideKey");
  EnsureDirectory(group);

  std::wstring app = JoinPath(install_dir, L"sidekey.exe");
  std::wstring uninstall = JoinPath(install_dir, L"Uninstall.exe");
  CreateShortcut(JoinPath(group, L"SideKey.lnk"), app, install_dir,
                 L"SideKey");
  CreateShortcut(JoinPath(group, L"Uninstall SideKey.lnk"), uninstall,
                 install_dir, L"Uninstall SideKey");
  CreateShortcut(JoinPath(desktop, L"SideKey.lnk"), app, install_dir,
                 L"SideKey");
  CoUninitialize();
}

void DeleteShortcuts() {
  std::wstring programs = KnownFolder(CSIDL_COMMON_PROGRAMS);
  std::wstring desktop = KnownFolder(CSIDL_COMMON_DESKTOPDIRECTORY);
  std::wstring group = JoinPath(programs, L"SideKey");
  DeleteFileW(JoinPath(group, L"SideKey.lnk").c_str());
  DeleteFileW(JoinPath(group, L"Uninstall SideKey.lnk").c_str());
  RemoveDirectoryW(group.c_str());
  DeleteFileW(JoinPath(desktop, L"SideKey.lnk").c_str());
}

void RunHidden(const std::wstring& file, const std::wstring& parameters,
               bool wait) {
  SHELLEXECUTEINFOW execute = {};
  execute.cbSize = sizeof(execute);
  execute.fMask = wait ? SEE_MASK_NOCLOSEPROCESS : 0;
  execute.lpFile = file.c_str();
  execute.lpParameters = parameters.c_str();
  execute.nShow = SW_HIDE;
  if (ShellExecuteExW(&execute) && execute.hProcess) {
    WaitForSingleObject(execute.hProcess, 3000);
    CloseHandle(execute.hProcess);
  }
}

void StopRunningSideKey() {
  RunHidden(L"taskkill.exe", L"/IM sidekey.exe /F", true);
}

void DeleteSelfAfterExit(const std::wstring& self,
                         const std::wstring& install_dir) {
  std::wstring parameters = L"/c ping 127.0.0.1 -n 3 > nul & del /f /q \"" +
                            self + L"\" & rmdir \"" + install_dir + L"\"";
  RunHidden(L"cmd.exe", parameters, false);
}

std::wstring InstallDir() {
  std::wstring program_files = KnownFolder(CSIDL_PROGRAM_FILES);
  return JoinPath(program_files.empty() ? L"C:\\Program Files" : program_files,
                  L"SideKey");
}

int Install(HINSTANCE instance) {
  std::wstring install_dir = InstallDir();
  if (!EnsureDirectory(install_dir)) {
    MessageBoxW(nullptr, L"Failed to create install directory.", L"SideKey",
                MB_ICONERROR);
    return 1;
  }
  StopRunningSideKey();

  std::wstring app = JoinPath(install_dir, L"sidekey.exe");
  if (!WriteResourceToFile(instance, IDR_SIDEKEY_EXE, app)) {
    MessageBoxW(nullptr, L"Failed to write sidekey.exe.", L"SideKey",
                MB_ICONERROR);
    return 1;
  }

  CopyFileW(ModulePath().c_str(), JoinPath(install_dir, L"Uninstall.exe").c_str(),
            FALSE);
  WriteUninstallRegistry(install_dir);
  CreateShortcuts(install_dir);

  MessageBoxW(nullptr, L"SideKey installed successfully.", L"SideKey", MB_OK);
  ShellExecuteW(nullptr, L"open", app.c_str(), nullptr, install_dir.c_str(),
                SW_SHOWNORMAL);
  return 0;
}

int Uninstall() {
  std::wstring install_dir = InstallDir();
  std::wstring app = JoinPath(install_dir, L"sidekey.exe");
  std::wstring uninstaller = JoinPath(install_dir, L"Uninstall.exe");
  StopRunningSideKey();
  DeleteLaunchAtLogin();
  DeleteShortcuts();
  DeleteUninstallRegistry();
  DeleteFileW(app.c_str());

  std::wstring self = ModulePath();
  MessageBoxW(nullptr, L"SideKey has been uninstalled.", L"SideKey", MB_OK);
  if (_wcsicmp(self.c_str(), uninstaller.c_str()) == 0) {
    MoveFileExW(self.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    DeleteSelfAfterExit(self, install_dir);
  } else {
    DeleteFileW(uninstaller.c_str());
    RemoveDirectoryW(install_dir.c_str());
  }
  return 0;
}

bool HasArg(LPWSTR command_line, const wchar_t* expected) {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(command_line, &argc);
  bool found = false;
  for (int i = 1; i < argc; ++i) {
    if (_wcsicmp(argv[i], expected) == 0) {
      found = true;
      break;
    }
  }
  LocalFree(argv);
  return found;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR command_line, int) {
  if (HasArg(command_line, L"/uninstall")) {
    return Uninstall();
  }
  return Install(instance);
}
