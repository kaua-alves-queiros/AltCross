import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/screens/arrangement_screen.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

const _fakeDisplays = [
  PhysicalDisplay(x: 0, y: 0, width: 1470, height: 956, isPrimary: true),
  PhysicalDisplay(x: 1470, y: 0, width: 1920, height: 1080, isPrimary: false),
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

void main() {
  testWidgets('mostra as telas físicas reais retornadas pelo Core',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore());

    expect(find.textContaining('1470×956'), findsOneWidget);
    expect(find.textContaining('1920×1080'), findsOneWidget);
    expect(find.textContaining('principal'), findsOneWidget);
  });

  testWidgets(
      'adicionar dispositivo fica flutuando, sem grudar em nenhuma borda sozinho',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);

    await addDevice(tester, 'pc-windows');

    // nada é decidido sozinho — precisa arrastar até encostar numa borda.
    expect(store.zones, isEmpty);
    expect(find.byKey(const Key('device-box-pc-windows')), findsOneWidget);
    expect(find.textContaining('arraste até encostar'), findsOneWidget);
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
