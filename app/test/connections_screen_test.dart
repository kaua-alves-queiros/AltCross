import 'package:altcross_app/models/discovered_device.dart';
import 'package:altcross_app/models/handoff_zone.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/pairing.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/native/altcross_native.dart';
import 'package:altcross_app/screens/connections_screen.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
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
  PushZoneToPeer? pushZoneToPeer,
  StartHandoff? startHandoff,
  StopHandoff? stopHandoff,
  IsHandoffRemote? isHandoffRemote,
  IsHandoffRemote? isHandoffActive,
  VoidCallback? restartConnectionMonitor,
  // false por padrão: a maioria dos testes aqui não é sobre o controle
  // entre dispositivos, e com ele ligado por padrão a ativação automática
  // dispararia sozinha (ver ConnectionsScreen.initState) e atropelaria
  // mensagens que esses testes checam (ex.: "Pareado com sucesso"). Os
  // testes do grupo "handoff" ligam explicitamente.
  bool handoffEnabled = false,
  ValueChanged<bool>? onHandoffEnabledChanged,
}) async {
  await tester.pumpWidget(MaterialApp(
    home: ConnectionsScreen(
      store: store,
      restartConnectionMonitor: restartConnectionMonitor ?? () {},
      handoffEnabled: handoffEnabled,
      onHandoffEnabledChanged: onHandoffEnabledChanged ?? (_) {},
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
      pushZoneToPeer: pushZoneToPeer ??
          ({
            required peerHost,
            required myName,
            required myEdge,
            required targetScreenIndex,
          }) =>
              true,
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
  // Sem um handler mockado, `Clipboard.getData` no ambiente de teste fica
  // esperando uma resposta que nunca chega (não há handler de verdade
  // registrado pro canal de plataforma) — trava o teste em vez de falhar.
  String? clipboardText;
  setUp(() {
    clipboardText = null;
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(SystemChannels.platform, (call) async {
      if (call.method == 'Clipboard.setData') {
        clipboardText = (call.arguments as Map)['text'] as String?;
        return null;
      }
      if (call.method == 'Clipboard.getData') {
        return {'text': clipboardText};
      }
      return null;
    });
  });

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

  testWidgets(
      'código certo completa o pareamento, configura a conexão e avisa o'
      ' outro PC pela rede', (tester) async {
    final store = HotZoneConfigStore();
    Map<String, Object?>? pushed;
    var monitorRestarts = 0;

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
      pushZoneToPeer: ({
        required peerHost,
        required myName,
        required myEdge,
        required targetScreenIndex,
      }) {
        pushed = {
          'peerHost': peerHost,
          'myEdge': myEdge,
          'targetScreenIndex': targetScreenIndex,
        };
        return true;
      },
      restartConnectionMonitor: () => monitorRestarts++,
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

    // Sem isso, o monitor de heartbeat nunca sabe desse dispositivo novo (só
    // lê o cadastro do disco 1 vez, na inicialização) e nenhuma notificação
    // de conexão/desconexão aparece pra ele depois.
    expect(monitorRestarts, 1);

    // Sem isso, o outro PC nunca fica sabendo que essa borda foi conectada
    // nele — o mapeamento ficava só configurado deste lado.
    expect(pushed, isNotNull);
    expect(pushed!['peerHost'], '192.168.0.42');
    expect(pushed!['myEdge'], HotZoneEdge.right);
  });

  testWidgets(
      'pareamento com borda já ocupada avisa o usuário em vez de fingir'
      ' sucesso', (tester) async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'ja-pareado',
      enabled: true,
    ));
    var pushCalls = 0;

    await pumpScreen(
      tester,
      store,
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        _device,
      ],
      confirmPairing: ({required peerHost, required code, timeoutMs = 0}) async =>
          PairingResult.accepted(deviceId: 'abc123', name: 'PC do Escritório'),
      pushZoneToPeer: ({
        required peerHost,
        required myName,
        required myEdge,
        required targetScreenIndex,
      }) {
        pushCalls++;
        return true;
      },
    );
    await discoverOneDevice(tester);

    await tester.tap(find.byKey(const Key('pair-button-abc123')));
    await tester.pumpAndSettle();

    await tester.enterText(
        find.byKey(const Key('pairing-code-field')), '482913');
    await tester.tap(find.text('Confirmar'));
    await tester.pumpAndSettle();

    // continua só com a zona antiga — não cria uma segunda igual sozinho.
    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'ja-pareado');
    expect(pushCalls, 0);
    expect(find.textContaining('borda direita já está em uso'),
        findsOneWidget);
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

    await tester.tap(find.byKey(const Key('copy-pairing-code-button')));
    await tester.pumpAndSettle();

    final clipboard = await Clipboard.getData(Clipboard.kTextPlain);
    expect(clipboard?.text, '123456');
    expect(find.text('Código copiado.'), findsOneWidget);
  });

  testWidgets(
      'quem está sendo adicionado também vê que o pareamento deu certo',
      (tester) async {
    final store = HotZoneConfigStore();
    var incomingPolls = 0;
    var completedPolls = 0;
    var monitorRestarts = 0;

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
      restartConnectionMonitor: () => monitorRestarts++,
    );

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();
    expect(find.text('Pedido de pareamento'), findsOneWidget);

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();

    // o diálogo de código fecha sozinho e a mensagem de sucesso aparece do
    // lado de quem foi adicionado também — não só do lado de quem pediu.
    // A hotzone em si NÃO é criada aqui: quem decide a borda de verdade é
    // quem iniciou o pareamento (`_startPairing`), e ela chega pra este
    // lado pela rede (`pushZoneToPeer` → `_pollIncomingZone`, em
    // main.dart), já na borda oposta certa — fora do escopo desta tela.
    expect(find.text('Pedido de pareamento'), findsNothing);
    expect(store.zones, isEmpty);
    expect(find.textContaining('Pareado com PC Remoto'), findsOneWidget);
    expect(monitorRestarts, 1);
  });

  group('handoff', () {
    testWidgets(
        'checkbox vem marcado por padrão e tenta ativar sozinho ao abrir a'
        ' tela', (tester) async {
      var startCalls = 0;
      await pumpScreen(
        tester,
        HotZoneConfigStore(),
        handoffEnabled: true,
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) {
          startCalls++;
          return true;
        },
      );
      await tester.pumpAndSettle();

      final checkbox =
          tester.widget<Checkbox>(find.byKey(const Key('handoff-enabled-checkbox')));
      expect(checkbox.value, isTrue);
      // Sem borda configurada, a tentativa automática não chama startHandoff
      // (não tem zona pra montar) — só avisa o motivo.
      expect(startCalls, 0);
      expect(find.textContaining('configure conexões no Arranjo'),
          findsOneWidget);
    });

    testWidgets(
        'ativação automática busca a tela real do alvo e liga o handoff com'
        ' ela, não um tamanho chutado', (tester) async {
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
        handoffEnabled: true,
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
        handoffEnabled: true,
        lookupHost: (deviceId) => null, // nunca pareado / sem host conhecido
        startHandoff: ({
          required localScreenWidth,
          required localScreenHeight,
          required zones,
        }) =>
            true,
      );
      await tester.pumpAndSettle();

      expect(find.textContaining('Nenhum dos dispositivos configurados'),
          findsOneWidget);
    });

    testWidgets('desmarcar o checkbox chama stopHandoff', (tester) async {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-windows',
        enabled: true,
      ));
      var stopped = false;
      bool? persisted;

      await pumpScreen(
        tester,
        store,
        handoffEnabled: true,
        onHandoffEnabledChanged: (value) => persisted = value,
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
      await tester.pumpAndSettle();

      expect(find.textContaining('Controle entre dispositivos ativado.'),
          findsWidgets);

      await tester.tap(find.byKey(const Key('handoff-enabled-checkbox')));
      await tester.pumpAndSettle();

      expect(stopped, isTrue);
      expect(persisted, isFalse);
      expect(find.textContaining('Controle entre dispositivos desativado.'),
          findsWidgets);
      final checkbox =
          tester.widget<Checkbox>(find.byKey(const Key('handoff-enabled-checkbox')));
      expect(checkbox.value, isFalse);
    });

    testWidgets('reabrir a tela com handoff já ativo mostra o estado certo',
        (tester) async {
      await pumpScreen(
        tester,
        HotZoneConfigStore(),
        handoffEnabled: true,
        isHandoffActive: () => true,
      );
      await tester.pumpAndSettle();

      final checkbox =
          tester.widget<Checkbox>(find.byKey(const Key('handoff-enabled-checkbox')));
      expect(checkbox.value, isTrue);
      expect(find.textContaining('ativado'), findsWidgets);
    });
  });
}
