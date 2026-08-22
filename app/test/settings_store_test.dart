import 'dart:io';

import 'package:altcross_app/services/settings_store.dart';
import 'package:flutter_test/flutter_test.dart';

String _settingsFilePath() {
  final base = Platform.isWindows
      ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
      : Platform.environment['HOME'] ?? Directory.systemTemp.path;
  return '$base/.altcross/settings.txt';
}

void main() {
  setUp(() {
    final file = File(_settingsFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  tearDown(() {
    final file = File(_settingsFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  test('retorna "system" quando nunca foi salvo nada', () {
    expect(SettingsStore.loadThemePreference(), AppThemePreference.system);
  });

  test('salva e recarrega cada preferência corretamente', () {
    for (final preference in AppThemePreference.values) {
      SettingsStore.saveThemePreference(preference);
      expect(SettingsStore.loadThemePreference(), preference);
    }
  });

  test('conteúdo inválido no arquivo cai de volta pra "system"', () {
    final file = File(_settingsFilePath());
    file.parent.createSync(recursive: true);
    file.writeAsStringSync('lixo-invalido');

    expect(SettingsStore.loadThemePreference(), AppThemePreference.system);
  });

  test('controle entre dispositivos vem LIGADO por padrão quando nunca foi salvo',
      () {
    expect(SettingsStore.loadHandoffEnabled(), isTrue);
  });

  test('desligar o controle entre dispositivos persiste entre reinícios', () {
    SettingsStore.saveHandoffEnabled(false);
    expect(SettingsStore.loadHandoffEnabled(), isFalse);

    SettingsStore.saveHandoffEnabled(true);
    expect(SettingsStore.loadHandoffEnabled(), isTrue);
  });

  test('tema e controle entre dispositivos persistem juntos sem se atropelar',
      () {
    SettingsStore.saveThemePreference(AppThemePreference.dark);
    SettingsStore.saveHandoffEnabled(false);

    expect(SettingsStore.loadThemePreference(), AppThemePreference.dark);
    expect(SettingsStore.loadHandoffEnabled(), isFalse);
  });
}
