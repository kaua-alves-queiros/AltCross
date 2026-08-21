import 'dart:io';

import 'package:flutter/material.dart';

import 'models/physical_display.dart';
import 'native/altcross_native.dart';
import 'screens/connections_screen.dart';
import 'screens/home_screen.dart';
import 'state/hot_zone_config_store.dart';

void main() {
  // Só escuta e responde via unicast — não manda broadcast, então não
  // deveria disparar o prompt de permissão de Rede Local por si só (ver
  // AltCrossNative.startDiscoveryResponder). É isso que faz esta máquina
  // aparecer quando outra roda "Buscar dispositivos".
  AltCrossNative.startDiscoveryResponder(name: Platform.localHostname);

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
