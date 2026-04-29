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

1. Flutter SDK。当前项目已按 Flutter 3.38.4 / Dart 3.10.3 验证。
2. Xcode。建议从 Mac App Store 安装完整 Xcode，而不是只装 Command Line Tools。
3. CocoaPods。
4. 如果要给别人分发，需要 Apple Developer Program 账号和 Developer ID Application 证书。

检查命令：

```bash
flutter doctor -v
pod --version
xcodebuild -version
xcrun notarytool --version
security find-identity -p codesigning -v
```

macOS 版本建议 macOS 13 或更高。

如果 CocoaPods 报 `requires your terminal to be using UTF-8 encoding`，先在当前终端执行：

```bash
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
```

建议把这两行也加入 `~/.zshrc`，否则 `flutter run` 和 `flutter build macos` 可能在 `pod install` 阶段失败。

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
flutter pub get
LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 flutter run -d macos
```

首次运行需要在系统设置里授权：

1. 系统设置 -> 隐私与安全性 -> 辅助功能，允许 SideKey。
2. 系统设置 -> 隐私与安全性 -> 输入监控，允许 SideKey。
3. 授权后重启 SideKey。

如果界面提示权限不足，可以点击软件里的“打开 macOS 权限设置”。

如果 `flutter devices` 看不到 macOS，可以先执行：

```bash
flutter config --enable-macos-desktop
flutter doctor -v
```

如果终端出现 `Failed to foreground app; open returned 1`，但后面已经显示应用日志，通常表示应用已经启动，只是 Flutter 没有把窗口自动置顶。可以从 Dock、任务切换器或生成目录手动打开。

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

#### 3.2.1 打包前检查

发布前建议先确认这些点：

1. Bundle ID 不要继续使用示例值。修改 `macos/Runner/Configs/AppInfo.xcconfig`：

   ```text
   PRODUCT_BUNDLE_IDENTIFIER = com.yourcompany.sidekey
   PRODUCT_COPYRIGHT = Copyright © 2026 Your Company. All rights reserved.
   ```

   `PRODUCT_NAME` 当前是 `sidekey`，所以构建产物默认是 `sidekey.app`。如果你把 `PRODUCT_NAME` 改成 `SideKey`，后面命令里的 App 路径也要同步改成 `build/macos/Build/Products/Release/SideKey.app`。

2. SideKey 需要全局监听鼠标侧键并模拟键盘输入，不适合开启 App Sandbox。当前 `macos/Runner/Release.entitlements` 是空 entitlements，这是有意的。
3. 如果之后增加网络、文件访问、自动更新等能力，需要重新检查 entitlements、签名和公证结果。
4. 运行发布前检查：

   ```bash
   flutter pub get
   flutter analyze
   flutter test
   ```

#### 3.2.2 生成 Release App

在 macOS 机器上运行：

```bash
LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 flutter build macos --release
```

生成 App：

```text
build/macos/Build/Products/Release/sidekey.app
```

本机自用时，可以直接打开：

```bash
open build/macos/Build/Products/Release/sidekey.app
```

但如果要发给别人，建议完整做 Developer ID 签名、公证和 DMG。否则用户大概率会遇到 Gatekeeper 拦截。

#### 3.2.3 准备 Developer ID 签名

需要先在 Apple Developer 账号里创建并安装 `Developer ID Application` 证书。安装后确认本机钥匙串里能看到有效身份：

```bash
security find-identity -p codesigning -v
```

输出里应该有类似：

```text
"Developer ID Application: 你的名字或公司名 (TEAMID)"
```

后续命令建议先设置变量，减少输错路径：

```bash
APP="build/macos/Build/Products/Release/sidekey.app"
TEAM_ID="你的 Team ID"
IDENTITY="Developer ID Application: 你的名字或公司名 (${TEAM_ID})"
PROFILE="sidekey-notary"
mkdir -p dist
```

如果 `security find-identity` 里有多个同名证书，可以把 `IDENTITY` 改成证书前面的 SHA-1 哈希。

#### 3.2.4 签名 App

对 `.app` 做 Developer ID 签名，并启用 Hardened Runtime：

```bash
codesign \
  --force \
  --deep \
  --options runtime \
  --timestamp \
  --entitlements macos/Runner/Release.entitlements \
  --sign "$IDENTITY" \
  "$APP"
```

验证签名：

```bash
codesign --verify --deep --strict --verbose=2 "$APP"
codesign -dv --verbose=4 "$APP"
```

公证前执行 `spctl --assess` 可能仍然提示未公证，这是正常的；签名通过只说明 Developer ID 签名本身有效。

#### 3.2.5 保存公证凭据

第一次公证前，把 Apple ID、Team ID 和应用专用密码保存到钥匙串：

```bash
xcrun notarytool store-credentials "$PROFILE" \
  --apple-id "你的 Apple ID 邮箱" \
  --team-id "$TEAM_ID" \
  --password "应用专用密码"
```

`PROFILE` 只是本机钥匙串里的配置名，后续可以重复使用。应用专用密码需要在 Apple Account 里生成，不是 Apple ID 登录密码。

#### 3.2.6 公证 App

`notarytool` 不能直接上传 `.app` 目录，先用 `ditto` 打包成 zip：

```bash
APP_ZIP="dist/SideKey-macOS-app-notary.zip"

ditto -c -k --sequesterRsrc --keepParent "$APP" "$APP_ZIP"
xcrun notarytool submit "$APP_ZIP" --keychain-profile "$PROFILE" --wait
```

如果结果是 `Accepted`，把公证票据装订到 App：

```bash
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"
spctl --assess --type execute --verbose=4 "$APP"
```

如果结果不是 `Accepted`，查看日志：

```bash
xcrun notarytool log "提交返回的 Submission ID" --keychain-profile "$PROFILE"
```

常见原因是签名身份不对、没有启用 Hardened Runtime、嵌套 framework 没有正确签名，或者 Bundle ID 仍然是示例值。

#### 3.2.7 生成、签名和公证 DMG

推荐给用户分发 DMG。先用已经签名并装订过的 App 生成 DMG：

```bash
DMG_ROOT="dist/dmg-root"
DMG="dist/SideKey-macOS.dmg"

rm -rf "$DMG_ROOT" "$DMG"
mkdir -p "$DMG_ROOT"
cp -R "$APP" "$DMG_ROOT/SideKey.app"
ln -s /Applications "$DMG_ROOT/Applications"

hdiutil create \
  -volname "SideKey" \
  -srcfolder "$DMG_ROOT" \
  -ov \
  -format UDZO \
  "$DMG"
```

签名 DMG：

```bash
codesign --force --timestamp --sign "$IDENTITY" "$DMG"
codesign --verify --verbose=2 "$DMG"
```

提交 DMG 公证并装订票据：

```bash
xcrun notarytool submit "$DMG" --keychain-profile "$PROFILE" --wait
xcrun stapler staple "$DMG"
xcrun stapler validate "$DMG"
spctl --assess --type open --context context:primary-signature --verbose=4 "$DMG"
```

如果这里通过，就可以把 `dist/SideKey-macOS.dmg` 发给用户。

#### 3.2.8 只分发 zip 的做法

如果不做 DMG，只想分发 zip，需要在 App 已经 `stapler staple` 之后重新打包：

```bash
FINAL_ZIP="dist/SideKey-macOS.zip"
rm -f "$FINAL_ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$FINAL_ZIP"
```

不要把公证前上传用的 `SideKey-macOS-app-notary.zip` 直接发给用户，因为那个 zip 里的 App 还没有装订票据。

#### 3.2.9 Apple 官方参考

- [Signing Mac Software with Developer ID](https://developer.apple.com/developer-id/)
- [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
- [Packaging Mac software for distribution](https://developer.apple.com/documentation/xcode/packaging-mac-software-for-distribution)

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

### macOS `pod install` 提示 UTF-8 编码错误

执行：

```bash
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
```

然后重新运行：

```bash
flutter pub get
LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 flutter run -d macos
```

如果确认有效，把两行 `export` 写入 `~/.zshrc`。

### macOS 打开时提示无法验证开发者

这是未签名、未公证或公证票据没有装订导致的。自用可以在系统设置里手动允许；分发给别人应按上面的 Developer ID 签名、公证和 DMG 流程重新打包。

### macOS 公证失败

先看公证日志：

```bash
xcrun notarytool log "提交返回的 Submission ID" --keychain-profile "sidekey-notary"
```

然后重新检查：

1. `security find-identity -p codesigning -v` 能看到 Developer ID Application 证书。
2. `codesign --verify --deep --strict --verbose=2 build/macos/Build/Products/Release/sidekey.app` 通过。
3. 签名时使用了 `--options runtime --timestamp`。
4. `PRODUCT_BUNDLE_IDENTIFIER` 已经改成自己的 Bundle ID。

### 前进键没有触发语音

SideKey 不内置语音识别。你需要先确认目标软件本身支持快捷键触发语音功能，然后在 SideKey 中录制这个快捷键。

### 浏览器仍然前进或后退

确认设置中的“拦截原始前进/后退行为”已开启。
