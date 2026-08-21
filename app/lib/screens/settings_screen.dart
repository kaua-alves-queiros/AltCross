import 'package:flutter/material.dart';

import '../services/settings_store.dart';

/// Tela de ajustes: hoje só a preferência de aparência. A tela não decide
/// como isso vira um `ThemeMode` de verdade nem persiste nada sozinha — só
/// mostra a opção atual e avisa `onChanged` quando o usuário escolhe outra
/// (quem decide o que fazer com isso é o `AltCrossApp`).
class SettingsScreen extends StatelessWidget {
  final AppThemePreference currentThemePreference;
  final ValueChanged<AppThemePreference> onThemePreferenceChanged;

  const SettingsScreen({
    super.key,
    required this.currentThemePreference,
    required this.onThemePreferenceChanged,
  });

  static String _label(AppThemePreference preference) {
    switch (preference) {
      case AppThemePreference.light:
        return 'Claro';
      case AppThemePreference.dark:
        return 'Escuro';
      case AppThemePreference.system:
        return 'Sistema';
    }
  }

  static IconData _icon(AppThemePreference preference) {
    switch (preference) {
      case AppThemePreference.light:
        return Icons.light_mode_outlined;
      case AppThemePreference.dark:
        return Icons.dark_mode_outlined;
      case AppThemePreference.system:
        return Icons.brightness_auto_outlined;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('AltCross')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Aparência', style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 4),
            Text(
              'Escolha como o AltCross deve aparecer nesta máquina.',
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 16),
            Card(
              child: RadioGroup<AppThemePreference>(
                groupValue: currentThemePreference,
                onChanged: (value) {
                  if (value != null) {
                    onThemePreferenceChanged(value);
                  }
                },
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    for (final preference in AppThemePreference.values)
                      RadioListTile<AppThemePreference>(
                        key: Key('theme-option-${preference.name}'),
                        value: preference,
                        secondary: Icon(_icon(preference)),
                        title: Text(_label(preference)),
                      ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
