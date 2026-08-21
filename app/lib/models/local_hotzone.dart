import 'hot_zone.dart';

/// Um salto de cursor de verdade entre 2 telas LOCAIS desta mesma máquina —
/// diferente de `HotZoneConfig` (que aponta pra um dispositivo REMOTO),
/// aqui os dois lados são monitores físicos daqui mesmo (ver
/// `PhysicalDisplay.displayId`), então o app pode teleportar o cursor
/// sozinho (ver `AltCrossNative.warpCursor`), sem precisar de rede nem de
/// outra máquina.
class LocalHotZoneMapping {
  /// Sair da tela `sourceDisplayId` por `sourceEdge`...
  final int sourceDisplayId;
  final HotZoneEdge sourceEdge;

  /// ...reaparece na tela `targetDisplayId`, entrando por `targetEdge`
  /// (posição ao longo dela mapeada proporcionalmente — ver
  /// `computeCursorWarp`).
  final int targetDisplayId;
  final HotZoneEdge targetEdge;

  const LocalHotZoneMapping({
    required this.sourceDisplayId,
    required this.sourceEdge,
    required this.targetDisplayId,
    required this.targetEdge,
  });

  Map<String, Object?> toJson() => {
        'sourceDisplayId': sourceDisplayId,
        'sourceEdge': sourceEdge.name,
        'targetDisplayId': targetDisplayId,
        'targetEdge': targetEdge.name,
      };

  factory LocalHotZoneMapping.fromJson(Map<String, Object?> json) =>
      LocalHotZoneMapping(
        sourceDisplayId: json['sourceDisplayId']! as int,
        sourceEdge: HotZoneEdge.values.byName(json['sourceEdge']! as String),
        targetDisplayId: json['targetDisplayId']! as int,
        targetEdge: HotZoneEdge.values.byName(json['targetEdge']! as String),
      );

  @override
  bool operator ==(Object other) =>
      other is LocalHotZoneMapping &&
      other.sourceDisplayId == sourceDisplayId &&
      other.sourceEdge == sourceEdge &&
      other.targetDisplayId == targetDisplayId &&
      other.targetEdge == targetEdge;

  @override
  int get hashCode =>
      Object.hash(sourceDisplayId, sourceEdge, targetDisplayId, targetEdge);
}
