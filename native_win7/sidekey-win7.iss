#define MyAppName "SideKey"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SideKey"
#define MyAppExeName "sidekey-win7.exe"
#define RootDir ".."

[Setup]
AppId={{7C6C8321-F91E-4A71-9E4A-4B59868E1598}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\SideKey
DefaultGroupName=SideKey
DisableProgramGroupPage=yes
OutputDir={#RootDir}\dist
OutputBaseFilename=SideKey-Setup-1.0.0-win7-x64
SetupIconFile={#RootDir}\windows\runner\resources\app_icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=6.1sp1
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务："; Flags: unchecked
Name: "launch"; Description: "安装完成后启动 SideKey"; GroupDescription: "安装完成："; Flags: checkedonce

[Files]
Source: "{#RootDir}\build\native_win7\x64\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SideKey"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 SideKey"; Filename: "{uninstallexe}"
Name: "{autodesktop}\SideKey"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 SideKey"; Flags: nowait postinstall skipifsilent; Tasks: launch

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\SideKey"
