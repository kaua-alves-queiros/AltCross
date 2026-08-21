import 'package:altcross_app/screens/settings_screen.dart';
import 'package:altcross_app/services/settings_store.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('mostra as 3 opções de aparência e marca a atual',
      (tester) async {
    await tester.pumpWidget(MaterialApp(
      home: SettingsScreen(
        currentThemePreference: AppThemePreference.dark,
        onThemePreferenceChanged: (_) {},
      ),
    ));

    expect(find.text('Claro'), findsOneWidget);
    expect(find.text('Escuro'), findsOneWidget);
    expect(find.text('Sistema'), findsOneWidget);

    final group = tester.widget<RadioGroup<AppThemePreference>>(
      find.byType(RadioGroup<AppThemePreference>),
    );
    expect(group.groupValue, AppThemePreference.dark);
  });

  testWidgets('selecionar uma opção chama onThemePreferenceChanged',
      (tester) async {
    AppThemePreference? changedTo;

    await tester.pumpWidget(MaterialApp(
      home: SettingsScreen(
        currentThemePreference: AppThemePreference.system,
        onThemePreferenceChanged: (value) => changedTo = value,
      ),
    ));

    await tester.tap(find.byKey(const Key('theme-option-light')));
    await tester.pumpAndSettle();

    expect(changedTo, AppThemePreference.light);
  });
}
