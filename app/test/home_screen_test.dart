import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/screens/home_screen.dart';
import 'package:altcross_app/services/settings_store.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:altcross_app/state/local_hotzone_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

Future<void> pumpHome(WidgetTester tester, HotZoneConfigStore store) async {
  await tester.pumpWidget(MaterialApp(
    home: HomeScreen(
      store: store,
      localHotZoneStore: LocalHotZoneStore(),
      currentThemePreference: AppThemePreference.system,
      onThemePreferenceChanged: (_) {},
      displayProvider: () => const [
        PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
      ],
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => [],
      // sem isso, o timer de polling da tela de Conexões (e o próprio
      // initState dela, que já checa isHandoffActive) chamaria o FFI de
      // verdade (não disponível no ambiente de teste) assim que navegada.
      pollIncomingPairingRequest: () => null,
      isHandoffActive: () => false,
      // idem: controle entre dispositivos vem ligado por padrão, e sem
      // desligar aqui a ativação automática tentaria mais FFI de verdade
      // assim que a tela de Conexões abrisse.
      handoffEnabled: false,
    ),
  ));
}

void main() {
  testWidgets('mostra os módulos do app', (tester) async {
    await pumpHome(tester, HotZoneConfigStore());

    expect(find.text('AltCross'), findsOneWidget);
    expect(find.text('Arranjo de telas'), findsOneWidget);
    expect(find.text('Conexões'), findsOneWidget);
    expect(find.text('Ajustes'), findsOneWidget);
  });

  testWidgets('abrir o módulo de arranjo navega pra tela de arranjo',
      (tester) async {
    await pumpHome(tester, HotZoneConfigStore());

    await tester.tap(find.text('Arranjo de telas'));
    await tester.pumpAndSettle();

    expect(find.textContaining('1920×1080'), findsOneWidget);
  });

  testWidgets('abrir o módulo de conexões navega pra tela de conexões',
      (tester) async {
    await pumpHome(tester, HotZoneConfigStore());

    await tester.tap(find.text('Conexões'));
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('run-discovery-button')), findsOneWidget);
  });

  testWidgets('abrir o módulo de ajustes navega pra tela de ajustes',
      (tester) async {
    await pumpHome(tester, HotZoneConfigStore());

    await tester.tap(find.text('Ajustes'));
    await tester.pumpAndSettle();

    expect(find.text('Aparência'), findsOneWidget);
  });
}
