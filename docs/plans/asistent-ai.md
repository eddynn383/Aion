# Asistentul AI din Aion — designul

> Cerința din 2026-10-29: un chat în aplicație, cu Claude, conectat printr-un **bridge** local pornit din terminal și
> împerecheat cu un **cod din 8 cifre**. Deocamdată îl folosește doar autorul. Planul de implementare:
> [`asistent-ai-implementare.md`](asistent-ai-implementare.md). Decizia se scrie în ADR 0041.

## 1. Ce vrem

Un panou de chat în Aion în care îi spui asistentului ce vrei („fă o scară în spirală de 3 m”, „cum fac o gaură
într-un perete?”, „pune toate ușile pe tag-ul Uși”). Asistentul:
- răspunde la întrebări despre aplicație (unelte, scurtături, moduri de lucru);
- citește modelul deschis (obiecte, selecție, modul curent) și se uită la viewport;
- face modificări prin scripturi și comenzi, **fiecare ca un pas de undo**, vizibil în chat.

### Obiective
1. Chat în aplicație, cu răspunsuri în streaming, în limba română.
2. Conexiunea printr-un bridge local (Node/TypeScript), împerecheat o singură dată cu un cod din 8 cifre.
3. Asistentul acționează doar prin uneltele Aion (citește scena, caută comenzi, rulează o comandă, rulează un
   script, captură de viewport). Nimic în afara aplicației.
4. Sursa de autentificare a bridge-ului e interschimbabilă: login-ul local Claude Code (uz personal) sau cheie API.

### Ce NU facem acum
- Versiunea web a asistentului (browserul și `ws://127.0.0.1` din pagină `https` au reguli diferite pe browsere).
- Distribuirea bridge-ului ca executabil descărcabil din aplicație (pentru uz personal rulează din `bridge/`).
- Păstrarea token-ului în Keychain (deocamdată în setările aplicației, ca text).
- Istoricul conversațiilor salvat între sesiuni; atașarea de fișiere sau imagini în chat.

### Ce ar însemna un eșec, chiar dacă „merge”
- Asistentul poate rula comenzi pe Mac în afara Aion (shell, fișiere): uneltele Claude Code trebuie oprite complet.
- Un site deschis în browser se poate conecta la bridge.
- O modificare a asistentului nu se poate anula cu ⌘Z sau strică modelul pe jumătate.
- Chat-ul îngheață aplicația sau rămâne „blocat” când bridge-ul se oprește.

## 2. Autentificarea (constrângere)

Documentația Agent SDK: *„Unless previously approved, Anthropic does not allow third party developers to offer
claude.ai login or rate limits for their products, including agents built on the Claude Agent SDK.”*
- **Acum (uz personal):** bridge-ul folosește login-ul tău local Claude Code (`--auth claude-code`).
- **Dacă Aion devine public:** fie aprobarea Anthropic, fie `--auth api-key` (`ANTHROPIC_API_KEY`, facturare pe API).
  Aplicația nu știe care sursă e folosită; bridge-ul o anunță doar ca etichetă.

## 3. Arhitectura

```
Aion (Qt)                                     aion-bridge (Node 20, TypeScript)
┌──────────────────────────────┐   WebSocket  ┌────────────────────────────────────────────┐
│ AssistantPanel.qml (chat)    │  127.0.0.1   │ server: Origin / Host, hello, împerechere   │
│ AssistantClient (C++)  ◄─────┼─────:47821───┼─► Session (o conexiune, o tură activă)      │
│  execută uneltele:           │              │   Conversation (Agent SDK `query`)          │
│  ScriptEngine, comenzi,      │ ◄── tool.call│   MCP „aion”: 5 unelte → ToolBroker → app   │
│  Viewport.captureThumbnail   │ tool.result ►│   uneltele Claude Code: toate oprite        │
└──────────────────────────────┘              └────────────────────────────────────────────┘
```

- **Protocolul** e JSON pe WebSocket, versiunea 1, descris într-un singur loc (`bridge/protocol/PROTOCOL.md`) cu
  exemple comune (`bridge/protocol/fixtures/`) testate și din C++, și din TypeScript.
- **Împerecherea:** bridge-ul afișează la pornire un cod `4821 7730`, valabil 5 minute, de unică folosință,
  blocat după 5 încercări greșite (atunci afișează un cod nou). La succes, aplicația primește un token aleator
  (32 de octeți, base64url). Bridge-ul păstrează doar hash-ul lui (SHA-256) în `~/.config/aion-bridge/pairings.json`;
  `aion-bridge unpair` le șterge pe toate.
- **Siguranța conexiunii:** bridge-ul ascultă doar pe `127.0.0.1`; refuză la upgrade orice `Origin` în afară de
  `aion://desktop` (Aion îl trimite; browserele trimit mereu originea paginii) și orice `Host` care nu e
  `127.0.0.1:<port>` sau `localhost:<port>`.
- **Uneltele** (serverul MCP `aion` din bridge, executate de aplicație):

  | Unealtă | Intrare | Ce face în Aion |
  |---|---|---|
  | `get_scene` | — | Modul, contextul, selecția, obiectele (cel mult 200), materialele, numărul de fețe / muchii, ultimul pas de undo |
  | `list_commands` | `query?` | Comenzile permise asistentului, filtrate fuzzy (`text::fuzzyMatch`) |
  | `run_command` | `id` | O comandă permisă (politica din core + `Document::commandAllowed`) |
  | `run_script` | `code`, `title` | `ScriptEngine::run` → un pas de undo „Asistent: <title>”; o eroare îl anulează |
  | `capture_viewport` | — | PNG de cel mult 512 px (`Viewport::captureThumbnail`), ca imagine |

- **Politica comenzilor** (core, testată): permise doar prefixele `view.`, `edit.`, `org.`, `solid.`, `style.`,
  `object.`, `scene.`, `material.`, `mode.`, `anim.`, `camera.`, `tool.`; refuzate explicit `edit.undo`, `edit.redo`.
  Tot ce ține de fișiere, aplicație, plugin-uri, debug și consola de scripturi rămâne al utilizatorului.
- **Undo:** fiecare `run_script` / `run_command` e un pas separat, ca utilizatorul să poată anula parțial. Promptul
  îi cere asistentului să facă o cerere într-un singur script când se poate.
- **Conversația:** o tură = un apel `query()` cu `resume` pe sesiunea anterioară; „Conversație nouă” uită sesiunea.
  O singură tură activă pe conexiune; „Stop” = `interrupt()`.
- **Modelul:** implicit `claude-opus-5-5`, schimbabil cu `--model`.
- **Modul de test `--fake`:** bridge-ul răspunde fără Claude, după un scenariu fix care folosește uneltele reale.
  Așa se verifică aplicația cap-coadă cu `AION_STARTUP`, fără costuri și fără rețea.

## 4. Interfața

- Panoul **Asistent** se deschide cu ⌘J (comanda `assistant.toggle`, categoria „Ajutor”, comună tuturor modurilor), în
  dreapta viewport-ului, cu lățimea de 360 px.
- Stări: **Oprit** (cum pornești bridge-ul + „Reîncearcă”), **Împerechere** (câmp pentru 8 cifre, afișate 4 + 4),
  **Gata** (chat), **Lucrează** (chat + „Stop”).
- Mesaje: ale tale (bule), ale asistentului (Markdown), rânduri pentru unelte („Scriptul „Scară”” ✓ / ✗,
  „Citește modelul”, „Privește viewport-ul”), erori.
- Subsolul: „Fiecare modificare e un pas de undo (⌘Z)”.
- Preferințe → **Asistent**: starea, portul, conectarea automată la pornire, „Uită împerecherea”.

## 5. Decizii luate de Claude (de verificat la citire)
- Portul implicit **47821**; reconectarea după 1, 2, 5, apoi la fiecare 10 s.
- Timpul maxim de așteptare pentru o unealtă: **30 s** (scripturile se opresc oricum după 15 s).
- Token-ul în `Settings` (categoria `assistant`), nu în Keychain, cât e uz personal.
- ⌘J pentru panou (liberă acum; ⌘⌥J rămâne consola de scripturi).
- Bridge-ul stă în repo, în `bridge/`, cu test în CI (Node 20).
