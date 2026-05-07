SideKey macOS 打开说明

这个版本使用 ad-hoc 签名，适合同事内部试用；它没有 Apple Developer ID 公证。

安装：
1. 打开 DMG。
2. 把 sidekey.app 拖到 Applications。
3. 第一次打开时，如果 macOS 提示无法验证开发者，请按住 Control 键点击 sidekey.app，选择“打开”，再确认“打开”。

如果仍提示无法打开，在终端执行：

  xattr -dr com.apple.quarantine /Applications/sidekey.app
  open /Applications/sidekey.app

首次运行后还需要授权：
1. 系统设置 -> 隐私与安全性 -> 辅助功能，允许 sidekey。
2. 系统设置 -> 隐私与安全性 -> 输入监控，允许 sidekey。
3. 退出并重新打开 sidekey。

如果要做到双击即开、没有 Gatekeeper 提示，需要使用 Apple Developer ID 证书签名并通过 Apple 公证。
