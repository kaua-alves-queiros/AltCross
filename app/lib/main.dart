import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';

import 'models/hot_zone.dart';
import 'models/physical_display.dart';
import 'models/screen_sync.dart';
import 'native/altcross_native.dart';
import 'screens/connections_screen.dart';
import 'screens/home_screen.dart';
import 'services/connection_status.dart';
import 'services/handoff_activation.dart';
import 'services/hot_zone_config_persistence.dart';
import 'services/local_hotzone_persistence.dart';
import 'services/local_hotzone_warp.dart';
import 'services/screen_connections.dart';
import 'services/settings_store.dart';
import 'state/hot_zone_config_store.dart';
import 'state/local_hotzone_store.dart';
import 'theme/app_theme.dart';
import 'widgets/connection_loss_overlay.dart';

typedef PollIncomingZone = IncomingZonePush? Function();
typedef GetCursorPosition = Offset? Function();
typedef WarpCursor = bool Function(int x, int y);

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

  final localHotZoneStore = LocalHotZoneStore();
  for (final mapping in LocalHotZonePersistence.load()) {
    localHotZoneStore.add(mapping);
  }
  localHotZoneStore.onChanged = LocalHotZonePersistence.save;

  final connectionStatus = ConnectionStatusNotifier();
  connectionStatus.start();

  runApp(AltCrossApp(
    store: store,
    localHotZoneStore: localHotZoneStore,
    connectionStatus: connectionStatus,
  ));
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
  final LocalHotZoneStore localHotZoneStore;
  final ConnectionStatusNotifier connectionStatus;
  final List<PhysicalDisplay> Function() displayProvider;
  final DiscoveryRunner? discoveryRunner;
  final SendPairingRequest? sendPairingRequest;
  final ConfirmPairing? confirmPairing;
  final PollIncomingPairingRequest? pollIncomingPairingRequest;
  final PollPairingCompleted? pollPairingCompleted;
  final PollIncomingZone pollIncomingZone;
  final GetCursorPosition getCursorPosition;
  final WarpCursor warpCursor;
  final LoadThemePreference loadThemePreference;
  final SaveThemePreference saveThemePreference;
  final LoadHandoffEnabled loadHandoffEnabled;
  final SaveHandoffEnabled saveHandoffEnabled;
  final LookupTrustedHost lookupHost;
  final QueryPeerScreens queryPeerScreens;
  final StartHandoff startHandoff;
  final IsHandoffRemote isHandoffActive;

  AltCrossApp({
    super.key,
    required this.store,
    required this.localHotZoneStore,
    required this.connectionStatus,
    List<PhysicalDisplay> Function()? displayProvider,
    this.discoveryRunner,
    this.sendPairingRequest,
    this.confirmPairing,
    this.pollIncomingPairingRequest,
    this.pollPairingCompleted,
    PollIncomingZone? pollIncomingZone,
    GetCursorPosition? getCursorPosition,
    WarpCursor? warpCursor,
    LoadThemePreference? loadThemePreference,
    SaveThemePreference? saveThemePreference,
    LoadHandoffEnabled? loadHandoffEnabled,
    SaveHandoffEnabled? saveHandoffEnabled,
    LookupTrustedHost? lookupHost,
    QueryPeerScreens? queryPeerScreens,
    StartHandoff? startHandoff,
    IsHandoffRemote? isHandoffActive,
  })  : displayProvider = displayProvider ?? AltCrossNative.enumerateDisplays,
        pollIncomingZone = pollIncomingZone ?? AltCrossNative.pollIncomingZone,
        getCursorPosition =
            getCursorPosition ?? AltCrossNative.getCursorPosition,
        warpCursor = warpCursor ?? AltCrossNative.warpCursor,
        loadThemePreference =
            loadThemePreference ?? SettingsStore.loadThemePreference,
        saveThemePreference =
            saveThemePreference ?? SettingsStore.saveThemePreference,
        loadHandoffEnabled =
            loadHandoffEnabled ?? SettingsStore.loadHandoffEnabled,
        saveHandoffEnabled =
            saveHandoffEnabled ?? SettingsStore.saveHandoffEnabled,
        lookupHost = lookupHost ?? AltCrossNative.lookupTrustedHost,
        queryPeerScreens = queryPeerScreens ??
            ((host) => AltCrossNative.queryPeerScreens(peerHost: host)),
        startHandoff = startHandoff ?? AltCrossNative.startHandoff,
        isHandoffActive = isHandoffActive ?? AltCrossNative.isHandoffActive;

  @override
  State<AltCrossApp> createState() => _AltCrossAppState();
}

class _AltCrossAppState extends State<AltCrossApp> {
  late AppThemePreference _themePreference;
  late bool _handoffEnabled;
  Timer? _incomingZonePollTimer;
  Timer? _cursorWarpTimer;

  @override
  void initState() {
    super.initState();
    _themePreference = widget.loadThemePreference();
    _handoffEnabled = widget.loadHandoffEnabled();
    // Vive no nível do app (não numa tela específica) — a sincronização de
    // arranjo entre hosts precisa funcionar mesmo com o usuário na Home ou
    // em Ajustes, não só quando o Arranjo/Conexões está aberto.
    _incomingZonePollTimer = Timer.periodic(
        const Duration(milliseconds: 700), (_) => _pollIncomingZone());
    // Mesma lógica: o salto de cursor entre telas locais tem que funcionar
    // o tempo todo que o app estiver aberto, não só na tela de Arranjo.
    // Intervalo curto (responsividade de um "hotzone" de verdade), mas é só
    // 1 leitura de posição + 1 enumeração local — leve, sem rede.
    _cursorWarpTimer = Timer.periodic(
        const Duration(milliseconds: 40), (_) => _checkCursorWarp());
    // Controle entre dispositivos é "sempre ligado" por padrão — tenta
    // ligar assim que o app abre, sem depender do usuário abrir a tela de
    // Conexões primeiro. Silencioso de propósito: sem zona pareada ainda
    // (1ª execução) isso falha sempre, e não faz sentido avisar disso antes
    // mesmo do usuário ter feito qualquer coisa — a tela de Conexões
    // (`ConnectionsScreen`, com o mesmo checkbox) continua tentando de novo
    // sozinha depois, e aí sim mostra o que está acontecendo.
    if (_handoffEnabled && !widget.isHandoffActive()) {
      activateHandoff(
        store: widget.store,
        lookupHost: widget.lookupHost,
        queryPeerScreens: widget.queryPeerScreens,
        localDisplaysProvider: widget.displayProvider,
        startHandoff: widget.startHandoff,
      );
    }
  }

  @override
  void dispose() {
    _incomingZonePollTimer?.cancel();
    _cursorWarpTimer?.cancel();
    super.dispose();
  }

  void _checkCursorWarp() {
    final mappings = widget.localHotZoneStore.mappings;
    if (mappings.isEmpty) {
      return;
    }
    final cursor = widget.getCursorPosition();
    if (cursor == null) {
      return;
    }
    final displaysById = <int, Rect>{
      for (final d in widget.displayProvider())
        if (d.displayId >= 0) d.displayId: d.rect,
    };
    final target = computeCursorWarp(
      cursor: cursor,
      displaysById: displaysById,
      mappings: mappings,
    );
    if (target != null) {
      widget.warpCursor(target.dx.round(), target.dy.round());
    }
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

  void _changeHandoffEnabled(bool enabled) {
    widget.saveHandoffEnabled(enabled);
    setState(() => _handoffEnabled = enabled);
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AltCross',
      theme: AppTheme.light(),
      darkTheme: AppTheme.dark(),
      themeMode: _toThemeMode(_themePreference),
      // ConnectionLossOverlay entra aqui (dentro do builder), não envolvendo
      // o MaterialApp por fora — o ScaffoldMessenger que ele precisa pra
      // mostrar o SnackBar é criado PELO MaterialApp, então o overlay
      // precisa estar por DENTRO da árvore dele, senão
      // `ScaffoldMessenger.of(context)` nunca acha nada e a notificação
      // nunca aparece (era exatamente isso que estava acontecendo).
      builder: (context, child) => ConnectionLossOverlay(
        notifier: widget.connectionStatus,
        child: child!,
      ),
      home: HomeScreen(
        store: widget.store,
        localHotZoneStore: widget.localHotZoneStore,
        displayProvider: widget.displayProvider,
        discoveryRunner: widget.discoveryRunner,
        sendPairingRequest: widget.sendPairingRequest,
        confirmPairing: widget.confirmPairing,
        pollIncomingPairingRequest: widget.pollIncomingPairingRequest,
        pollPairingCompleted: widget.pollPairingCompleted,
        restartConnectionMonitor: widget.connectionStatus.refresh,
        handoffEnabled: _handoffEnabled,
        onHandoffEnabledChanged: _changeHandoffEnabled,
        currentThemePreference: _themePreference,
        onThemePreferenceChanged: _changeThemePreference,
      ),
    );
  }
}
