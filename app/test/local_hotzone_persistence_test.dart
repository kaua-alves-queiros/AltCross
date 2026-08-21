import 'dart:io';

import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/local_hotzone.dart';
import 'package:altcross_app/services/local_hotzone_persistence.dart';
import 'package:flutter_test/flutter_test.dart';

String _localHotZonesFilePath() {
  final base = Platform.isWindows
      ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
      : Platform.environment['HOME'] ?? Directory.systemTemp.path;
  return '$base/.altcross/local_hotzones.json';
}

void main() {
  setUp(() {
    final file = File(_localHotZonesFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  tearDown(() {
    final file = File(_localHotZonesFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  test('load() retorna lista vazia quando nunca foi salvo nada', () {
    expect(LocalHotZonePersistence.load(), isEmpty);
  });

  test('save() e load() fazem round-trip dos mapeamentos', () {
    const mappings = [
      LocalHotZoneMapping(
        sourceDisplayId: 1,
        sourceEdge: HotZoneEdge.right,
        targetDisplayId: 2,
        targetEdge: HotZoneEdge.left,
      ),
      LocalHotZoneMapping(
        sourceDisplayId: 2,
        sourceEdge: HotZoneEdge.left,
        targetDisplayId: 1,
        targetEdge: HotZoneEdge.right,
      ),
    ];

    LocalHotZonePersistence.save(mappings);

    expect(LocalHotZonePersistence.load(), mappings);
  });

  test('conteúdo inválido no arquivo cai de volta pra lista vazia', () {
    final file = File(_localHotZonesFilePath());
    file.parent.createSync(recursive: true);
    file.writeAsStringSync('lixo-invalido');

    expect(LocalHotZonePersistence.load(), isEmpty);
  });
}
