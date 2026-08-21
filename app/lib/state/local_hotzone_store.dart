import '../models/local_hotzone.dart';

/// Mantém os saltos de cursor configurados entre telas LOCAIS (ver
/// `LocalHotZoneMapping`) — espelha `HotZoneConfigStore`, mas pra
/// mapeamentos que ficam inteiramente nesta máquina.
class LocalHotZoneStore {
  final List<LocalHotZoneMapping> _mappings = [];

  /// Chamado depois de qualquer mutação bem-sucedida, com a lista já
  /// atualizada — mesmo padrão de `HotZoneConfigStore.onChanged`.
  void Function(List<LocalHotZoneMapping> mappings)? onChanged;

  List<LocalHotZoneMapping> get mappings => List.unmodifiable(_mappings);

  void add(LocalHotZoneMapping mapping) {
    _mappings.removeWhere((m) =>
        m.sourceDisplayId == mapping.sourceDisplayId &&
        m.sourceEdge == mapping.sourceEdge);
    _mappings.add(mapping);
    onChanged?.call(mappings);
  }

  void removeForDisplay(int displayId) {
    _mappings.removeWhere((m) =>
        m.sourceDisplayId == displayId || m.targetDisplayId == displayId);
    onChanged?.call(mappings);
  }
}
