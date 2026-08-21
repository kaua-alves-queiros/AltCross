import 'dart:io';

/// Preferência de aparência escolhida pelo usuário — "system" segue o tema
/// do SO automaticamente (padrão).
enum AppThemePreference { light, dark, system }

typedef LoadThemePreference = AppThemePreference Function();
typedef SaveThemePreference = void Function(AppThemePreference preference);

/// Persistência simples (arquivo texto, mesma pasta `.altcross` usada pra
/// identidade/pareamento — ver AltCrossNative) da preferência de tema. Não
/// precisa de Core em C nem FFI: é só uma preferência de UI, pura Dart.
class SettingsStore {
  static String _filePath() {
    final base = Platform.isWindows
        ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
        : Platform.environment['HOME'] ?? Directory.systemTemp.path;
    final dir = Directory('$base/.altcross');
    if (!dir.existsSync()) {
      dir.createSync(recursive: true);
    }
    return '${dir.path}/settings.txt';
  }

  static AppThemePreference loadThemePreference() {
    final file = File(_filePath());
    if (!file.existsSync()) {
      return AppThemePreference.system;
    }
    final content = file.readAsStringSync().trim();
    return AppThemePreference.values.firstWhere(
      (value) => value.name == content,
      orElse: () => AppThemePreference.system,
    );
  }

  static void saveThemePreference(AppThemePreference preference) {
    File(_filePath()).writeAsStringSync(preference.name);
  }
}
