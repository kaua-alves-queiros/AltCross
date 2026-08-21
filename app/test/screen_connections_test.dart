import 'dart:ui';

import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/services/screen_connections.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('oppositeEdge', () {
    test('bordas retas são opostas entre si', () {
      expect(oppositeEdge(HotZoneEdge.top), HotZoneEdge.bottom);
      expect(oppositeEdge(HotZoneEdge.bottom), HotZoneEdge.top);
      expect(oppositeEdge(HotZoneEdge.left), HotZoneEdge.right);
      expect(oppositeEdge(HotZoneEdge.right), HotZoneEdge.left);
    });

    test('cantos são opostos na diagonal', () {
      expect(oppositeEdge(HotZoneEdge.topLeft), HotZoneEdge.bottomRight);
      expect(oppositeEdge(HotZoneEdge.bottomRight), HotZoneEdge.topLeft);
      expect(oppositeEdge(HotZoneEdge.topRight), HotZoneEdge.bottomLeft);
      expect(oppositeEdge(HotZoneEdge.bottomLeft), HotZoneEdge.topRight);
    });

    test('none continua none', () {
      expect(oppositeEdge(HotZoneEdge.none), HotZoneEdge.none);
    });
  });

  group('isOuterEdge', () {
    // uma única tela local que preenche sozinha os limites combinados —
    // toda borda dela é externa.
    const soloBounds = Rect.fromLTWH(0, 0, 1000, 600);

    test('quando a tela é a única, todas as bordas são externas', () {
      for (final edge in [
        HotZoneEdge.top,
        HotZoneEdge.bottom,
        HotZoneEdge.left,
        HotZoneEdge.right,
        HotZoneEdge.topLeft,
        HotZoneEdge.topRight,
        HotZoneEdge.bottomLeft,
        HotZoneEdge.bottomRight,
      ]) {
        expect(isOuterEdge(soloBounds, soloBounds, edge), isTrue);
      }
    });

    test('borda interna entre 2 telas locais não é externa', () {
      // 2 telas lado a lado, bounds combinado 0..2000
      const bounds = Rect.fromLTWH(0, 0, 2000, 600);
      const leftScreen = Rect.fromLTWH(0, 0, 1000, 600);

      // a borda direita da tela da esquerda é interna (encosta na outra
      // tela local, não na borda de fora)
      expect(isOuterEdge(leftScreen, bounds, HotZoneEdge.right), isFalse);
      // já a esquerda dela é externa de verdade
      expect(isOuterEdge(leftScreen, bounds, HotZoneEdge.left), isTrue);
    });

    test('borda externa da tela da direita é reconhecida', () {
      const bounds = Rect.fromLTWH(0, 0, 2000, 600);
      const rightScreen = Rect.fromLTWH(1000, 0, 1000, 600);

      expect(isOuterEdge(rightScreen, bounds, HotZoneEdge.right), isTrue);
      expect(isOuterEdge(rightScreen, bounds, HotZoneEdge.left), isFalse);
    });

    test('canto só é externo se as 2 bordas dele forem externas', () {
      const bounds = Rect.fromLTWH(0, 0, 2000, 600);
      const leftScreen = Rect.fromLTWH(0, 0, 1000, 600);

      // canto superior esquerdo: top é externo, left é externo -> externo
      expect(isOuterEdge(leftScreen, bounds, HotZoneEdge.topLeft), isTrue);
      // canto superior direito: top é externo, mas right é interno -> não
      expect(isOuterEdge(leftScreen, bounds, HotZoneEdge.topRight), isFalse);
    });
  });
}
