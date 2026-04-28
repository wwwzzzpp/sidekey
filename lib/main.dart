import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:tray_manager/tray_manager.dart';
import 'package:window_manager/window_manager.dart';

const MethodChannel _nativeChannel = MethodChannel('sidekey/native');
const String _configKey = 'sidekey.config.v1';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  if (Platform.isWindows || Platform.isMacOS) {
    await windowManager.ensureInitialized();
    const options = WindowOptions(
      size: Size(920, 680),
      minimumSize: Size(760, 560),
      center: true,
      title: 'SideKey',
      titleBarStyle: TitleBarStyle.normal,
    );
    await windowManager.waitUntilReadyToShow(options, () async {
      await windowManager.show();
      await windowManager.focus();
      await windowManager.setPreventClose(true);
    });
  }

  runApp(const SideKeyApp());
}

enum VoiceTriggerMode {
  tap('tap', '点击一次'),
  hold('hold', '按住说话');

  const VoiceTriggerMode(this.value, this.label);

  final String value;
  final String label;

  static VoiceTriggerMode fromValue(String? value) {
    return values.firstWhere(
      (mode) => mode.value == value,
      orElse: () => VoiceTriggerMode.tap,
    );
  }
}

class KeyBinding {
  const KeyBinding({
    required this.label,
    required this.windowsVk,
    required this.macosKeyCode,
    this.modifiers = const <String>[],
  });

  final String label;
  final int windowsVk;
  final int macosKeyCode;
  final List<String> modifiers;

  bool get isEmpty => windowsVk <= 0 && macosKeyCode <= 0;

  Map<String, Object?> toJson() {
    return <String, Object?>{
      'label': label,
      'windowsVk': windowsVk,
      'macosKeyCode': macosKeyCode,
      'modifiers': modifiers,
    };
  }

  Map<String, Object?> toNativeJson() => toJson();

  static KeyBinding? fromJson(Object? raw) {
    if (raw is! Map) {
      return null;
    }
    final modifiers = raw['modifiers'];
    return KeyBinding(
      label: (raw['label'] as String?) ?? '未设置',
      windowsVk: (raw['windowsVk'] as num?)?.toInt() ?? 0,
      macosKeyCode: (raw['macosKeyCode'] as num?)?.toInt() ?? 0,
      modifiers: modifiers is List
          ? modifiers.whereType<String>().toList(growable: false)
          : const <String>[],
    );
  }

  static const enter = KeyBinding(
    label: 'Enter',
    windowsVk: 0x0D,
    macosKeyCode: 36,
  );
}

class AppConfig {
  const AppConfig({
    required this.enabled,
    required this.interceptOriginal,
    required this.launchAtLogin,
    required this.voiceMode,
    required this.voiceBinding,
  });

  final bool enabled;
  final bool interceptOriginal;
  final bool launchAtLogin;
  final VoiceTriggerMode voiceMode;
  final KeyBinding? voiceBinding;

  AppConfig copyWith({
    bool? enabled,
    bool? interceptOriginal,
    bool? launchAtLogin,
    VoiceTriggerMode? voiceMode,
    KeyBinding? voiceBinding,
    bool clearVoiceBinding = false,
  }) {
    return AppConfig(
      enabled: enabled ?? this.enabled,
      interceptOriginal: interceptOriginal ?? this.interceptOriginal,
      launchAtLogin: launchAtLogin ?? this.launchAtLogin,
      voiceMode: voiceMode ?? this.voiceMode,
      voiceBinding: clearVoiceBinding
          ? null
          : (voiceBinding ?? this.voiceBinding),
    );
  }

  Map<String, Object?> toJson() {
    return <String, Object?>{
      'enabled': enabled,
      'interceptOriginal': interceptOriginal,
      'launchAtLogin': launchAtLogin,
      'voiceMode': voiceMode.value,
      'voiceBinding': voiceBinding?.toJson(),
      'backBinding': KeyBinding.enter.toJson(),
    };
  }

  Map<String, Object?> toNativeJson() {
    return <String, Object?>{
      'enabled': enabled,
      'interceptOriginal': interceptOriginal,
      'launchAtLogin': launchAtLogin,
      'voiceMode': voiceMode.value,
      'voiceBinding': voiceBinding?.toNativeJson(),
      'backBinding': KeyBinding.enter.toNativeJson(),
    };
  }

  static AppConfig fromJson(Map<String, Object?> json) {
    return AppConfig(
      enabled: (json['enabled'] as bool?) ?? true,
      interceptOriginal: (json['interceptOriginal'] as bool?) ?? true,
      launchAtLogin: (json['launchAtLogin'] as bool?) ?? false,
      voiceMode: VoiceTriggerMode.fromValue(json['voiceMode'] as String?),
      voiceBinding: KeyBinding.fromJson(json['voiceBinding']),
    );
  }

  static const defaults = AppConfig(
    enabled: true,
    interceptOriginal: true,
    launchAtLogin: false,
    voiceMode: VoiceTriggerMode.tap,
    voiceBinding: null,
  );
}

class NativeStatus {
  const NativeStatus({
    this.running = false,
    this.permission = 'unknown',
    this.message = '等待初始化',
  });

  final bool running;
  final String permission;
  final String message;

  static NativeStatus fromJson(Object? raw) {
    if (raw is! Map) {
      return const NativeStatus(message: '平台层未返回状态');
    }
    return NativeStatus(
      running: raw['running'] == true,
      permission: (raw['permission'] as String?) ?? 'unknown',
      message: (raw['message'] as String?) ?? '状态正常',
    );
  }
}

class KeySpec {
  const KeySpec(this.label, this.windowsVk, this.macosKeyCode);

  final String label;
  final int windowsVk;
  final int macosKeyCode;
}

final Map<LogicalKeyboardKey, KeySpec> _keySpecs =
    <LogicalKeyboardKey, KeySpec>{
      LogicalKeyboardKey.keyA: KeySpec('A', 0x41, 0),
      LogicalKeyboardKey.keyB: KeySpec('B', 0x42, 11),
      LogicalKeyboardKey.keyC: KeySpec('C', 0x43, 8),
      LogicalKeyboardKey.keyD: KeySpec('D', 0x44, 2),
      LogicalKeyboardKey.keyE: KeySpec('E', 0x45, 14),
      LogicalKeyboardKey.keyF: KeySpec('F', 0x46, 3),
      LogicalKeyboardKey.keyG: KeySpec('G', 0x47, 5),
      LogicalKeyboardKey.keyH: KeySpec('H', 0x48, 4),
      LogicalKeyboardKey.keyI: KeySpec('I', 0x49, 34),
      LogicalKeyboardKey.keyJ: KeySpec('J', 0x4A, 38),
      LogicalKeyboardKey.keyK: KeySpec('K', 0x4B, 40),
      LogicalKeyboardKey.keyL: KeySpec('L', 0x4C, 37),
      LogicalKeyboardKey.keyM: KeySpec('M', 0x4D, 46),
      LogicalKeyboardKey.keyN: KeySpec('N', 0x4E, 45),
      LogicalKeyboardKey.keyO: KeySpec('O', 0x4F, 31),
      LogicalKeyboardKey.keyP: KeySpec('P', 0x50, 35),
      LogicalKeyboardKey.keyQ: KeySpec('Q', 0x51, 12),
      LogicalKeyboardKey.keyR: KeySpec('R', 0x52, 15),
      LogicalKeyboardKey.keyS: KeySpec('S', 0x53, 1),
      LogicalKeyboardKey.keyT: KeySpec('T', 0x54, 17),
      LogicalKeyboardKey.keyU: KeySpec('U', 0x55, 32),
      LogicalKeyboardKey.keyV: KeySpec('V', 0x56, 9),
      LogicalKeyboardKey.keyW: KeySpec('W', 0x57, 13),
      LogicalKeyboardKey.keyX: KeySpec('X', 0x58, 7),
      LogicalKeyboardKey.keyY: KeySpec('Y', 0x59, 16),
      LogicalKeyboardKey.keyZ: KeySpec('Z', 0x5A, 6),
      LogicalKeyboardKey.digit0: KeySpec('0', 0x30, 29),
      LogicalKeyboardKey.digit1: KeySpec('1', 0x31, 18),
      LogicalKeyboardKey.digit2: KeySpec('2', 0x32, 19),
      LogicalKeyboardKey.digit3: KeySpec('3', 0x33, 20),
      LogicalKeyboardKey.digit4: KeySpec('4', 0x34, 21),
      LogicalKeyboardKey.digit5: KeySpec('5', 0x35, 23),
      LogicalKeyboardKey.digit6: KeySpec('6', 0x36, 22),
      LogicalKeyboardKey.digit7: KeySpec('7', 0x37, 26),
      LogicalKeyboardKey.digit8: KeySpec('8', 0x38, 28),
      LogicalKeyboardKey.digit9: KeySpec('9', 0x39, 25),
      LogicalKeyboardKey.enter: KeySpec('Enter', 0x0D, 36),
      LogicalKeyboardKey.escape: KeySpec('Esc', 0x1B, 53),
      LogicalKeyboardKey.space: KeySpec('Space', 0x20, 49),
      LogicalKeyboardKey.tab: KeySpec('Tab', 0x09, 48),
      LogicalKeyboardKey.backspace: KeySpec('Backspace', 0x08, 51),
      LogicalKeyboardKey.delete: KeySpec('Delete', 0x2E, 117),
      LogicalKeyboardKey.arrowUp: KeySpec('Up', 0x26, 126),
      LogicalKeyboardKey.arrowDown: KeySpec('Down', 0x28, 125),
      LogicalKeyboardKey.arrowLeft: KeySpec('Left', 0x25, 123),
      LogicalKeyboardKey.arrowRight: KeySpec('Right', 0x27, 124),
      LogicalKeyboardKey.home: KeySpec('Home', 0x24, 115),
      LogicalKeyboardKey.end: KeySpec('End', 0x23, 119),
      LogicalKeyboardKey.pageUp: KeySpec('PageUp', 0x21, 116),
      LogicalKeyboardKey.pageDown: KeySpec('PageDown', 0x22, 121),
      LogicalKeyboardKey.insert: KeySpec('Insert', 0x2D, 114),
      LogicalKeyboardKey.f1: KeySpec('F1', 0x70, 122),
      LogicalKeyboardKey.f2: KeySpec('F2', 0x71, 120),
      LogicalKeyboardKey.f3: KeySpec('F3', 0x72, 99),
      LogicalKeyboardKey.f4: KeySpec('F4', 0x73, 118),
      LogicalKeyboardKey.f5: KeySpec('F5', 0x74, 96),
      LogicalKeyboardKey.f6: KeySpec('F6', 0x75, 97),
      LogicalKeyboardKey.f7: KeySpec('F7', 0x76, 98),
      LogicalKeyboardKey.f8: KeySpec('F8', 0x77, 100),
      LogicalKeyboardKey.f9: KeySpec('F9', 0x78, 101),
      LogicalKeyboardKey.f10: KeySpec('F10', 0x79, 109),
      LogicalKeyboardKey.f11: KeySpec('F11', 0x7A, 103),
      LogicalKeyboardKey.f12: KeySpec('F12', 0x7B, 111),
      LogicalKeyboardKey.minus: KeySpec('-', 0xBD, 27),
      LogicalKeyboardKey.equal: KeySpec('=', 0xBB, 24),
      LogicalKeyboardKey.bracketLeft: KeySpec('[', 0xDB, 33),
      LogicalKeyboardKey.bracketRight: KeySpec(']', 0xDD, 30),
      LogicalKeyboardKey.backslash: KeySpec(r'\', 0xDC, 42),
      LogicalKeyboardKey.semicolon: KeySpec(';', 0xBA, 41),
      LogicalKeyboardKey.quoteSingle: KeySpec("'", 0xDE, 39),
      LogicalKeyboardKey.comma: KeySpec(',', 0xBC, 43),
      LogicalKeyboardKey.period: KeySpec('.', 0xBE, 47),
      LogicalKeyboardKey.slash: KeySpec('/', 0xBF, 44),
      LogicalKeyboardKey.backquote: KeySpec('`', 0xC0, 50),
    };

final Set<LogicalKeyboardKey> _modifierKeys = <LogicalKeyboardKey>{
  LogicalKeyboardKey.controlLeft,
  LogicalKeyboardKey.controlRight,
  LogicalKeyboardKey.shiftLeft,
  LogicalKeyboardKey.shiftRight,
  LogicalKeyboardKey.altLeft,
  LogicalKeyboardKey.altRight,
  LogicalKeyboardKey.metaLeft,
  LogicalKeyboardKey.metaRight,
};

class SideKeyApp extends StatelessWidget {
  const SideKeyApp({super.key});

  @override
  Widget build(BuildContext context) {
    final colorScheme = ColorScheme.fromSeed(
      seedColor: const Color(0xFF0F766E),
      brightness: Brightness.light,
    );
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'SideKey',
      theme: ThemeData(
        colorScheme: colorScheme,
        scaffoldBackgroundColor: const Color(0xFFF7F9FB),
        useMaterial3: true,
        cardTheme: const CardThemeData(
          elevation: 0,
          margin: EdgeInsets.zero,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.all(Radius.circular(8)),
            side: BorderSide(color: Color(0xFFE2E8F0)),
          ),
        ),
      ),
      home: const SideKeyHome(),
    );
  }
}

class SideKeyHome extends StatefulWidget {
  const SideKeyHome({super.key});

  @override
  State<SideKeyHome> createState() => _SideKeyHomeState();
}

class _SideKeyHomeState extends State<SideKeyHome>
    with TrayListener, WindowListener {
  final FocusNode _recorderFocusNode = FocusNode(debugLabel: 'HotkeyRecorder');
  AppConfig _config = AppConfig.defaults;
  NativeStatus _status = const NativeStatus();
  bool _loading = true;
  bool _recording = false;
  String? _error;
  Timer? _statusTimer;

  @override
  void initState() {
    super.initState();
    trayManager.addListener(this);
    windowManager.addListener(this);
    unawaited(_bootstrap());
  }

  @override
  void dispose() {
    _statusTimer?.cancel();
    trayManager.removeListener(this);
    windowManager.removeListener(this);
    _recorderFocusNode.dispose();
    super.dispose();
  }

  Future<void> _bootstrap() async {
    final prefs = await SharedPreferences.getInstance();
    final saved = prefs.getString(_configKey);
    final decoded = saved == null ? null : jsonDecode(saved);
    final loaded = decoded is Map
        ? AppConfig.fromJson(decoded.cast<String, Object?>())
        : AppConfig.defaults;

    setState(() {
      _config = loaded;
      _loading = false;
    });

    await _initTray();
    await _applyConfig();
    _statusTimer = Timer.periodic(
      const Duration(seconds: 5),
      (_) => unawaited(_refreshPermissionStatus()),
    );
  }

  Future<void> _initTray() async {
    if (!Platform.isWindows && !Platform.isMacOS) {
      return;
    }
    final iconPath = Platform.isWindows
        ? 'assets/tray_icon.ico'
        : 'assets/tray_icon.png';
    await trayManager.setIcon(iconPath, isTemplate: Platform.isMacOS);
    await trayManager.setToolTip('SideKey');
    await _rebuildTrayMenu();
  }

  Future<void> _rebuildTrayMenu() async {
    await trayManager.setContextMenu(
      Menu(
        items: <MenuItem>[
          MenuItem(
            key: 'show',
            label: '打开设置',
            onClick: (_) => unawaited(_showWindow()),
          ),
          MenuItem.checkbox(
            key: 'enabled',
            label: _config.enabled ? '暂停映射' : '启用映射',
            checked: _config.enabled,
            onClick: (_) => unawaited(
              _updateConfig(_config.copyWith(enabled: !_config.enabled)),
            ),
          ),
          MenuItem.separator(),
          MenuItem(
            key: 'quit',
            label: '退出',
            onClick: (_) => unawaited(_quitApp()),
          ),
        ],
      ),
    );
  }

  Future<void> _saveConfig() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_configKey, jsonEncode(_config.toJson()));
  }

  Future<void> _updateConfig(AppConfig config) async {
    setState(() {
      _config = config;
      _error = null;
    });
    await _saveConfig();
    await _applyConfig();
    await _rebuildTrayMenu();
  }

  Future<void> _applyConfig() async {
    try {
      final raw = await _nativeChannel.invokeMethod<Object?>(
        'applyConfig',
        _config.toNativeJson(),
      );
      setState(() {
        _status = NativeStatus.fromJson(raw);
        _error = null;
      });
    } on Object catch (error) {
      setState(() {
        _status = const NativeStatus(message: '平台层初始化失败');
        _error = '$error';
      });
    }
  }

  Future<void> _refreshPermissionStatus() async {
    try {
      final raw = await _nativeChannel.invokeMethod<Object?>(
        'getPermissionStatus',
      );
      if (!mounted) {
        return;
      }
      setState(() {
        _status = NativeStatus.fromJson(raw);
      });
    } on Object {
      // Keep the last status. applyConfig already reports the actionable error.
    }
  }

  Future<void> _setLaunchAtLogin(bool value) async {
    try {
      final enabled =
          await _nativeChannel.invokeMethod<bool>(
            'setLaunchAtLogin',
            <String, Object?>{'enabled': value},
          ) ??
          false;
      await _updateConfig(_config.copyWith(launchAtLogin: enabled));
    } on Object catch (error) {
      setState(() => _error = '$error');
    }
  }

  Future<void> _openPermissionSettings() async {
    try {
      await _nativeChannel.invokeMethod<void>('openPermissionSettings');
    } on Object catch (error) {
      setState(() => _error = '$error');
    }
  }

  Future<void> _testAction(String action) async {
    try {
      await _nativeChannel.invokeMethod<void>('testAction', <String, Object?>{
        'action': action,
      });
    } on Object catch (error) {
      setState(() => _error = '$error');
    }
  }

  void _startRecording() {
    setState(() {
      _recording = true;
      _error = null;
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _recorderFocusNode.requestFocus();
    });
  }

  void _handleRecorderKey(KeyEvent event) {
    if (!_recording || event is! KeyDownEvent) {
      return;
    }
    final logicalKey = event.logicalKey;
    if (_modifierKeys.contains(logicalKey)) {
      return;
    }
    final spec = _keySpecs[logicalKey];
    if (spec == null) {
      setState(() {
        _error = '暂不支持录制 ${logicalKey.keyLabel}，请换一个常用键或功能键。';
      });
      return;
    }

    final modifiers = <String>[
      if (HardwareKeyboard.instance.isControlPressed) 'ctrl',
      if (HardwareKeyboard.instance.isShiftPressed) 'shift',
      if (HardwareKeyboard.instance.isAltPressed) 'alt',
      if (HardwareKeyboard.instance.isMetaPressed) 'meta',
    ];
    final label = <String>[
      ...modifiers.map(_modifierLabel),
      spec.label,
    ].join(' + ');

    unawaited(
      _updateConfig(
        _config.copyWith(
          voiceBinding: KeyBinding(
            label: label,
            windowsVk: spec.windowsVk,
            macosKeyCode: spec.macosKeyCode,
            modifiers: modifiers,
          ),
        ),
      ),
    );
    setState(() {
      _recording = false;
    });
  }

  String _modifierLabel(String modifier) {
    return switch (modifier) {
      'ctrl' => Platform.isMacOS ? 'Control' : 'Ctrl',
      'shift' => 'Shift',
      'alt' => Platform.isMacOS ? 'Option' : 'Alt',
      'meta' => Platform.isMacOS ? 'Command' : 'Win',
      _ => modifier,
    };
  }

  Future<void> _showWindow() async {
    await windowManager.show();
    await windowManager.focus();
  }

  Future<void> _quitApp() async {
    await _nativeChannel.invokeMethod<void>('shutdown');
    await trayManager.destroy();
    await windowManager.setPreventClose(false);
    await windowManager.destroy();
    exit(0);
  }

  @override
  Future<void> onWindowClose() async {
    await windowManager.hide();
  }

  @override
  void onTrayIconMouseDown() {
    unawaited(_showWindow());
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }

    return KeyboardListener(
      focusNode: _recorderFocusNode,
      onKeyEvent: _handleRecorderKey,
      child: Scaffold(
        body: SafeArea(
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 980),
              child: ListView(
                padding: const EdgeInsets.all(24),
                children: <Widget>[
                  _Header(status: _status),
                  const SizedBox(height: 18),
                  _StatusPanel(
                    config: _config,
                    status: _status,
                    error: _error,
                    onEnabledChanged: (value) =>
                        _updateConfig(_config.copyWith(enabled: value)),
                    onOpenPermissionSettings: _openPermissionSettings,
                  ),
                  const SizedBox(height: 14),
                  _VoicePanel(
                    config: _config,
                    recording: _recording,
                    onRecord: _startRecording,
                    onClear: () => _updateConfig(
                      _config.copyWith(clearVoiceBinding: true),
                    ),
                    onModeChanged: (mode) =>
                        _updateConfig(_config.copyWith(voiceMode: mode)),
                    onTest: () => _testAction('voice'),
                  ),
                  const SizedBox(height: 14),
                  _BackButtonPanel(onTest: () => _testAction('back')),
                  const SizedBox(height: 14),
                  _BehaviorPanel(
                    config: _config,
                    onInterceptChanged: (value) => _updateConfig(
                      _config.copyWith(interceptOriginal: value),
                    ),
                    onLaunchAtLoginChanged: _setLaunchAtLogin,
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _Header extends StatelessWidget {
  const _Header({required this.status});

  final NativeStatus status;

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    return Row(
      children: <Widget>[
        Container(
          width: 44,
          height: 44,
          decoration: BoxDecoration(
            color: colorScheme.primary,
            borderRadius: BorderRadius.circular(8),
          ),
          child: const Icon(Icons.keyboard_command_key, color: Colors.white),
        ),
        const SizedBox(width: 14),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text(
                'SideKey',
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              Text(
                '前进键触发语音热键，后退键触发 Enter',
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: const Color(0xFF475569),
                ),
              ),
            ],
          ),
        ),
        _StateChip(
          icon: status.running ? Icons.check_circle : Icons.pause_circle,
          label: status.running ? '映射运行中' : '映射未运行',
          good: status.running,
        ),
      ],
    );
  }
}

class _StatusPanel extends StatelessWidget {
  const _StatusPanel({
    required this.config,
    required this.status,
    required this.error,
    required this.onEnabledChanged,
    required this.onOpenPermissionSettings,
  });

  final AppConfig config;
  final NativeStatus status;
  final String? error;
  final ValueChanged<bool> onEnabledChanged;
  final VoidCallback onOpenPermissionSettings;

  @override
  Widget build(BuildContext context) {
    final needsMacPermission =
        Platform.isMacOS && status.permission != 'granted';
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                const Icon(Icons.power_settings_new),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    '全局映射',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
                Switch(value: config.enabled, onChanged: onEnabledChanged),
              ],
            ),
            const SizedBox(height: 10),
            Text(
              status.message,
              style: Theme.of(
                context,
              ).textTheme.bodyMedium?.copyWith(color: const Color(0xFF475569)),
            ),
            if (needsMacPermission) ...<Widget>[
              const SizedBox(height: 12),
              FilledButton.icon(
                onPressed: onOpenPermissionSettings,
                icon: const Icon(Icons.lock_open),
                label: const Text('打开 macOS 权限设置'),
              ),
            ],
            if (error != null) ...<Widget>[
              const SizedBox(height: 12),
              _InlineMessage(icon: Icons.error_outline, text: error!),
            ],
          ],
        ),
      ),
    );
  }
}

class _VoicePanel extends StatelessWidget {
  const _VoicePanel({
    required this.config,
    required this.recording,
    required this.onRecord,
    required this.onClear,
    required this.onModeChanged,
    required this.onTest,
  });

  final AppConfig config;
  final bool recording;
  final VoidCallback onRecord;
  final VoidCallback onClear;
  final ValueChanged<VoiceTriggerMode> onModeChanged;
  final VoidCallback onTest;

  @override
  Widget build(BuildContext context) {
    final binding = config.voiceBinding;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Row(
              children: <Widget>[
                const Icon(Icons.mic),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    '前进键：语音热键',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
                OutlinedButton.icon(
                  onPressed: onRecord,
                  icon: const Icon(Icons.keyboard),
                  label: Text(recording ? '正在录制' : '录制热键'),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: <Widget>[
                _BindingBadge(
                  label: binding?.label ?? '未录制',
                  active: binding != null,
                ),
                if (binding != null)
                  TextButton.icon(
                    onPressed: onClear,
                    icon: const Icon(Icons.close),
                    label: const Text('清除'),
                  ),
                FilledButton.tonalIcon(
                  onPressed: binding == null ? null : onTest,
                  icon: const Icon(Icons.play_arrow),
                  label: const Text('测试'),
                ),
              ],
            ),
            const SizedBox(height: 14),
            SegmentedButton<VoiceTriggerMode>(
              segments: const <ButtonSegment<VoiceTriggerMode>>[
                ButtonSegment<VoiceTriggerMode>(
                  value: VoiceTriggerMode.tap,
                  icon: Icon(Icons.touch_app),
                  label: Text('点击一次'),
                ),
                ButtonSegment<VoiceTriggerMode>(
                  value: VoiceTriggerMode.hold,
                  icon: Icon(Icons.keyboard_tab),
                  label: Text('按住触发'),
                ),
              ],
              selected: <VoiceTriggerMode>{config.voiceMode},
              onSelectionChanged: (selection) => onModeChanged(selection.first),
            ),
            if (recording) ...<Widget>[
              const SizedBox(height: 12),
              const _InlineMessage(
                icon: Icons.radio_button_checked,
                text: '请直接按下目标语音软件的快捷键，例如 Ctrl + Shift + V。',
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _BackButtonPanel extends StatelessWidget {
  const _BackButtonPanel({required this.onTest});

  final VoidCallback onTest;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Row(
          children: <Widget>[
            const Icon(Icons.subdirectory_arrow_left),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text(
                    '后退键：Enter',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  Text(
                    '按下鼠标后退键时发送一次 Enter，并阻止浏览器后退。',
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: const Color(0xFF475569),
                    ),
                  ),
                ],
              ),
            ),
            FilledButton.tonalIcon(
              onPressed: onTest,
              icon: const Icon(Icons.play_arrow),
              label: const Text('测试'),
            ),
          ],
        ),
      ),
    );
  }
}

class _BehaviorPanel extends StatelessWidget {
  const _BehaviorPanel({
    required this.config,
    required this.onInterceptChanged,
    required this.onLaunchAtLoginChanged,
  });

  final AppConfig config;
  final ValueChanged<bool> onInterceptChanged;
  final ValueChanged<bool> onLaunchAtLoginChanged;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          children: <Widget>[
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              secondary: const Icon(Icons.block),
              title: const Text('拦截原始前进/后退行为'),
              subtitle: const Text('开启后，侧键不会再触发浏览器前进或后退。'),
              value: config.interceptOriginal,
              onChanged: onInterceptChanged,
            ),
            const Divider(height: 8),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              secondary: const Icon(Icons.rocket_launch),
              title: const Text('开机自动启动'),
              subtitle: const Text('默认关闭，打开后随系统登录在后台运行。'),
              value: config.launchAtLogin,
              onChanged: onLaunchAtLoginChanged,
            ),
            const Divider(height: 8),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: const Icon(Icons.privacy_tip_outlined),
              title: const Text('隐私说明'),
              subtitle: Text(
                'SideKey 不录音、不识别文字、不联网；只监听鼠标侧键并发送你配置的键盘快捷键。',
                style: Theme.of(context).textTheme.bodyMedium,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _BindingBadge extends StatelessWidget {
  const _BindingBadge({required this.label, required this.active});

  final String label;
  final bool active;

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    return Container(
      height: 38,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: BoxDecoration(
        color: active ? colorScheme.primaryContainer : const Color(0xFFF1F5F9),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(
          color: active ? colorScheme.primary : const Color(0xFFCBD5E1),
        ),
      ),
      alignment: Alignment.center,
      child: Text(
        label,
        style: TextStyle(
          fontWeight: FontWeight.w700,
          color: active ? colorScheme.onPrimaryContainer : Colors.black87,
        ),
      ),
    );
  }
}

class _InlineMessage extends StatelessWidget {
  const _InlineMessage({required this.icon, required this.text});

  final IconData icon;
  final String text;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: const Color(0xFFFFFBEB),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: const Color(0xFFFDE68A)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: <Widget>[
          Icon(icon, size: 18, color: const Color(0xFF92400E)),
          const SizedBox(width: 8),
          Expanded(
            child: Text(text, style: const TextStyle(color: Color(0xFF78350F))),
          ),
        ],
      ),
    );
  }
}

class _StateChip extends StatelessWidget {
  const _StateChip({
    required this.icon,
    required this.label,
    required this.good,
  });

  final IconData icon;
  final String label;
  final bool good;

  @override
  Widget build(BuildContext context) {
    final bg = good ? const Color(0xFFDCFCE7) : const Color(0xFFF1F5F9);
    final fg = good ? const Color(0xFF166534) : const Color(0xFF475569);
    return Container(
      height: 36,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Icon(icon, size: 18, color: fg),
          const SizedBox(width: 6),
          Text(
            label,
            style: TextStyle(color: fg, fontWeight: FontWeight.w700),
          ),
        ],
      ),
    );
  }
}
