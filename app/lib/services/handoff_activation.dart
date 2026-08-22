import '../models/handoff_zone.dart';
import '../models/physical_display.dart';
import '../native/altcross_native.dart';
import '../state/hot_zone_config_store.dart';

typedef LocalDisplaysProvider = List<PhysicalDisplay> Function();

/// Resultado de [activateHandoff] — quem chama decide como mostrar isso
/// (mensagem na tela, ou nada, se for a ativação automática ao abrir o app).
class HandoffActivationResult {
  final bool started;
  final List<String> skippedDeviceIds;
  final String? failureReason;

  const HandoffActivationResult({
    required this.started,
    required this.skippedDeviceIds,
    this.failureReason,
  });
}

/// Monta as bordas de verdade (com a tela REAL de cada alvo, buscada pela
/// rede — nunca um tamanho chutado, ver AGENTS.md) e liga o handoff.
/// Compartilhado entre a ativação automática ao abrir o app
/// (`AltCrossApp`, controle "sempre ativo por padrão") e o checkbox manual
/// em `ConnectionsScreen` — a mesma lógica, pra não duplicar (e não
/// divergir) entre os dois pontos de entrada.
Future<HandoffActivationResult> activateHandoff({
  required HotZoneConfigStore store,
  required LookupTrustedHost lookupHost,
  required QueryPeerScreens queryPeerScreens,
  required LocalDisplaysProvider localDisplaysProvider,
  required StartHandoff startHandoff,
}) async {
  final enabledZones = store.zones.where((z) => z.enabled).toList();
  final zoneSpecs = <HandoffZoneSpec>[];
  final skipped = <String>[];

  for (final zone in enabledZones) {
    final host = lookupHost(zone.targetDeviceId);
    if (host == null) {
      skipped.add(zone.targetDeviceId);
      continue;
    }
    final displays = await queryPeerScreens(host);
    if (displays.isEmpty) {
      skipped.add(zone.targetDeviceId);
      continue;
    }
    var bounds = displays.first.rect;
    for (final d in displays.skip(1)) {
      bounds = bounds.expandToInclude(d.rect);
    }
    zoneSpecs.add(HandoffZoneSpec(
      edge: zone.edge,
      targetDeviceId: zone.targetDeviceId,
      targetScreenWidth: bounds.width,
      targetScreenHeight: bounds.height,
    ));
  }

  if (zoneSpecs.isEmpty) {
    return HandoffActivationResult(
      started: false,
      skippedDeviceIds: skipped,
      failureReason: enabledZones.isEmpty
          ? 'Nenhuma borda configurada — configure conexões no Arranjo primeiro.'
          : 'Nenhum dos dispositivos configurados está pareado e online agora.',
    );
  }

  final localDisplays = localDisplaysProvider();
  var localBounds = localDisplays.first.rect;
  for (final d in localDisplays.skip(1)) {
    localBounds = localBounds.expandToInclude(d.rect);
  }

  final ok = startHandoff(
    localScreenWidth: localBounds.width,
    localScreenHeight: localBounds.height,
    zones: zoneSpecs,
  );

  return HandoffActivationResult(
    started: ok,
    skippedDeviceIds: skipped,
    failureReason: ok
        ? null
        : 'Não consegui ativar — no macOS, confira Ajustes > Privacidade e'
            ' Segurança > Acessibilidade.',
  );
}
