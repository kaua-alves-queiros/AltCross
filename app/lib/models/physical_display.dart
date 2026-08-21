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

  /// Id estável (dentro desta execução do processo) que identifica QUAL
  /// monitor físico é — só populado quando vem de `enumerateDisplays`
  /// (telas LOCAIS desta máquina, ver `altcross_displays_enumerate_ex`).
  /// `-1` significa "não aplicável" (ex.: veio de `queryPeerScreens`, tela
  /// de um dispositivo remoto — nunca dá pra reposicionar isso daqui).
  final int displayId;

  const PhysicalDisplay({
    required this.x,
    required this.y,
    required this.width,
    required this.height,
    required this.isPrimary,
    this.displayId = -1,
  });

  Rect get rect => Rect.fromLTWH(x, y, width, height);
}
