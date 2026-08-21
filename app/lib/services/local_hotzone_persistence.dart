import 'dart:convert';
import 'dart:io';

import '../models/local_hotzone.dart';

/// Persistência em disco dos saltos de cursor entre telas locais (mesma
/// pasta `.altcross` das outras — ver `HotZoneConfigPersistence`/
/// `SettingsStore`).
class LocalHotZonePersistence {
  static String _filePath() {
    final base = Platform.isWindows
        ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
        : Platform.environment['HOME'] ?? Directory.systemTemp.path;
    final dir = Directory('$base/.altcross');
    if (!dir.existsSync()) {
      dir.createSync(recursive: true);
    }
    return '${dir.path}/local_hotzones.json';
  }

  static List<LocalHotZoneMapping> load() {
    final file = File(_filePath());
    if (!file.existsSync()) {
      return const [];
    }
    try {
      final decoded = jsonDecode(file.readAsStringSync()) as List<dynamic>;
      return [
        for (final entry in decoded)
          LocalHotZoneMapping.fromJson(entry as Map<String, Object?>),
      ];
    } catch (_) {
      return const [];
    }
  }

  static void save(List<LocalHotZoneMapping> mappings) {
    final encoded = jsonEncode([for (final m in mappings) m.toJson()]);
    File(_filePath()).writeAsStringSync(encoded);
  }
}
