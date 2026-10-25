# DeathStar

AutoClicker interno leve para Minecraft 1.8.9.

> [!WARNING]
> Projeto educacional. Use por sua conta e risco — use apenas em servidores que permitem mods ou em contas alternativas. Pode causar ban pelo anticheat.

## Funcionalidades

**Combate**
- **AutoClicker** — CPS mín/máx randomizado, jitter, modo burst (trava no CPS máximo), só-clicando-segurando, não clicar em menus, controle de quebrar blocos, keybind de toggle (suporte a M4/M5), contador de CPS
- **NoHitDelay** — remove o delay de hit
- **NoJumpDelay** — remove o delay de pulo
- **BlockHit** — os modos Manual e Automático exigem segurar o botão direito; o Automático permite configurar o intervalo entre cliques
- **Alt Manager** — troca localmente o nome da sessão e permite restaurar a sessão original

**Overlay**
- **Watermark in-game** — nome, usuário, FPS, CPS, hora, posição por canto + cor customizável, desenhada via render hook

**Loader (ImGui + DX11)**
- Seletor de processo (`minecraft.exe` / `javaw.exe`) com detecção de client
- Inject / Eject, status ao vivo (`dllAttached`)
- Editor de config com persistência, trava de instância única

## Clients Suportados

Mapeados via `DLL/src/game/mapping/Mapper.cpp`:

- Badlion
- Forge 1.8.9
- Feather 1.8.9
- Lunar

## Arquitetura

Omega.sln
├── Loader/   # UI ImGui DX11, Config, Injector (ManualMap), seletor de processo
├── DLL/      # Módulo injetado: GameInstance JNI, Mapper, Features, RenderHook
│   └── src/
│       ├── dllmain.cpp       # sync via memória compartilhada (~100ms) + tick do jogo (~60Hz)
│       ├── core/RenderHook.cpp
│       ├── Features/AutoClicker, NoHitDelay, NoJumpDelay, render/Watermark
│       └── game/jni + game/mapping + game/classes
├── Shared/Shared.h  # SharedConfig via DeathStar::SharedConfig (file mapping)
└── external/ # imgui, json.hpp, minhook

Como funciona:
1. O Loader cria a memória compartilhada (`SharedConfig`) e faz manual-map da `DeathStar.dll` no jogo.
2. A DLL anexa à JVM (`GameInstance`), detecta a versão e carrega os mappings (tenta de novo se injetado no menu principal).
3. Loader <-> DLL sincronizam as configs a cada 100ms; lógica do jogo roda a ~60Hz. O toggle pela keybind in-game reflete de volta no checkbox do Loader.

## Requisitos

- Windows 10/11 x64
- Visual Studio 2022 (v143, SDK 10+) com workload C++ desktop
- Minecraft Java 1.8.9 em um dos clients suportados

## Compilação

1. Clone:
   ```bash
   git clone https://github.com/celeridade/DeathStar.git
2. Abra o Omega.sln no Visual Studio.
3. Selecione Release | x64.
4. Compile a solution — gera Loader.exe + DeathStar.dll.
Sem vcpkg/NuGet — imgui, minhook e json.hpp já estão vendorizados em external/.
Como Usar
1. Abra o Minecraft (client 1.8.9).
2. Rode o Loader.exe como Administrador.
3. Selecione o processo do jogo no dropdown.
4. Configure as abas Combate / Watermark.
   - Em Combate, ative BlockHit e segure o botão direito. O modo Automático permite ajustar o intervalo entre os cliques.
   - Em Alt Manager, aplique um nick offline ou restaure a sessão original.
5. Clique em Inject.
6. Alterne o AutoClicker in-game pela keybind configurada. Fechar o Loader ejeta a DLL.
A config é salva automaticamente ao sair.

O Alt Manager altera somente a sessão local do cliente; não autentica outra conta nem altera a identidade já validada pelo servidor.
Configuração
Opção	Padrão	Descrição
autoClickerMinCps / MaxCps	10 / 14	Intervalo de clique randomizado
jitter	off	Adiciona randomização extra
onlyWhileHolding	on	Só clica segurando o M1
dontClickInMenus	on	Pausa em inventários/menus
breakBlocks	off	Permite quebrar blocos
burst	off	Trava direto no CPS máximo
keybind	0 (livre)	Tecla de toggle VK, 0x05/0x06 = M4/M5
watermarkCorner	0	0=sup-esq, 1=sup-dir, 2=inf-esq, 3=inf-dir
Stack
- C++17, Win32 API
- ImGui + DirectX 11 (UI do Loader + overlay in-game)
- MinHook (render hook)
- JNI (acesso ao jogo)
- Injeção ManualMap + IPC por memória compartilhada
Aviso Legal
Apenas para fins de estudo / engenharia reversa. Os autores não se responsabilizam por bans ou mau uso.
