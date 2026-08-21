import 'package:flutter/material.dart';

import '../models/hot_zone.dart';
import '../models/physical_display.dart';
import '../native/altcross_native.dart';
import '../services/arrangement.dart';
import '../state/hot_zone_config_store.dart';

/// Tamanho padrão dado a um dispositivo remoto recém-adicionado, até o
/// pareamento de verdade trocar a resolução real da tela dele (ver
/// "Status da funcionalidade" no AGENTS.md — pareamento ainda não manda essa
/// informação). Usa uma resolução comum (Full HD) só pra ficar em escala
/// parecida com telas físicas de verdade no canvas.
const _defaultRemoteSize = Size(1920, 1080);
const _edgeGap = 160.0;
const _canvasMargin = 240.0;

/// Fundo escuro fixo do canvas de arranjo — de propósito independente do
/// tema claro/escuro do resto do app (ver Ajustes): é uma metáfora visual
/// deliberada ("olhar suas telas de cima, num painel escuro"), igual o
/// painel de Arranjo de Monitores do próprio macOS/Windows, que também não
/// muda com o tema do sistema.
const _canvasBg = Color(0xFF0E1226);

/// Cores de destaque da borda de conexão entre 2 telas — alternam por
/// dispositivo, reaproveitando a paleta da marca (ver lib/theme/app_theme.dart)
/// em vez de cores genéricas, mas mantendo o efeito "linha acesa" do painel
/// nativo.
const _connectionColors = [Color(0xFF2BE5CC), Color(0xFF9385F5), Color(0xFFFFB86B)];

class _DeviceBox {
  final String deviceId;
  Offset position;
  final Size size;

  _DeviceBox({
    required this.deviceId,
    required this.position,
    required this.size,
  });

  Rect get rect => position & size;
}

/// Tela de arranjo de monitores: mostra as telas físicas reais desta máquina
/// (via Core em C, `AltCrossNative.enumerateDisplays`) e as dos dispositivos
/// já configurados. Tudo é posicionado **manualmente**, arrastando — igual o
/// painel de Arranjo de Monitores nativo do macOS/Windows: nada gruda numa
/// borda sozinho, é o usuário que decide onde cada tela fica ao arrastar.
class ArrangementScreen extends StatefulWidget {
  final HotZoneConfigStore store;
  final List<PhysicalDisplay> Function() displayProvider;

  const ArrangementScreen({
    super.key,
    required this.store,
    List<PhysicalDisplay> Function()? displayProvider,
  }) : displayProvider = displayProvider ?? AltCrossNative.enumerateDisplays;

  @override
  State<ArrangementScreen> createState() => _ArrangementScreenState();
}

class _ArrangementScreenState extends State<ArrangementScreen> {
  late final List<PhysicalDisplay> _localDisplays;
  late final Rect _localBounds;
  final List<_DeviceBox> _boxes = [];
  String? _message;

  @override
  void initState() {
    super.initState();
    _localDisplays = widget.displayProvider();
    _localBounds = _boundsOf(_localDisplays);
    for (final zone in widget.store.zones) {
      _boxes.add(_DeviceBox(
        deviceId: zone.targetDeviceId,
        position: _anchorFor(zone.edge, _defaultRemoteSize),
        size: _defaultRemoteSize,
      ));
    }
  }

  static Rect _boundsOf(List<PhysicalDisplay> displays) {
    if (displays.isEmpty) {
      return Rect.zero;
    }
    var rect = displays.first.rect;
    for (final display in displays.skip(1)) {
      rect = rect.expandToInclude(display.rect);
    }
    return rect;
  }

  /// Só usado pra posicionar uma zona já salva (de uma sessão anterior) num
  /// lugar plausível ao reabrir a tela — não é usado mais como "encaixe
  /// automático" ao adicionar um dispositivo novo (isso agora é sempre
  /// manual, ver `_placeNewDevice`).
  Offset _anchorFor(HotZoneEdge edge, Size size) {
    switch (edge) {
      case HotZoneEdge.right:
        return Offset(_localBounds.right + _edgeGap,
            _localBounds.center.dy - size.height / 2);
      case HotZoneEdge.left:
        return Offset(_localBounds.left - _edgeGap - size.width,
            _localBounds.center.dy - size.height / 2);
      case HotZoneEdge.top:
        return Offset(_localBounds.center.dx - size.width / 2,
            _localBounds.top - _edgeGap - size.height);
      case HotZoneEdge.bottom:
        return Offset(
            _localBounds.center.dx - size.width / 2, _localBounds.bottom + _edgeGap);
      case HotZoneEdge.topLeft:
        return Offset(_localBounds.left - _edgeGap - size.width,
            _localBounds.top - _edgeGap - size.height);
      case HotZoneEdge.topRight:
        return Offset(
            _localBounds.right + _edgeGap, _localBounds.top - _edgeGap - size.height);
      case HotZoneEdge.bottomLeft:
        return Offset(
            _localBounds.left - _edgeGap - size.width, _localBounds.bottom + _edgeGap);
      case HotZoneEdge.bottomRight:
        return Offset(_localBounds.right + _edgeGap, _localBounds.bottom + _edgeGap);
      case HotZoneEdge.none:
        return Offset(
            _localBounds.right + _edgeGap * 2, _localBounds.bottom + _edgeGap * 2);
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

  /// Um dispositivo novo sempre aparece flutuando, sem tocar em nenhuma
  /// borda — nada é decidido por padrão. É arrastando até encostar numa
  /// borda das suas telas que o usuário define a hotzone, igual no painel
  /// nativo do macOS/Windows.
  void _placeNewDevice(String deviceId) {
    final box = _DeviceBox(
      deviceId: deviceId,
      position: _anchorFor(HotZoneEdge.none, _defaultRemoteSize),
      size: _defaultRemoteSize,
    );
    setState(() {
      _boxes.add(box);
      _message = '$deviceId: arraste até encostar numa borda das suas telas.';
    });
  }

  void _tryCommit(_DeviceBox box, HotZoneEdge edge) {
    widget.store.remove(box.deviceId);
    if (edge == HotZoneEdge.none) {
      _message = '${box.deviceId}: arraste até encostar numa borda das suas telas.';
      return;
    }
    try {
      widget.store.add(HotZoneConfig(
        edge: edge,
        targetDeviceId: box.deviceId,
        enabled: true,
      ));
      _message = null;
    } on StateError catch (e) {
      _message = e.message;
    } on ArgumentError {
      _message = 'Borda inválida.';
    }
  }

  void _removeDevice(String deviceId) {
    setState(() {
      _boxes.removeWhere((box) => box.deviceId == deviceId);
      widget.store.remove(deviceId);
    });
  }

  Rect _totalBounds() {
    var rect = _localBounds;
    for (final box in _boxes) {
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
              'Arraste os dispositivos até encostar numa borda das suas telas.',
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
                      final totalBounds = _totalBounds();
                      final scale = _scaleFor(constraints, totalBounds);
                      return Stack(
                        children: [
                          for (final display in _localDisplays)
                            _buildFixedDisplay(display, totalBounds, scale),
                          for (var i = 0; i < _boxes.length; i++)
                            ..._buildDeviceBoxWithHighlight(
                                _boxes[i], i, totalBounds, scale),
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

  Widget _buildFixedDisplay(
      PhysicalDisplay display, Rect totalBounds, double scale) {
    final screenRect = _toScreenRect(display.rect, totalBounds, scale);
    final label = display.isPrimary
        ? '${display.width.round()}×${display.height.round()}\nprincipal'
        : '${display.width.round()}×${display.height.round()}';
    return Positioned.fromRect(
      rect: screenRect,
      child: _MonitorFrame(label: label),
    );
  }

  List<Widget> _buildDeviceBoxWithHighlight(
      _DeviceBox box, int index, Rect totalBounds, double scale) {
    final edge = detectTouchingEdge(_localBounds, box.rect);
    final color = _connectionColors[index % _connectionColors.length];
    final widgets = <Widget>[];

    final highlightRect = _highlightRectFor(edge);
    if (highlightRect != null) {
      widgets.add(Positioned.fromRect(
        rect: _toScreenRect(highlightRect, totalBounds, scale),
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: color,
            boxShadow: [BoxShadow(color: color.withValues(alpha: 0.6), blurRadius: 12)],
          ),
        ),
      ));
    }

    final screenRect = _toScreenRect(box.rect, totalBounds, scale);
    widgets.add(Positioned.fromRect(
      key: Key('device-box-${box.deviceId}'),
      rect: screenRect,
      child: GestureDetector(
        onPanUpdate: (details) {
          setState(() => box.position += details.delta / scale);
        },
        onPanEnd: (_) {
          final edge = detectTouchingEdge(_localBounds, box.rect);
          setState(() => _tryCommit(box, edge));
        },
        child: _MonitorFrame(
          label: box.deviceId,
          accent: edge != HotZoneEdge.none ? color : null,
          trailing: IconButton(
            key: Key('remove-device-${box.deviceId}'),
            icon: const Icon(Icons.close, size: 16, color: Color(0xFFAEB4D6)),
            tooltip: 'Remover',
            onPressed: () => _removeDevice(box.deviceId),
          ),
        ),
      ),
    ));

    return widgets;
  }

  static const _highlightThickness = 10.0;

  Rect? _highlightRectFor(HotZoneEdge edge) {
    final t = _highlightThickness;
    switch (edge) {
      case HotZoneEdge.top:
        return Rect.fromLTWH(
            _localBounds.left, _localBounds.top - t / 2, _localBounds.width, t);
      case HotZoneEdge.bottom:
        return Rect.fromLTWH(_localBounds.left, _localBounds.bottom - t / 2,
            _localBounds.width, t);
      case HotZoneEdge.left:
        return Rect.fromLTWH(
            _localBounds.left - t / 2, _localBounds.top, t, _localBounds.height);
      case HotZoneEdge.right:
        return Rect.fromLTWH(_localBounds.right - t / 2, _localBounds.top, t,
            _localBounds.height);
      case HotZoneEdge.topLeft:
        return Rect.fromCenter(
            center: _localBounds.topLeft, width: t * 2, height: t * 2);
      case HotZoneEdge.topRight:
        return Rect.fromCenter(
            center: _localBounds.topRight, width: t * 2, height: t * 2);
      case HotZoneEdge.bottomLeft:
        return Rect.fromCenter(
            center: _localBounds.bottomLeft, width: t * 2, height: t * 2);
      case HotZoneEdge.bottomRight:
        return Rect.fromCenter(
            center: _localBounds.bottomRight, width: t * 2, height: t * 2);
      case HotZoneEdge.none:
        return null;
    }
  }
}

/// Moldura de monitor "vista de cima", igual o painel nativo de Arranjo de
/// Monitores do macOS/Windows: bisel em gradiente claro simulando luz vindo
/// de cima-esquerda, e a "tela" escura por dentro. `accent`, quando não nulo,
/// pinta a moldura na cor da conexão ativa daquele lado.
class _MonitorFrame extends StatelessWidget {
  final String label;
  final Color? accent;
  final Widget? trailing;

  const _MonitorFrame({required this.label, this.accent, this.trailing});

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(6),
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: accent != null
              ? [accent!.withValues(alpha: 0.95), accent!.withValues(alpha: 0.55)]
              : const [Color(0xFFEDEBF5), Color(0xFF7B7695), Color(0xFF524C6B)],
          stops: accent != null ? const [0.0, 1.0] : const [0.0, 0.55, 1.0],
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
