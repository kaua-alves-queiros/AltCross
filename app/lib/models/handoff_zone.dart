import 'hot_zone.dart';

/// Uma borda configurada pronta pra ativar o handoff de verdade — já
/// carrega o tamanho REAL da tela do dispositivo remoto (via
/// `AltCrossNative.queryPeerScreens`), não um valor chutado (ver
/// AGENTS.md). Monta essa lista é responsabilidade de quem ativa o handoff
/// (ver `ConnectionsScreen`), não do `AltCrossNative` — ele só embrulha pra
/// FFI o que já foi resolvido.
class HandoffZoneSpec {
  final HotZoneEdge edge;
  final String targetDeviceId;
  final double targetScreenWidth;
  final double targetScreenHeight;

  const HandoffZoneSpec({
    required this.edge,
    required this.targetDeviceId,
    required this.targetScreenWidth,
    required this.targetScreenHeight,
  });
}
