import 'package:altcross_app/models/discovered_device.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/pairing.dart';
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

Future<void> pumpScreen(
  WidgetTester tester,
  HotZoneConfigStore store, {
  DiscoveryRunner? discoveryRunner,
  SendPairingRequest? sendPairingRequest,
  ConfirmPairing? confirmPairing,
  PollIncomingPairingRequest? pollIncomingPairingRequest,
  PollPairingCompleted? pollPairingCompleted,
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
}
