#ifndef ALTCROSS_EXPORT_H
#define ALTCROSS_EXPORT_H

/* Marca símbolos que cruzam a fronteira FFI (chamados pelo Flutter via
 * dart:ffi). No Windows uma DLL não exporta nada por padrão, então essa
 * macro é obrigatória ali; em Linux/macOS ela só deixa a intenção explícita.
 * Fora do build da lib compartilhada expande para nada: os únicos consumidores
 * do .dll/.dylib via FFI não usam estes headers, e dllimport nas fontes da
 * lib estática conflita com a definição local (C4273 no MSVC). */
#if defined(_WIN32)
#if defined(ALTCROSS_BUILDING_SHARED)
#define ALTCROSS_API __declspec(dllexport)
#else
#define ALTCROSS_API
#endif
#else
#define ALTCROSS_API __attribute__((visibility("default")))
#endif

#endif
