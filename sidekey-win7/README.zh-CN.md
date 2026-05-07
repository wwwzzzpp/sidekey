# SideKey Win7 原生版

这是从 Flutter 版独立出来的 Windows 7 专用工程，技术栈是：

- 原生 Win32/C++17
- `WH_MOUSE_LL` 全局鼠标侧键 hook
- `WH_KEYBOARD_LL` 热键录制 hook
- `SendInput` 模拟键盘输入
- 自带 Win32 安装器生成正式安装程序

这个版本不依赖 Flutter、.NET、Electron 或 WebView，目标是兼容 Windows 7。

## 功能

- 鼠标前进侧键：发送录制的快捷键。
- 鼠标后退侧键：发送 `Enter`。
- 支持字母、功能键、左右 Ctrl/Shift/Alt/Win。
- 支持单独按一次 `Alt`/`Ctrl` 这种纯修饰键。
- 支持连续按两次同一个键，例如 `Left Ctrl x2`。
- 支持拦截浏览器原始前进/后退。
- 支持托盘运行、暂停映射、开机启动。

## 在 Windows 上构建安装包

推荐在 Windows 10/11 上构建，然后把安装包发给 Windows 7 使用。

需要安装：

1. CMake。
2. Visual Studio 2019/2022 Build Tools，勾选 C++ 桌面开发。
构建：

```powershell
cd sidekey-win7
.\build-windows.ps1
```

输出：

```text
sidekey-win7\dist\SideKey-Win7-Setup.exe
```

把这个 `.exe` 发给同事安装即可。

## 在 macOS 上交叉编译安装包

需要 Homebrew：

```bash
brew install cmake mingw-w64
```

构建：

```bash
cd sidekey-win7
./build-macos-cross.sh
```

输出：

```text
sidekey-win7/dist/SideKey-Win7-Setup.exe
```

## Windows 7 使用注意

1. 如果目标软件以管理员权限运行，SideKey 也需要以管理员权限运行，否则 Windows 可能拦截 `SendInput`。
2. 从 macOS 用 Homebrew MinGW-w64 交叉编译出来的包可能仍会导入 Windows UCRT。Windows 7 SP1 如果缺少 UCRT，请先安装 Microsoft Visual C++ 2015-2022 Redistributable x86，或安装 Windows 7 的 Universal C Runtime 更新。要尽量减少运行库依赖，建议在 Windows 上用 Visual Studio Build Tools 执行 `build-windows.ps1` 生成最终包。
3. Windows 7 没有现代 SmartScreen 的同等体验，但杀毒软件仍可能提示未知程序。要消除这类提示，需要购买代码签名证书给安装包和 exe 签名。
4. 这个安装包本身是正式安装程序格式，但默认未做商业代码签名。

## 卸载

在控制面板的“程序和功能”里卸载 SideKey，或运行安装目录里的 `Uninstall.exe`。
