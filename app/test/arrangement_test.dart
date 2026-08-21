import 'dart:ui';

import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/services/arrangement.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final localBounds = Rect.fromLTWH(0, 0, 1000, 600);

  test('detecta encostado na borda direita', () {
    final dropped = Rect.fromLTWH(1005, 200, 300, 200);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.right);
  });

  test('detecta encostado na borda esquerda', () {
    final dropped = Rect.fromLTWH(-300, 200, 300, 200);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.left);
  });

  test('detecta encostado em cima', () {
    final dropped = Rect.fromLTWH(200, -250, 300, 250);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.top);
  });

  test('detecta encostado embaixo', () {
    final dropped = Rect.fromLTWH(200, 605, 300, 250);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.bottom);
  });

  test('detecta canto superior esquerdo quando toca 2 bordas', () {
    final dropped = Rect.fromLTWH(-300, -250, 300, 250);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.topLeft);
  });

  test('detecta canto inferior direito quando toca 2 bordas', () {
    final dropped = Rect.fromLTWH(1005, 605, 300, 250);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.bottomRight);
  });

  test('retorna none quando não toca em nenhuma borda', () {
    final dropped = Rect.fromLTWH(2000, 2000, 300, 200);
    expect(detectTouchingEdge(localBounds, dropped), HotZoneEdge.none);
  });

  test('respeita a tolerância informada', () {
    final dropped = Rect.fromLTWH(1030, 200, 300, 200); // 30px de distância
    expect(
      detectTouchingEdge(localBounds, dropped, tolerance: 10),
      HotZoneEdge.none,
    );
    expect(
      detectTouchingEdge(localBounds, dropped, tolerance: 40),
      HotZoneEdge.right,
    );
  });
}
