import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('HotZoneConfigStore', () {
    test('starts empty', () {
      final store = HotZoneConfigStore();
      expect(store.zones, isEmpty);
    });

    test('add() adds an enabled zone', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      expect(store.zones, hasLength(1));
      expect(store.zones.single.targetDeviceId, 'pc-2');
    });

    test('add() rejects HotZoneEdge.none', () {
      final store = HotZoneConfigStore();
      expect(
        () => store.add(const HotZoneConfig(
          edge: HotZoneEdge.none,
          targetDeviceId: 'pc-2',
          enabled: true,
        )),
        throwsArgumentError,
      );
    });

    test('add() rejects a second enabled zone on the same edge', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      expect(
        () => store.add(const HotZoneConfig(
          edge: HotZoneEdge.right,
          targetDeviceId: 'pc-3',
          enabled: true,
        )),
        throwsStateError,
      );
    });

    test('add() allows a disabled duplicate edge', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-3',
        enabled: false,
      ));
      expect(store.zones, hasLength(2));
    });

    test('remove() removes the zone for a device', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      store.remove('pc-2');
      expect(store.zones, isEmpty);
    });

    test('setEnabled() toggles a zone by device id', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      store.setEnabled('pc-2', false);
      expect(store.zones.single.enabled, isFalse);
    });

    test('resolve() returns the enabled zone matching the edge', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.top,
        targetDeviceId: 'pc-1',
        enabled: true,
      ));
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: true,
      ));
      expect(store.resolve(HotZoneEdge.right)?.targetDeviceId, 'pc-2');
    });

    test('resolve() ignores disabled zones', () {
      final store = HotZoneConfigStore();
      store.add(const HotZoneConfig(
        edge: HotZoneEdge.right,
        targetDeviceId: 'pc-2',
        enabled: false,
      ));
      expect(store.resolve(HotZoneEdge.right), isNull);
    });

    test('resolve() returns null for HotZoneEdge.none', () {
      final store = HotZoneConfigStore();
      expect(store.resolve(HotZoneEdge.none), isNull);
    });
  });

  group('HotZoneConfig json', () {
    test('round-trips through toJson/fromJson', () {
      const original = HotZoneConfig(
        edge: HotZoneEdge.topLeft,
        targetDeviceId: 'pc-9',
        enabled: true,
      );
      final restored = HotZoneConfig.fromJson(original.toJson());
      expect(restored, original);
    });
  });
}
