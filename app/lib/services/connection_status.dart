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
    AltCrossNative.startConnectionMonitor(onStatusChange: (deviceId, online) {
      if (!online) {
        // Failsafe: se for esse peer que está controlando o input local
        // agora (handoff remoto ativo), o Core solta o controle na hora —
        // sem isso, o mouse/teclado local ficam presos capturados se o link
        // cair no meio de uma sessão remota, sem nenhuma borda pra devolver
        // o controle. Fica aqui (não em [updateStatus]) pra esse método
        // continuar puro e testável sem precisar do FFI de verdade.
        AltCrossNative.notifyPeerOffline(deviceId);
      }
      updateStatus(deviceId, online);
    });
  }

  /// Aplica uma mudança de status de peer — chamado pelo callback nativo em
  /// `start()`, e diretamente por testes (sem precisar do FFI de verdade).
  void updateStatus(String deviceId, bool online) {
    if (!online) {
      _offlineDevices.add(deviceId);
    } else {
      _offlineDevices.remove(deviceId);
    }
    notifyListeners();
  }

  void stop() {
    _timer?.cancel();
    AltCrossNative.stopConnectionMonitor();
    _offlineDevices.clear();
  }

  /// Reinicia o monitor pra ele reler a lista de pareados do disco agora —
  /// o Core só carrega essa lista uma vez, em `start()` (ver
  /// `altcross_connection_monitor_start`/`populate_peers_from_store`), então
  /// sem isso um dispositivo pareado DEPOIS do app já aberto nunca entra no
  /// heartbeat e nunca gera notificação de conexão/desconexão. Chamar isso
  /// logo depois de qualquer pareamento novo.
  void refresh() {
    stop();
    start();
  }
}
