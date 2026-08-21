# AltCross

AltCross é um sistema de compartilhamento de tela, teclado, mouse, áudio e área de
transferência entre múltiplos computadores/dispositivos (Windows, Linux, macOS, Android,
iOS) na mesma rede local, com criação de monitores virtuais e navegação entre sistemas via
`Alt+Tab` — como um KVM/Synergy avançado com telas virtuais e roteamento de áudio.

Este arquivo é a fonte de verdade sobre a arquitetura e o escopo de funcionalidades do
produto. Qualquer agente de IA (Claude ou outro) trabalhando neste repositório deve ler
este documento antes de propor mudanças estruturais, e mantê-lo atualizado conforme a
arquitetura evoluir de fato no código.

> **Escopo atual de desenvolvimento**: o produto final é multiplataforma (Windows,
> Linux, macOS, Android, iOS — ver seção 6), mas o desenvolvimento ativo agora foca
> **só em macOS e Windows**. Linux, Android e iOS têm suporte planejado, mas não devem
> receber trabalho de integração/build agora — não gaste esforço nisso a menos que o
> usuário peça explicitamente.

## 🏛️ Arquitetura Técnica do Sistema

```
┌────────────────────────────────────────────────────────┐
│                   INTERFACE FLUTTER                     │
│      (Desenvolvimento único em Dart para a UI)          │
│                                                          │
│  • Visual adaptativo por SO (Fluent, Cupertino, Yaru)    │
│  • Mapeamento de telas, bordas e arranjo gráfico         │
│  • Painel de volume, dispositivos e conexões mDNS        │
└───────────────────────────┬──────────────────────────────┘
                             │
                Comunicação FFI (dart:ffi)
                             │
┌───────────────────────────▼──────────────────────────────┐
│                    MOTOR NATIVO EM C                      │
│          (Daemon / Serviço de Alta Performance)           │
│                                                            │
│  • Driver de Monitor Virtual (IDD / Dummy Display)         │
│  • Captura/Injeção Global de Input (Win32 / uinput)        │
│  • Driver de Áudio Virtual e Codec Opus                    │
│  • Sockets UDP / QUIC + WebRTC para Vídeo                  │
└────────────────────────────────────────────────────────────┘
```

- **Core em C**: roda como um serviço de fundo leve (10–20 MB de RAM). Responsável pelo
  acesso direto ao hardware e ao kernel (gráficos, periféricos, áudio e rede de baixa
  latência).
- **UI em Flutter**: atua apenas como casca de configuração e visualização. Comunica-se
  com o Core em C com zero custo de latência via `dart:ffi`.

## 🗂️ Estrutura dos Projetos

Dois projetos independentes na raiz do repositório, cada um com seu próprio ciclo de
build/teste. Já existe um binding FFI real (não é só plumbing de build): a enumeração
de monitores físicos (`altcross_displays_enumerate`, ver `displays.h` abaixo) é chamada
de verdade pelo Dart via `dart:ffi` em `app/lib/native/altcross_native.dart`.

### `core/` — motor nativo em C

```
core/
├── CMakeLists.txt          # gera 2 alvos a partir das mesmas fontes:
│                           #   altcross_core        (STATIC, linkado pelos testes)
│                           #   altcross_core_shared (SHARED -> .dylib/.so/.dll,
│                           #                          o que o Flutter carrega via FFI)
│                           # + o executável altcrossd (ver tools/, só em macOS/Windows)
├── include/altcross/       # headers públicos
│   ├── export.h            # macro ALTCROSS_API (dllexport/visibility) pra marcar
│   │                       # símbolos que cruzam a fronteira FFI
│   ├── hotzone.h           # detecção de borda/canto + resolução pra device_id
│   ├── input_control.h     # máquina de estados "quem é dono do input agora"
│   │                       # (local vs. dispositivo remoto) + cálculo de onde o
│   │                       # cursor entra/sai em cada lado da troca
│   ├── keycode.h           # enum de tecla neutro (independe de SO — ver mapeamento
│   │                       # em cada platform_input.c)
│   ├── platform_input.h    # interface comum de captura/injeção global de
│   │                       # mouse+teclado (ver src/platform/<so>/)
│   ├── protocol.h          # pacotes versionados trocados entre as 2 máquinas
│   │                       # (ENTER/MOUSE_DELTA/MOUSE_BUTTON/KEY/LEAVE)
│   ├── net_socket.h        # wrapper fino de socket UDP (BSD sockets/Winsock) —
│   │                       # inclui receive_from (dá o IP de quem mandou) e
│   │                       # enable_broadcast (aciona o prompt de permissão de
│   │                       # Rede Local — só chamar a partir de ação do usuário)
│   ├── pairing.h           # cadastro de dispositivos confiáveis (sobrevive a
│   │                       # troca de IP), código de confirmação, token secreto,
│   │                       # identidade estável da máquina local
│   ├── discovery.h         # protocolo de "quem está aí" / "sou eu" por broadcast
│   │                       # na rede local (não é mDNS/DNS-SD de verdade, ver nota
│   │                       # em "Fluxo de Desenvolvimento") + altcross_discovery_
│   │                       # run_query, que já manda o broadcast de verdade e
│   │                       # coleta respostas — é o binding FFI usado pela tela
│   │                       # de Conexões (rodado numa isolate no lado Dart)
│   └── displays.h          # enumeração dos monitores físicos reais da máquina —
│                           # é o binding FFI que a tela de arranjo usa de verdade
├── src/                    # implementação (.c), um arquivo por módulo/domínio
│   ├── hotzone.c
│   ├── input_control.c
│   ├── protocol.c
│   ├── net_socket.c
│   ├── pairing.c
│   ├── discovery.c
│   └── platform/
│       ├── macos/platform_input.c   # real, via CGEventTap/CGEventPost — compila
│       │                            # e é coberto por build nesta sessão (macOS)
│       ├── macos/displays.m         # real, via NSScreen (único .m do Core —
│       │                            # precisa de Objective-C; ver enable_language(OBJC)
│       │                            # no CMakeLists) — testado nesta sessão com `nm` e
│       │                            # rodando list_displays de verdade (achou os 2
│       │                            # monitores reais desta máquina)
│       ├── windows/platform_input.c # real, via SetWindowsHookEx/SendInput — compila
│       │                            # no MSVC num Windows real (build sem warnings);
│       │                            # captura/injeção em runtime ainda não exercitada
│       └── windows/displays.c       # real, via EnumDisplayMonitors — mesma ressalva
│                                    # acima (compila; runtime ainda não exercitado)
├── tools/
│   ├── altcrossd.c         # daemon real que liga tudo (só builda em macOS/Windows)
│   │                       # — NUNCA rodar automaticamente, ver aviso no arquivo
│   ├── list_displays.c     # ferramenta segura (só lê monitores, sem captura de
│   │                       # input nem rede) — pode rodar manualmente sem aviso
│   └── discovery_demo.c    # ferramenta segura (`responder`/`query`) usada pra
│                           # verificar a descoberta de rede de ponta a ponta com
│                           # processos reais — sem hook de input, roda sem aviso
└── tests/                  # testes unitários (framework próprio, sem dependências
    ├── test_framework.h    # externas — ver "Testes" nas boas práticas)
    ├── test_framework.c
    ├── test_main.c         # agrega e roda todos os run_<modulo>_tests()
    ├── test_hotzone.c
    ├── test_input_control.c
    ├── test_protocol.c
    ├── test_net_socket.c   # inclui round-trip real por socket UDP em loopback
    ├── test_pairing.c
    └── test_discovery.c
```

**Status da funcionalidade "mouse/teclado passa de um PC pro outro"** (seção 2 da
especificação): lógica de decisão (`input_control`), protocolo de rede
(`protocol`/`net_socket`), pareamento/identidade persistente (`pairing`) e descoberta
(`discovery`) estão implementados e com testes passando — tudo isso é testável sem
tocar em SO de verdade. A captura/injeção real (`platform_input`) está escrita e
compila no macOS; a versão Windows também compila no MSVC e o suite de testes do
Core passa num Windows real (build + ctest verificados), mas a captura/injeção em
runtime ainda não foi exercitada de verdade. A enumeração de monitores físicos (`displays`) está
implementada, compila no macOS e **já tem FFI real ligado na tela de arranjo**
(`app/lib/screens/arrangement_screen.dart`), que mostra as telas físicas de verdade
desta máquina. **A descoberta na rede está completa e verificada end-to-end de
verdade** — não é só o lado que pergunta: `altcross_discovery_start_responder`
(`discovery.c`) sobe uma thread real (pthread no macOS/Linux, `CreateThread` no
Windows) que escuta `ALTCROSS_DISCOVERY_PORT` e responde a quem perguntar. O app
Flutter chama isso automaticamente ao abrir (`main()`, via
`AltCrossNative.startDiscoveryResponder`) — só escuta/responde por unicast, nunca
manda broadcast sozinho, então não aciona o prompt de permissão de Rede Local por
conta própria. O botão "Buscar dispositivos" na tela de Conexões
(`AltCrossNative.runDiscovery`, rodando numa isolate Dart) já usa esse caminho de
verdade.

Verificado nesta sessão rodando processos reais, não só lido/assumido: (1) dois
processos `discovery_demo` (`core/tools/discovery_demo.c`, ferramenta segura de
verificação manual, sem hook de input) — um como `responder`, outro como `query` —
se encontraram de verdade pelo IP real da máquina; (2) o **app de verdade** (buildado
via `flutter build macos`, rodado como processo real) foi encontrado por um
`discovery_demo query` externo, retornando seu hostname real e o device_id gerado.
Nesse processo achei e corrigi um bug real: o app roda com **App Sandbox** habilitado
(`macos/Runner/*.entitlements`) e tinha `com.apple.security.network.server` mas
**faltava `com.apple.security.network.client`** — o socket conseguia fazer bind e
receber (por isso a busca parecia "não achar nada" silenciosamente), mas o sandbox
bloqueava o `sendto` da resposta. Corrigido em `DebugProfile.entitlements` e
`Release.entitlements` (os dois precisam de `network.client` E `network.server`, já
que o mesmo processo pergunta E responde). Fique de olho nisso ao adicionar qualquer
funcionalidade de rede nova — App Sandbox no macOS é silencioso quando bloqueia, não
lança erro nenhum do lado do socket, só a operação nunca completa.

Falta ainda: (1) o handshake de pareamento (`PAIR_REQUEST`/`PAIR_CONFIRM`) usando o
código gerado por `pairing.h` sobre a rede — sem isso, os "dispositivos remotos" tanto
na tela de arranjo quanto os "encontrados" na tela de Conexões não são pareados de
verdade (arranjo: nome digitado manualmente; conexões: dado bruto da resposta de
descoberta, sem autenticação, e a porta de pareamento ainda vai sempre `0` já que
`startDiscoveryResponder` não tem uma porta real de pareamento pra anunciar ainda);
(2) trocar a resolução de tela real do dispositivo remoto (hoje a tela de arranjo
assume Full HD por padrão pra todo mundo, até o pareamento trocar essa informação de
verdade); (3) rodar `altcrossd` (o daemon de mouse/teclado) de verdade end-to-end
entre 2 máquinas reais — note que a descoberta (responder automático no app Flutter)
e o handoff de mouse/teclado (`altcrossd`, hook global) são caminhos **separados**
hoje; a descoberta não depende do daemon arriscado rodar. **Nunca ative o hook de
captura (`altcross_platform_input_start`) automaticamente** — isso captura o
mouse/teclado real de quem estiver rodando; só rodar manualmente, avisando antes. Já
`list_displays`, `discovery_demo` e o respondedor de descoberta
(`altcross_discovery_start_responder`) são seguros de rodar/ligar automaticamente (não
capturam input, só rede); só o `altcross_discovery_run_query` (lado que pergunta,
manda broadcast) deve ficar atrás de uma ação explícita do usuário — o botão já faz
isso certo.

Convenção ao adicionar um módulo novo (ex.: `clipboard`, `audio_mixer`, `input_inject`):
um header em `include/altcross/<modulo>.h`, implementação em `src/<modulo>.c`, testes em
`tests/test_<modulo>.c` com uma função `run_<modulo>_tests(void)` chamada a partir de
`tests/test_main.c`. Código específico de plataforma (Win32/uinput/CGEvent) entra em
`src/platform/<so>/` atrás de uma interface comum — nunca `#ifdef` misturado à lógica de
domínio. Toda função que vai ser chamada pelo Flutter via `dart:ffi` deve ser marcada
com `ALTCROSS_API` (de `altcross/export.h`) no header — sem isso o símbolo não é
exportado da DLL no Windows.

Build e testes:

```bash
cmake -S core -B core/build -DCMAKE_BUILD_TYPE=Debug
cmake --build core/build
ctest --test-dir core/build --output-on-failure
```

### `app/` — UI em Flutter

Projeto Flutter padrão (`flutter create`), com suporte a Windows, Linux, macOS, Android
e iOS já habilitado. Pastas relevantes para o dia a dia (o resto —
`android/`, `ios/`, `linux/`, `macos/`, `windows/` — é boilerplate de cada plataforma,
gerado pelo próprio `flutter create`, e só deve ser tocado para configuração específica
de plataforma, ex.: registrar o binding FFI nativo):

```
app/
├── lib/
│   ├── main.dart           # monta o MaterialApp e a HotZoneConfigStore raiz
│   ├── models/             # modelos de dados puros
│   │   ├── hot_zone.dart           # equivalente Dart dos structs de hotzone do Core,
│   │   │                           # com toJson/fromJson
│   │   ├── physical_display.dart   # equivalente Dart de altcross_display_t
│   │   └── discovered_device.dart  # dispositivo achado por AltCrossNative.runDiscovery
│   ├── native/             # única pasta que fala dart:ffi com o Core — todo o resto
│   │   └── altcross_native.dart   # do app passa por aqui, nunca `dart:ffi` direto
│   │                               # em outro lugar. DynamicLibrary.open resolve o
│   │                               # caminho do .dylib/.dll relativo ao executável.
│   │                               # Chamadas bloqueantes (runDiscovery) rodam numa
│   │                               # Isolate.run pra não travar a UI.
│   ├── services/           # lógica pura de UI que não é estado nem modelo
│   │   ├── arrangement.dart    # geometria de "que borda esse retângulo está tocando"
│   │   │                       # — usada pelo arrangement_screen, testável sem widget
│   │   └── hot_zone_labels.dart  # rótulo/ícone de cada borda em pt-BR — compartilhado
│   │                              # entre arrangement_screen e connections_screen
│   ├── state/              # lógica de estado/orquestração da UI (stores/notifiers),
│   │   └── hot_zone_config_store.dart  # sem lógica de baixo nível — isso é do Core
│   └── screens/            # telas — pequenas, delegam estado pra lib/state/
│       ├── home_screen.dart         # página inicial: cards de módulo (Arranjo,
│       │                            # Conexões) — só navega, sem lógica própria
│       ├── arrangement_screen.dart  # tela de arranjo: mostra as telas físicas reais
│       │                            # (via native/) + dispositivos remotos
│       │                            # arrastáveis, grudando na borda mais próxima
│       └── connections_screen.dart  # mapeia as conexões já configuradas
│                                     # (HotZoneConfigStore) + botão "Buscar
│                                     # dispositivos" (descoberta real via native/)
└── test/                   # testes com flutter_test, um arquivo por classe de lib/
    ├── widget_test.dart              # smoke test do AltCrossApp
    ├── hot_zone_config_store_test.dart
    ├── arrangement_test.dart         # testa a geometria pura de arrangement.dart
    ├── arrangement_screen_test.dart  # testa render/add/remove; arrastar de verdade
    │                                  # (drag) não é testado automaticamente — a
    │                                  # escala do canvas é dinâmica e recalculada a
    │                                  # cada frame, o que torna simular um arrasto
    │                                  # pixel-perfeito no teste frágil demais pro
    │                                  # benefício; validar arrastando manualmente
    ├── home_screen_test.dart
    └── connections_screen_test.dart  # discoveryRunner sempre injetado nos testes —
                                       # nunca dispara o broadcast/permissão de rede de
                                       # verdade durante `flutter test`
```

Convenção: cada arquivo em `lib/state/`, `lib/models/`, `lib/services/` ou `lib/screens/`
tem seu par em `test/` com o mesmo nome + `_test.dart` (exceto `lib/native/`, que só
compila com a lib nativa de verdade presente — ver limitação de teste acima). Telas
ficam em `lib/screens/`, sempre pequenas e delegando estado para `lib/state/` (ver
`arrangement_screen.dart`/`connections_screen.dart` — a tela não decide regra de
negócio, só chama `store.add/remove` e a geometria/rótulos de `lib/services/`).

Build e testes:

```bash
cd app
flutter analyze
flutter test
```

## 🛠️ Fluxo de Desenvolvimento (Dev Build)

O projeto tem três velocidades de build/teste durante o desenvolvimento — use a mais
rápida que sirva para o que você está fazendo antes de subir para a próxima:

1. **Lógica pura (a maior parte do dia a dia)**: `flutter test` no `app/` e `ctest` no
   `core/`, sem nunca abrir o app de verdade. É onde mora toda a lógica de negócio
   testável (ex.: hot zones), e não depende de nenhum build nativo linkado.
2. **Dev build da UI**: `flutter run -d macos` (ou `linux`/`windows`) dá hot reload real
   para iterar em telas. Para isso funcionar mesmo antes do FFI existir, a UI só deve
   falar com o Core através de uma interface Dart (ex.: um `CoreClient` abstrato) — isso
   **não é mock** (mock é só coisa de teste, ver boas práticas), é uma implementação
   real trocável; mais tarde a implementação via `dart:ffi` entra atrás dessa mesma
   interface sem precisar mexer na UI.
3. **Dev build integrado (UI + Core nativo via FFI)**: builda o `core/` como lib
   compartilhada (`.so`/`.dylib`/`.dll`) e deixa ela acessível pro processo do Flutter.
   Implementado hoje (Windows e macOS; Linux ainda não):

   - **Windows — integração direta via CMake, sem script**: `app/windows/CMakeLists.txt`
     dá `add_subdirectory` no `core/` e instala o `altcross_core.dll` junto do
     executável (mesma pasta, via a regra `install(...)` que o Flutter já usa pros
     plugins). Basta rodar `flutter run -d windows` normalmente — o CMake do próprio
     Flutter builda o Core junto, nada extra a fazer. **Validado num Windows real**:
     `flutter build windows --debug` builda app + `altcross_core.dll` sem warnings
     (os flags de warning do core são escolhidos por compilador — GCC/Clang vs.
     MSVC, ver `core/CMakeLists.txt`).
   - **macOS — integração direta via build phase no Xcode, sem script**: o
     `macos/Runner.xcodeproj` tem um Run Script Build Phase (`Build AltCross Core
     (CMake)`) no target `Runner` que builda `core/` com CMake e copia
     `libaltcross_core.dylib` para `Contents/Frameworks/` dentro do próprio `.app`.
     Basta rodar `flutter run -d macos` ou `flutter build macos` normalmente —
     verificado nesta sessão (`nm` confirma os símbolos `altcross_*` dentro do
     `.dylib` empacotado no bundle). De propósito **sem output declarado** no build
     phase (aceita o warning "roda em todo build" do Xcode): declarar o `.dylib` como
     output faria o Xcode pular o script quando o arquivo já existisse, mesmo que o
     código-fonte do core tivesse mudado — arriscaria empacotar uma engine
     desatualizada silenciosamente. O phase foi adicionado programaticamente com a
     gem `xcodeproj` (mesma lib que o CocoaPods usa por baixo) em vez de editar o
     `project.pbxproj` na mão — só foi possível porque o CocoaPods deste ambiente,
     que estava quebrado por causa do `LANG`/`LC_ALL` não apontarem pra UTF-8, foi
     consertado exportando `LANG=en_US.UTF-8` e `LC_ALL=en_US.UTF-8` antes de rodar
     `pod`/`ruby`. Se for mexer de novo nesse build phase (ex.: trocar o script), use
     a mesma gem `xcodeproj` em vez de editar o `.pbxproj` manualmente, e sempre valide
     depois com `plutil -lint project.pbxproj` e `xcodebuild -list -project
     Runner.xcodeproj`.
   - **Linux**: não integrado — fora do escopo de desenvolvimento atual (ver nota no
     topo do arquivo). Fazer só quando o suporte a Linux entrar de fato em pauta.

   Esse é o build mais pesado — use só quando o trabalho for especificamente na
   fronteira FFI; para todo o resto, fique nos níveis 1 e 2.

**Trade-off a ter em mente**: hot reload do Flutter funciona liso do lado Dart, mas
qualquer mudança de assinatura/struct do lado C força hot *restart* + rebuild nativo.
Por isso, mantenha a superfície de FFI pequena e estável, e sempre deixe a lógica de
verdade testável em Dart/C puro (nível 1) antes de expor via FFI (nível 3).

## 📋 Especificação Completa de Funcionalidades

### 1. Gestão de Telas Virtuais & Chaveamento (`Alt+Tab`)

- **Criação de Monitores Virtuais na Tela Principal**: o Core em C gera uma ou mais telas
  "fantasmas" via software (Indirect Display Driver no Windows, Dummy Display no Linux,
  CGVirtualDisplay no macOS) diretamente na tela física atual.
- **Navegação por `Alt+Tab` entre Sistemas**: o Core intercepta o atalho globalmente no SO
  (`SetWindowsHookEx` / `CGEventTap`) para alternar a exibição da tela principal
  instantaneamente entre a área de trabalho local e os sistemas remotos.
- **Mapeamento de Rotação Personalizado**: definição via UI Flutter de quais telas,
  monitores virtuais e dispositivos entram na fila do `Alt+Tab` e qual a ordem de
  navegação.
- **Troca com Retenção de Estado**: mudar para outro sistema operacional mantendo todas
  as janelas e programas da máquina anterior abertos no exato ponto em que foram
  deixados.

### 2. Mapeamento de Bordas, Cantos e Atuação de Periféricos

- **Zonas Reativas (Hot Zones) e Cantos Mapeados**: configuração de bordas e cantos
  específicos para disparar a troca de controle (ex.: bater o mouse no canto superior
  direito para saltar para outro PC).
- **Ancoragem Direcionada do Cursor**: definição de coordenadas (X, Y) exatas de entrada
  e saída do ponteiro entre as telas.
- **Redirecionamento Automático de Teclado & Mouse**: injeção direta de eventos no
  sistema operacional de destino (`SendInput` no Windows, `/dev/uinput` no Linux,
  `CGEvent` no macOS).
- **Sensibilidade e Trava Anti-Escape**: ajuste de força na borda e atalhos de trava
  (Ctrl, Scroll Lock) para impedir a saída do mouse durante jogos ou programas em tela
  cheia.

### 3. Sincronização e Roteamento de Áudio & Microfone

- **Matriz de Áudio Cruzada**: captura do áudio do sistema remoto via C e transmissão
  com compressão Opus via UDP para reprodução nas caixas/fones da máquina principal.
- **Microfone Virtual Integrado**: instalação de um driver de áudio virtual no C para
  injetar a voz capturada de qualquer celular ou PC remoto como se fosse um microfone
  físico plugado no sistema.
- **Mixer de Volume por Dispositivo**: controle de ganho e volume de cada sistema remoto
  diretamente na interface do Flutter.
- **Cancelamento de Eco e Baixa Latência**: sincronismo automático entre áudio e o fluxo
  de vídeo (lip-sync).

### 4. Produtividade & Área de Transferência

- **Clipboard Unificado com Histórico**: sincronização de texto, códigos e imagens entre
  áreas de trabalho (`Ctrl+C` → `Cmd+V`) com histórico recente armazenado.
- **Arrastar e Soltar Arquivos nas Bordas (Drag & Drop)**: arrastar arquivos até a borda
  da tela para enviá-los via socket de rede direto para o outro sistema.
- **Perfis Contextuais Automáticos**: regras automáticas de ativação de bordas e áudio
  baseadas no executável/app que estiver aberto no momento.

### 5. Conectividade, Desempenho & Controle Remoto

- **Descoberta Automática na Rede Local (mDNS)**: localização automática dos
  dispositivos na rede Wi-Fi/cabo sem necessidade de digitar IPs manualmente.
- **Painel de Desempenho (Overlay)**: indicador em tempo real de latência (ms), uso de
  banda e FPS.
- **Gestão de Energia Remota (Wake-on-LAN)**: ligar, suspender, reiniciar ou desligar
  máquinas conectadas.
- **Execução em Segundo Plano (Modo Silencioso)**: o Core em C continua ativo na bandeja
  do sistema mesmo se a janela em Flutter for fechada.

### 6. Compatibilidade e Interface Adaptativa

- **Multiplataforma**: compatível com Windows, Linux, macOS, Android e iOS.
- **UI Adaptativa Nativa**: renderização visual ajustada ao sistema operacional
  hospedeiro (Fluent UI no Windows, Cupertino no macOS/iOS, Material 3 no Android, Yaru
  no Linux).

## Notas para agentes de IA

- **Nunca commite direto na main.** Toda mudança deve ir via branch + pull request.
  Fluxo obrigatório: crie uma branch (`git checkout -b <tipo>/<descrição>`), faça as
  alterações, commite na branch, faça push e abra um PR contra `main`. O usuário revisa
  e faz merge. Exemplos de nomes de branch: `feat/virtual-monitor`, `fix/hotzone-boundary`,
  `refactor/protocol-parsing`.
- O repositório já está estruturado (`app/` Flutter, `core/` motor em C, git
  inicializado com remoto no GitHub — ver seção "Estrutura dos Projetos" acima). Não
  proponha recriar essa estrutura do zero; ao adicionar algo novo, siga as convenções
  já estabelecidas em cada pasta.
- **Nunca dê `git push`/`git commit` sem o usuário pedir explicitamente** — o
  repositório tem remoto real configurado (`origin` no GitHub), então um push afeta
  algo visível fora desta máquina.
- O Core em C lida com drivers de kernel/sistema (monitor virtual, injeção de input,
  áudio virtual) — mudanças nessa camada são sensíveis por plataforma (Windows, Linux,
  macOS) e devem ser tratadas com cautela, já que normalmente exigem privilégios
  elevados e assinatura de driver.
- Ao adicionar uma funcionalidade nova, localize a seção correspondente acima e
  mantenha esta especificação em sincronia com o que for de fato implementado.

## 🧭 Boas Práticas de Codificação (obrigatórias para todas as IAs)

Estas regras valem para qualquer agente (Claude, GPT, Copilot, etc.) que gerar ou
alterar código neste repositório. Em caso de conflito entre uma prática abaixo e um
pedido pontual do usuário, avise o usuário e siga a orientação dele — mas por padrão,
siga estas regras.

### Gerais (todo o repositório)

- **Sem gambiarra "porque funciona"**: entenda a causa raiz antes de corrigir um bug.
  Não silencie erros, não use `try/catch` vazio, não desative warnings/lints só para o
  build passar.
- **Não superengenharia**: implemente exatamente o que a funcionalidade pede. Sem
  abstrações, configs ou "flexibilidade para o futuro" que ninguém pediu ainda. Três
  linhas parecidas são melhores que uma abstração prematura.
- **Sem comentário óbvio**: comente apenas o *porquê* de algo não óbvio (uma
  invariante, um workaround de bug específico de SO, uma decisão de performance). Nunca
  comente o que o código já diz por si (nomes de variáveis/funções claros substituem
  comentário).
- **Consistência de nomenclatura entre as camadas**: nomes de funções/structs expostos
  via FFI devem ser idênticos (ou seguir uma convenção 1:1 documentada) entre o header C
  e o binding Dart, para evitar bugs silenciosos de mismatch.
- **Tratamento de erro nas fronteiras, não em todo lugar**: valide entrada em fronteiras
  do sistema (rede, FFI, entrada do usuário, parsing de config). Não valide de novo o
  que uma camada interna já garante.
- **Segurança por padrão**: toda comunicação entre dispositivos (input, clipboard,
  áudio, vídeo, arquivos) deve ser autenticada e, quando fizer sentido, criptografada
  (ex.: pareamento com PIN/QR + chave de sessão, TLS/DTLS/QUIC). Nunca aceite conexões
  ou comandos de injeção de input/arquivo sem esse handshake. Isso é crítico: este app
  literalmente injeta teclado, mouse e arquivos em outra máquina.
- **Sem credenciais ou segredos hardcoded** em código, configs versionadas ou logs.
- **Commits/PRs pequenos e coerentes**: uma mudança = um propósito. Mensagens de commit
  explicam o *porquê*, não o *o quê* (o diff já mostra o quê).
- **Mock só em teste, nunca em código real**: dublês de teste (mock/stub/fake/spy) só
  podem existir dentro de arquivos de teste. Código de produção (C ou Dart) nunca deve
  conter implementação "fake"/mockada, atalho condicional para ambiente de teste, ou
  branch de código só para permitir mock (`if (isTest) ...`). Se uma dependência
  precisa ser substituível em teste, isole-a atrás de uma interface/ponteiro de função
  real e injete a implementação de teste de fora — a implementação de produção em si
  nunca sabe que está sendo testada.

### Core nativo em C

- **Compilar sem warnings** com `-Wall -Wextra` (e `-Wpedantic` quando possível). Trate
  warning como erro em CI (`-Werror`) nos módulos novos.
- **Gestão de memória disciplinada**: todo `malloc`/recurso do SO (handle, socket, file
  descriptor, driver handle) tem um dono claro e um caminho de liberação único. Prefira
  padrões RAII-like (`init`/`destroy` pareados) e cheque *todo* retorno de alocação.
  Rode com ASan/Valgrind localmente antes de considerar um módulo pronto.
- **Nunca panique o processo por erro recuperável**: falha de rede, dispositivo remoto
  ausente ou driver indisponível deve retornar erro tratável para a camada acima, não
  derrubar o daemon inteiro (ele roda em segundo plano e precisa ser resiliente).
  Falhas irrecuperáveis logam claramente e saem, sem deixar handles de SO vazando ou
  no meio-termo (ex.: monitor virtual criado mas não destruído).
- **Isolar código específico de plataforma**: código Win32/uinput/CGEvent fica atrás de
  uma interface comum (ex.: `platform.h` com uma implementação por SO), nunca `#ifdef`
  espalhado pela lógica de negócio.
- **Concorrência explícita**: documente qual thread possui qual recurso (loop de
  captura de input, loop de rede, loop de áudio). Sem estado global mutável compartilhado
  sem sincronização.
- **Protocolo de rede versionado**: todo pacote (input, áudio, clipboard, controle) tem
  um campo de versão/tipo desde o primeiro commit, para permitir evolução sem quebrar
  compatibilidade entre versões do app.

### UI em Flutter/Dart

- **UI só orquestra, não decide lógica de negócio de baixo nível**: decisões de
  hot zone, redirecionamento de input, mixagem de áudio etc. vivem no Core em C; o
  Flutter manda intenção/config e reflete estado.
- **Chamadas FFI sempre tratadas como potencialmente falhas**: nunca assuma que uma
  chamada nativa retornou sucesso; trate erro e evite travar a UI thread com chamadas
  bloqueantes no Core (use isolates/`compute` ou canais assíncronos quando a chamada
  não for trivial).
- **Sem lógica duplicada entre plataformas de UI**: os temas adaptativos (Fluent,
  Cupertino, Material 3, Yaru) trocam apenas a casca visual; o estado e a lógica de
  navegação/configuração ficam em um único lugar (state management único, ex.: Riverpod
  ou Bloc — escolher um e manter consistência em todo o app).
- **Widgets pequenos e nomeados pelo que representam**, evitando `build()` gigantes.
- **Seguir `dart format`/`flutter analyze` sem exceções suprimidas** (`// ignore:` só
  com justificativa comentada do porquê é seguro ali).

### Testes

- **Core em C**: cobrir com testes unitários a lógica pura (parsing de protocolo,
  cálculo de hot zones/coordenadas, matriz de áudio) que não depende de driver real.
  Código dependente de driver/SO precisa de um plano de teste manual documentado, já
  que é difícil de automatizar em CI.
- **Flutter**: testes de widget para telas de configuração; testes de unidade para
  qualquer lógica de mapeamento/estado antes de ela chegar ao FFI.
- Não é necessário perseguir 100% de cobertura — priorize os caminhos críticos de
  segurança (autenticação de pareamento, injeção de input) e os que mexem com estado
  de driver/SO (criação/destruição de monitor virtual, dispositivo de áudio virtual).
