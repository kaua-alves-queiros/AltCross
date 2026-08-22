import 'package:altcross_app/models/handoff_zone.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/services/handoff_activation.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter_test/flutter_test.dart';

const _localDisplays = [
  PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
];

void main() {
  test('sem zona habilitada, não chama startHandoff', () async {
    var startCalls = 0;
    final result = await activateHandoff(
      store: HotZoneConfigStore(),
      lookupHost: (_) => '192.168.0.1',
      queryPeerScreens: (_) async => _localDisplays,
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) {
        startCalls++;
        return true;
      },
    );

    expect(startCalls, 0);
    expect(result.started, isFalse);
    expect(result.failureReason, contains('Nenhuma borda configurada'));
  });

  test('zona sem host conhecido é pulada e some da lista de habilitadas',
      () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'nunca-pareado',
      enabled: true,
    ));

    final result = await activateHandoff(
      store: store,
      lookupHost: (_) => null,
      queryPeerScreens: (_) async => _localDisplays,
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) =>
          true,
    );

    expect(result.started, isFalse);
    expect(result.skippedDeviceIds, ['nunca-pareado']);
    expect(result.failureReason, contains('pareado e online'));
  });

  test('zona com host mas peer offline (sem telas) também é pulada',
      () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-offline',
      enabled: true,
    ));

    final result = await activateHandoff(
      store: store,
      lookupHost: (_) => '192.168.0.42',
      queryPeerScreens: (_) async => const [],
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) =>
          true,
    );

    expect(result.started, isFalse);
    expect(result.skippedDeviceIds, ['pc-offline']);
  });

  test('zona válida liga o handoff com a tela real do alvo', () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
    ));

    List<HandoffZoneSpec>? startedZones;
    final result = await activateHandoff(
      store: store,
      lookupHost: (deviceId) =>
          deviceId == 'pc-windows' ? '192.168.0.42' : null,
      queryPeerScreens: (peerHost) async {
        expect(peerHost, '192.168.0.42');
        return const [
          PhysicalDisplay(
              x: 0, y: 0, width: 3440, height: 1440, isPrimary: true),
        ];
      },
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) {
        expect(localScreenWidth, 1920);
        expect(localScreenHeight, 1080);
        startedZones = zones;
        return true;
      },
    );

    expect(result.started, isTrue);
    expect(result.skippedDeviceIds, isEmpty);
    expect(result.failureReason, isNull);
    expect(startedZones, hasLength(1));
    expect(startedZones!.single.targetDeviceId, 'pc-windows');
    expect(startedZones!.single.targetScreenWidth, 3440);
    expect(startedZones!.single.targetScreenHeight, 1440);
  });

  test(
      'alvo com múltiplas telas usa só a tela apontada por targetScreenIndex, '
      'nunca a fusão de todas', () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
      targetScreenIndex: 1,
    ));

    List<HandoffZoneSpec>? startedZones;
    final result = await activateHandoff(
      store: store,
      lookupHost: (deviceId) =>
          deviceId == 'pc-windows' ? '192.168.0.42' : null,
      queryPeerScreens: (peerHost) async => const [
        PhysicalDisplay(
            x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
        PhysicalDisplay(
            x: 1920, y: 0, width: 2560, height: 1440, isPrimary: false),
      ],
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) {
        startedZones = zones;
        return true;
      },
    );

    expect(result.started, isTrue);
    // Se fundisse as 2 telas do alvo, isso daria 4480x1440 (soma das
    // larguras) — o bug que travava o controle local ao trocar de máquina.
    expect(startedZones!.single.targetScreenWidth, 2560);
    expect(startedZones!.single.targetScreenHeight, 1440);
  });

  test('startHandoff recusando (ex.: sem permissão) reporta o motivo',
      () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
    ));

    final result = await activateHandoff(
      store: store,
      lookupHost: (_) => '192.168.0.42',
      queryPeerScreens: (_) async => _localDisplays,
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) =>
          false,
    );

    expect(result.started, isFalse);
    expect(result.failureReason, contains('Acessibilidade'));
  });

  test('zona desabilitada não entra na tentativa de ativação', () async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: false,
    ));

    var lookupCalls = 0;
    final result = await activateHandoff(
      store: store,
      lookupHost: (_) {
        lookupCalls++;
        return '192.168.0.42';
      },
      queryPeerScreens: (_) async => _localDisplays,
      localDisplaysProvider: () => _localDisplays,
      startHandoff: ({
        required localScreenWidth,
        required localScreenHeight,
        required zones,
      }) =>
          true,
    );

    expect(lookupCalls, 0);
    expect(result.started, isFalse);
    expect(result.failureReason, contains('Nenhuma borda configurada'));
  });
}
