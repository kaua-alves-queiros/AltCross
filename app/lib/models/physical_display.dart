import 'dart:ui';

/// Um monitor físico conectado à máquina — espelha `altcross_display_t` do
/// Core em C. Coordenadas no espaço virtual do SO (origem no canto superior
/// esquerdo do monitor principal, Y crescendo pra baixo).
class PhysicalDisplay {
  final double x;
  final double y;
  final double width;
  final double height;
  final bool isPrimary;

  const PhysicalDisplay({
    required this.x,
    required this.y,
    required this.width,
    required this.height,
    required this.isPrimary,
  });

  Rect get rect => Rect.fromLTWH(x, y, width, height);
}
