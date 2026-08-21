/// Pedido de pareamento recebido de outro dispositivo, ainda esperando
/// confirmação — a UI mostra `code` na tela pra quem pediu digitar do outro
/// lado (prova de presença física, como pareamento de Apple TV).
class IncomingPairingRequest {
  final String requesterDeviceId;
  final String requesterName;
  final int code;

  const IncomingPairingRequest({
    required this.requesterDeviceId,
    required this.requesterName,
    required this.code,
  });
}

enum PairingOutcome { accepted, rejected, error }

/// Resultado de `AltCrossNative.confirmPairing`.
class PairingResult {
  final PairingOutcome outcome;
  final String? deviceId;
  final String? name;

  const PairingResult._(this.outcome, this.deviceId, this.name);

  factory PairingResult.accepted({
    required String deviceId,
    required String name,
  }) =>
      PairingResult._(PairingOutcome.accepted, deviceId, name);

  factory PairingResult.rejected() =>
      const PairingResult._(PairingOutcome.rejected, null, null);

  factory PairingResult.error() =>
      const PairingResult._(PairingOutcome.error, null, null);
}
