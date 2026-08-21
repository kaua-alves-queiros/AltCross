import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

import '../models/physical_display.dart';

/// Espelha `altcross_display_t` de core/include/altcross/displays.h — os
/// campos e a ordem têm que bater exatamente com o struct em C.
final class _NativeDisplay extends Struct {
  @Double()
  external double x;
  @Double()
  external double y;
  @Double()
  external double width;
  @Double()
  external double height;
  @Int32()
  external int isPrimary;
}

typedef _EnumerateDisplaysNative = Int32 Function(
    Pointer<_NativeDisplay> out, Int32 maxCount);
typedef _EnumerateDisplaysDart = int Function(
    Pointer<_NativeDisplay> out, int maxCount);

const int _maxDisplays = 16;

/// Ponte para o Core em C (altcross_core) via dart:ffi. Só existe uma
/// instância de fato carregada por processo — todas as chamadas passam por
/// aqui, nunca via `dart:ffi` direto no resto do app (mantém a fronteira
/// nativa em um lugar só).
class AltCrossNative {
  static DynamicLibrary? _lib;
  static _EnumerateDisplaysDart? _enumerateDisplays;

  static DynamicLibrary _library() {
    return _lib ??= _open();
  }

  static DynamicLibrary _open() {
    if (Platform.isMacOS) {
      // O build phase do Xcode (ver macos/Runner.xcodeproj) copia o .dylib
      // pra Contents/Frameworks/ dentro do próprio .app — não está no
      // caminho de busca padrão do dyld, então resolvemos relativo ao
      // executável em execução.
      final exeDir = File(Platform.resolvedExecutable).parent.path;
      return DynamicLibrary.open(
          '$exeDir/../Frameworks/libaltcross_core.dylib');
    }
    if (Platform.isWindows) {
      // windows/CMakeLists.txt instala o .dll na mesma pasta do .exe, que o
      // Windows já busca por padrão.
      return DynamicLibrary.open('altcross_core.dll');
    }
    throw UnsupportedError(
        'AltCross native core ainda não tem suporte nesta plataforma.');
  }

  /// Monitores físicos conectados a esta máquina agora, em coordenadas do
  /// espaço virtual do SO. Chama o Core em C de verdade (NSScreen no macOS,
  /// EnumDisplayMonitors no Windows) — não é mock nem dado inventado.
  static List<PhysicalDisplay> enumerateDisplays() {
    final enumerate = _enumerateDisplays ??= _library()
        .lookup<NativeFunction<_EnumerateDisplaysNative>>(
            'altcross_displays_enumerate')
        .asFunction();

    final buffer = calloc<_NativeDisplay>(_maxDisplays);
    try {
      final total = enumerate(buffer, _maxDisplays);
      final count = total < _maxDisplays ? total : _maxDisplays;
      return [
        for (var i = 0; i < count; i++)
          PhysicalDisplay(
            x: buffer[i].x,
            y: buffer[i].y,
            width: buffer[i].width,
            height: buffer[i].height,
            isPrimary: buffer[i].isPrimary != 0,
          ),
      ];
    } finally {
      calloc.free(buffer);
    }
  }
}
