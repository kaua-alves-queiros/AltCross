import 'dart:ui';

import '../models/hot_zone.dart';
import '../models/local_hotzone.dart';

/// Se o cursor estiver, agora, tocando a borda de origem de algum mapeamento
/// local configurado, devolve pra onde ele deve ser teleportado — ou null
/// se não está tocando nenhuma. Pura geometria (sem FFI), testável sozinha;
/// quem chama (ver o watcher em main.dart) é responsável por ler a posição
/// real do cursor e de fato mover ela via `AltCrossNative.warpCursor`.
///
/// `displaysById` deve vir de uma leitura FRESCA de
/// `AltCrossNative.enumerateDisplays` — a posição/tamanho real de cada tela
/// pode mudar (arrastar com snap, ver arrangement_screen.dart), então nunca
/// cacheia por muito tempo.
Offset? computeCursorWarp({
  required Offset cursor,
  required Map<int, Rect> displaysById,
  required List<LocalHotZoneMapping> mappings,
  double edgeTolerance = 2,
  double entryInset = 3,
}) {
  for (final mapping in mappings) {
    final source = displaysById[mapping.sourceDisplayId];
    final target = displaysById[mapping.targetDisplayId];
    if (source == null || target == null) {
      continue;
    }

    final fraction =
        _touchFraction(cursor, source, mapping.sourceEdge, edgeTolerance);
    if (fraction == null) {
      continue;
    }

    return _entryPoint(fraction, target, mapping.targetEdge, entryInset);
  }
  return null;
}

/// Fração (0..1) de onde, ao longo de `edge` de `rect`, o cursor está —
/// null se ele não está tocando essa borda agora (fora da tolerância, ou
/// fora do alcance perpendicular da própria tela). Cantos não são
/// suportados (mesma limitação da UI de Arranjo — só bordas retas são
/// clicáveis, ver `_clickableEdges`).
double? _touchFraction(
    Offset cursor, Rect rect, HotZoneEdge edge, double tolerance) {
  switch (edge) {
    case HotZoneEdge.left:
      if ((cursor.dx - rect.left).abs() > tolerance) return null;
      if (cursor.dy < rect.top || cursor.dy > rect.bottom) return null;
      return rect.height == 0
          ? 0
          : ((cursor.dy - rect.top) / rect.height).clamp(0.0, 1.0);
    case HotZoneEdge.right:
      if ((cursor.dx - rect.right).abs() > tolerance) return null;
      if (cursor.dy < rect.top || cursor.dy > rect.bottom) return null;
      return rect.height == 0
          ? 0
          : ((cursor.dy - rect.top) / rect.height).clamp(0.0, 1.0);
    case HotZoneEdge.top:
      if ((cursor.dy - rect.top).abs() > tolerance) return null;
      if (cursor.dx < rect.left || cursor.dx > rect.right) return null;
      return rect.width == 0
          ? 0
          : ((cursor.dx - rect.left) / rect.width).clamp(0.0, 1.0);
    case HotZoneEdge.bottom:
      if ((cursor.dy - rect.bottom).abs() > tolerance) return null;
      if (cursor.dx < rect.left || cursor.dx > rect.right) return null;
      return rect.width == 0
          ? 0
          : ((cursor.dx - rect.left) / rect.width).clamp(0.0, 1.0);
    default:
      return null;
  }
}

/// Onde o cursor reaparece ao entrar em `rect` por `edge`, na fração dada —
/// um pouco pra DENTRO da borda (`inset`), não exatamente em cima dela, pra
/// não ficar imediatamente "tocando" essa mesma borda de novo (o que
/// causaria um salto de volta instantâneo se ela também tiver mapeamento).
Offset _entryPoint(double fraction, Rect rect, HotZoneEdge edge, double inset) {
  switch (edge) {
    case HotZoneEdge.left:
      return Offset(rect.left + inset, rect.top + fraction * rect.height);
    case HotZoneEdge.right:
      return Offset(rect.right - inset, rect.top + fraction * rect.height);
    case HotZoneEdge.top:
      return Offset(rect.left + fraction * rect.width, rect.top + inset);
    case HotZoneEdge.bottom:
      return Offset(rect.left + fraction * rect.width, rect.bottom - inset);
    default:
      return rect.center;
  }
}
