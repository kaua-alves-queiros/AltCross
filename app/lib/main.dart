import 'package:flutter/material.dart';

import 'models/physical_display.dart';
import 'screens/connections_screen.dart';
import 'screens/home_screen.dart';
import 'state/hot_zone_config_store.dart';

void main() {
  runApp(AltCrossApp(store: HotZoneConfigStore()));
}

class AltCrossApp extends StatelessWidget {
  final HotZoneConfigStore store;
  final List<PhysicalDisplay> Function()? displayProvider;
  final DiscoveryRunner? discoveryRunner;

  const AltCrossApp({
    super.key,
    required this.store,
    this.displayProvider,
    this.discoveryRunner,
  });

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AltCross',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.indigo),
        useMaterial3: true,
      ),
      home: HomeScreen(
        store: store,
        displayProvider: displayProvider,
        discoveryRunner: discoveryRunner,
      ),
    );
  }
}
