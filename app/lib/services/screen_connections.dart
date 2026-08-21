import 'dart:ui';

import '../models/hot_zone.dart';

/// Borda oposta — usada pra saber em qual lado do dispositivo remoto o
/// cursor "reaparece" quando sai por uma borda local (mesma convenção do
/// Core em C, ver `opposite_edge` em core/src/input_control.c).
HotZoneEdge oppositeEdge(HotZoneEdge edge) {
  switch (edge) {
    case HotZoneEdge.top:
      return HotZoneEdge.bottom;
    case HotZoneEdge.bottom:
      return HotZoneEdge.top;
    case HotZoneEdge.left:
      return HotZoneEdge.right;
    case HotZoneEdge.right:
      return HotZoneEdge.left;
    case HotZoneEdge.topLeft:
      return HotZoneEdge.bottomRight;
    case HotZoneEdge.topRight:
      return HotZoneEdge.bottomLeft;
    case HotZoneEdge.bottomLeft:
      return HotZoneEdge.topRight;
    case HotZoneEdge.bottomRight:
      return HotZoneEdge.topLeft;
    case HotZoneEdge.none:
      return HotZoneEdge.none;
  }
}

/// Diz se `edge` de `boxRect` é uma borda EXTERNA de `boundsRect` (o
/// retângulo que engloba todas as telas locais) — ou seja, se essa borda
/// realmente separa "dentro da área local" de "fora dela".
///
/// Existe porque, com várias telas locais arrastáveis, a borda direita de
/// UMA tela pode na verdade ser uma borda INTERNA (encostada em outra tela
/// sua), não uma borda de saída de verdade — só bordas externas fazem
/// sentido pra conectar a um dispositivo remoto (o Core só entende uma
/// "tela local" combinada, ver altcross_screen_config_t).
bool isOuterEdge(
  Rect boxRect,
  Rect boundsRect,
  HotZoneEdge edge, {
  double epsilon = 0.5,
}) {
  bool touchesLeft() => (boxRect.left - boundsRect.left).abs() < epsilon;
  bool touchesRight() => (boxRect.right - boundsRect.right).abs() < epsilon;
  bool touchesTop() => (boxRect.top - boundsRect.top).abs() < epsilon;
  bool touchesBottom() => (boxRect.bottom - boundsRect.bottom).abs() < epsilon;

  switch (edge) {
    case HotZoneEdge.left:
      return touchesLeft();
    case HotZoneEdge.right:
      return touchesRight();
    case HotZoneEdge.top:
      return touchesTop();
    case HotZoneEdge.bottom:
      return touchesBottom();
    case HotZoneEdge.topLeft:
      return touchesTop() && touchesLeft();
    case HotZoneEdge.topRight:
      return touchesTop() && touchesRight();
    case HotZoneEdge.bottomLeft:
      return touchesBottom() && touchesLeft();
    case HotZoneEdge.bottomRight:
      return touchesBottom() && touchesRight();
    case HotZoneEdge.none:
      return false;
  }
}
