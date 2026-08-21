import 'dart:ui';

import '../models/hot_zone.dart';

/// Dado o retângulo que engloba as telas físicas locais e o retângulo de um
/// dispositivo remoto solto pelo usuário na tela de arranjo, decide qual
/// borda foi encostada (dentro de [tolerance] pixels). Cantos têm prioridade
/// sobre bordas simples quando o retângulo toca duas de uma vez — mesma
/// convenção usada pelo Core em C (`altcross_hotzone_detect`).
HotZoneEdge detectTouchingEdge(
  Rect localBounds,
  Rect dropped, {
  double tolerance = 24,
}) {
  final left = (dropped.right - localBounds.left).abs() <= tolerance;
  final right = (dropped.left - localBounds.right).abs() <= tolerance;
  final top = (dropped.bottom - localBounds.top).abs() <= tolerance;
  final bottom = (dropped.top - localBounds.bottom).abs() <= tolerance;

  if (top && left) return HotZoneEdge.topLeft;
  if (top && right) return HotZoneEdge.topRight;
  if (bottom && left) return HotZoneEdge.bottomLeft;
  if (bottom && right) return HotZoneEdge.bottomRight;
  if (top) return HotZoneEdge.top;
  if (bottom) return HotZoneEdge.bottom;
  if (left) return HotZoneEdge.left;
  if (right) return HotZoneEdge.right;
  return HotZoneEdge.none;
}
