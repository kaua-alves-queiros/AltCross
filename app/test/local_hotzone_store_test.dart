import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/local_hotzone.dart';
import 'package:altcross_app/state/local_hotzone_store.dart';
import 'package:flutter_test/flutter_test.dart';

const _mapping = LocalHotZoneMapping(
  sourceDisplayId: 1,
  sourceEdge: HotZoneEdge.right,
  targetDisplayId: 2,
  targetEdge: HotZoneEdge.left,
);

void main() {
  group('LocalHotZoneStore', () {
    test('starts empty', () {
      expect(LocalHotZoneStore().mappings, isEmpty);
    });

    test('add() adds a mapping', () {
      final store = LocalHotZoneStore();
      store.add(_mapping);
      expect(store.mappings, [_mapping]);
    });

    test('add() com a mesma origem substitui o mapeamento anterior', () {
      final store = LocalHotZoneStore();
      store.add(_mapping);
      store.add(const LocalHotZoneMapping(
        sourceDisplayId: 1,
        sourceEdge: HotZoneEdge.right,
        targetDisplayId: 3,
        targetEdge: HotZoneEdge.top,
      ));
      expect(store.mappings, hasLength(1));
      expect(store.mappings.single.targetDisplayId, 3);
    });

    test('removeForDisplay() tira mapeamentos onde a tela é origem ou destino',
        () {
      final store = LocalHotZoneStore();
      store.add(_mapping);
      store.removeForDisplay(2);
      expect(store.mappings, isEmpty);
    });

    test('onChanged é chamado com a lista atualizada', () {
      final store = LocalHotZoneStore();
      final snapshots = <List<LocalHotZoneMapping>>[];
      store.onChanged = snapshots.add;

      store.add(_mapping);
      store.removeForDisplay(1);

      expect(snapshots, hasLength(2));
      expect(snapshots[0], [_mapping]);
      expect(snapshots[1], isEmpty);
    });
  });

  group('LocalHotZoneMapping json', () {
    test('round-trips through toJson/fromJson', () {
      final restored = LocalHotZoneMapping.fromJson(_mapping.toJson());
      expect(restored, _mapping);
    });
  });
}
