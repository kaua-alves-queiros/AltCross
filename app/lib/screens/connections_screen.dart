import 'package:flutter/material.dart';

import '../models/discovered_device.dart';
import '../models/hot_zone.dart';
import '../native/altcross_native.dart';
import '../services/hot_zone_labels.dart';
import '../state/hot_zone_config_store.dart';

typedef DiscoveryRunner = Future<List<DiscoveredDevice>> Function({
  int timeoutMs,
  int maxResults,
});

/// Tela de conexões: mapeia os dispositivos já configurados (mesma fonte de
/// dados da tela de arranjo, `HotZoneConfigStore`) e deixa disparar uma
/// busca por dispositivos AltCross na rede local.
class ConnectionsScreen extends StatefulWidget {
  final HotZoneConfigStore store;
  final DiscoveryRunner discoveryRunner;

  const ConnectionsScreen({super.key, required this.store, DiscoveryRunner? discoveryRunner})
      : discoveryRunner = discoveryRunner ?? AltCrossNative.runDiscovery;

  @override
  State<ConnectionsScreen> createState() => _ConnectionsScreenState();
}

enum _SearchState { idle, searching, done, error }

class _ConnectionsScreenState extends State<ConnectionsScreen> {
  _SearchState _searchState = _SearchState.idle;
  List<DiscoveredDevice> _discovered = [];
  String? _error;
  String? _addMessage;

  Future<void> _runDiscovery() async {
    setState(() {
      _searchState = _SearchState.searching;
      _error = null;
    });
    try {
      final found = await widget.discoveryRunner();
      setState(() {
        _discovered = found;
        _searchState = _SearchState.done;
      });
    } catch (e) {
      setState(() {
        _error = e.toString();
        _searchState = _SearchState.error;
      });
    }
  }

  void _addDiscoveredDevice(DiscoveredDevice device) {
    try {
      widget.store.add(HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: device.deviceId,
        enabled: true,
      ));
      setState(() => _addMessage = null);
    } on StateError catch (e) {
      setState(() => _addMessage = e.message);
    }
  }

  @override
  Widget build(BuildContext context) {
    final zones = widget.store.zones;

    return Scaffold(
      appBar: AppBar(title: const Text('AltCross')),
      body: ListView(
        padding: const EdgeInsets.all(24),
        children: [
          Text('Conexões configuradas',
              style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 8),
          if (zones.isEmpty)
            const Text('Nenhuma conexão configurada ainda.')
          else
            for (final zone in zones)
              Card(
                child: ListTile(
                  leading: CircleAvatar(child: Icon(edgeIcon(zone.edge))),
                  title: Text(zone.targetDeviceId),
                  subtitle: Text(
                      '${edgeLabel(zone.edge)} · ${zone.enabled ? "ativa" : "desativada"}'),
                ),
              ),
          const SizedBox(height: 32),
          Row(
            children: [
              Expanded(
                child: Text('Descoberta automática',
                    style: Theme.of(context).textTheme.titleLarge),
              ),
              FilledButton.icon(
                key: const Key('run-discovery-button'),
                onPressed:
                    _searchState == _SearchState.searching ? null : _runDiscovery,
                icon: _searchState == _SearchState.searching
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.wifi_find),
                label: const Text('Buscar dispositivos'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          if (_searchState == _SearchState.error)
            Text(
              'Falha ao iniciar a busca: ${_error ?? ""}',
              style: TextStyle(color: Theme.of(context).colorScheme.error),
            )
          else if (_searchState == _SearchState.done && _discovered.isEmpty)
            const Text('Nenhum dispositivo encontrado na rede local.')
          else if (_searchState == _SearchState.done)
            for (final device in _discovered)
              Card(
                child: ListTile(
                  leading: const CircleAvatar(child: Icon(Icons.devices)),
                  title: Text(device.name),
                  subtitle: Text('${device.host}:${device.port}'),
                  trailing: TextButton(
                    onPressed: () => _addDiscoveredDevice(device),
                    child: const Text('Adicionar'),
                  ),
                ),
              ),
          if (_addMessage != null)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                _addMessage!,
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
        ],
      ),
    );
  }
}
