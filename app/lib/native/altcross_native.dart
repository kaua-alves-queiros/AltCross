import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:ffi/ffi.dart';

import '../models/discovered_device.dart';
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

typedef _LoadIdentityNative = Int32 Function(
    Pointer<Utf8> path, Pointer<Uint8> outDeviceId);
typedef _LoadIdentityDart = int Function(
    Pointer<Utf8> path, Pointer<Uint8> outDeviceId);

typedef _RunDiscoveryNative = Int32 Function(
    Pointer<Utf8> fromDeviceId,
    Int32 timeoutMs,
    Pointer<Uint8> outDeviceIds,
    Pointer<Uint8> outNames,
    Pointer<Int32> outPorts,
    Pointer<Uint8> outHosts,
    Int32 maxCount);
typedef _RunDiscoveryDart = int Function(
    Pointer<Utf8> fromDeviceId,
    int timeoutMs,
    Pointer<Uint8> outDeviceIds,
    Pointer<Uint8> outNames,
    Pointer<Int32> outPorts,
    Pointer<Uint8> outHosts,
    int maxCount);

const int _maxDisplays = 16;

// Precisam bater exatamente com core/include/altcross/pairing.h e
// core/include/altcross/discovery.h.
const int _deviceIdSize = 33;
const int _deviceNameSize = 64;
const int _hostSize = 46;

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

  static String _identityFilePath() {
    final base = Platform.isWindows
        ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
        : Platform.environment['HOME'] ?? Directory.systemTemp.path;
    final dir = Directory('$base/.altcross');
    if (!dir.existsSync()) {
      dir.createSync(recursive: true);
    }
    return '${dir.path}/identity.txt';
  }

  /// Identidade estável desta máquina (ver `altcross_pairing_local_identity_
  /// load_or_create`): a mesma sempre que chamada de novo, mesmo que o IP
  /// mude — é o que permite reconhecer "essa é a mesma máquina de antes" ao
  /// receber uma resposta de descoberta.
  static String localDeviceId() {
    final loadOrCreate = _library()
        .lookup<NativeFunction<_LoadIdentityNative>>(
            'altcross_pairing_local_identity_load_or_create')
        .asFunction<_LoadIdentityDart>();

    final pathNative = _identityFilePath().toNativeUtf8();
    final outBuffer = calloc<Uint8>(_deviceIdSize);
    try {
      final rc = loadOrCreate(pathNative, outBuffer);
      if (rc != 0) {
        throw StateError('Não consegui carregar/criar a identidade local.');
      }
      return _readFixedString(outBuffer, 0, _deviceIdSize);
    } finally {
      calloc.free(pathNative);
      calloc.free(outBuffer);
    }
  }

  /// Busca dispositivos AltCross na rede local (broadcast UDP de verdade —
  /// ver aviso em altcross_discovery_run_query no Core sobre o prompt de
  /// permissão de Rede Local). Roda numa isolate separada pra não travar a
  /// UI enquanto espera respostas por [timeoutMs].
  ///
  /// LIMITAÇÃO ATUAL: só o lado que pergunta está pronto — encontra outra
  /// máquina só se ela estiver rodando algo que responda à pergunta (o
  /// daemon `altcrossd` ainda não implementa esse lado). Ver AGENTS.md.
  static Future<List<DiscoveredDevice>> runDiscovery({
    int timeoutMs = 1500,
    int maxResults = 16,
  }) {
    return Isolate.run(() => _runDiscoverySync(timeoutMs, maxResults));
  }

  static List<DiscoveredDevice> _runDiscoverySync(
      int timeoutMs, int maxResults) {
    final runQuery = _library()
        .lookup<NativeFunction<_RunDiscoveryNative>>(
            'altcross_discovery_run_query')
        .asFunction<_RunDiscoveryDart>();

    final fromDeviceId = localDeviceId().toNativeUtf8();
    final deviceIdsBuffer = calloc<Uint8>(maxResults * _deviceIdSize);
    final namesBuffer = calloc<Uint8>(maxResults * _deviceNameSize);
    final hostsBuffer = calloc<Uint8>(maxResults * _hostSize);
    final portsBuffer = calloc<Int32>(maxResults);

    try {
      final total = runQuery(fromDeviceId, timeoutMs, deviceIdsBuffer,
          namesBuffer, portsBuffer, hostsBuffer, maxResults);
      if (total < 0) {
        throw StateError('Falha ao iniciar a busca na rede local.');
      }

      final count = total < maxResults ? total : maxResults;
      return [
        for (var i = 0; i < count; i++)
          DiscoveredDevice(
            deviceId:
                _readFixedString(deviceIdsBuffer, i * _deviceIdSize, _deviceIdSize),
            name: _readFixedString(
                namesBuffer, i * _deviceNameSize, _deviceNameSize),
            host: _readFixedString(hostsBuffer, i * _hostSize, _hostSize),
            port: portsBuffer[i],
          ),
      ];
    } finally {
      calloc.free(fromDeviceId);
      calloc.free(deviceIdsBuffer);
      calloc.free(namesBuffer);
      calloc.free(hostsBuffer);
      calloc.free(portsBuffer);
    }
  }

  static String _readFixedString(Pointer<Uint8> buffer, int offset, int maxLen) {
    final bytes = <int>[];
    for (var i = 0; i < maxLen; i++) {
      final byte = buffer[offset + i];
      if (byte == 0) break;
      bytes.add(byte);
    }
    return utf8.decode(bytes);
  }
}
