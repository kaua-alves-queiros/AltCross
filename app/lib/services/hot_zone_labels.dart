import 'package:flutter/material.dart';

import '../models/hot_zone.dart';

/// Rótulo em pt-BR de uma borda/canto — compartilhado entre a tela de
/// arranjo e a de conexões pra não duplicar o texto em dois lugares.
String edgeLabel(HotZoneEdge edge) {
  switch (edge) {
    case HotZoneEdge.none:
      return 'Nenhuma';
    case HotZoneEdge.top:
      return 'Cima';
    case HotZoneEdge.bottom:
      return 'Baixo';
    case HotZoneEdge.left:
      return 'Esquerda';
    case HotZoneEdge.right:
      return 'Direita';
    case HotZoneEdge.topLeft:
      return 'Canto superior esquerdo';
    case HotZoneEdge.topRight:
      return 'Canto superior direito';
    case HotZoneEdge.bottomLeft:
      return 'Canto inferior esquerdo';
    case HotZoneEdge.bottomRight:
      return 'Canto inferior direito';
  }
}

IconData edgeIcon(HotZoneEdge edge) {
  switch (edge) {
    case HotZoneEdge.top:
      return Icons.arrow_upward;
    case HotZoneEdge.bottom:
      return Icons.arrow_downward;
    case HotZoneEdge.left:
      return Icons.arrow_back;
    case HotZoneEdge.right:
      return Icons.arrow_forward;
    case HotZoneEdge.topLeft:
      return Icons.north_west;
    case HotZoneEdge.topRight:
      return Icons.north_east;
    case HotZoneEdge.bottomLeft:
      return Icons.south_west;
    case HotZoneEdge.bottomRight:
      return Icons.south_east;
    case HotZoneEdge.none:
      return Icons.help_outline;
  }
}
