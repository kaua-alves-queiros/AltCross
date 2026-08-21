import 'dart:convert';
import 'dart:io';

import '../models/hot_zone.dart';

/// Persistência em disco das hotzones configuradas pelo usuário (mesma
/// pasta `.altcross` usada pra tema/identidade/pareamento — ver
/// SettingsStore/AltCrossNative). Sem isso, `HotZoneConfigStore` é só
/// memória: toda vez que o app reabre (ou o processo reinicia), as conexões
/// já pareadas e configuradas via Arranjo/Conexões somem.
class HotZoneConfigPersistence {
  static String _filePath() {
    final base = Platform.isWindows
        ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
        : Platform.environment['HOME'] ?? Directory.systemTemp.path;
    final dir = Directory('$base/.altcross');
    if (!dir.existsSync()) {
      dir.createSync(recursive: true);
    }
    return '${dir.path}/hotzones.json';
  }

  /// Lê as hotzones salvas, se houver. Qualquer arquivo ausente, vazio ou
  /// corrompido é tratado como "nenhuma salva ainda" — não trava a
  /// inicialização do app por causa de um arquivo ruim.
  static List<HotZoneConfig> load() {
    final file = File(_filePath());
    if (!file.existsSync()) {
      return const [];
    }
    try {
      final decoded = jsonDecode(file.readAsStringSync()) as List<dynamic>;
      return [
        for (final entry in decoded)
          HotZoneConfig.fromJson(entry as Map<String, Object?>),
      ];
    } catch (_) {
      return const [];
    }
  }

  static void save(List<HotZoneConfig> zones) {
    final encoded = jsonEncode([for (final zone in zones) zone.toJson()]);
    File(_filePath()).writeAsStringSync(encoded);
  }
}
