# SideKey

SideKey is a small Windows/macOS Flutter desktop utility that remaps mouse side
buttons globally:

- Forward side button: sends a recorded voice-app hotkey.
- Back side button: sends `Enter`.

The app does not record audio, perform speech recognition, or upload data. It
only listens for mouse side-button events and sends the configured keyboard
shortcut locally.

## Features

- Global mouse side-button mapping.
- Voice hotkey recording from the settings window.
- Tap mode and hold mode for the forward button.
- Back button fixed to `Enter`.
- Tray/menu-bar controls for open, pause, and quit.
- Optional launch at login.
- macOS permission guidance for Accessibility and Input Monitoring.

## Development

```bash
flutter pub get
flutter analyze
flutter test
```

Windows desktop builds require:

- Windows Developer Mode enabled, because Flutter desktop plugins use symlinks.
- Visual Studio with the "Desktop development with C++" workload.

macOS builds require running on macOS with Xcode installed. The app intentionally
does not use the macOS App Sandbox, because global event taps and key injection
need system privacy permissions.

## Platform Notes

Windows uses a low-level `WH_MOUSE_LL` hook and `SendInput`. Injected keys may
not reach applications running with higher privileges than SideKey.

macOS uses a Core Graphics event tap and `CGEvent` keyboard events. Users must
grant Accessibility and Input Monitoring permissions before global mapping can
work.
