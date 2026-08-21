enum HotZoneEdge {
  none,
  top,
  bottom,
  left,
  right,
  topLeft,
  topRight,
  bottomLeft,
  bottomRight,
}

class HotZoneConfig {
  final HotZoneEdge edge;
  final String targetDeviceId;
  final bool enabled;

  /// Índice, na lista de telas físicas reais que `targetDeviceId` devolve
  /// via `AltCrossNative.queryPeerScreens` (ver screen_sync_protocol.h), de
  /// qual tela dele especificamente essa borda alcança — um dispositivo
  /// pode ter mais de 1 monitor. `0` (o padrão de quem ainda não tinha esse
  /// campo, ver `fromJson`) é a tela primária/única na prática.
  final int targetScreenIndex;

  const HotZoneConfig({
    required this.edge,
    required this.targetDeviceId,
    required this.enabled,
    this.targetScreenIndex = 0,
  });

  HotZoneConfig copyWith({bool? enabled}) => HotZoneConfig(
        edge: edge,
        targetDeviceId: targetDeviceId,
        enabled: enabled ?? this.enabled,
        targetScreenIndex: targetScreenIndex,
      );

  Map<String, Object?> toJson() => {
        'edge': edge.name,
        'targetDeviceId': targetDeviceId,
        'enabled': enabled,
        'targetScreenIndex': targetScreenIndex,
      };

  factory HotZoneConfig.fromJson(Map<String, Object?> json) => HotZoneConfig(
        edge: HotZoneEdge.values.byName(json['edge']! as String),
        targetDeviceId: json['targetDeviceId']! as String,
        enabled: json['enabled']! as bool,
        // arquivo salvo antes desse campo existir — assume a tela 0 (única
        // conhecida até então, já que a resolução real nem era buscada).
        targetScreenIndex: json['targetScreenIndex'] as int? ?? 0,
      );

  @override
  bool operator ==(Object other) =>
      other is HotZoneConfig &&
      other.edge == edge &&
      other.targetDeviceId == targetDeviceId &&
      other.enabled == enabled &&
      other.targetScreenIndex == targetScreenIndex;

  @override
  int get hashCode =>
      Object.hash(edge, targetDeviceId, enabled, targetScreenIndex);
}
