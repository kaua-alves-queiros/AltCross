import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/discovered_device.dart';
import '../models/hot_zone.dart';
import '../models/pairing.dart';
import '../native/altcross_native.dart';
import '../services/handoff_activation.dart';
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

typedef PushZoneToPeer = bool Function({
  required String peerHost,
  required String myName,
  required HotZoneEdge myEdge,
  required int targetScreenIndex,
});

void _noopRestartConnectionMonitor() {}

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
  final LookupTrustedHost lookupHost;
  final QueryPeerScreens queryPeerScreens;
  final PushZoneToPeer pushZoneToPeer;
  final LocalDisplaysProvider localDisplaysProvider;
  final StartHandoff startHandoff;
  final StopHandoff stopHandoff;
  final IsHandoffRemote isHandoffRemote;
  final IsHandoffRemote isHandoffActive;
  final VoidCallback restartConnectionMonitor;
  final bool handoffEnabled;
  final ValueChanged<bool> onHandoffEnabledChanged;

  ConnectionsScreen({
    super.key,
    required this.store,
    DiscoveryRunner? discoveryRunner,
    SendPairingRequest? sendPairingRequest,
    ConfirmPairing? confirmPairing,
    PollIncomingPairingRequest? pollIncomingPairingRequest,
    PollPairingCompleted? pollPairingCompleted,
    LookupTrustedHost? lookupHost,
    QueryPeerScreens? queryPeerScreens,
    PushZoneToPeer? pushZoneToPeer,
    LocalDisplaysProvider? localDisplaysProvider,
    StartHandoff? startHandoff,
    StopHandoff? stopHandoff,
    IsHandoffRemote? isHandoffRemote,
    IsHandoffRemote? isHandoffActive,
    VoidCallback? restartConnectionMonitor,
    this.handoffEnabled = true,
    ValueChanged<bool>? onHandoffEnabledChanged,
  })  : discoveryRunner = discoveryRunner ?? AltCrossNative.runDiscovery,
        sendPairingRequest =
            sendPairingRequest ?? AltCrossNative.sendPairingRequest,
        confirmPairing = confirmPairing ?? AltCrossNative.confirmPairing,
        pollIncomingPairingRequest = pollIncomingPairingRequest ??
            AltCrossNative.pollIncomingPairingRequest,
        pollPairingCompleted =
            pollPairingCompleted ?? AltCrossNative.pollPairingCompleted,
        lookupHost = lookupHost ?? AltCrossNative.lookupTrustedHost,
        queryPeerScreens = queryPeerScreens ??
            ((host) => AltCrossNative.queryPeerScreens(peerHost: host)),
        pushZoneToPeer = pushZoneToPeer ?? AltCrossNative.pushZoneToPeer,
        localDisplaysProvider =
            localDisplaysProvider ?? AltCrossNative.enumerateDisplays,
        startHandoff = startHandoff ?? AltCrossNative.startHandoff,
        stopHandoff = stopHandoff ?? AltCrossNative.stopHandoff,
        isHandoffRemote = isHandoffRemote ?? AltCrossNative.isHandoffRemote,
        isHandoffActive = isHandoffActive ?? AltCrossNative.isHandoffActive,
        onHandoffEnabledChanged = onHandoffEnabledChanged ?? ((_) {}),
        restartConnectionMonitor =
            restartConnectionMonitor ?? _noopRestartConnectionMonitor;

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

  bool _handoffActive = false;
  bool _handoffRemoteNow = false;
  bool _startingHandoff = false;
  late bool _handoffEnabled = widget.handoffEnabled;

  @override
  void initState() {
    super.initState();
    // O handoff pode já estar rodando de uma visita anterior a esta tela
    // (ele sobrevive à navegação) — sem isso, reabrir a tela mostraria
    // "desativado" mesmo com o hook de verdade ainda ligado.
    _handoffActive = widget.isHandoffActive();
    if (_handoffEnabled && !_handoffActive) {
      // Controle entre dispositivos é "sempre ligado" por padrão — ativa
      // sozinho ao abrir a tela, sem precisar de clique manual toda vez.
      // Adiado pro pós-frame por causa do setState dentro de
      // `_activateHandoff` (ver mesmo motivo em ArrangementScreen).
      WidgetsBinding.instance.addPostFrameCallback((_) => _activateHandoff());
    }
    _incomingPollTimer = Timer.periodic(const Duration(milliseconds: 700), (_) {
      _pollIncoming();
      _pollCompleted();
      _pollHandoffState();
    });
  }

  @override
  void dispose() {
    _incomingPollTimer?.cancel();
    super.dispose();
  }

  void _pollHandoffState() {
    if (!mounted) {
      return;
    }
    if (_handoffActive) {
      final remoteNow = widget.isHandoffRemote();
      if (remoteNow != _handoffRemoteNow) {
        setState(() => _handoffRemoteNow = remoteNow);
      }
      return;
    }
    // Continua tentando ligar sozinho enquanto estiver habilitado mas
    // ainda não tiver conseguido — cobre tanto "acabei de parear" (a zona
    // chega pelo `_pollIncomingZone` do main.dart, de forma assíncrona,
    // sem avisar esta tela) quanto "o dispositivo pareado estava offline e
    // acabou de voltar". `_startingHandoff` evita empilhar tentativas
    // enquanto uma já está em andamento (cada uma pode levar até uns
    // segundos, por causa do timeout de rede em `queryPeerScreens`).
    if (_handoffEnabled && !_startingHandoff) {
      _activateHandoff(silent: true);
    }
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
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('${incoming.requesterName} quer se conectar.\n\n'
                'Informe este código para ele confirmar:'),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: Text(
                    '${incoming.code}',
                    key: const Key('incoming-pairing-code'),
                    style: Theme.of(dialogContext)
                        .textTheme
                        .headlineSmall
                        ?.copyWith(fontWeight: FontWeight.bold, letterSpacing: 4),
                  ),
                ),
                IconButton(
                  key: const Key('copy-pairing-code-button'),
                  icon: const Icon(Icons.copy),
                  tooltip: 'Copiar código',
                  onPressed: () async {
                    await Clipboard.setData(
                        ClipboardData(text: '${incoming.code}'));
                    if (!dialogContext.mounted) {
                      return;
                    }
                    ScaffoldMessenger.of(dialogContext).showSnackBar(
                      const SnackBar(
                        content: Text('Código copiado.'),
                        duration: Duration(seconds: 2),
                      ),
                    );
                  },
                ),
              ],
            ),
          ],
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
  ///
  /// Não cria a hotzone aqui sozinho: quem decide a borda de verdade é quem
  /// pediu o pareamento (`_startPairing`), e essa borda chega pra este lado
  /// pela rede (`pushZoneToPeer` → `_pollIncomingZone` em main.dart), já na
  /// borda oposta certa — inventar uma borda própria aqui (sempre "right",
  /// sem coordenação nenhuma com o outro lado) é o que fazia o mapeamento
  /// entre 2 hosts nunca bater com o que o usuário via na tela de Arranjo.
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
    widget.restartConnectionMonitor();
    setState(() {
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
        var zoneConfigured = true;
        try {
          widget.store.add(HotZoneConfig(
            edge: HotZoneEdge.right,
            targetDeviceId: result.deviceId!,
            enabled: true,
          ));
        } on StateError {
          /* já existe outra hotzone habilitada na borda direita — não dá
           * pra criar automaticamente pra este dispositivo novo, o usuário
           * precisa escolher uma borda livre na tela de Arranjo. */
          zoneConfigured = false;
        }
        if (zoneConfigured) {
          // Avisa o outro PC de verdade que conectei minha borda direita
          // nele — sem isso, o mapeamento fica só configurado deste lado
          // (o outro nunca sabe que precisa mapear a borda oposta de
          // volta), e por isso não funciona "plug and play" como entre 2
          // telas locais.
          widget.pushZoneToPeer(
            peerHost: device.host,
            myName: Platform.localHostname,
            myEdge: HotZoneEdge.right,
            targetScreenIndex: 0,
          );
        }
        widget.restartConnectionMonitor();
        setState(() => _message = zoneConfigured
            ? 'Pareado com ${result.name} com sucesso.'
            : 'Pareado com ${result.name}, mas a borda direita já está em'
                ' uso — escolha uma borda livre na tela de Arranjo.');
        if (zoneConfigured && _handoffEnabled && !_handoffActive) {
          // A zona já foi adicionada ao store 2 linhas acima — tenta ligar
          // o controle na hora, sem esperar o próximo tick do polling.
          _activateHandoff();
        }
        break;
      case PairingOutcome.rejected:
        setState(() => _message = 'Código incorreto — pareamento recusado.');
        break;
      case PairingOutcome.error:
        setState(() => _message = 'Falha de rede ao confirmar o pareamento.');
        break;
    }
  }

  /// Liga (ou tenta) o controle entre dispositivos usando o serviço
  /// compartilhado (mesma lógica usada na ativação automática ao abrir o
  /// app — ver `services/handoff_activation.dart`). `silent` é usado pelas
  /// tentativas automáticas de segundo plano (`_pollHandoffState`): se
  /// falhar (nada pareado ainda, ou offline agora), não fica reescrevendo
  /// `_message` a cada ~700ms por cima do que a tela já está mostrando —
  /// só avisa de verdade quando FUNCIONA, ou quando foi um clique
  /// explícito do usuário (`silent: false`, o padrão).
  Future<void> _activateHandoff({bool silent = false}) async {
    setState(() => _startingHandoff = true);

    final result = await activateHandoff(
      store: widget.store,
      lookupHost: widget.lookupHost,
      queryPeerScreens: widget.queryPeerScreens,
      localDisplaysProvider: widget.localDisplaysProvider,
      startHandoff: widget.startHandoff,
    );

    if (!mounted) {
      return;
    }

    setState(() {
      _startingHandoff = false;
      _handoffActive = result.started;
      _handoffRemoteNow = false;
      if (result.started) {
        _message = result.skippedDeviceIds.isNotEmpty
            ? 'Ativado (menos pra: ${result.skippedDeviceIds.join(", ")} —'
                ' offline agora).'
            : 'Controle entre dispositivos ativado.';
      } else if (!silent) {
        _message = result.failureReason;
      }
    });
  }

  /// Chamado pelo checkbox — controle entre dispositivos vem marcado por
  /// padrão (ver `SettingsStore.loadHandoffEnabled`), então desmarcar é a
  /// única ação manual que existe aqui; marcar de novo só reativa.
  Future<void> _setHandoffEnabled(bool enabled) async {
    setState(() => _handoffEnabled = enabled);
    widget.onHandoffEnabledChanged(enabled);
    if (enabled) {
      await _activateHandoff();
      return;
    }
    widget.stopHandoff();
    setState(() {
      _handoffActive = false;
      _handoffRemoteNow = false;
      _message = 'Controle entre dispositivos desativado.';
    });
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
          const SizedBox(height: 24),
          Card(
            color: _handoffActive
                ? Theme.of(context).colorScheme.primaryContainer
                : null,
            child: Padding(
              padding: const EdgeInsets.fromLTRB(8, 12, 16, 12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Checkbox(
                        key: const Key('handoff-enabled-checkbox'),
                        value: _handoffEnabled,
                        onChanged: (value) => _setHandoffEnabled(value ?? false),
                      ),
                      Icon(_handoffRemoteNow
                          ? Icons.swipe_right_alt
                          : (_handoffActive ? Icons.link : Icons.link_off)),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Text(
                          _handoffRemoteNow
                              ? 'Controle está em outro dispositivo agora —'
                                  ' Ctrl+Esc pra voltar.'
                              : _handoffActive
                                  ? 'Controle entre dispositivos ativado.'
                                  : _handoffEnabled
                                      ? 'Tentando ativar o controle entre'
                                          ' dispositivos…'
                                      : 'Controle entre dispositivos'
                                          ' desativado.',
                        ),
                      ),
                      if (_startingHandoff)
                        const SizedBox(
                            width: 20,
                            height: 20,
                            child: CircularProgressIndicator(strokeWidth: 2)),
                    ],
                  ),
                  Padding(
                    padding: const EdgeInsets.only(left: 44),
                    child: Text(
                      'Ctrl+Esc solta o controle de volta a qualquer'
                      ' momento. No macOS isso pede permissão de'
                      ' Acessibilidade na primeira vez.',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
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
