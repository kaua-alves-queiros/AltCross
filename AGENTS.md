# AltCross

AltCross é um sistema de compartilhamento de tela, teclado, mouse, áudio e área de
transferência entre múltiplos computadores/dispositivos (Windows, Linux, macOS, Android,
iOS) na mesma rede local, com criação de monitores virtuais e navegação entre sistemas via
`Alt+Tab` — como um KVM/Synergy avançado com telas virtuais e roteamento de áudio.

Este arquivo é a fonte de verdade sobre a arquitetura e o escopo de funcionalidades do
produto. Qualquer agente de IA (Claude ou outro) trabalhando neste repositório deve ler
este documento antes de propor mudanças estruturais, e mantê-lo atualizado conforme a
arquitetura evoluir de fato no código.

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

- Este projeto ainda não tem código-fonte (repositório recém-criado, sem git
  inicializado). Ao começar a implementação, proponha e confirme com o usuário a
  estrutura de diretórios (ex.: `app/` para o Flutter, `core/` ou `native/` para o
  daemon em C, e o binding FFI entre eles) antes de gerar muitos arquivos.
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
