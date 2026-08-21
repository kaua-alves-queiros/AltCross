import 'package:altcross_app/main.dart';
import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/models/screen_sync.dart';
import 'package:altcross_app/services/connection_status.dart';
import 'package:altcross_app/services/settings_store.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:altcross_app/state/local_hotzone_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('AltCrossApp abre na home com os módulos',
      (WidgetTester tester) async {
    await tester.pumpWidget(AltCrossApp(
      store: HotZoneConfigStore(),
      localHotZoneStore: LocalHotZoneStore(),
      // sem chamar .start() — isso é feito só em main() na vida real; nos
      // testes é só um notifier inerte, sem tocar no FFI de verdade.
      connectionStatus: ConnectionStatusNotifier(),
      displayProvider: () => const [
        PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
      ],
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => [],
      loadThemePreference: () => AppThemePreference.system,
      saveThemePreference: (_) {},
      pollIncomingZone: () => null,
      getCursorPosition: () => null,
    ));

    expect(find.text('AltCross'), findsOneWidget);
    expect(find.text('Arranjo de telas'), findsOneWidget);
    expect(find.text('Conexões'), findsOneWidget);
    expect(find.text('Ajustes'), findsOneWidget);
  });

  testWidgets('trocar o tema nos ajustes persiste e aplica na hora',
      (WidgetTester tester) async {
    AppThemePreference? saved;

    await tester.pumpWidget(AltCrossApp(
      store: HotZoneConfigStore(),
      localHotZoneStore: LocalHotZoneStore(),
      // sem chamar .start() — isso é feito só em main() na vida real; nos
      // testes é só um notifier inerte, sem tocar no FFI de verdade.
      connectionStatus: ConnectionStatusNotifier(),
      loadThemePreference: () => AppThemePreference.system,
      saveThemePreference: (value) => saved = value,
      pollIncomingZone: () => null,
      getCursorPosition: () => null,
    ));

    await tester.tap(find.text('Ajustes'));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('theme-option-dark')));
    await tester.pumpAndSettle();

    expect(saved, AppThemePreference.dark);

    final app = tester.widget<MaterialApp>(find.byType(MaterialApp));
    expect(app.themeMode, ThemeMode.dark);
  });

  testWidgets(
      'notificação de zone push de outro host aplica a conexão espelhada no meu store',
      (WidgetTester tester) async {
    final store = HotZoneConfigStore();
    var polled = false;

    await tester.pumpWidget(AltCrossApp(
      store: store,
      localHotZoneStore: LocalHotZoneStore(),
      // sem chamar .start() — isso é feito só em main() na vida real; nos
      // testes é só um notifier inerte, sem tocar no FFI de verdade.
      connectionStatus: ConnectionStatusNotifier(),
      loadThemePreference: () => AppThemePreference.system,
      saveThemePreference: (_) {},
      getCursorPosition: () => null,
      pollIncomingZone: () {
        if (polled) return null;
        polled = true;
        // o outro host conectou a borda DIREITA dele em mim — do meu lado,
        // a borda correspondente é a oposta (esquerda).
        return const IncomingZonePush(
          senderDeviceId: 'pc-windows',
          senderName: 'PC do Windows',
          senderEdge: HotZoneEdge.right,
          senderTargetScreenIndex: 0,
        );
      },
    ));

    await tester.pump(const Duration(milliseconds: 800));
    await tester.pumpAndSettle();

    expect(store.zones, hasLength(1));
    expect(store.zones.single.targetDeviceId, 'pc-windows');
    expect(store.zones.single.edge, HotZoneEdge.left);
  });
}
