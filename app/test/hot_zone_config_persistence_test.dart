import 'dart:io';

import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/services/hot_zone_config_persistence.dart';
import 'package:flutter_test/flutter_test.dart';

String _hotZonesFilePath() {
  final base = Platform.isWindows
      ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
      : Platform.environment['HOME'] ?? Directory.systemTemp.path;
  return '$base/.altcross/hotzones.json';
}

void main() {
  setUp(() {
    final file = File(_hotZonesFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  tearDown(() {
    final file = File(_hotZonesFilePath());
    if (file.existsSync()) {
      file.deleteSync();
    }
  });

  test('load() retorna lista vazia quando nunca foi salvo nada', () {
    expect(HotZoneConfigPersistence.load(), isEmpty);
  });

  test('save() e load() fazem round-trip das zonas configuradas', () {
    const zones = [
      HotZoneConfig(
          edge: HotZoneEdge.right, targetDeviceId: 'pc-2', enabled: true),
      HotZoneConfig(
          edge: HotZoneEdge.top, targetDeviceId: 'pc-3', enabled: false),
    ];

    HotZoneConfigPersistence.save(zones);

    expect(HotZoneConfigPersistence.load(), zones);
  });

  test('save() sobrescreve o conteúdo salvo antes', () {
    HotZoneConfigPersistence.save(const [
      HotZoneConfig(
          edge: HotZoneEdge.right, targetDeviceId: 'pc-2', enabled: true),
    ]);
    HotZoneConfigPersistence.save(const []);

    expect(HotZoneConfigPersistence.load(), isEmpty);
  });

  test('conteúdo inválido no arquivo cai de volta pra lista vazia', () {
    final file = File(_hotZonesFilePath());
    file.parent.createSync(recursive: true);
    file.writeAsStringSync('lixo-invalido');

    expect(HotZoneConfigPersistence.load(), isEmpty);
  });
}
