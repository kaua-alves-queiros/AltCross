import 'package:altcross_app/services/connection_status.dart';
import 'package:altcross_app/widgets/connection_loss_overlay.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('dispositivo ficando offline mostra aviso neutro (sem cor de erro)',
      (tester) async {
    final notifier = ConnectionStatusNotifier();
    await tester.pumpWidget(MaterialApp(
      home: ConnectionLossOverlay(
        notifier: notifier,
        child: const Scaffold(body: SizedBox()),
      ),
    ));

    notifier.updateStatus('pc-windows', false);
    await tester.pump();

    expect(find.textContaining('pc-windows desconectou'), findsOneWidget);
    final snackBar = tester.widget<SnackBar>(find.byType(SnackBar));
    expect(snackBar.backgroundColor, isNull);
  });

  testWidgets('dispositivo voltando também gera aviso (não só ao cair)',
      (tester) async {
    final notifier = ConnectionStatusNotifier();
    await tester.pumpWidget(MaterialApp(
      home: ConnectionLossOverlay(
        notifier: notifier,
        child: const Scaffold(body: SizedBox()),
      ),
    ));

    notifier.updateStatus('pc-windows', false);
    await tester.pump();
    await tester.pump(const Duration(seconds: 5));

    notifier.updateStatus('pc-windows', true);
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 750));

    expect(find.textContaining('pc-windows conectou'), findsOneWidget);
  });

  testWidgets(
      'cair e voltar mais de uma vez continua avisando (não trava depois da 1ª vez)',
      (tester) async {
    final notifier = ConnectionStatusNotifier();
    await tester.pumpWidget(MaterialApp(
      home: ConnectionLossOverlay(
        notifier: notifier,
        child: const Scaffold(body: SizedBox()),
      ),
    ));

    notifier.updateStatus('pc-windows', false);
    await tester.pump();
    await tester.pump(const Duration(seconds: 5));
    notifier.updateStatus('pc-windows', true);
    await tester.pump();
    await tester.pump(const Duration(seconds: 5));

    notifier.updateStatus('pc-windows', false);
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 750));

    expect(find.textContaining('pc-windows desconectou'), findsOneWidget);
  });
}
