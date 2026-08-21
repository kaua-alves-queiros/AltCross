import 'package:altcross_app/main.dart';
import 'package:altcross_app/models/physical_display.dart';
import 'package:altcross_app/state/hot_zone_config_store.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('AltCrossApp abre na tela de arranjo', (WidgetTester tester) async {
    await tester.pumpWidget(AltCrossApp(
      store: HotZoneConfigStore(),
      displayProvider: () => const [
        PhysicalDisplay(x: 0, y: 0, width: 1920, height: 1080, isPrimary: true),
      ],
    ));

    expect(find.text('AltCross'), findsOneWidget);
    expect(find.text('Arranjo de telas'), findsOneWidget);
  });
}
