# SideKey Win7 native package

This directory contains the Windows 7 compatible native build of SideKey.

Why this exists:
- The Flutter desktop build targets modern Windows and is not a reliable Windows 7 deliverable.
- This native build avoids Flutter and the Visual C++ runtime DLLs.
- The executable is linked for Windows subsystem version 6.01 and only imports common Win32 system DLLs.

Build:

```powershell
powershell -ExecutionPolicy Bypass -File native_win7\build_win7.ps1
```

Package:

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" native_win7\sidekey-win7.iss
```

Output:

```text
dist\SideKey-Setup-1.0.0-win7-x64.exe
```

Runtime notes:
- Windows 7 SP1 x64 or newer.
- Back mouse side button sends Enter.
- Forward mouse side button sends the recorded voice hotkey.
- The app stays in the system tray and can be paused or opened from the tray menu.
- The installer is not code-signed unless a signing certificate is added later.
