import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';

import '../models/discovered_device.dart';
import '../models/hot_zone.dart';
import '../models/pairing.dart';
import '../native/altcross_native.dart';
import '../services/hot_zone_labels.dart';
import '../state/hot_zone_config_store.dart';

typedef DiscoveryRunner = Future<List<DiscoveredDevice>> Function({
  int timeoutMs,
  int maxResults,
});

typedef SendPairingRequest = bool Function({
  required String peerHost,
  required String myName,
});

typedef ConfirmPairing = Future<PairingResult> Function({
  required String peerHost,
  required int code,
  int timeoutMs,
});

typedef PollIncomingPairingRequest = IncomingPairingRequest? Function();

typedef PollPairingCompleted = PairingCompleted? Function();

/// Tela de conexões: mapeia os dispositivos já pareados (mesma fonte de
/// dados da tela de arranjo, `HotZoneConfigStore`) e deixa parear com
/// dispositivos encontrados na rede — de verdade, com troca de código de
/// confirmação, não só "clicar e já adicionar".
class ConnectionsScreen extends StatefulWidget {
  final HotZoneConfigStore store;
  final DiscoveryRunner discoveryRunner;
  final SendPairingRequest sendPairingRequest;
  final ConfirmPairing confirmPairing;
  final PollIncomingPairingRequest pollIncomingPairingRequest;
  final PollPairingCompleted pollPairingCompleted;

  const ConnectionsScreen({
    super.key,
    required this.store,
    DiscoveryRunner? discoveryRunner,
    SendPairingRequest? sendPairingRequest,
    ConfirmPairing? confirmPairing,
    PollIncomingPairingRequest? pollIncomingPairingRequest,
    PollPairingCompleted? pollPairingCompleted,
  })  : discoveryRunner = discoveryRunner ?? AltCrossNative.runDiscovery,
        sendPairingRequest =
            sendPairingRequest ?? AltCrossNative.sendPairingRequest,
        confirmPairing = confirmPairing ?? AltCrossNative.confirmPairing,
        pollIncomingPairingRequest = pollIncomingPairingRequest ??
            AltCrossNative.pollIncomingPairingRequest,
        pollPairingCompleted =
            pollPairingCompleted ?? AltCrossNative.pollPairingCompleted;

  @override
  State<ConnectionsScreen> createState() => _ConnectionsScreenState();
}

enum _SearchState { idle, searching, done, error }

class _ConnectionsScreenState extends State<ConnectionsScreen> {
  _SearchState _searchState = _SearchState.idle;
  List<DiscoveredDevice> _discovered = [];
  String? _error;
  String? _message;

  Timer? _incomingPollTimer;
  bool _showingIncomingDialog = false;
  String? _awaitingConfirmDeviceId;

  @override
  void initState() {
    super.initState();
    _incomingPollTimer = Timer.periodic(const Duration(milliseconds: 700), (_) {
      _pollIncoming();
      _pollCompleted();
    });
  }

  @override
  void dispose() {
    _incomingPollTimer?.cancel();
    super.dispose();
  }

  void _pollIncoming() {
    if (_showingIncomingDialog || !mounted) {
      return;
    }
    final incoming = widget.pollIncomingPairingRequest();
    if (incoming == null) {
      return;
    }
    _showingIncomingDialog = true;
    _awaitingConfirmDeviceId = incoming.requesterDeviceId;
    showDialog<void>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text('Pedido de pareamento'),
        content: Text(
          '${incoming.requesterName} quer se conectar.\n\n'
          'Informe este código para ele confirmar:\n\n${incoming.code}',
          key: const Key('incoming-pairing-code'),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text('Fechar'),
          ),
        ],
      ),
    ).then((_) {
      _showingIncomingDialog = false;
      _awaitingConfirmDeviceId = null;
    });
  }

  /// Do lado de quem RECEBEU o pedido: sem isso, quem está sendo adicionado
  /// nunca fica sabendo que o outro lado digitou o código certo — só quem
  /// pediu o pareamento via `_startPairing` recebe esse retorno.
  void _pollCompleted() {
    if (!mounted) {
      return;
    }
    final completed = widget.pollPairingCompleted();
    if (completed == null) {
      return;
    }
    if (_showingIncomingDialog &&
        _awaitingConfirmDeviceId == completed.peerDeviceId &&
        Navigator.of(context).canPop()) {
      Navigator.of(context).pop();
    }
    setState(() {
      try {
        widget.store.add(HotZoneConfig(
          edge: HotZoneEdge.right,
          targetDeviceId: completed.peerDeviceId,
          enabled: true,
        ));
      } on StateError {
        /* já configurado numa borda — pareamento em si já foi salvo */
      }
      _message = 'Pareado com ${completed.peerName} com sucesso.';
    });
  }

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

  Future<void> _startPairing(DiscoveredDevice device) async {
    final sent = widget.sendPairingRequest(
      peerHost: device.host,
      myName: Platform.localHostname,
    );
    if (!sent) {
      setState(() => _message = 'Não consegui mandar o pedido de pareamento.');
      return;
    }

    final code = await showDialog<int>(
      context: context,
      builder: (dialogContext) => _PairingCodeDialog(deviceName: device.name),
    );
    if (code == null || !mounted) {
      return;
    }

    setState(() => _message = 'Confirmando pareamento com ${device.name}...');
    final result = await widget.confirmPairing(
      peerHost: device.host,
      code: code,
    );
    if (!mounted) {
      return;
    }

    switch (result.outcome) {
      case PairingOutcome.accepted:
        try {
          widget.store.add(HotZoneConfig(
            edge: HotZoneEdge.right,
            targetDeviceId: result.deviceId!,
            enabled: true,
          ));
        } on StateError {
          /* já configurado numa borda — pareamento em si já foi salvo */
        }
        setState(() => _message = 'Pareado com ${result.name} com sucesso.');
        break;
      case PairingOutcome.rejected:
        setState(() => _message = 'Código incorreto — pareamento recusado.');
        break;
      case PairingOutcome.error:
        setState(() => _message = 'Falha de rede ao confirmar o pareamento.');
        break;
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
                    key: Key('pair-button-${device.deviceId}'),
                    onPressed: () => _startPairing(device),
                    child: const Text('Adicionar'),
                  ),
                ),
              ),
          if (_message != null)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(_message!, key: const Key('pairing-message')),
            ),
        ],
      ),
    );
  }
}

class _PairingCodeDialog extends StatefulWidget {
  final String deviceName;

  const _PairingCodeDialog({required this.deviceName});

  @override
  State<_PairingCodeDialog> createState() => _PairingCodeDialogState();
}

class _PairingCodeDialogState extends State<_PairingCodeDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text('Parear com ${widget.deviceName}'),
      content: TextField(
        key: const Key('pairing-code-field'),
        controller: _controller,
        autofocus: true,
        keyboardType: TextInputType.number,
        decoration: const InputDecoration(
          labelText: 'Código mostrado no outro dispositivo',
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('Cancelar'),
        ),
        FilledButton(
          onPressed: () {
            final code = int.tryParse(_controller.text.trim());
            Navigator.of(context).pop(code);
          },
          child: const Text('Confirmar'),
        ),
      ],
    );
  }
}
