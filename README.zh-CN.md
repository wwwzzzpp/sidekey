# SideKey 中文运行与打包说明

SideKey 是一个 Windows/macOS 桌面小工具，用鼠标侧键代替键盘操作：

- 鼠标前进键：触发你录制的“语音软件快捷键”。
- 鼠标后退键：触发 `Enter`。
- 软件不录音、不做语音识别、不上传数据，只监听鼠标侧键并发送本地键盘快捷键。

## 1. 环境准备

项目使用 Flutter 桌面端开发。先确认 Flutter 可用：

```bash
flutter doctor -v
flutter pub get
```

### Windows 需要安装

1. Flutter SDK。
2. Visual Studio 2022，并勾选 **Desktop development with C++** 工作负载。
3. 开启 Windows Developer Mode。

开启 Developer Mode：

```powershell
start ms-settings:developers
```

打开设置页后启用“开发人员模式”。Flutter 桌面插件需要创建符号链接，不开启会导致构建失败。

### macOS 需要安装

1. Flutter SDK。
2. Xcode。
3. CocoaPods。

检查命令：

```bash
flutter doctor -v
pod --version
```

macOS 版本建议 macOS 13 或更高。

## 2. 开发运行

### Windows 运行

在项目根目录运行：

```powershell
flutter run -d windows
```

首次运行后，在 SideKey 界面里录制语音软件快捷键。例如目标软件的语音输入快捷键是 `Ctrl + Shift + V`，点击“录制热键”后按下这个组合键即可。

注意：如果目标软件以管理员权限运行，而 SideKey 不是管理员权限运行，Windows 可能会阻止 `SendInput` 注入按键。

### macOS 运行

在项目根目录运行：

```bash
flutter run -d macos
```

首次运行需要在系统设置里授权：

1. 系统设置 -> 隐私与安全性 -> 辅助功能，允许 SideKey。
2. 系统设置 -> 隐私与安全性 -> 输入监控，允许 SideKey。
3. 授权后重启 SideKey。

如果界面提示权限不足，可以点击软件里的“打开 macOS 权限设置”。

## 3. 打包发布

### Windows 打包

运行：

```powershell
flutter build windows --release
```

生成目录：

```text
build\windows\x64\runner\Release\
```

这个目录里会包含 `sidekey.exe` 和运行所需 DLL。最简单的分发方式是把整个 `Release` 文件夹压缩成 zip。

建议发布前检查：

```powershell
flutter analyze
flutter test
flutter build windows --release
```

如果要做安装包，可以后续使用 Inno Setup、WiX Toolset 或 MSIX。当前项目还没有内置安装器脚本。

### macOS 打包

在 macOS 机器上运行：

```bash
flutter build macos --release
```

生成 App：

```text
build/macos/Build/Products/Release/sidekey.app
```

本地自用时，可以直接打开这个 `.app`。如果要发给别人使用，建议做签名、公证和 DMG：

```bash
codesign --deep --force --options runtime --sign "Developer ID Application: 你的开发者名称" build/macos/Build/Products/Release/sidekey.app
ditto -c -k --keepParent build/macos/Build/Products/Release/sidekey.app SideKey-macOS.zip
xcrun notarytool submit SideKey-macOS.zip --keychain-profile "你的公证配置" --wait
xcrun stapler staple build/macos/Build/Products/Release/sidekey.app
```

之后可以用 `hdiutil` 或第三方 DMG 工具生成安装包。

注意：SideKey 需要全局监听鼠标和模拟键盘输入，macOS App Sandbox 不适合这个软件，所以项目的 macOS entitlements 已关闭 sandbox。

## 4. 常用开发命令

```bash
flutter pub get
flutter analyze
flutter test
dart format lib test
```

Windows 构建：

```powershell
flutter build windows --release
```

macOS 构建：

```bash
flutter build macos --release
```

## 5. 使用方式

1. 启动 SideKey。
2. 点击“录制热键”。
3. 按下目标语音软件的快捷键。
4. 选择前进键触发模式：
   - 点击一次：按一下鼠标前进键就发送一次快捷键。
   - 按住触发：按住鼠标前进键时按下快捷键，松开时释放快捷键。
5. 鼠标后退键默认固定为 `Enter`。
6. 如需后台常驻，可以在托盘/菜单栏中打开或暂停映射。
7. 如需开机启动，在设置里打开“开机自动启动”。

## 6. 常见问题

### Windows 提示需要 Developer Mode

执行：

```powershell
start ms-settings:developers
```

打开后启用“开发人员模式”，再重新运行构建命令。

### Windows 提示缺少 Visual Studio

安装 Visual Studio 2022，并选择 **Desktop development with C++** 工作负载。

### macOS 运行后侧键没有反应

检查“辅助功能”和“输入监控”权限是否都已经授权。授权后退出并重新打开 SideKey。

### 前进键没有触发语音

SideKey 不内置语音识别。你需要先确认目标软件本身支持快捷键触发语音功能，然后在 SideKey 中录制这个快捷键。

### 浏览器仍然前进或后退

确认设置中的“拦截原始前进/后退行为”已开启。
