import 'package:altcross_app/models/discovered_device.dart';
import 'package:altcross_app/models/handoff_zone.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/pairing.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/native/altcross_native.dart';
import 'package:altcross_app/screens/connections_screen.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

const _device = DiscoveredDevice(
  deviceId: 'abc123',
  name: 'PC do Escritório',
  host: '192.168.0.42',
  port: 45100,
);

const _fakeLocalDisplays = [
  PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
];

Future<void> pumpScreen(
  WidgetTester tester,
  HotZoneConfigStore store, {
  DiscoveryRunner? discoveryRunner,
  SendPairingRequest? sendPairingRequest,
  ConfirmPairing? confirmPairing,
  PollIncomingPairingRequest? pollIncomingPairingRequest,
  PollPairingCompleted? pollPairingCompleted,
  LookupTrustedHost? lookupHost,
  QueryPeerScreens? queryPeerScreens,
  StartHandoff? startHandoff,
  StopHandoff? stopHandoff,
  IsHandoffRemote? isHandoffRemote,
  IsHandoffRemote? isHandoffActive,
}) async {
  await tester.pumpWidget(MaterialApp(
    home: ConnectionsScreen(
      store: store,
      discoveryRunner: discoveryRunner ??
          ({timeoutMs = 0, maxResults = 0}) async => const [],
      sendPairingRequest: sendPairingRequest ??
          ({required peerHost, required myName}) => true,
      confirmPairing: confirmPairing ??
          ({required peerHost, required code, timeoutMs = 0}) async =>
              PairingResult.rejected(),
      // sem override, o timer de polling chamaria o FFI de verdade (não
      // disponível no ambiente de teste) — nenhum teste aqui depende de um
      // pedido chegando, então sempre retorna null por padrão.
      pollIncomingPairingRequest:
          pollIncomingPairingRequest ?? () => null,
      pollPairingCompleted: pollPairingCompleted ?? () => null,
      // idem pro handoff — sem override, ligaria o hook de captura de
      // verdade nesta máquina; nenhum teste aqui deveria depender disso
      // sem dizer explicitamente que espera essa chamada.
      lookupHost: lookupHost ?? (_) => null,
      queryPeerScreens: queryPeerScreens ?? (_) async => const [],
      localDisplaysProvider: () => _fakeLocalDisplays,
      startHandoff: startHandoff ??
          ({required localScreenWidth, required localScreenHeight, required zones}) =>
              false,
      stopHandoff: stopHandoff ?? () {},
      isHandoffRemote: isHandoffRemote ?? () => false,
      isHandoffActive: isHandoffActive ?? () => false,
    ),
  ));
}

Future<void> discoverOneDevice(WidgetTester tester) async {
  await tester.tap(find.byKey(const Key('run-discovery-button')));
  await tester.pumpAndSettle();
}

void main() {
  testWidgets('mostra estado vazio quando não há conexões configuradas',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore());

    expect(find.textContaining('Nenhuma conexão configurada'), findsOneWidget);
  });

  testWidgets('mapeia as conexões já configuradas no store', (tester) async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
    ));

    await pumpScreen(tester, store);

    expect(find.text('pc-windows'), findsOneWidget);
    expect(find.textContaining('Direita'), findsOneWidget);
  });

  testWidgets('buscar dispositivos mostra os resultados da descoberta',
      (tester) async {
    await pumpScreen(
      tester,
      HotZoneConfigStore(),
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        _device,
      ],
    );

    await discoverOneDevice(tester);

    expect(find.text('PC do Escritório'), findsOneWidget);
    expect(find.textContaining('192.168.0.42'), findsOneWidget);
  });

  testWidgets('buscar dispositivos sem achar nada mostra aviso',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore());

    await discoverOneDevice(tester);

    expect(find.textContaining('Nenhum dispositivo encontrado'),
        findsOneWidget);
  });

  testWidgets('erro na busca mostra mensagem de falha', (tester) async {
    await pumpScreen(
      tester,
      HotZoneConfigStore(),
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async {
        throw StateError('Falha ao iniciar a busca na rede local.');
      },
    );

    await discoverOneDevice(tester);

    expect(find.textContaining('Falha ao iniciar a busca'), findsOneWidget);
  });

  testWidgets(
      'clicar em Adicionar não configura nada sozinho — só depois do código certo',
      (tester) async {
    final store = HotZoneConfigStore();
    var requestSent = false;

    await pumpScreen(
      tester,
      store,
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        _device,
      ],
      sendPairingRequest: ({required peerHost, required myName}) {
        requestSent = true;
        return true;
      },
    );
    await discoverOneDevice(tester);

    await tester.tap(find.byKey(const Key('pair-button-abc123')));
    await tester.pumpAndSettle();

    // o pedido foi mandado, mas nada foi configurado ainda — o diálogo de
    // código está esperando o usuário digitar.
    expect(requestSent, isTrue);
    expect(store.zones, isEmpty);
    expect(find.byKey(const Key('pairing-code-field')), findsOneWidget);
  });

  testWidgets('código certo completa o pareamento e configura a conexão',
      (tester) async {
    final store = HotZoneConfigStore();

    await pumpScreen(
      tester,
      store,
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        _device,
      ],
      confirmPairing: ({required peerHost, required code, timeoutMs = 0}) async {
        expect(code, 482913);
        return PairingResult.accepted(deviceId: 'abc123', name: 'PC do Escritório');
      },
    );
    await discoverOneDevice(tester);

    await tester.tap(find.byKey(const Key('pair-button-abc123')));
    await tester.pumpAndSettle();

    await tester.enterText(
        find.byKey(const Key('pairing-code-field')), '482913');
    await tester.tap(find.text('Confirmar'));
    await tester.pumpAndSettle();

    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'abc123');
    expect(find.textContaining('Pareado com'), findsOneWidget);
  });

  testWidgets('código errado não configura nada e mostra aviso',
      (tester) async {
    final store = HotZoneConfigStore();

    await pumpScreen(
      tester,
      store,
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        _device,
      ],
      confirmPairing: ({required peerHost, required code, timeoutMs = 0}) async =>
          PairingResult.rejected(),
    );
    await discoverOneDevice(tester);

    await tester.tap(find.byKey(const Key('pair-button-abc123')));
    await tester.pumpAndSettle();

    await tester.enterText(
        find.byKey(const Key('pairing-code-field')), '111111');
    await tester.tap(find.text('Confirmar'));
    await tester.pumpAndSettle();

    expect(store.zones, isEmpty);
    expect(find.textContaining('Código incorreto'), findsOneWidget);
  });

  testWidgets('pedido de pareamento recebido mostra o código na tela',
      (tester) async {
    var polled = false;
    await pumpScreen(
      tester,
      HotZoneConfigStore(),
      pollIncomingPairingRequest: () {
        if (polled) return null;
        polled = true;
        return const IncomingPairingRequest(
          requesterDeviceId: 'device-remote',
          requesterName: 'PC Remoto',
          code: 123456,
        );
      },
    );

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();

    expect(find.text('Pedido de pareamento'), findsOneWidget);
    expect(find.byKey(const Key('incoming-pairing-code')), findsOneWidget);
    expect(find.textContaining('123456'), findsOneWidget);
  });

  testWidgets(
      'quem está sendo adicionado também vê que o pareamento deu certo',
      (tester) async {
    final store = HotZoneConfigStore();
    var incomingPolls = 0;
    var completedPolls = 0;

    await pumpScreen(
      tester,
      store,
      pollIncomingPairingRequest: () {
        incomingPolls++;
        if (incomingPolls != 1) return null;
        return const IncomingPairingRequest(
          requesterDeviceId: 'device-remote',
          requesterName: 'PC Remoto',
          code: 123456,
        );
      },
      pollPairingCompleted: () {
        completedPolls++;
        // só reporta pronto num tick DEPOIS do pedido ter chegado — reflete
        // a ordem real: o outro lado só confirma depois de ver o código na
        // tela, nunca no mesmo instante em que o pedido aparece.
        if (completedPolls != 2) return null;
        return const PairingCompleted(
          peerDeviceId: 'device-remote',
          peerName: 'PC Remoto',
        );
      },
    );

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();
    expect(find.text('Pedido de pareamento'), findsOneWidget);

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();

    // o diálogo de código fecha sozinho e a conexão aparece do lado de quem
    // foi adicionado também — não só do lado de quem pediu.
    expect(find.text('Pedido de pareamento'), findsNothing);
    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'device-remote');
    expect(find.textContaining('Pareado com PC Remoto'), findsOneWidget);
  });

  group('handoff', () {
    testWidgets('sem conexão configurada, ativar mostra aviso e não liga nada',
        (tester) async {
      var startCalls = 0;
      await pumpScreen(
        tester,
        HotZoneConfigStore(),
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) {
          startCalls++;
          return true;
        },
      );

      await tester.tap(find.byKey(const Key('toggle-handoff-button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('confirm-activate-handoff-button')));
      await tester.pumpAndSettle();

      expect(startCalls, 0);
      expect(find.textContaining('configure conexões no Arranjo'),
          findsOneWidget);
    });

    testWidgets(
        'ativar busca a tela real do alvo e liga o handoff com ela, não um tamanho chutado',
        (tester) async {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-windows',
        enabled: true,
      ));

      Map<String, Object?>? started;
      await pumpScreen(
        tester,
        store,
        lookupHost: (deviceId) =>
            deviceId == 'pc-windows' ? '192.168.0.42' : null,
        queryPeerScreens: (peerHost) async {
          expect(peerHost, '192.168.0.42');
          return const [
            PhysicalDisplay(
                x: 0, y: 0, width: 3440, height: 1440, isPrimary: true),
          ];
        },
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) {
          started = {
            'localScreenWidth': localScreenWidth,
            'localScreenHeight': localScreenHeight,
            'zones': zones,
          };
          return true;
        },
      );

      await tester.tap(find.byKey(const Key('toggle-handoff-button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('confirm-activate-handoff-button')));
      await tester.pumpAndSettle();

      expect(started, isNotNull);
      expect(started!['localScreenWidth'], 1920);
      expect(started!['localScreenHeight'], 1080);
      final zones = started!['zones'] as List<HandoffZoneSpec>;
      expect(zones, hasLength(1));
      expect(zones.single.targetDeviceId, 'pc-windows');
      expect(zones.single.targetScreenWidth, 3440);
      expect(zones.single.targetScreenHeight, 1440);
      expect(find.textContaining('ativado'), findsWidgets);
    });

    testWidgets('dispositivo offline é pulado, mas outros ativam mesmo assim',
        (tester) async {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-offline',
        enabled: true,
      ));

      await pumpScreen(
        tester,
        store,
        lookupHost: (deviceId) => null, // nunca pareado / sem host conhecido
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) =>
            true,
      );

      await tester.tap(find.byKey(const Key('toggle-handoff-button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('confirm-activate-handoff-button')));
      await tester.pumpAndSettle();

      expect(find.textContaining('Nenhum dos dispositivos configurados'),
          findsOneWidget);
    });

    testWidgets('desativar chama stopHandoff e volta o botão pra Ativar',
        (tester) async {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-windows',
        enabled: true,
      ));
      var stopped = false;

      await pumpScreen(
        tester,
        store,
        lookupHost: (_) => '192.168.0.42',
        queryPeerScreens: (_) async => const [
          PhysicalDisplay(
              x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
        ],
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) =>
            true,
        stopHandoff: () => stopped = true,
      );

      await tester.tap(find.byKey(const Key('toggle-handoff-button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('confirm-activate-handoff-button')));
      await tester.pumpAndSettle();

      expect(find.text('Desativar'), findsOneWidget);

      await tester.tap(find.byKey(const Key('toggle-handoff-button')));
      await tester.pumpAndSettle();

      expect(stopped, isTrue);
      expect(find.text('Ativar'), findsOneWidget);
    });

    testWidgets('reabrir a tela com handoff já ativo mostra o estado certo',
        (tester) async {
      await pumpScreen(
        tester,
        HotZoneConfigStore(),
        isHandoffActive: () => true,
      );
      await tester.pumpAndSettle();

      expect(find.text('Desativar'), findsOneWidget);
      expect(find.textContaining('ativado'), findsWidgets);
    });
  });
}
