import 'package:altcross_app/models/discovered_device.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/screens/connections_screen.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

Future<void> pumpScreen(
  WidgetTester tester,
  HotZoneConfigStore store, {
  Future<List<DiscoveredDevice>> Function({int timeoutMs, int maxResults})?
      discoveryRunner,
}) async {
  await tester.pumpWidget(MaterialApp(
    home: ConnectionsScreen(store: store, discoveryRunner: discoveryRunner),
  ));
}

void main() {
  testWidgets('mostra estado vazio quando não há conexões configuradas',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore(),
        discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => []);

    expect(find.textContaining('Nenhuma conexão configurada'), findsOneWidget);
  });

  testWidgets('mapeia as conexões já configuradas no store', (tester) async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
    ));

    await pumpScreen(tester, store,
        discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => []);

    expect(find.text('pc-windows'), findsOneWidget);
    expect(find.textContaining('Direita'), findsOneWidget);
  });

  testWidgets('buscar dispositivos mostra os resultados da descoberta',
      (tester) async {
    await pumpScreen(
      tester,
      HotZoneConfigStore(),
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => const [
        DiscoveredDevice(
            deviceId: 'abc123',
            name: 'PC do Escritório',
            host: '192.168.0.42',
            port: 45100),
      ],
    );

    await tester.tap(find.byKey(const Key('run-discovery-button')));
    await tester.pumpAndSettle();

    expect(find.text('PC do Escritório'), findsOneWidget);
    expect(find.textContaining('192.168.0.42'), findsOneWidget);
  });

  testWidgets('buscar dispositivos sem achar nada mostra aviso',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore(),
        discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => []);

    await tester.tap(find.byKey(const Key('run-discovery-button')));
    await tester.pumpAndSettle();

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

    await tester.tap(find.byKey(const Key('run-discovery-button')));
    await tester.pumpAndSettle();

    expect(find.textContaining('Falha ao iniciar a busca'), findsOneWidget);
  });
}
