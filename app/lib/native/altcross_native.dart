import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:ffi/ffi.dart';

import '../models/discovered_device.dart';
import '../models/pairing.dart';
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

typedef _StartResponderNative = Int32 Function(
    Pointer<Utf8> deviceId, Pointer<Utf8> name, Int32 port);
typedef _StartResponderDart = int Function(
    Pointer<Utf8> deviceId, Pointer<Utf8> name, int port);

typedef _StopResponderNative = Void Function();
typedef _StopResponderDart = void Function();

/// Espelha `altcross_pairing_incoming_request_t` de
/// core/include/altcross/pairing_protocol.h.
final class _NativeIncomingRequest extends Struct {
  @Int32()
  external int pending;
  @Array(33)
  external Array<Uint8> requesterDeviceId;
  @Array(64)
  external Array<Uint8> requesterName;
  @Int32()
  external int code;
}

typedef _StartPairingResponderNative = Int32 Function(
    Pointer<Utf8> deviceId, Pointer<Utf8> name, Pointer<Utf8> storePath);
typedef _StartPairingResponderDart = int Function(
    Pointer<Utf8> deviceId, Pointer<Utf8> name, Pointer<Utf8> storePath);

typedef _PollIncomingRequestNative = Int32 Function(
    Pointer<_NativeIncomingRequest> out);
typedef _PollIncomingRequestDart = int Function(
    Pointer<_NativeIncomingRequest> out);

/// Espelha `altcross_pairing_completed_t` de
/// core/include/altcross/pairing_protocol.h.
final class _NativeCompleted extends Struct {
  @Int32()
  external int pending;
  @Array(33)
  external Array<Uint8> peerDeviceId;
  @Array(64)
  external Array<Uint8> peerName;
}

typedef _PollCompletedNative = Int32 Function(Pointer<_NativeCompleted> out);
typedef _PollCompletedDart = int Function(Pointer<_NativeCompleted> out);

typedef _SendPairingRequestNative = Int32 Function(
    Pointer<Utf8> myDeviceId, Pointer<Utf8> myName, Pointer<Utf8> peerHost);
typedef _SendPairingRequestDart = int Function(
    Pointer<Utf8> myDeviceId, Pointer<Utf8> myName, Pointer<Utf8> peerHost);

typedef _ConfirmPairingNative = Int32 Function(
    Pointer<Utf8> myDeviceId,
    Pointer<Utf8> peerHost,
    Int32 code,
    Int32 timeoutMs,
    Pointer<Utf8> storePath,
    Pointer<Uint8> outDeviceId,
    Pointer<Uint8> outName);
typedef _ConfirmPairingDart = int Function(
    Pointer<Utf8> myDeviceId,
    Pointer<Utf8> peerHost,
    int code,
    int timeoutMs,
    Pointer<Utf8> storePath,
    Pointer<Uint8> outDeviceId,
    Pointer<Uint8> outName);

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

  static bool _responderStarted = false;

  /// Sobe o respondedor de descoberta (ver `altcross_discovery_start_
  /// responder`): a partir daqui, esta máquina passa a responder quando
  /// outra rodar `runDiscovery`. Só escuta e responde via unicast — não
  /// manda broadcast, então não deveria disparar o prompt de permissão de
  /// Rede Local por si só (quem dispara isso é `runDiscovery`, do lado de
  /// quem pergunta). Idempotente: chamar de novo não faz nada se já estiver
  /// rodando.
  static void startDiscoveryResponder({required String name, int port = 0}) {
    if (_responderStarted) {
      return;
    }

    final start = _library()
        .lookup<NativeFunction<_StartResponderNative>>(
            'altcross_discovery_start_responder')
        .asFunction<_StartResponderDart>();

    final deviceIdNative = localDeviceId().toNativeUtf8();
    final nameNative = name.toNativeUtf8();
    try {
      final rc = start(deviceIdNative, nameNative, port);
      _responderStarted = rc == 0;
    } finally {
      calloc.free(deviceIdNative);
      calloc.free(nameNative);
    }
  }

  static void stopDiscoveryResponder() {
    if (!_responderStarted) {
      return;
    }
    _library()
        .lookup<NativeFunction<_StopResponderNative>>(
            'altcross_discovery_stop_responder')
        .asFunction<_StopResponderDart>()();
    _responderStarted = false;
  }

  /// Busca dispositivos AltCross na rede local (broadcast UDP de verdade —
  /// ver aviso em altcross_discovery_run_query no Core sobre o prompt de
  /// permissão de Rede Local). Roda numa isolate separada pra não travar a
  /// UI enquanto espera respostas por [timeoutMs]. Só encontra máquinas com
  /// o respondedor rodando (ver `startDiscoveryResponder`).
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

  static String _pairingStorePath() {
    final base = Platform.isWindows
        ? Platform.environment['APPDATA'] ?? Directory.systemTemp.path
        : Platform.environment['HOME'] ?? Directory.systemTemp.path;
    final dir = Directory('$base/.altcross');
    if (!dir.existsSync()) {
      dir.createSync(recursive: true);
    }
    return '${dir.path}/trusted_devices.txt';
  }

  static bool _pairingResponderStarted = false;

  /// Sobe o respondedor de pareamento (ver `altcross_pairing_start_
  /// responder`): a partir daqui, esta máquina passa a aceitar pedidos de
  /// pareamento (`sendPairingRequest`) de outras. Só escuta/responde
  /// unicast pra quem pediu — não faz broadcast. Idempotente.
  static void startPairingResponder({required String name}) {
    if (_pairingResponderStarted) {
      return;
    }
    final start = _library()
        .lookup<NativeFunction<_StartPairingResponderNative>>(
            'altcross_pairing_start_responder')
        .asFunction<_StartPairingResponderDart>();

    final deviceIdNative = localDeviceId().toNativeUtf8();
    final nameNative = name.toNativeUtf8();
    final storePathNative = _pairingStorePath().toNativeUtf8();
    try {
      final rc = start(deviceIdNative, nameNative, storePathNative);
      _pairingResponderStarted = rc == 0;
    } finally {
      calloc.free(deviceIdNative);
      calloc.free(nameNative);
      calloc.free(storePathNative);
    }
  }

  static String _readFixedArray(Array<Uint8> array, int maxLen) {
    final bytes = <int>[];
    for (var i = 0; i < maxLen; i++) {
      final byte = array[i];
      if (byte == 0) break;
      bytes.add(byte);
    }
    return utf8.decode(bytes);
  }

  /// Consome (se houver) um pedido de pareamento recebido de outro
  /// dispositivo — a UI deve mostrar `code` na tela pra quem pediu digitar
  /// do outro lado. Retorna null se não há nenhum pedido pendente agora.
  static IncomingPairingRequest? pollIncomingPairingRequest() {
    final poll = _library()
        .lookup<NativeFunction<_PollIncomingRequestNative>>(
            'altcross_pairing_poll_incoming_request')
        .asFunction<_PollIncomingRequestDart>();

    final out = calloc<_NativeIncomingRequest>();
    try {
      final found = poll(out);
      if (found == 0) {
        return null;
      }
      return IncomingPairingRequest(
        requesterDeviceId:
            _readFixedArray(out.ref.requesterDeviceId, _deviceIdSize),
        requesterName: _readFixedArray(out.ref.requesterName, _deviceNameSize),
        code: out.ref.code,
      );
    } finally {
      calloc.free(out);
    }
  }

  /// Consome (se houver) a notificação de que um pareamento que ESTE
  /// dispositivo aprovou (mostrando o código pro outro lado digitar) acabou
  /// de ser confirmado com sucesso — é assim que quem está SENDO adicionado
  /// fica sabendo que deu certo, já que só quem inicia o pedido recebe o
  /// resultado direto de `confirmPairing`. Retorna null se não há nenhuma
  /// notificação pendente agora.
  static PairingCompleted? pollPairingCompleted() {
    final poll = _library()
        .lookup<NativeFunction<_PollCompletedNative>>(
            'altcross_pairing_poll_completed')
        .asFunction<_PollCompletedDart>();

    final out = calloc<_NativeCompleted>();
    try {
      final found = poll(out);
      if (found == 0) {
        return null;
      }
      return PairingCompleted(
        peerDeviceId: _readFixedArray(out.ref.peerDeviceId, _deviceIdSize),
        peerName: _readFixedArray(out.ref.peerName, _deviceNameSize),
      );
    } finally {
      calloc.free(out);
    }
  }

  /// Manda um pedido de pareamento de verdade pra peerHost. Não espera
  /// resposta (o código de confirmação aparece do lado de lá) — retorna
  /// true se conseguiu mandar o pacote.
  static bool sendPairingRequest({
    required String peerHost,
    required String myName,
  }) {
    final send = _library()
        .lookup<NativeFunction<_SendPairingRequestNative>>(
            'altcross_pairing_send_request')
        .asFunction<_SendPairingRequestDart>();

    final deviceIdNative = localDeviceId().toNativeUtf8();
    final nameNative = myName.toNativeUtf8();
    final hostNative = peerHost.toNativeUtf8();
    try {
      return send(deviceIdNative, nameNative, hostNative) == 0;
    } finally {
      calloc.free(deviceIdNative);
      calloc.free(nameNative);
      calloc.free(hostNative);
    }
  }

  /// Depois que o usuário digitou o código mostrado na tela do outro
  /// dispositivo, confirma o pareamento de verdade pela rede e — se aceito —
  /// já salva o segredo compartilhado (sobrevive a troca de IP). Roda numa
  /// isolate pra não travar a UI enquanto espera a resposta.
  static Future<PairingResult> confirmPairing({
    required String peerHost,
    required int code,
    int timeoutMs = 3000,
  }) {
    return Isolate.run(() => _confirmPairingSync(peerHost, code, timeoutMs));
  }

  static PairingResult _confirmPairingSync(
      String peerHost, int code, int timeoutMs) {
    final confirm = _library()
        .lookup<NativeFunction<_ConfirmPairingNative>>(
            'altcross_pairing_confirm')
        .asFunction<_ConfirmPairingDart>();

    final deviceIdNative = localDeviceId().toNativeUtf8();
    final hostNative = peerHost.toNativeUtf8();
    final storePathNative = _pairingStorePath().toNativeUtf8();
    final outDeviceId = calloc<Uint8>(_deviceIdSize);
    final outName = calloc<Uint8>(_deviceNameSize);
    try {
      final rc = confirm(deviceIdNative, hostNative, code, timeoutMs,
          storePathNative, outDeviceId, outName);
      if (rc == 1) {
        return PairingResult.accepted(
          deviceId: _readFixedString(outDeviceId, 0, _deviceIdSize),
          name: _readFixedString(outName, 0, _deviceNameSize),
        );
      }
      if (rc == 0) {
        return PairingResult.rejected();
      }
      return PairingResult.error();
    } finally {
      calloc.free(deviceIdNative);
      calloc.free(hostNative);
      calloc.free(storePathNative);
      calloc.free(outDeviceId);
      calloc.free(outName);
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
