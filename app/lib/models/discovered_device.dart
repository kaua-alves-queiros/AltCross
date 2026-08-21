/// Um dispositivo AltCross encontrado na rede local via
/// `AltCrossNative.runDiscovery` — espelha `altcross_discovery_reply_t` +
/// o host de onde a resposta veio.
class DiscoveredDevice {
  final String deviceId;
  final String name;
  final String host;
  final int port;

  const DiscoveredDevice({
    required this.deviceId,
    required this.name,
    required this.host,
    required this.port,
  });
}
