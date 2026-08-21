import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';

import 'models/hot_zone.dart';
import 'models/physical_display.dart';
import 'models/screen_sync.dart';
import 'native/altcross_native.dart';
import 'screens/connections_screen.dart';
import 'screens/home_screen.dart';
import 'services/hot_zone_config_persistence.dart';
import 'services/screen_connections.dart';
import 'services/settings_store.dart';
import 'state/hot_zone_config_store.dart';
import 'theme/app_theme.dart';

typedef PollIncomingZone = IncomingZonePush? Function();

void main() {
  // Só escuta e responde via unicast — não manda broadcast, então não
  // deveria disparar o prompt de permissão de Rede Local por si só (ver
  // AltCrossNative.startDiscoveryResponder). É isso que faz esta máquina
  // aparecer quando outra roda "Buscar dispositivos".
  AltCrossNative.startDiscoveryResponder(name: Platform.localHostname);

  // Sobe o respondedor de pareamento também — sem isso, ninguém consegue
  // parear com esta máquina (só descobrir que ela existe).
  AltCrossNative.startPairingResponder(name: Platform.localHostname);

  // E o respondedor de sincronização de telas — sem isso, ninguém consegue
  // perguntar quais são as telas físicas reais desta máquina, nem avisar
  // que conectou uma borda numa delas. Só responde perguntas/avisos
  // explícitos, não captura input nenhum — seguro de ligar automaticamente
  // (mesma categoria de descoberta/pareamento).
  AltCrossNative.startScreenSyncResponder(name: Platform.localHostname);

  final store = HotZoneConfigStore();
  // carrega as hotzones salvas de uma sessão anterior ANTES de plugar
  // onChanged — senão cada zona carregada dispararia uma gravação
  // desnecessária (inofensiva, mas redundante) do mesmo conteúdo.
  for (final zone in HotZoneConfigPersistence.load()) {
    try {
      store.add(zone);
    } on StateError {
      /* arquivo salvo com 2 zonas habilitadas na mesma borda — ignora a
       * duplicata em vez de travar a inicialização do app. */
    }
  }
  store.onChanged = HotZoneConfigPersistence.save;

  runApp(AltCrossApp(store: store));
}

ThemeMode _toThemeMode(AppThemePreference preference) {
  switch (preference) {
    case AppThemePreference.light:
      return ThemeMode.light;
    case AppThemePreference.dark:
      return ThemeMode.dark;
    case AppThemePreference.system:
      return ThemeMode.system;
  }
}

class AltCrossApp extends StatefulWidget {
  final HotZoneConfigStore store;
  final List<PhysicalDisplay> Function()? displayProvider;
  final DiscoveryRunner? discoveryRunner;
  final SendPairingRequest? sendPairingRequest;
  final ConfirmPairing? confirmPairing;
  final PollIncomingPairingRequest? pollIncomingPairingRequest;
  final PollPairingCompleted? pollPairingCompleted;
  final PollIncomingZone pollIncomingZone;
  final LoadThemePreference loadThemePreference;
  final SaveThemePreference saveThemePreference;

  const AltCrossApp({
    super.key,
    required this.store,
    this.displayProvider,
    this.discoveryRunner,
    this.sendPairingRequest,
    this.confirmPairing,
    this.pollIncomingPairingRequest,
    this.pollPairingCompleted,
    PollIncomingZone? pollIncomingZone,
    LoadThemePreference? loadThemePreference,
    SaveThemePreference? saveThemePreference,
  })  : pollIncomingZone = pollIncomingZone ?? AltCrossNative.pollIncomingZone,
        loadThemePreference =
            loadThemePreference ?? SettingsStore.loadThemePreference,
        saveThemePreference =
            saveThemePreference ?? SettingsStore.saveThemePreference;

  @override
  State<AltCrossApp> createState() => _AltCrossAppState();
}

class _AltCrossAppState extends State<AltCrossApp> {
  late AppThemePreference _themePreference;
  Timer? _incomingZonePollTimer;

  @override
  void initState() {
    super.initState();
    _themePreference = widget.loadThemePreference();
    // Vive no nível do app (não numa tela específica) — a sincronização de
    // arranjo entre hosts precisa funcionar mesmo com o usuário na Home ou
    // em Ajustes, não só quando o Arranjo/Conexões está aberto.
    _incomingZonePollTimer = Timer.periodic(
        const Duration(milliseconds: 700), (_) => _pollIncomingZone());
  }

  @override
  void dispose() {
    _incomingZonePollTimer?.cancel();
    super.dispose();
  }

  void _pollIncomingZone() {
    final push = widget.pollIncomingZone();
    if (push == null) {
      return;
    }
    try {
      widget.store.add(HotZoneConfig(
        edge: oppositeEdge(push.senderEdge),
        targetDeviceId: push.senderDeviceId,
        enabled: true,
      ));
    } on StateError {
      /* já existe uma hotzone habilitada nessa borda minha — o outro lado
       * mandou de novo (ex.: reenvio), não travar por isso. */
    }
  }

  void _changeThemePreference(AppThemePreference preference) {
    widget.saveThemePreference(preference);
    setState(() => _themePreference = preference);
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AltCross',
      theme: AppTheme.light(),
      darkTheme: AppTheme.dark(),
      themeMode: _toThemeMode(_themePreference),
      home: HomeScreen(
        store: widget.store,
        displayProvider: widget.displayProvider,
        discoveryRunner: widget.discoveryRunner,
        sendPairingRequest: widget.sendPairingRequest,
        confirmPairing: widget.confirmPairing,
        pollIncomingPairingRequest: widget.pollIncomingPairingRequest,
        pollPairingCompleted: widget.pollPairingCompleted,
        currentThemePreference: _themePreference,
        onThemePreferenceChanged: _changeThemePreference,
      ),
    );
  }
}
