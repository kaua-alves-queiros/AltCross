import 'hot_zone.dart';

/// Notificação de que outro dispositivo já pareado conectou a borda dele
/// numa das MINHAS telas — é assim que o arranjo fica sincronizado nos 2
/// lados sem cada host precisar configurar a mesma ligação manualmente 2
/// vezes (ver `AltCrossNative.pollIncomingZone` /
/// `core/include/altcross/screen_sync_protocol.h`).
class IncomingZonePush {
  final String senderDeviceId;
  final String senderName;

  /// Borda do lado de QUEM MANDOU — a minha borda correspondente (pra eu
  /// enxergar a conexão do meu lado) é a oposta (ver `oppositeEdge` em
  /// screen_connections.dart).
  final HotZoneEdge senderEdge;

  /// Qual das MINHAS telas (na ordem que eu devolvo via
  /// `altcross_displays_enumerate`/`queryPeerScreens`) o remetente escolheu
  /// como alvo — não usado ainda pra decidir nada do lado receptor (a
  /// hotzone dele aponta de volta pro dispositivo como um todo), mas
  /// carregado pra quando o modelo local também granularizar por tela.
  final int senderTargetScreenIndex;

  const IncomingZonePush({
    required this.senderDeviceId,
    required this.senderName,
    required this.senderEdge,
    required this.senderTargetScreenIndex,
  });
}
