import 'package:flutter/material.dart';

import '../models/physical_display.dart';
import '../native/altcross_native.dart';
import '../services/settings_store.dart';
import '../state/hot_zone_config_store.dart';
import '../state/local_hotzone_store.dart';
import 'arrangement_screen.dart';
import 'connections_screen.dart';
import 'settings_screen.dart';

/// Página inicial: painel com os módulos do app. Cada card navega pra tela
/// correspondente — a home não guarda estado nem lógica de negócio própria.
class HomeScreen extends StatelessWidget {
  final HotZoneConfigStore store;
  final LocalHotZoneStore localHotZoneStore;
  final List<PhysicalDisplay> Function()? displayProvider;
  final DiscoveryRunner? discoveryRunner;
  final SendPairingRequest? sendPairingRequest;
  final ConfirmPairing? confirmPairing;
  final PollIncomingPairingRequest? pollIncomingPairingRequest;
  final PollPairingCompleted? pollPairingCompleted;
  final LookupTrustedHost? lookupHost;
  final QueryPeerScreens? queryPeerScreens;
  final StartHandoff? startHandoff;
  final StopHandoff? stopHandoff;
  final IsHandoffRemote? isHandoffRemote;
  final IsHandoffRemote? isHandoffActive;
  final AppThemePreference currentThemePreference;
  final ValueChanged<AppThemePreference> onThemePreferenceChanged;

  const HomeScreen({
    super.key,
    required this.store,
    required this.localHotZoneStore,
    required this.currentThemePreference,
    required this.onThemePreferenceChanged,
    this.displayProvider,
    this.discoveryRunner,
    this.sendPairingRequest,
    this.confirmPairing,
    this.pollIncomingPairingRequest,
    this.pollPairingCompleted,
    this.lookupHost,
    this.queryPeerScreens,
    this.startHandoff,
    this.stopHandoff,
    this.isHandoffRemote,
    this.isHandoffActive,
  });

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('AltCross')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: GridView.count(
          crossAxisCount: 2,
          crossAxisSpacing: 16,
          mainAxisSpacing: 16,
          childAspectRatio: 1.6,
          children: [
            _ModuleCard(
              icon: Icons.dashboard_customize_outlined,
              title: 'Arranjo de telas',
              subtitle: 'Suas telas físicas e hotzones pra cada PC conectado',
              onTap: () => Navigator.of(context).push(MaterialPageRoute(
                builder: (_) => ArrangementScreen(
                  store: store,
                  localHotZoneStore: localHotZoneStore,
                  displayProvider: displayProvider,
                  lookupHost: lookupHost,
                  queryPeerScreens: queryPeerScreens,
                ),
              )),
            ),
            _ModuleCard(
              icon: Icons.hub_outlined,
              title: 'Conexões',
              subtitle: 'Dispositivos pareados e busca automática na rede',
              onTap: () => Navigator.of(context).push(MaterialPageRoute(
                builder: (_) => ConnectionsScreen(
                  store: store,
                  discoveryRunner: discoveryRunner,
                  sendPairingRequest: sendPairingRequest,
                  confirmPairing: confirmPairing,
                  pollIncomingPairingRequest: pollIncomingPairingRequest,
                  pollPairingCompleted: pollPairingCompleted,
                  lookupHost: lookupHost,
                  queryPeerScreens: queryPeerScreens,
                  localDisplaysProvider: displayProvider,
                  startHandoff: startHandoff,
                  stopHandoff: stopHandoff,
                  isHandoffRemote: isHandoffRemote,
                  isHandoffActive: isHandoffActive,
                ),
              )),
            ),
            _ModuleCard(
              icon: Icons.tune_outlined,
              title: 'Ajustes',
              subtitle: 'Aparência do app (claro, escuro ou sistema)',
              onTap: () => Navigator.of(context).push(MaterialPageRoute(
                builder: (_) => SettingsScreen(
                  currentThemePreference: currentThemePreference,
                  onThemePreferenceChanged: onThemePreferenceChanged,
                ),
              )),
            ),
          ],
        ),
      ),
    );
  }
}

class _ModuleCard extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  const _ModuleCard({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Icon(icon, size: 32),
              const SizedBox(height: 12),
              Text(title, style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 4),
              Text(
                subtitle,
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
      ),
    );
  }
}
