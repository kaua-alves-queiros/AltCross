#!/usr/bin/env bash
# Dev build para macOS: builda o Core em C com CMake e copia a lib
# compartilhada pra dentro de app/, depois roda `flutter run -d macos`.
#
# Por quê um script em vez de integração direta no Xcode (como fizemos no
# Windows via CMake puro): o Runner.xcodeproj do macOS precisaria de um build
# phase novo pra chamar o CMake, e não há como adicionar isso com segurança
# de forma automatizada aqui (CocoaPods está quebrado neste ambiente e editar
# o project.pbxproj na mão arrisca corromper o projeto). Esse script cobre o
# fluxo de dev; a integração via build phase no Xcode fica como próximo passo
# manual (rápido de fazer direto na IDE) quando o FFI real entrar.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE_BUILD_DIR="$ROOT_DIR/core/build"
NATIVE_DEV_DIR="$ROOT_DIR/app/build/native/macos"

cmake -S "$ROOT_DIR/core" -B "$CORE_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$CORE_BUILD_DIR" --target altcross_core_shared

mkdir -p "$NATIVE_DEV_DIR"
cp "$CORE_BUILD_DIR/libaltcross_core.dylib" "$NATIVE_DEV_DIR/"

cd "$ROOT_DIR/app"
exec flutter run -d macos "$@"
