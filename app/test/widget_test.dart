import 'package:altcross_app/main.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/services/settings_store.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('AltCrossApp abre na home com os módulos',
      (WidgetTester tester) async {
    await tester.pumpWidget(AltCrossApp(
      store: HotZoneConfigStore(),
      displayProvider: () => const [
        PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
      ],
      discoveryRunner: ({timeoutMs = 0, maxResults = 0}) async => [],
      loadThemePreference: () => AppThemePreference.system,
      saveThemePreference: (_) {},
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
      loadThemePreference: () => AppThemePreference.system,
      saveThemePreference: (value) => saved = value,
    ));

    await tester.tap(find.text('Ajustes'));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('theme-option-dark')));
    await tester.pumpAndSettle();

    expect(saved, AppThemePreference.dark);

    final app = tester.widget<MaterialApp>(find.byType(MaterialApp));
    expect(app.themeMode, ThemeMode.dark);
  });
}
