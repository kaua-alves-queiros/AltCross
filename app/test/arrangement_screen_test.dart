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

void main() {
  testWidgets('mostra as telas físicas reais retornadas pelo Core',
      (tester) async {
    await pumpScreen(tester, HotZoneConfigStore());

    expect(find.textContaining('1470×956'), findsOneWidget);
    expect(find.textContaining('1920×1080'), findsOneWidget);
    expect(find.textContaining('principal'), findsOneWidget);
  });

  testWidgets('adicionar dispositivo gruda na borda direita por padrão',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);

    await tester.tap(find.byKey(const Key('add-device-button')));
    await tester.pumpAndSettle();
    await tester.enterText(
        find.byKey(const Key('device-id-field')), 'pc-windows');
    await tester.tap(find.text('Adicionar'));
    await tester.pumpAndSettle();

    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'pc-windows');
    expect(find.byKey(const Key('device-box-pc-windows')), findsOneWidget);
  });

  testWidgets(
      'segundo dispositivo na mesma borda não comita e mostra aviso',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);

    for (final id in ['pc-windows', 'pc-linux']) {
      await tester.tap(find.byKey(const Key('add-device-button')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byKey(const Key('device-id-field')), id);
      await tester.tap(find.text('Adicionar'));
      await tester.pumpAndSettle();
    }

    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'pc-windows');
    expect(find.textContaining('Já existe uma hotzone'), findsOneWidget);
    // a caixa do segundo dispositivo continua no canvas, só não comitada
    expect(find.byKey(const Key('device-box-pc-linux')), findsOneWidget);
  });

  testWidgets('remover dispositivo tira do canvas e do store',
      (tester) async {
    final store = HotZoneConfigStore();
    await pumpScreen(tester, store);

    await tester.tap(find.byKey(const Key('add-device-button')));
    await tester.pumpAndSettle();
    await tester.enterText(
        find.byKey(const Key('device-id-field')), 'pc-windows');
    await tester.tap(find.text('Adicionar'));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('remove-device-pc-windows')));
    await tester.pumpAndSettle();

    expect(store.zones, isEmpty);
    expect(find.byKey(const Key('device-box-pc-windows')), findsNothing);
  });
}
