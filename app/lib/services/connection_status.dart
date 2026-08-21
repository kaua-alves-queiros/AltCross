import 'dart:async';

import 'package:flutter/material.dart';

import '../native/altcross_native.dart';

/// Monitora a conectividade dos peers pareados via heartbeat e mantém
/// um registro dos dispositivos offline para exibir notificações.
class ConnectionStatusNotifier extends ChangeNotifier {
  final Set<String> _offlineDevices = {};
  Timer? _timer;

  Set<String> get offlineDevices => Set.unmodifiable(_offlineDevices);

  /// Inicia o monitoramento. Chamar uma única vez (ex.: no main ou
  /// ao montar o MaterialApp com [NavigatorState]).
  void start() {
    AltCrossNative.startConnectionMonitor(
      onStatusChange: (deviceId, online) {
        if (!online) {
          _offlineDevices.add(deviceId);
        } else {
          _offlineDevices.remove(deviceId);
        }
        notifyListeners();
      },
    );
  }

  void stop() {
    _timer?.cancel();
    AltCrossNative.stopConnectionMonitor();
    _offlineDevices.clear();
  }

  /// Descarta o dispositivo da lista de offline (chamado quando o usuário
  /// fecha a notificação).
  void dismissDevice(String deviceId) {
    _offlineDevices.remove(deviceId);
    notifyListeners();
  }
}
