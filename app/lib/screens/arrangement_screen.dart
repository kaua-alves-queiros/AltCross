import 'dart:io';

import 'package:flutter/material.dart';

import '../models/hot_zone.dart';
import '../models/physical_display.dart';
import '../native/altcross_native.dart';
import '../services/screen_connections.dart';
import '../state/hot_zone_config_store.dart';

typedef LookupTrustedHost = String? Function(String deviceId);
typedef QueryPeerScreens = Future<List<PhysicalDisplay>> Function(
    String peerHost);
typedef PushZoneToPeer = bool Function({
  required String peerHost,
  required String myName,
  required HotZoneEdge myEdge,
  required int targetScreenIndex,
});
typedef SetLocalDisplayOrigin = bool Function({
  required int displayId,
  required int x,
  required int y,
});

/// Tamanho só de ENQUANTO a tela real de um dispositivo remoto ainda não foi
/// confirmada pela rede (carregando, nunca pareado, ou offline agora) — o
/// label do box sempre deixa claro qual desses 3 casos é, nunca finge que
/// esse número é a resolução real dele (ver AGENTS.md: nada de chutar
/// resolução — a tela de arranjo tem que buscar de verdade via
/// `AltCrossNative.queryPeerScreens`/screen_sync_protocol.h).
const _unknownRemoteSize = Size(420, 260);
const _edgeGap = 160.0;
const _canvasMargin = 240.0;
const _edgeHotspotThickness = 18.0;
const _highlightThickness = 10.0;

/// Fundo escuro fixo do canvas de arranjo — de propósito independente do
/// tema claro/escuro do resto do app (ver Ajustes): é uma metáfora visual
/// deliberada ("olhar suas telas de cima, num painel escuro"), igual o
/// painel de Arranjo de Monitores do próprio macOS/Windows, que também não
/// muda com o tema do sistema.
const _canvasBg = Color(0xFF0E1226);

/// Cores de destaque de conexão — uma por par conectado, reaproveitando a
/// paleta da marca (ver lib/theme/app_theme.dart) em vez de cores genéricas.
const _connectionColors = [Color(0xFF2BE5CC), Color(0xFF9385F5), Color(0xFFFFB86B)];
const _pendingColor = Color(0xFFFFFFFF);

const _clickableEdges = [
  HotZoneEdge.top,
  HotZoneEdge.bottom,
  HotZoneEdge.left,
  HotZoneEdge.right,
];

/// Uma tela no canvas — física desta máquina ou de um dispositivo remoto.
/// As duas são igualmente arrastáveis: o usuário organiza tudo do jeito que
/// quiser, tanto as telas dos outros hosts quanto as próprias.
class _ScreenBox {
  String label;
  final String? deviceId; // null = tela física local, não removível
  Offset position;
  Size size;

  /// Id estável do monitor físico (ver `PhysicalDisplay.displayId`) — só
  /// telas locais têm um (é o que permite `setLocalDisplayOrigin`
  /// reposicionar de verdade); telas remotas ficam com null porque nunca
  /// dá pra mover um monitor de outra máquina daqui.
  final int? displayId;

  _ScreenBox({
    required this.label,
    required this.position,
    required this.size,
    this.deviceId,
    this.displayId,
  });

  bool get isLocal => deviceId == null;
  Rect get rect => position & size;
}

/// Uma conexão de verdade entre a borda de uma tela e a borda de outra —
/// é isso (não proximidade/arrasto) que define por onde o mouse passa de
/// uma pra outra. Quando uma das duas é uma tela local, vira uma
/// `HotZoneConfig` no store; entre 2 telas locais é só visual.
class _Connection {
  final _ScreenBox boxA;
  final HotZoneEdge edgeA;
  final _ScreenBox boxB;
  final HotZoneEdge edgeB;
  final Color color;

  _Connection({
    required this.boxA,
    required this.edgeA,
    required this.boxB,
    required this.edgeB,
    required this.color,
  });

  bool uses(_ScreenBox box, HotZoneEdge edge) =>
      (identical(boxA, box) && edgeA == edge) ||
      (identical(boxB, box) && edgeB == edge);

  bool references(_ScreenBox box) => identical(boxA, box) || identical(boxB, box);
}

class _PendingSelection {
  final _ScreenBox box;
  final HotZoneEdge edge;

  _PendingSelection({required this.box, required this.edge});
}

/// Tela de arranjo de monitores: mostra as telas físicas reais desta máquina
/// (via Core em C, `AltCrossNative.enumerateDisplays`) e as dos dispositivos
/// já configurados. Arrastar organiza a posição (tanto telas locais quanto
/// remotas, do jeito que o usuário quiser); quem define de fato por onde o
/// mouse passa de uma tela pra outra é tocar numa borda e depois na borda da
/// outra tela — igual escolher manualmente no painel nativo, sem depender de
/// arrasto impreciso "encostando" sozinho.
class ArrangementScreen extends StatefulWidget {
  final HotZoneConfigStore store;
  final List<PhysicalDisplay> Function() displayProvider;
  final LookupTrustedHost lookupHost;
  final QueryPeerScreens queryPeerScreens;
  final PushZoneToPeer pushZoneToPeer;
  final SetLocalDisplayOrigin setLocalDisplayOrigin;

  ArrangementScreen({
    super.key,
    required this.store,
    List<PhysicalDisplay> Function()? displayProvider,
    LookupTrustedHost? lookupHost,
    QueryPeerScreens? queryPeerScreens,
    PushZoneToPeer? pushZoneToPeer,
    SetLocalDisplayOrigin? setLocalDisplayOrigin,
  })  : displayProvider = displayProvider ?? AltCrossNative.enumerateDisplays,
        lookupHost = lookupHost ?? AltCrossNative.lookupTrustedHost,
        queryPeerScreens = queryPeerScreens ??
            ((host) => AltCrossNative.queryPeerScreens(peerHost: host)),
        pushZoneToPeer = pushZoneToPeer ?? AltCrossNative.pushZoneToPeer,
        setLocalDisplayOrigin =
            setLocalDisplayOrigin ?? AltCrossNative.setLocalDisplayOrigin;

  @override
  State<ArrangementScreen> createState() => _ArrangementScreenState();
}

class _ArrangementScreenState extends State<ArrangementScreen> {
  final List<_ScreenBox> _localBoxes = [];
  final List<_ScreenBox> _remoteBoxes = [];
  final List<_Connection> _connections = [];
  _PendingSelection? _pendingSelection;
  String? _message;

  // Zoom/enquadramento travado durante um arraste — sem isso, mover uma
  // tela muda o retângulo total, que muda a escala, e todas as telas
  // parecem se mexer sozinhas.
  Rect? _frozenTotalBounds;
  double? _frozenScale;

  @override
  void initState() {
    super.initState();
    for (final display in widget.displayProvider()) {
      _localBoxes.add(_ScreenBox(
        label: display.isPrimary
            ? '${display.width.round()}×${display.height.round()}\nprincipal'
            : '${display.width.round()}×${display.height.round()}',
        position: Offset(display.x, display.y),
        size: Size(display.width, display.height),
        displayId: display.displayId,
      ));
    }
    final seenDeviceIds = <String>{};
    for (final zone in widget.store.zones) {
      final firstBoxForDevice = seenDeviceIds.add(zone.targetDeviceId);
      final remoteBox = firstBoxForDevice
          ? _ScreenBox(
              deviceId: zone.targetDeviceId,
              label: '${zone.targetDeviceId}\ncarregando tela real…',
              position: _anchorFor(zone.edge, _unknownRemoteSize),
              size: _unknownRemoteSize,
            )
          : _remoteBoxes.firstWhere((b) => b.deviceId == zone.targetDeviceId);
      if (firstBoxForDevice) {
        _remoteBoxes.add(remoteBox);
        // Adiado pro pós-frame — chamar setState (dentro de
        // _fetchRealScreens) de forma síncrona ainda dentro de initState
        // bate no assert "setState() or markNeedsBuild() called during
        // build" do Flutter, já que o framework considera montagem+build
        // do elemento uma coisa só.
        WidgetsBinding.instance.addPostFrameCallback(
            (_) => _fetchRealScreens(zone.targetDeviceId));
      }
      final localBox = _firstLocalBoxForOuterEdge(zone.edge);
      if (localBox != null) {
        _connections.add(_Connection(
          boxA: localBox,
          edgeA: zone.edge,
          boxB: remoteBox,
          edgeB: oppositeEdge(zone.edge),
          color: _nextColor(),
        ));
      }
    }
  }

  /// Busca de verdade, pela rede, as telas físicas atuais de um dispositivo
  /// já pareado (nunca um tamanho chutado) — atualiza o box quando a
  /// resposta chega, ou deixa claro no label quando não dá (nunca pareado /
  /// offline agora) em vez de fingir uma resolução.
  Future<void> _fetchRealScreens(String deviceId) async {
    final host = widget.lookupHost(deviceId);
    if (host == null) {
      if (!mounted) return;
      setState(() => _relabelRemoteBox(
          deviceId, '$deviceId\n(nunca pareado — sem tela real conhecida)'));
      return;
    }

    final displays = await widget.queryPeerScreens(host);
    if (!mounted) return;
    if (displays.isEmpty) {
      setState(() => _relabelRemoteBox(
          deviceId, '$deviceId\n(offline agora — tela real desconhecida)'));
      return;
    }

    setState(() {
      final box = _remoteBoxes.firstWhere((b) => b.deviceId == deviceId,
          orElse: () => _ScreenBox(
              deviceId: deviceId, label: deviceId, position: Offset.zero, size: _unknownRemoteSize));
      var bounds = displays.first.rect;
      for (final d in displays.skip(1)) {
        bounds = bounds.expandToInclude(d.rect);
      }
      box.size = bounds.size;
      box.label = displays.length > 1
          ? '$deviceId\n${bounds.width.round()}×${bounds.height.round()}'
              ' · ${displays.length} telas'
          : '$deviceId\n${bounds.width.round()}×${bounds.height.round()}';
    });
  }

  void _relabelRemoteBox(String deviceId, String label) {
    for (final box in _remoteBoxes) {
      if (box.deviceId == deviceId) {
        box.label = label;
      }
    }
  }

  /// A "tela local" pra fins de hotzone é o retângulo que engloba todos os
  /// seus monitores físicos — reflete a posição atual deles, então arrastar
  /// um monitor local também move essa referência (ver Core:
  /// altcross_screen_config_t só conhece uma tela local combinada por vez).
  Rect get _localBounds {
    if (_localBoxes.isEmpty) {
      return Rect.zero;
    }
    var rect = _localBoxes.first.rect;
    for (final box in _localBoxes.skip(1)) {
      rect = rect.expandToInclude(box.rect);
    }
    return rect;
  }

  _ScreenBox? _firstLocalBoxForOuterEdge(HotZoneEdge edge) {
    for (final box in _localBoxes) {
      if (isOuterEdge(box.rect, _localBounds, edge)) {
        return box;
      }
    }
    return null;
  }

  Color _nextColor() => _connectionColors[_connections.length % _connectionColors.length];

  /// Só usado pra posicionar uma zona já salva (de uma sessão anterior) ou
  /// um dispositivo novo num lugar plausível no canvas — não define nada
  /// sozinho. É tocar nas bordas que conecta de verdade (ver `_onEdgeTapped`).
  Offset _anchorFor(HotZoneEdge edge, Size size) {
    final bounds = _localBounds;
    switch (edge) {
      case HotZoneEdge.right:
        return Offset(
            bounds.right + _edgeGap, bounds.center.dy - size.height / 2);
      case HotZoneEdge.left:
        return Offset(bounds.left - _edgeGap - size.width,
            bounds.center.dy - size.height / 2);
      case HotZoneEdge.top:
        return Offset(
            bounds.center.dx - size.width / 2, bounds.top - _edgeGap - size.height);
      case HotZoneEdge.bottom:
        return Offset(bounds.center.dx - size.width / 2, bounds.bottom + _edgeGap);
      case HotZoneEdge.topLeft:
        return Offset(bounds.left - _edgeGap - size.width,
            bounds.top - _edgeGap - size.height);
      case HotZoneEdge.topRight:
        return Offset(bounds.right + _edgeGap, bounds.top - _edgeGap - size.height);
      case HotZoneEdge.bottomLeft:
        return Offset(bounds.left - _edgeGap - size.width, bounds.bottom + _edgeGap);
      case HotZoneEdge.bottomRight:
        return Offset(bounds.right + _edgeGap, bounds.bottom + _edgeGap);
      case HotZoneEdge.none:
        return Offset(bounds.right + _edgeGap * 2, bounds.bottom + _edgeGap * 2);
    }
  }

  void _openAddDialog() {
    final controller = TextEditingController();
    showDialog<void>(
      context: context,
      builder: (dialogContext) {
        return AlertDialog(
          title: const Text('Novo dispositivo'),
          content: TextField(
            key: const Key('device-id-field'),
            controller: controller,
            autofocus: true,
            decoration: const InputDecoration(
              labelText: 'ID do dispositivo',
              hintText: 'ex.: pc-windows',
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(dialogContext).pop(),
              child: const Text('Cancelar'),
            ),
            FilledButton(
              onPressed: () {
                final deviceId = controller.text.trim();
                if (deviceId.isEmpty) {
                  return;
                }
                Navigator.of(dialogContext).pop();
                _placeNewDevice(deviceId);
              },
              child: const Text('Adicionar'),
            ),
          ],
        );
      },
    );
  }

  /// Um dispositivo novo sempre aparece flutuando — nada é decidido por
  /// padrão. É tocando numa borda de uma tela local e depois numa borda
  /// dele que o usuário conecta de fato.
  void _placeNewDevice(String deviceId) {
    // cada dispositivo novo cai um pouco deslocado do anterior, senão todo
    // mundo cai exatamente empilhado no mesmo lugar e um cobre o outro.
    final cascade = Offset(
        _remoteBoxes.length * 700.0, _remoteBoxes.length * 700.0);
    final box = _ScreenBox(
      deviceId: deviceId,
      label: '$deviceId\n(nunca pareado — sem tela real conhecida)',
      position: _anchorFor(HotZoneEdge.none, _unknownRemoteSize) + cascade,
      size: _unknownRemoteSize,
    );
    setState(() {
      _remoteBoxes.add(box);
      _message =
          '$deviceId: toque numa borda de uma tela local e depois numa borda dele pra conectar.';
    });
  }

  void _onEdgeTapped(_ScreenBox box, HotZoneEdge edge) {
    final pending = _pendingSelection;
    if (pending == null) {
      setState(() => _pendingSelection = _PendingSelection(box: box, edge: edge));
      return;
    }
    if (identical(pending.box, box) && pending.edge == edge) {
      setState(() => _pendingSelection = null);
      return;
    }
    setState(() {
      _pendingSelection = null;
      _connect(pending.box, pending.edge, box, edge);
    });
  }

  void _connect(_ScreenBox boxA, HotZoneEdge edgeA, _ScreenBox boxB, HotZoneEdge edgeB) {
    if (identical(boxA, boxB)) {
      _message = 'Escolha bordas de 2 telas diferentes.';
      return;
    }
    if (!boxA.isLocal && !boxB.isLocal) {
      _message =
          'Conecte a borda de uma tela sua a um dispositivo remoto — não dois dispositivos remotos entre si.';
      return;
    }
    if (boxA.isLocal && boxB.isLocal) {
      if (_repositionLocalBox(boxA, boxB, edgeB)) {
        _replaceConnection(boxA, edgeA, boxB, edgeB);
        _message = null;
      }
      return;
    }

    final aIsLocal = boxA.isLocal;
    final localBox = aIsLocal ? boxA : boxB;
    final localEdge = aIsLocal ? edgeA : edgeB;
    final remoteBox = aIsLocal ? boxB : boxA;
    final remoteEdge = aIsLocal ? edgeB : edgeA;

    if (!isOuterEdge(localBox.rect, _localBounds, localEdge)) {
      _message =
          'Essa borda é interna, entre suas próprias telas — escolha uma borda externa pra conectar num dispositivo.';
      return;
    }

    widget.store.remove(remoteBox.deviceId!);
    try {
      widget.store.add(HotZoneConfig(
        edge: localEdge,
        targetDeviceId: remoteBox.deviceId!,
        enabled: true,
      ));
      _message = null;
      _replaceConnection(localBox, localEdge, remoteBox, remoteEdge);
      _pushZoneIfPaired(remoteBox.deviceId!, localEdge);
    } on StateError catch (e) {
      _message = e.message;
    }
  }

  /// Avisa o dispositivo remoto de verdade, pela rede, que acabei de
  /// conectar minha borda nele — é isso que sincroniza o arranjo nos 2
  /// lados (ver screen_sync_protocol.h/pollIncomingZone) em vez de cada
  /// host precisar configurar a mesma ligação manualmente 2 vezes. Sem
  /// host conhecido (dispositivo nunca pareado, só digitado manualmente)
  /// não tem pra onde mandar — fica só configurado deste lado mesmo.
  void _pushZoneIfPaired(String deviceId, HotZoneEdge localEdge) {
    final host = widget.lookupHost(deviceId);
    if (host == null) {
      return;
    }
    widget.pushZoneToPeer(
      peerHost: host,
      myName: Platform.localHostname,
      myEdge: localEdge,
      targetScreenIndex: 0,
    );
  }

  /// Encosta `moving` de verdade, no sistema operacional, na borda `moving`
  /// que foi tocada — reposiciona o monitor físico real (não é só desenhar
  /// mais perto no canvas), por isso "conectar 2 telas locais" agora arruma
  /// de verdade em vez de ser só visual. Retorna false (e deixa `_message`
  /// com o motivo) se não deu pra mover de verdade — nesse caso NENHUMA
  /// conexão visual é registrada, pra não fingir que funcionou.
  bool _repositionLocalBox(
      _ScreenBox anchor, _ScreenBox moving, HotZoneEdge movingEdge) {
    final displayId = moving.displayId;
    if (displayId == null) {
      _message =
          'Não sei qual monitor é esse (sem id de tela) — não dá pra mover de verdade.';
      return false;
    }

    double newX = moving.position.dx;
    double newY = moving.position.dy;
    switch (movingEdge) {
      case HotZoneEdge.left:
        newX = anchor.rect.right;
        break;
      case HotZoneEdge.right:
        newX = anchor.rect.left - moving.size.width;
        break;
      case HotZoneEdge.top:
        newY = anchor.rect.bottom;
        break;
      case HotZoneEdge.bottom:
        newY = anchor.rect.top - moving.size.height;
        break;
      default:
        _message =
            'Só dá pra encostar telas locais pelas bordas retas (cima/baixo/esquerda/direita).';
        return false;
    }

    final ok = widget.setLocalDisplayOrigin(
      displayId: displayId,
      x: newX.round(),
      y: newY.round(),
    );
    if (!ok) {
      _message = 'O sistema operacional recusou mover esse monitor — no'
          ' macOS isso exige o App Sandbox desligado (ver'
          ' macos/Runner/*.entitlements).';
      return false;
    }

    _refreshLocalBoxesFromOs();
    return true;
  }

  /// Depois de mover um monitor de verdade, relê os monitores reais do SO
  /// (fonte da verdade) em vez de confiar na nossa própria conta — atualiza
  /// os `_ScreenBox` locais EXISTENTES no lugar (não recria a lista, pra
  /// não invalidar as referências que `_connections` já guarda).
  void _refreshLocalBoxesFromOs() {
    final fresh = widget.displayProvider();
    for (final display in fresh) {
      for (final box in _localBoxes) {
        if (box.displayId == display.displayId) {
          box.position = Offset(display.x, display.y);
          box.size = Size(display.width, display.height);
        }
      }
    }
  }

  void _replaceConnection(
      _ScreenBox boxA, HotZoneEdge edgeA, _ScreenBox boxB, HotZoneEdge edgeB) {
    _connections.removeWhere((c) => c.uses(boxA, edgeA) || c.uses(boxB, edgeB));
    _connections.add(_Connection(
        boxA: boxA, edgeA: edgeA, boxB: boxB, edgeB: edgeB, color: _nextColor()));
  }

  void _removeDevice(String deviceId) {
    setState(() {
      final box = _remoteBoxes.firstWhere((b) => b.deviceId == deviceId);
      _connections.removeWhere((c) => c.references(box));
      if (identical(_pendingSelection?.box, box)) {
        _pendingSelection = null;
      }
      _remoteBoxes.remove(box);
      widget.store.remove(deviceId);
    });
  }

  Rect _totalBounds() {
    var rect = _localBounds;
    for (final box in _remoteBoxes) {
      rect = rect.expandToInclude(box.rect);
    }
    return rect.inflate(_canvasMargin);
  }

  double _scaleFor(BoxConstraints constraints, Rect totalBounds) {
    if (totalBounds.width <= 0 || totalBounds.height <= 0) {
      return 1;
    }
    final scaleX = constraints.maxWidth / totalBounds.width;
    final scaleY = constraints.maxHeight / totalBounds.height;
    return scaleX < scaleY ? scaleX : scaleY;
  }

  Rect _toScreenRect(Rect rect, Rect totalBounds, double scale) {
    return Rect.fromLTWH(
      (rect.left - totalBounds.left) * scale,
      (rect.top - totalBounds.top) * scale,
      rect.width * scale,
      rect.height * scale,
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('AltCross')),
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (_message != null)
            Container(
              color: Theme.of(context).colorScheme.errorContainer,
              padding: const EdgeInsets.all(8),
              child: Text(_message!, textAlign: TextAlign.center),
            ),
          Padding(
            padding: const EdgeInsets.fromLTRB(24, 16, 24, 0),
            child: Row(
              children: [
                Text('Arranjo de telas',
                    style: Theme.of(context).textTheme.titleLarge),
                const Spacer(),
                IconButton.filled(
                  key: const Key('add-device-button'),
                  onPressed: _openAddDialog,
                  icon: const Icon(Icons.add),
                  tooltip: 'Adicionar dispositivo',
                ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(24, 8, 24, 0),
            child: Text(
              'Arraste pra organizar. Toque numa borda e depois na borda da'
              ' outra tela pra conectar de verdade.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: ColoredBox(
                  color: _canvasBg,
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      // Enquanto uma tela está sendo arrastada, o zoom/
                      // enquadramento fica TRAVADO no que já estava antes do
                      // arraste começar — sem isso, mover uma tela muda o
                      // retângulo total, que muda a escala, e todas as
                      // telas parecem se mexer sozinhas.
                      final totalBounds = _frozenTotalBounds ?? _totalBounds();
                      final scale =
                          _frozenScale ?? _scaleFor(constraints, totalBounds);
                      return Stack(
                        children: [
                          for (final box in _localBoxes)
                            _buildScreenBox(box, totalBounds, scale),
                          for (final box in _remoteBoxes)
                            _buildScreenBox(box, totalBounds, scale),
                          ..._buildConnectionMarkers(totalBounds, scale),
                        ],
                      );
                    },
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildScreenBox(_ScreenBox box, Rect totalBounds, double scale) {
    final screenRect = _toScreenRect(box.rect, totalBounds, scale);
    return Positioned.fromRect(
      key: Key(box.isLocal ? 'local-box-${box.label}' : 'device-box-${box.deviceId}'),
      rect: screenRect,
      child: Stack(
        children: [
          Positioned.fill(
            child: GestureDetector(
              onPanStart: (_) => setState(() {
                _frozenTotalBounds = totalBounds;
                _frozenScale = scale;
              }),
              onPanUpdate: (details) {
                final activeScale = _frozenScale ?? scale;
                setState(() => box.position += details.delta / activeScale);
              },
              onPanEnd: (_) => setState(() {
                _frozenTotalBounds = null;
                _frozenScale = null;
              }),
              child: _MonitorFrame(
                label: box.label,
                trailing: box.isLocal
                    ? null
                    : IconButton(
                        key: Key('remove-device-${box.deviceId}'),
                        icon: const Icon(Icons.close,
                            size: 16, color: Color(0xFFAEB4D6)),
                        tooltip: 'Remover',
                        onPressed: () => _removeDevice(box.deviceId!),
                      ),
              ),
            ),
          ),
          for (final edge in _clickableEdges) _buildEdgeHotspot(box, edge),
        ],
      ),
    );
  }

  Widget _buildEdgeHotspot(_ScreenBox box, HotZoneEdge edge) {
    final selected =
        identical(_pendingSelection?.box, box) && _pendingSelection?.edge == edge;
    final hotspot = MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        key: Key(
            '${box.isLocal ? 'local-box-${box.label}' : 'device-box-${box.deviceId}'}-edge-${edge.name}'),
        behavior: HitTestBehavior.opaque,
        onTap: () => _onEdgeTapped(box, edge),
        child: selected
            ? DecoratedBox(
                decoration:
                    BoxDecoration(color: _pendingColor.withValues(alpha: 0.35)))
            : const SizedBox.expand(),
      ),
    );

    switch (edge) {
      case HotZoneEdge.top:
        return Positioned(
            top: 0, left: 0, right: 0, height: _edgeHotspotThickness, child: hotspot);
      case HotZoneEdge.bottom:
        return Positioned(
            bottom: 0, left: 0, right: 0, height: _edgeHotspotThickness, child: hotspot);
      case HotZoneEdge.left:
        return Positioned(
            left: 0, top: 0, bottom: 0, width: _edgeHotspotThickness, child: hotspot);
      case HotZoneEdge.right:
        return Positioned(
            right: 0, top: 0, bottom: 0, width: _edgeHotspotThickness, child: hotspot);
      default:
        return const SizedBox.shrink();
    }
  }

  List<Widget> _buildConnectionMarkers(Rect totalBounds, double scale) {
    final widgets = <Widget>[
      for (final c in _connections) ...[
        _buildEdgeMarker(c.boxA.rect, c.edgeA, c.color, totalBounds, scale),
        _buildEdgeMarker(c.boxB.rect, c.edgeB, c.color, totalBounds, scale),
      ],
    ];
    final pending = _pendingSelection;
    if (pending != null) {
      widgets.add(_buildEdgeMarker(
          pending.box.rect, pending.edge, _pendingColor, totalBounds, scale));
    }
    return widgets;
  }

  Widget _buildEdgeMarker(
      Rect rect, HotZoneEdge edge, Color color, Rect totalBounds, double scale) {
    final markerRect = _edgeMarkerRect(rect, edge);
    return Positioned.fromRect(
      rect: _toScreenRect(markerRect, totalBounds, scale),
      child: IgnorePointer(
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: color,
            boxShadow: [BoxShadow(color: color.withValues(alpha: 0.6), blurRadius: 12)],
          ),
        ),
      ),
    );
  }

  Rect _edgeMarkerRect(Rect rect, HotZoneEdge edge) {
    const t = _highlightThickness;
    const shrink = 0.2;
    switch (edge) {
      case HotZoneEdge.top:
        final w = rect.width * (1 - shrink * 2);
        return Rect.fromLTWH(rect.left + rect.width * shrink, rect.top - t / 2, w, t);
      case HotZoneEdge.bottom:
        final w = rect.width * (1 - shrink * 2);
        return Rect.fromLTWH(
            rect.left + rect.width * shrink, rect.bottom - t / 2, w, t);
      case HotZoneEdge.left:
        final h = rect.height * (1 - shrink * 2);
        return Rect.fromLTWH(rect.left - t / 2, rect.top + rect.height * shrink, t, h);
      case HotZoneEdge.right:
        final h = rect.height * (1 - shrink * 2);
        return Rect.fromLTWH(
            rect.right - t / 2, rect.top + rect.height * shrink, t, h);
      case HotZoneEdge.topLeft:
        return Rect.fromCenter(center: rect.topLeft, width: t * 2, height: t * 2);
      case HotZoneEdge.topRight:
        return Rect.fromCenter(center: rect.topRight, width: t * 2, height: t * 2);
      case HotZoneEdge.bottomLeft:
        return Rect.fromCenter(center: rect.bottomLeft, width: t * 2, height: t * 2);
      case HotZoneEdge.bottomRight:
        return Rect.fromCenter(center: rect.bottomRight, width: t * 2, height: t * 2);
      case HotZoneEdge.none:
        return Rect.zero;
    }
  }
}

/// Moldura de monitor "vista de cima", igual o painel nativo de Arranjo de
/// Monitores do macOS/Windows: bisel em gradiente claro simulando luz vindo
/// de cima-esquerda, e a "tela" escura por dentro.
class _MonitorFrame extends StatelessWidget {
  final String label;
  final Widget? trailing;

  const _MonitorFrame({required this.label, this.trailing});

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(6),
        gradient: const LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0xFFEDEBF5), Color(0xFF7B7695), Color(0xFF524C6B)],
          stops: [0.0, 0.55, 1.0],
        ),
      ),
      padding: const EdgeInsets.all(13),
      child: Stack(
        children: [
          Container(
            decoration: BoxDecoration(
              color: const Color(0xFF161A34),
              borderRadius: BorderRadius.circular(2),
            ),
            alignment: Alignment.center,
            child: Text(
              label,
              textAlign: TextAlign.center,
              style: const TextStyle(
                color: Color(0xFF8A90B8),
                fontSize: 13,
                fontWeight: FontWeight.w600,
                height: 1.3,
              ),
            ),
          ),
          if (trailing != null)
            Positioned(right: 0, top: 0, child: trailing!),
        ],
      ),
    );
  }
}
