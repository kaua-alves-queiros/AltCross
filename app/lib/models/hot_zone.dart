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

  const HotZoneConfig({
    required this.edge,
    required this.targetDeviceId,
    required this.enabled,
  });

  HotZoneConfig copyWith({bool? enabled}) => HotZoneConfig(
        edge: edge,
        targetDeviceId: targetDeviceId,
        enabled: enabled ?? this.enabled,
      );

  Map<String, Object?> toJson() => {
        'edge': edge.name,
        'targetDeviceId': targetDeviceId,
        'enabled': enabled,
      };

  factory HotZoneConfig.fromJson(Map<String, Object?> json) => HotZoneConfig(
        edge: HotZoneEdge.values.byName(json['edge']! as String),
        targetDeviceId: json['targetDeviceId']! as String,
        enabled: json['enabled']! as bool,
      );

  @override
  bool operator ==(Object other) =>
      other is HotZoneConfig &&
      other.edge == edge &&
      other.targetDeviceId == targetDeviceId &&
      other.enabled == enabled;

  @override
  int get hashCode => Object.hash(edge, targetDeviceId, enabled);
}
