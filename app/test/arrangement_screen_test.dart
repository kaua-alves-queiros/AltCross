import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/screens/arrangement_screen.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

const _fakeDisplays = [
  PhysicalDisplay(x: 0, y: 0, width: 1470, height: 956, isPrimary: true),
];

Future<void> pumpScreen(WidgetTester tester, HotZoneConfigStore store) async {
  await tester.pumpWidget(MaterialApp(
    home: ArrangementScreen(
      store: store,
      displayProvider: () => _fakeDisplays,
    ),
  ));
}

Future<void> addDevice(WidgetTester tester, String deviceId) async {
  await tester.tap(find.byKey(const Key('add-device-button')));
  await tester.pumpAndSettle();
  await tester.enterText(find.byKey(const Key('device-id-field')), deviceId);
  await tester.tap(find.text('Adicionar'));
  await tester.pumpAndSettle();
}

Key _localEdgeKey(HotZoneEdge edge) =>
    Key('local-box-1470×956\nprincipal-edge-${edge.name}');

Key _deviceEdgeKey(String deviceId, HotZoneEdge edge) =>
    Key('device-box-$deviceId-edge-${edge.name}');

void main() {
  testWidgets('mostra a tela física real retornada pelo Core', (tester) async {
    await pumpScreen(tester, HotZoneConfigStore());

    expect(find.textContaining('1470×956'), findsOneWidget);
    expect(find.textContaining('principal'), findsOneWidget);
  });

  testWidgets(
      'adicionar dispositivo fica flutuando, sem conectar em nada sozinho',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);

    await addDevice(tester, 'pc-windows');

    expect(store.zones, isEmpty);
    expect(find.byKey(const Key('device-box-pc-windows')), findsOneWidget);
    expect(find.textContaining('toque numa borda'), findsOneWidget);
  });

  testWidgets(
      'tocar na borda direita local e depois na esquerda do dispositivo conecta de verdade',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);
    await addDevice(tester, 'pc-windows');

    await tester.tap(find.byKey(_localEdgeKey(HotZoneEdge.right)));
    await tester.pump();
    await tester.tap(find.byKey(_deviceEdgeKey('pc-windows', HotZoneEdge.left)));
    await tester.pumpAndSettle();

    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'pc-windows');
    expect(store.zones.single.edge, HotZoneEdge.right);
  });

  testWidgets('tocar na mesma borda 2 vezes cancela a seleção',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);
    await addDevice(tester, 'pc-windows');

    await tester.tap(find.byKey(_localEdgeKey(HotZoneEdge.right)));
    await tester.pump();
    await tester.tap(find.byKey(_localEdgeKey(HotZoneEdge.right)));
    await tester.pump();
    await tester.tap(find.byKey(_deviceEdgeKey('pc-windows', HotZoneEdge.left)));
    await tester.pumpAndSettle();

    // a seleção foi cancelada, então esse segundo toque virou uma NOVA
    // seleção pendente (do lado do dispositivo), não uma conexão.
    expect(store.zones, isEmpty);
  });

  testWidgets('conectar 2 dispositivos remotos entre si mostra erro',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);
    await addDevice(tester, 'pc-a');
    await addDevice(tester, 'pc-b');

    await tester.tap(find.byKey(_deviceEdgeKey('pc-a', HotZoneEdge.right)));
    await tester.pump();
    await tester.tap(find.byKey(_deviceEdgeKey('pc-b', HotZoneEdge.left)));
    await tester.pumpAndSettle();

    expect(store.zones, isEmpty);
    expect(find.textContaining('não dois dispositivos remotos'),
        findsOneWidget);
  });

  testWidgets('remover dispositivo tira do canvas e do store',
      (tester) async {
    final store = HotZoneConfigStore();
    store.add(const HotZoneConfig(
      edge: HotZoneEdge.right,
      targetDeviceId: 'pc-windows',
      enabled: true,
    ));
    await pumpScreen(tester, store);

    expect(find.byKey(const Key('device-box-pc-windows')), findsOneWidget);

    await tester.tap(find.byKey(const Key('remove-device-pc-windows')));
    await tester.pumpAndSettle();

    expect(store.zones, isEmpty);
    expect(find.byKey(const Key('device-box-pc-windows')), findsNothing);
  });
}
