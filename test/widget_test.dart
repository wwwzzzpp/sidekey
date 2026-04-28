import 'package:flutter_test/flutter_test.dart';
import 'package:sidekey/main.dart';

void main() {
  test('default config maps forward to voice hotkey and back to Enter', () {
    final native = AppConfig.defaults.toNativeJson();

    expect(native['enabled'], true);
    expect(native['voiceMode'], VoiceTriggerMode.tap.value);
    expect(native['voiceBinding'], isNull);
    expect((native['backBinding']! as Map<String, Object?>)['label'], 'Enter');
  });

  test('key binding round trips through json', () {
    const binding = KeyBinding(
      label: 'Ctrl + Shift + V',
      windowsVk: 0x56,
      macosKeyCode: 9,
      modifiers: <String>['ctrl', 'shift'],
    );

    final restored = KeyBinding.fromJson(binding.toJson());

    expect(restored?.label, binding.label);
    expect(restored?.windowsVk, binding.windowsVk);
    expect(restored?.macosKeyCode, binding.macosKeyCode);
    expect(restored?.modifiers, binding.modifiers);
  });
}
