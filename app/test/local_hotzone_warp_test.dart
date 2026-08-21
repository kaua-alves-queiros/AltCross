import 'dart:ui';

import 'package:altcross_app/models/hot_zone.dart';
import 'package:altcross_app/models/local_hotzone.dart';
import 'package:altcross_app/services/local_hotzone_warp.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('computeCursorWarp', () {
    // tela 1: 1920x1080 em (0,0). tela 2: 1470x956 em (3000,500) — não
    // encostadas, exatamente como o cenário "wrap-around" que o usuário
    // configurou de propósito (canto esquerdo de uma pra direito da outra).
    final displays = {
      1: const Rect.fromLTWH(0, 0, 1920, 1080),
      2: const Rect.fromLTWH(3000, 500, 1470, 956),
    };
    final mappings = [
      const LocalHotZoneMapping(
        sourceDisplayId: 1,
        sourceEdge: HotZoneEdge.left,
        targetDisplayId: 2,
        targetEdge: HotZoneEdge.right,
      ),
      const LocalHotZoneMapping(
        sourceDisplayId: 2,
        sourceEdge: HotZoneEdge.right,
        targetDisplayId: 1,
        targetEdge: HotZoneEdge.left,
      ),
    ];

    test('cursor tocando a borda mapeada teleporta pro lado certo, mesma fração',
        () {
      // metade da altura da tela 1 (1080/2 = 540) — tem que reaparecer na
      // metade da altura da tela 2 (500 + 956/2 = 978).
      final result = computeCursorWarp(
        cursor: const Offset(0, 540),
        displaysById: displays,
        mappings: mappings,
      );

      expect(result, isNotNull);
      expect(result!.dx, closeTo(4470 - 3, 0.01)); // rect.right(4470) - inset(3)
      expect(result.dy, closeTo(978, 0.01));
    });

    test('funciona nos 2 sentidos (mapeamento é 2 entradas separadas)', () {
      final result = computeCursorWarp(
        cursor: const Offset(4470, 978), // borda direita da tela 2, meio
        displaysById: displays,
        mappings: mappings,
      );

      expect(result, isNotNull);
      expect(result!.dx, closeTo(0 + 3, 0.01)); // rect.left(0) + inset(3)
      expect(result.dy, closeTo(540, 0.01));
    });

    test('cursor longe da borda não teleporta', () {
      final result = computeCursorWarp(
        cursor: const Offset(960, 540), // bem no meio da tela 1
        displaysById: displays,
        mappings: mappings,
      );
      expect(result, isNull);
    });

    test('cursor na borda certa mas fora do alcance da tela (perpendicular) não teleporta',
        () {
      final result = computeCursorWarp(
        cursor: const Offset(0, 2000), // x bate, mas y está bem fora da tela 1
        displaysById: displays,
        mappings: mappings,
      );
      expect(result, isNull);
    });

    test('sem mapeamento nenhum, nunca teleporta', () {
      final result = computeCursorWarp(
        cursor: const Offset(0, 540),
        displaysById: displays,
        mappings: const [],
      );
      expect(result, isNull);
    });

    test('mapeamento referenciando tela que não existe mais é ignorado com segurança',
        () {
      final result = computeCursorWarp(
        cursor: const Offset(0, 540),
        displaysById: {1: const Rect.fromLTWH(0, 0, 1920, 1080)}, // tela 2 sumiu
        mappings: mappings,
      );
      expect(result, isNull);
    });

    test('entra alinhado (borda de cima) mapeia a fração certa no eixo X', () {
      final topDisplays = {
        1: const Rect.fromLTWH(0, 0, 1000, 1000),
        2: const Rect.fromLTWH(0, -500, 2000, 500),
      };
      final topMappings = [
        const LocalHotZoneMapping(
          sourceDisplayId: 1,
          sourceEdge: HotZoneEdge.top,
          targetDisplayId: 2,
          targetEdge: HotZoneEdge.bottom,
        ),
      ];

      // 1/4 do caminho ao longo do topo da tela 1 (x=250 de 1000)
      final result = computeCursorWarp(
        cursor: const Offset(250, 0),
        displaysById: topDisplays,
        mappings: topMappings,
      );

      expect(result, isNotNull);
      // mesma fração (1/4) ao longo da largura da tela 2 (2000) = x=500
      expect(result!.dx, closeTo(500, 0.01));
      expect(result.dy, closeTo(0 - 3, 0.01)); // rect.bottom(0) - inset(3)
    });
  });
}
