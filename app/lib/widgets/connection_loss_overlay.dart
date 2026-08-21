import 'package:flutter/material.dart';

import '../services/connection_status.dart';

/// Widget que observa [ConnectionStatusNotifier] e exibe um SnackBar
/// quando um dispositivo pareado fica offline.
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
  final Set<String> _shown = {};

  @override
  void initState() {
    super.initState();
    widget.notifier.addListener(_onStatusChange);
  }

  @override
  void dispose() {
    widget.notifier.removeListener(_onStatusChange);
    super.dispose();
  }

  void _onStatusChange() {
    if (!mounted) return;
    for (final deviceId in widget.notifier.offlineDevices) {
      if (_shown.add(deviceId)) {
        _showNotification(deviceId);
      }
    }
  }

  void _showNotification(String deviceId) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('Conexão perdida com $deviceId'),
        backgroundColor: Colors.red.shade700,
        duration: const Duration(seconds: 6),
        action: SnackBarAction(
          label: 'Dispensar',
          textColor: Colors.white,
          onPressed: () {
            widget.notifier.dismissDevice(deviceId);
            _shown.remove(deviceId);
          },
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) => widget.child;
}
