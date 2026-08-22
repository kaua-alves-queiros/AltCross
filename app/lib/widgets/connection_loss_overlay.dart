import 'package:flutter/material.dart';

import '../services/connection_status.dart';

/// Widget que observa [ConnectionStatusNotifier] e exibe um SnackBar tanto
/// quando um dispositivo pareado fica offline quanto quando ele volta —
/// aviso neutro (nem "erro", nem cor de alerta), já que ficar/voltar
/// offline é normal (o usuário fechou o app no outro PC), não uma falha.
class ConnectionLossOverlay extends StatefulWidget {
  final ConnectionStatusNotifier notifier;
  final Widget child;

  const ConnectionLossOverlay({
    super.key,
    required this.notifier,
    required this.child,
  });

  @override
  State<ConnectionLossOverlay> createState() => _ConnectionLossOverlayState();
}

class _ConnectionLossOverlayState extends State<ConnectionLossOverlay> {
  Set<String> _previousOffline = const {};

  @override
  void initState() {
    super.initState();
    _previousOffline = widget.notifier.offlineDevices;
    widget.notifier.addListener(_onStatusChange);
  }

  @override
  void dispose() {
    widget.notifier.removeListener(_onStatusChange);
    super.dispose();
  }

  void _onStatusChange() {
    if (!mounted) return;
    final current = widget.notifier.offlineDevices;
    for (final deviceId in current) {
      if (!_previousOffline.contains(deviceId)) {
        _showNotification(deviceId, online: false);
      }
    }
    for (final deviceId in _previousOffline) {
      if (!current.contains(deviceId)) {
        _showNotification(deviceId, online: true);
      }
    }
    _previousOffline = current;
  }

  void _showNotification(String deviceId, {required bool online}) {
    final messenger = ScaffoldMessenger.of(context);
    // Sem isso, um evento novo (ex.: voltou online) fica enfileirado atrás
    // do aviso anterior em vez de aparecer na hora — com heartbeat de 5s
    // isso é fácil de acontecer (cai e volta rápido).
    messenger.clearSnackBars();
    messenger.showSnackBar(
      SnackBar(
        content: Row(
          children: [
            Icon(online ? Icons.link : Icons.link_off, size: 18),
            const SizedBox(width: 12),
            Expanded(
              child:
                  Text(online ? '$deviceId conectou' : '$deviceId desconectou'),
            ),
          ],
        ),
        duration: const Duration(seconds: 4),
      ),
    );
  }

  @override
  Widget build(BuildContext context) => widget.child;
}
