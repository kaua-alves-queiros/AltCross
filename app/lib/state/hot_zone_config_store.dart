import '../models/hot_zone.dart';

/// Mantém a configuração de hot zones definida pelo usuário na UI.
///
/// Espelha a regra de resolução usada pelo Core em C
/// (`altcross_hotzone_resolve`): a primeira zona habilitada cujo `edge`
/// bate com o procurado vence.
class HotZoneConfigStore {
  final List<HotZoneConfig> _zones = [];

  /// Chamado depois de qualquer mutação bem-sucedida, com a lista já
  /// atualizada — é assim que quem quiser persistir em disco (ver
  /// `HotZoneConfigPersistence`) salva sozinho, sem acoplar este store
  /// (usado puro, sem I/O, em dezenas de testes) a arquivo.
  void Function(List<HotZoneConfig> zones)? onChanged;

  List<HotZoneConfig> get zones => List.unmodifiable(_zones);

  void add(HotZoneConfig zone) {
    if (zone.edge == HotZoneEdge.none) {
      throw ArgumentError('HotZoneEdge.none não pode ser configurado');
    }
    if (zone.enabled &&
        _zones.any((z) => z.enabled && z.edge == zone.edge)) {
      throw StateError(
          'Já existe uma hotzone habilitada para ${zone.edge}');
    }
    _zones.add(zone);
    onChanged?.call(zones);
  }

  void remove(String targetDeviceId) {
    _zones.removeWhere((z) => z.targetDeviceId == targetDeviceId);
    onChanged?.call(zones);
  }

  void setEnabled(String targetDeviceId, bool enabled) {
    for (var i = 0; i < _zones.length; i++) {
      if (_zones[i].targetDeviceId == targetDeviceId) {
        _zones[i] = _zones[i].copyWith(enabled: enabled);
      }
    }
    onChanged?.call(zones);
  }

  HotZoneConfig? resolve(HotZoneEdge edge) {
    if (edge == HotZoneEdge.none) {
      return null;
    }
    for (final zone in _zones) {
      if (zone.enabled && zone.edge == edge) {
        return zone;
      }
    }
    return null;
  }
}
