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
const _edgeGap = 24.0;
const _canvasMargin = 240.0;

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
/// já configurados, deixando arrastar até encostar numa borda pra definir a
/// hotzone — como a tela de arranjo de monitores do macOS/Windows.
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
        return Offset(_localBounds.right + _edgeGap * 6, _localBounds.top);
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

  void _placeNewDevice(String deviceId) {
    final box = _DeviceBox(
      deviceId: deviceId,
      position: _anchorFor(HotZoneEdge.right, _defaultRemoteSize),
      size: _defaultRemoteSize,
    );
    setState(() {
      _boxes.add(box);
      _tryCommit(box, HotZoneEdge.right);
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
          Expanded(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: LayoutBuilder(
                builder: (context, constraints) {
                  final totalBounds = _totalBounds();
                  final scale = _scaleFor(constraints, totalBounds);
                  return Stack(
                    children: [
                      for (final display in _localDisplays)
                        _buildFixedDisplay(display, totalBounds, scale),
                      for (final box in _boxes)
                        _buildDeviceBox(box, totalBounds, scale),
                    ],
                  );
                },
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
        ? '${display.width.round()}×${display.height.round()}\n(principal)'
        : '${display.width.round()}×${display.height.round()}';
    return Positioned.fromRect(
      rect: screenRect,
      child: Container(
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.primaryContainer,
          border: Border.all(color: Colors.black26),
          borderRadius: BorderRadius.circular(8),
        ),
        alignment: Alignment.center,
        child: Text(label, textAlign: TextAlign.center),
      ),
    );
  }

  Widget _buildDeviceBox(_DeviceBox box, Rect totalBounds, double scale) {
    final screenRect = _toScreenRect(box.rect, totalBounds, scale);
    return Positioned.fromRect(
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
        child: Container(
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.secondaryContainer,
            border: Border.all(color: Colors.black45),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Stack(
            children: [
              Center(
                child: Text(box.deviceId, textAlign: TextAlign.center),
              ),
              Positioned(
                right: 0,
                top: 0,
                child: IconButton(
                  key: Key('remove-device-${box.deviceId}'),
                  icon: const Icon(Icons.close, size: 16),
                  tooltip: 'Remover',
                  onPressed: () => _removeDevice(box.deviceId),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
