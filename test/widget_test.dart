import 'package:flutter_test/flutter_test.dart';
import 'package:sidekey/main.dart';

void main() {
  test('default config maps forward to voice hotkey and back to Enter', () {
    final native = AppConfig.defaults.toNativeJson();

    expect(native['enabled'], true);
    expect(native['voiceMode'], VoiceTriggerMode.tap.value);
    expect(native['voiceBinding'], isNull);
    expect((native['backBinding']! as Map<String, Object?>)['label'], 'Enter');
    expect(
      ((native['backBinding']! as Map<String, Object?>)['strokes']! as List)
          .length,
      1,
    );
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
    expect(restored?.strokes, hasLength(1));
    expect(restored?.strokes.first.modifiers, <String>['ctrl', 'shift']);
  });

  test('key binding supports modifier-only sequences', () {
    final binding = KeyBinding.sequence(const <KeyStroke>[
      KeyStroke(label: 'Left Control', windowsVk: 0xA2, macosKeyCode: 59),
      KeyStroke(label: 'Left Control', windowsVk: 0xA2, macosKeyCode: 59),
    ]);

    final json = binding.toJson();
    final restored = KeyBinding.fromJson(json);

    expect(binding.label, 'Left Control x2');
    expect((json['strokes']! as List), hasLength(2));
    expect(restored?.label, 'Left Control x2');
    expect(restored?.strokes, hasLength(2));
    expect(restored?.strokes.first.windowsVk, 0xA2);
    expect(restored?.strokes.first.macosKeyCode, 59);
  });

  test('key binding supports modifier-only chords', () {
    final binding = KeyBinding.sequence(const <KeyStroke>[
      KeyStroke(
        label: 'Left Control + Left Option',
        windowsVk: 0xA4,
        macosKeyCode: 58,
        modifiers: <String>['ctrlLeft'],
      ),
    ]);

    final stroke =
        (binding.toNativeJson()['strokes']! as List).single
            as Map<String, Object?>;

    expect(binding.label, 'Left Control + Left Option');
    expect(stroke['windowsVk'], 0xA4);
    expect(stroke['macosKeyCode'], 58);
    expect(stroke['modifiers'], <String>['ctrlLeft']);
  });

  test('key binding supports macOS Fn as a single stroke', () {
    final binding = KeyBinding.sequence(const <KeyStroke>[
      KeyStroke(label: 'Fn', windowsVk: 0, macosKeyCode: 63),
    ]);

    final native = binding.toNativeJson();
    final strokes = native['strokes']! as List;
    final fn = strokes.single as Map<String, Object?>;

    expect(binding.label, 'Fn');
    expect(fn['windowsVk'], 0);
    expect(fn['macosKeyCode'], 63);
  });
}
