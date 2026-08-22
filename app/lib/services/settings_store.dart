import 'dart:io';

/// Preferência de aparência escolhida pelo usuário — "system" segue o tema
/// do SO automaticamente (padrão).
enum AppThemePreference { light, dark, system }

typedef LoadThemePreference = AppThemePreference Function();
typedef SaveThemePreference = void Function(AppThemePreference preference);
typedef LoadHandoffEnabled = bool Function();
typedef SaveHandoffEnabled = void Function(bool enabled);

/// Persistência simples (arquivo texto, mesma pasta `.altcross` usada pra
/// identidade/pareamento — ver AltCrossNative) das preferências de UI. Não
/// precisa de Core em C nem FFI: é pura Dart.
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

  /// Lê o arquivo como pares chave=valor (1 por linha). Formato antigo (de
  /// antes de existir mais de uma preferência) era só o nome cru da
  /// preferência de tema, sem "chave=" nenhuma — detecta esse caso e trata
  /// como `theme=<conteúdo>`, senão quem já tinha o arquivo salvo assim
  /// perderia a preferência ao atualizar o app.
  static Map<String, String> _readAll() {
    final file = File(_filePath());
    if (!file.existsSync()) {
      return {};
    }
    final content = file.readAsStringSync().trim();
    if (content.isEmpty) {
      return {};
    }
    if (!content.contains('=')) {
      return {'theme': content};
    }
    final values = <String, String>{};
    for (final line in content.split('\n')) {
      final parts = line.split('=');
      if (parts.length == 2) {
        values[parts[0].trim()] = parts[1].trim();
      }
    }
    return values;
  }

  static void _writeAll(Map<String, String> values) {
    final content = values.entries.map((e) => '${e.key}=${e.value}').join('\n');
    File(_filePath()).writeAsStringSync(content);
  }

  static AppThemePreference loadThemePreference() {
    final raw = _readAll()['theme'];
    if (raw == null) {
      return AppThemePreference.system;
    }
    return AppThemePreference.values.firstWhere(
      (value) => value.name == raw,
      orElse: () => AppThemePreference.system,
    );
  }

  static void saveThemePreference(AppThemePreference preference) {
    final values = _readAll();
    values['theme'] = preference.name;
    _writeAll(values);
  }

  /// Controle entre dispositivos: padrão LIGADO (o app é um KVM, o ponto
  /// dele é já funcionar entre as máquinas sem precisar lembrar de ativar
  /// toda vez que abre) — só volta pra desligado se o usuário desmarcar o
  /// checkbox explicitamente, e isso persiste entre reinícios do app.
  static bool loadHandoffEnabled() {
    final raw = _readAll()['handoffEnabled'];
    if (raw == null) {
      return true;
    }
    return raw == 'true';
  }

  static void saveHandoffEnabled(bool enabled) {
    final values = _readAll();
    values['handoffEnabled'] = enabled.toString();
    _writeAll(values);
  }
}
