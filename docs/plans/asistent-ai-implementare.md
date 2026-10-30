# Asistentul AI — planul de implementare

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Scopul:** un chat în Aion cu Claude, prin bridge-ul local `aion-bridge` (Node/TypeScript), împerecheat cu un cod
din 8 cifre; asistentul acționează doar prin cinci unelte Aion, fiecare modificare fiind un pas de undo.

**Arhitectura:** Aion (C++/Qt, `AssistantClient` + `AssistantPanel.qml`) vorbește JSON pe WebSocket cu bridge-ul de pe
`127.0.0.1:47821`. Bridge-ul rulează conversația cu Agent SDK (`query()` + un server MCP `aion` în proces) și trimite
fiecare apel de unealtă înapoi la aplicație, care îl execută cu `ScriptEngine`, comenzile și `Viewport`. Partea pură
(protocolul, codul de împerechere, politica comenzilor) stă în `core/` și e testată cu Catch2.

**Tehnologii:** C++20, Qt 6.10 (Quick, **WebSockets**), nlohmann-json (deja în core), Catch2; Node 20,
TypeScript 5, `@anthropic-ai/claude-agent-sdk`, `zod`, `ws`, `vitest`, `tsx`.

**Spec:** [`docs/plans/asistent-ai.md`](asistent-ai.md)

## Constrângeri globale
- `core/` nu include Qt; tot ce e nou în `core/` vine cu teste (CLAUDE.md).
- Protocolul: versiunea `1`; portul implicit `47821`; gazda `127.0.0.1`; `Origin` acceptat: lipsă sau `aion://desktop`.
- Codul de împerechere: 8 cifre, afișat `NNNN NNNN`, valabil 5 minute, unic, blocat după 5 încercări greșite.
- Token-ul: 32 de octeți aleatori, base64url; bridge-ul păstrează doar SHA-256 (hex) în
  `~/.config/aion-bridge/pairings.json`.
- Timpul maxim pentru o unealtă: 30 000 ms. Reconectarea aplicației: 1, 2, 5, apoi 10 s.
- Modelul implicit: `claude-opus-5-5`. Agentul **nu** are nicio unealtă Claude Code (fără shell, fișiere, web).
- Textele din UI și răspunsurile asistentului sunt în română; culorile doar din `Theme.qml`; setările noi folosesc
  `SettingsGroup` / `SettingsRow`.
- În QML nu folosi `long`, `int`, `short`, `byte`, `char`, `float`, `double` ca nume de variabile JS (qmlcachegen din CI).
- Nicio cheie, token sau cod real în repo sau în fixture-uri (fixture-urile folosesc valori evident false).

## Ce trebuie urmărit la revizie
1. **Un site deschis în browser încearcă `ws://127.0.0.1:47821`.** Conexiunea trebuie refuzată la upgrade (Origin),
   înainte de orice mesaj. Testul: Task 4, `refuză originile din browser`.
2. **Bridge-ul se oprește în mijlocul unei ture sau aplicația se deconectează cât o unealtă așteaptă.** Unealta
   trebuie să eșueze în cel mult o secundă (nu după 30 s), iar aplicația trece în „Oprit” și se reconectează singură.
   Testul: Task 5, `deconectarea eșuează apelurile în așteptare`; verificarea manuală din Task 9.
3. **Codul ghicit prin încercări.** După 5 coduri greșite, codul curent nu mai merge nici dacă e corect. Testul:
   Task 4, `blochează codul după 5 încercări`.
4. **Scriptul asistentului aruncă o eroare sau se blochează.** Pasul se anulează, agentul primește eroarea cu linia,
   iar chat-ul continuă. Testul: Task 5, `rezultatul cu eroare ajunge la agent ca isError`; Task 7, verificarea cu
   `--fake` (scenariul `eroare`).
5. **Token-ul aplicației nu mai e cunoscut de bridge** (după `aion-bridge unpair`). Aplicația trebuie să ceară un cod
   nou, nu să intre într-o buclă de reconectare. Testul: Task 4, `hello cu token necunoscut cere împerechere`.

---

### Task 1: Protocolul (o singură sursă) și codec-ul C++ din core

**Fișiere:**
- Creează: `bridge/protocol/PROTOCOL.md`, `bridge/protocol/fixtures/*.json` (lista de mai jos)
- Creează: `core/include/aion/core/assistant/Protocol.h`, `core/src/assistant/Protocol.cpp`
- Modifică: `core/CMakeLists.txt` (sursa nouă), `tests/CMakeLists.txt` (testul nou + definiția
  `AION_PROTOCOL_FIXTURES="${CMAKE_SOURCE_DIR}/bridge/protocol/fixtures"`)
- Test: `tests/core/AssistantProtocolTests.cpp`

**Mesajele (toate obiecte JSON cu `type`):**

| Direcție | `type` | Câmpuri |
|---|---|---|
| app → bridge | `hello` | `protocol: 1`, `client: "aion"`, `appVersion`, `token?` |
| app → bridge | `pair` | `code` (8 cifre, fără spații) |
| app → bridge | `chat.send` | `id`, `text`, `newConversation: bool` |
| app → bridge | `chat.cancel` | `id` |
| app → bridge | `tool.result` | `callId`, `ok`, `text`, `image?: { mimeType: "image/png", data }` |
| bridge → app | `hello.ok` | `protocol`, `bridgeVersion`, `paired`, `auth: "claude-code"\|"api-key"\|"fake"`, `model` |
| bridge → app | `hello.error` | `reason: "protocol"` (apoi închide) |
| bridge → app | `pair.ok` | `token` |
| bridge → app | `pair.error` | `reason: "wrong"\|"expired"\|"locked"`, `attemptsLeft` |
| bridge → app | `chat.delta` | `id`, `text` |
| bridge → app | `tool.call` | `id`, `callId`, `name`, `input` (obiect) |
| bridge → app | `chat.done` | `id` |
| bridge → app | `chat.error` | `id`, `code: "busy"\|"unpaired"\|"auth"\|"agent"\|"cancelled"`, `message` |

Fixture-uri: `app-hello.json`, `app-hello-token.json`, `app-pair.json`, `app-chat-send.json`, `app-chat-cancel.json`,
`app-tool-result.json`, `app-tool-result-image.json`, `bridge-hello-ok.json`, `bridge-hello-error.json`,
`bridge-pair-ok.json`, `bridge-pair-error.json`, `bridge-chat-delta.json` (text cu „ăâîșț”), `bridge-tool-call.json`
(`run_script` cu `code` și `title`), `bridge-chat-done.json`, `bridge-chat-error.json`.

**Interfețe:**
- Produce (namespace `aion::assistant`):
  - `inline constexpr int kProtocolVersion = 1;`
  - structurile `HelloOk { int protocol; std::string bridgeVersion; bool paired; std::string auth, model; }`,
    `HelloError { std::string reason; }`, `PairOk { std::string token; }`,
    `PairError { std::string reason; int attemptsLeft; }`, `ChatDelta { std::string id, text; }`,
    `ToolCall { std::string id, callId, name, inputJson; }`, `ChatDone { std::string id; }`,
    `ChatError { std::string id, code, message; }`
  - `using BridgeMessage = std::variant<HelloOk, HelloError, PairOk, PairError, ChatDelta, ToolCall, ChatDone, ChatError>;`
  - `std::optional<BridgeMessage> decodeBridgeMessage(std::string_view json);` (nullopt la JSON invalid, `type`
    necunoscut sau câmp obligatoriu lipsă)
  - `std::string encodeHello(std::string_view appVersion, std::optional<std::string_view> token);`
  - `std::string encodePair(std::string_view digits);`
  - `std::string encodeChatSend(std::string_view id, std::string_view text, bool newConversation);`
  - `std::string encodeChatCancel(std::string_view id);`
  - `struct ToolResult { std::string callId; bool ok; std::string text; std::optional<std::string> pngBase64; };`
  - `std::string encodeToolResult(const ToolResult& result);`
- nlohmann rămâne doar în `.cpp` (core îl leagă `PRIVATE`).

- [ ] **Pasul 1: Scrie `PROTOCOL.md` și fixture-urile** (tabelul de mai sus, plus fluxul: `hello` → `hello.ok`;
      dacă `paired: false` → `pair` → `pair.ok`; apoi `chat.send` → `chat.delta`* / `tool.call` ↔ `tool.result` →
      `chat.done` sau `chat.error`).
- [ ] **Pasul 2: Scrie testele care pică**

```cpp
TEST_CASE("decodează fiecare mesaj al bridge-ului din fixture-uri", "[assistant]")
// pentru fiecare bridge-*.json: decodeBridgeMessage(...) are valoare și alternativa potrivită;
// bridge-chat-delta.json → std::get<ChatDelta>(*m).text == "Am creat scara în spirală ăâîșț"
// bridge-tool-call.json → name == "run_script", nlohmann::json::parse(inputJson)["title"] == "Scară"
TEST_CASE("mesajele aplicației sunt identice cu fixture-urile", "[assistant]")
// json::parse(encodeHello("0.1.0", std::nullopt)) == json::parse(fișierul app-hello.json); la fel pentru toate app-*.json
TEST_CASE("mesajele invalide nu se decodează", "[assistant]")
// "", "{", R"({"type":"necunoscut"})", R"({"type":"chat.delta","id":"t1"})" → nullopt
```

- [ ] **Pasul 3: Rulează și verifică faptul că pică.** `make build && build/mac-debug/tests/aion_tests "[assistant]"`
      → eroare de compilare (`Protocol.h` lipsește).
- [ ] **Pasul 4: Implementează `Protocol.cpp`** (nlohmann; `input` se serializează înapoi cu `dump()`).
- [ ] **Pasul 5: Rulează testele.** Aceeași comandă → toate trec. Apoi `make test` → tot verde.
- [ ] **Pasul 6: Commit** `Asistent: protocolul v1 (PROTOCOL.md, fixture-uri) și codec-ul din core`

---

### Task 2: Codul de împerechere și politica comenzilor (core)

**Fișiere:**
- Creează: `core/include/aion/core/assistant/Pairing.h`, `core/src/assistant/Pairing.cpp`,
  `core/include/aion/core/assistant/CommandPolicy.h`, `core/src/assistant/CommandPolicy.cpp`
- Test: `tests/core/AssistantPolicyTests.cpp`

**Interfețe:**
- Produce:
  - `std::optional<std::string> normalizePairingCode(std::string_view typed);` — scoate spațiile și cratimele,
    cere exact 8 cifre ASCII.
  - `std::string formatPairingCode(std::string_view digits);` — `"48217730"` → `"4821 7730"`.
  - `enum class CommandAccess { Allowed, Denied };`
  - `CommandAccess agentCommandAccess(std::string_view commandId);` — prefixele permise și refuzurile explicite din
    spec (secțiunea 3).

- [ ] **Pasul 1: Scrie testele care pică**

```cpp
TEST_CASE("codul tastat se normalizează", "[assistant]")
// "4821 7730" → "48217730"; "4821-7730" → "48217730"; " 48217730 " → "48217730";
// "4821773" → nullopt; "482177301" → nullopt; "4821 773a" → nullopt; "４８２１７７３０" (cifre late) → nullopt
TEST_CASE("codul se afișează în două grupe", "[assistant]")
// formatPairingCode("48217730") == "4821 7730"
TEST_CASE("asistentul poate rula doar comenzile de modelare și vizualizare", "[assistant]")
// Allowed: "view.top", "edit.array", "org.group", "solid.union", "style.xray", "object.light.spot",
//          "scene.add", "material.new", "mode.animate", "anim.play", "camera.look", "tool.line"
// Denied:  "edit.undo", "edit.redo", "file.save", "file.open", "file.exportPng", "file.recent:0", "app.preferences",
//          "library.save", "plugin:gard/gard6", "script.console", "debug.crash", "", "view"
```

- [ ] **Pasul 2: Rulează și verifică faptul că pică** (`aion_tests "[assistant]"`: simbolurile lipsesc).
- [ ] **Pasul 3: Implementează cele trei funcții.**
- [ ] **Pasul 4: Rulează testele** → trec; `make test` → verde.
- [ ] **Pasul 5: Commit** `Asistent: codul de împerechere și politica comenzilor (core)`

---

### Task 3: Bridge-ul — scheletul și protocolul în TypeScript

**Fișiere:**
- Creează: `bridge/package.json` (`name: "aion-bridge"`, `type: "module"`, `engines.node: ">=20"`,
  `bin: { "aion-bridge": "dist/main.js" }`, scripturile `build: tsc`, `start: tsx src/main.ts`, `test: vitest run`,
  `typecheck: tsc --noEmit`), `bridge/tsconfig.json` (strict, `module: NodeNext`), `bridge/.gitignore`
  (`node_modules/`, `dist/`), `bridge/src/protocol.ts`
- Test: `bridge/test/protocol.test.ts`

**Interfețe:**
- Produce (`src/protocol.ts`): schemele zod `AppMessage` și `BridgeMessage` (uniuni discriminate pe `type`, exact
  tabelul din Task 1), tipurile inferate, `PROTOCOL_VERSION = 1`,
  `parseAppMessage(raw: string): AppMessage | null`, `encode(message: BridgeMessage): string`.

- [ ] **Pasul 1: `npm install`** cu `@anthropic-ai/claude-agent-sdk`, `zod`, `ws` și, pentru dezvoltare,
      `typescript`, `tsx`, `vitest`, `@types/ws`, `@types/node`; se commit-ează `package-lock.json`.
- [ ] **Pasul 2: Scrie testele care pică**

```ts
test("acceptă fiecare mesaj al aplicației din fixture-uri") // toate app-*.json → parseAppMessage(...) !== null, type corect
test("encode produce exact fixture-urile bridge-ului")       // JSON.parse(encode(obiect)) toEqual JSON.parse(bridge-*.json)
test("respinge mesajele invalide")                          // "", "{", '{"type":"x"}', '{"type":"pair","code":"123"}' → null
```

- [ ] **Pasul 3: Rulează** `cd bridge && npm test` → pică (`protocol.ts` lipsește).
- [ ] **Pasul 4: Implementează `protocol.ts`** (codul din `pair` trebuie să fie `/^\d{8}$/`).
- [ ] **Pasul 5: Rulează** `npm test && npm run typecheck` → trec.
- [ ] **Pasul 6: Commit** `Bridge: scheletul Node/TypeScript și protocolul v1`

---

### Task 4: Bridge-ul — împerecherea, token-urile și serverul

**Fișiere:**
- Creează: `bridge/src/store.ts`, `bridge/src/pairing.ts`, `bridge/src/server.ts`
- Test: `bridge/test/pairing.test.ts`, `bridge/test/server.test.ts`

**Interfețe:**
- Consumă: `parseAppMessage`, `encode` (Task 3).
- Produce:
  - `class TokenStore { constructor(file: string); has(token: string): boolean; add(token: string): void; clear(): void }`
    (scrie doar SHA-256 hex; creează dosarul; fișierul cu permisiuni `0600`)
  - `type PairResult = { ok: true; token: string } | { ok: false; reason: "wrong" | "expired" | "locked"; attemptsLeft: number }`
  - `class PairingManager { constructor(store: TokenStore, options?: { now?: () => number; newCode?: () => string; onCode?: (formatted: string) => void }); currentCode(): string; tryPair(code: string): PairResult; isPaired(token: string): boolean }`
    — un cod nou se generează (și se anunță prin `onCode`) la creare, după expirare, după blocare și după o
    împerechere reușită.
  - `function startServer(options: { port: number; host: "127.0.0.1"; pairing: PairingManager; info: { bridgeVersion: string; auth: string; model: string }; onSession: (socket: WebSocket) => void }): Promise<{ close(): Promise<void>; port: number }>`
    — verifică `Origin` (lipsă sau `aion://desktop`) și `Host` la upgrade (HTTP 403 altfel), apoi tratează `hello` /
    `pair`; doar o conexiune împerecheată ajunge la `onSession`. Orice alt mesaj înainte de împerechere →
    `chat.error` cu `code: "unpaired"`. `protocol` diferit → `hello.error` și închidere.

- [ ] **Pasul 1: Scrie testele care pică**

```ts
// pairing.test.ts (ceas și coduri injectate)
test("codul corect dă un token și devine invalid")      // tryPair("11112222").ok; a doua oară → { ok:false, reason:"wrong" }
test("codul expiră după 5 minute")                      // now += 300_001 → reason "expired"
test("blochează codul după 5 încercări")                // 5 × "00000000" → al 5-lea: reason "locked", attemptsLeft 0;
                                                        // apoi codul vechi corect → ok:false; onCode a fost apelat cu un cod nou
test("store-ul păstrează doar hash-ul")                 // conținutul fișierului nu conține token-ul; has(token) === true
// server.test.ts (port 0, client `ws`)
test("refuză originile din browser")                    // Origin "https://exemplu.ro" → conexiunea eșuează cu 403
test("refuză un Host străin")                           // Host "evil.test:port" → 403
test("hello cu token necunoscut cere împerechere")      // → hello.ok { paired:false }
test("împerecherea completă, apoi hello cu token")      // pair → pair.ok {token}; conexiune nouă cu token → paired:true, onSession apelat
test("mesajele dinainte de împerechere sunt refuzate")  // chat.send → chat.error { code:"unpaired" }
test("protocol greșit")                                 // hello protocol 2 → hello.error { reason:"protocol" }, socket închis
```

- [ ] **Pasul 2: Rulează** `npm test` → pică.
- [ ] **Pasul 3: Implementează** `store.ts` (`crypto.createHash("sha256")`), `pairing.ts` (`crypto.randomInt` pentru
      cifre, `crypto.randomBytes(32).toString("base64url")` pentru token), `server.ts` (`WebSocketServer` din `ws` cu
      `verifyClient`, legat de `127.0.0.1`).
- [ ] **Pasul 4: Rulează** `npm test && npm run typecheck` → trec.
- [ ] **Pasul 5: Commit** `Bridge: împerecherea cu cod din 8 cifre, token-uri și serverul local`

---

### Task 5: Bridge-ul — uneltele Aion și transmiterea lor la aplicație

**Fișiere:**
- Creează: `bridge/src/toolBroker.ts`, `bridge/src/aionTools.ts`
- Test: `bridge/test/toolBroker.test.ts`, `bridge/test/aionTools.test.ts`

**Interfețe:**
- Consumă: tipurile din `protocol.ts`.
- Produce:
  - `type ToolOutcome = { ok: boolean; text: string; image?: { mimeType: "image/png"; data: string } }`
  - `class ToolBroker { constructor(send: (m: BridgeMessage) => void, timeoutMs = 30_000); call(turnId: string, name: string, input: unknown): Promise<ToolOutcome>; resolve(result: { callId: string; ok: boolean; text: string; image?: ToolOutcome["image"] }): void; failAll(reason: string): void }`
    — `callId` nou pentru fiecare apel; la timeout → `{ ok:false, text:"Aion nu a răspuns în 30 s." }`;
    `failAll` rezolvă imediat toate apelurile în așteptare cu `ok:false`.
  - `const AION_TOOL_NAMES = ["get_scene", "list_commands", "run_command", "run_script", "capture_viewport"] as const`
  - `function createAionMcpServer(broker: ToolBroker, currentTurn: () => string)` — `createSdkMcpServer({ name: "aion", tools: [...] })`
    cu `tool(...)` pentru fiecare unealtă (schemele zod: `list_commands { query?: string }`, `run_command { id: string }`,
    `run_script { code: string, title: string }`; descrierile în română, cu unitățile în mm și referirea la API-ul
    `aion`); handler-ul transformă `ToolOutcome` în `CallToolResult`: text → `{ type: "text", text }`, imagine →
    `{ type: "image", data, mimeType }`, `ok:false` → `isError: true`. Adnotări: `readOnlyHint` pentru `get_scene`,
    `list_commands`, `capture_viewport`; `destructiveHint` pentru `run_script`, `run_command`.
  - `function aionToolIds(): string[]` → `["mcp__aion__get_scene", …]`

- [ ] **Pasul 1: Scrie testele care pică**

```ts
test("call trimite tool.call și se rezolvă la tool.result")           // send primește { type:"tool.call", id:"t1", name:"get_scene" }; resolve(...) → outcome
test("timeout după timeoutMs")                                        // timeoutMs 50 → { ok:false, text conține "30 s" sau timpul configurat }
test("deconectarea eșuează apelurile în așteptare")                   // două apeluri, failAll("deconectat") → ambele ok:false în < 1000 ms
test("rezultatul cu eroare ajunge la agent ca isError")               // handler-ul run_script cu outcome ok:false → { isError:true, content[0].text }
test("captura devine conținut de tip imagine")                        // outcome cu image → content[0] = { type:"image", mimeType:"image/png", data }
test("ID-urile uneltelor au prefixul serverului")                     // aionToolIds() conține "mcp__aion__run_script"
```

- [ ] **Pasul 2: Rulează** `npm test` → pică.
- [ ] **Pasul 3: Implementează** cele două fișiere (handler-ele se testează apelând direct definițiile întoarse de
      `tool(...)`; dacă SDK-ul nu le expune, extrage `handleOutcome(outcome): CallToolResult` și testează-l pe ea).
- [ ] **Pasul 4: Rulează** `npm test && npm run typecheck` → trec.
- [ ] **Pasul 5: Commit** `Bridge: uneltele Aion (MCP) transmise aplicației, cu timeout`

---

### Task 6: Bridge-ul — conversația cu Claude, autentificarea, modul `--fake` și CLI-ul

**Fișiere:**
- Creează: `bridge/src/auth.ts`, `bridge/src/prompt.ts`, `bridge/prompt/system.md`, `bridge/src/conversation.ts`,
  `bridge/src/fakeConversation.ts`, `bridge/src/session.ts`, `bridge/src/main.ts`, `bridge/README.md`
- Test: `bridge/test/auth.test.ts`, `bridge/test/conversation.test.ts`, `bridge/test/session.test.ts`

**Interfețe:**
- Consumă: `ToolBroker`, `createAionMcpServer`, `aionToolIds` (Task 5); `startServer`, `PairingManager`,
  `TokenStore` (Task 4).
- Produce:
  - `type AuthMode = "claude-code" | "api-key" | "fake"`;
    `resolveAuth(flag: "auto" | AuthMode, env: NodeJS.ProcessEnv): { mode: AuthMode; childEnv: NodeJS.ProcessEnv }`
    — `auto` = `api-key` dacă există `ANTHROPIC_API_KEY`, altfel `claude-code`; pentru `claude-code`, `childEnv`
    **nu** conține `ANTHROPIC_API_KEY` și nici `ANTHROPIC_AUTH_TOKEN`; `api-key` fără cheie → eroare clară.
  - `buildSystemPrompt(repoRoot: string): string` — `bridge/prompt/system.md` + conținutul lui `docs/scripting.md`.
  - `interface ConversationLike { send(turnId: string, text: string, emit: (m: BridgeMessage) => void): Promise<void>; cancel(): void; reset(): void }`
  - `class Conversation implements ConversationLike { constructor(deps: { queryFn: typeof query; broker: ToolBroker; systemPrompt: string; model: string; env: NodeJS.ProcessEnv }) }`
    — o tură = un `query()` cu opțiunile: `model`, `systemPrompt` (string), `mcpServers: { aion: createAionMcpServer(broker, () => turnId) }`,
    `tools: []` (nicio unealtă Claude Code), `allowedTools: aionToolIds()`, `canUseTool` care refuză orice nume din
    afara `aionToolIds()` (plasa de siguranță), `settingSources: []` (fără setările, skill-urile și hook-urile din
    `~/.claude`), `includePartialMessages: true`, `maxTurns: 20`, `env`, `resume` = `session_id`-ul turei anterioare
    (luat din primul mesaj care îl are).
    Text: evenimentele parțiale `content_block_delta` cu `text_delta` → `chat.delta`. Sfârșit: mesajul `result` →
    `chat.done` (sau `chat.error` cu `code:"agent"` dacă `subtype` nu e `success`; erorile de autentificare →
    `code:"auth"`). `cancel()` → `interrupt()` și `chat.error` cu `code:"cancelled"`. `reset()` uită sesiunea.
  - `class FakeConversation implements ConversationLike` — scenariul după text: `"eroare"` → `run_script` cu
    `throw new Error("test")` și apoi textul cu eroarea primită; orice altceva → `get_scene`, `run_script`
    (`aion.box(0, 0, 0, 1000, 1000, 1000, "Cub de test")`, titlul „Cub de test”), `capture_viewport`, apoi
    `chat.delta` „Am creat un cub de 1 m.” și `chat.done`.
  - `class Session { constructor(socket: WebSocket, deps: { broker: ToolBroker; conversation: ConversationLike }) }`
    — o tură activă: `chat.send` în timpul alteia → `chat.error code:"busy"`; `newConversation` → `reset()`;
    `tool.result` → `broker.resolve`; la închiderea socket-ului → `broker.failAll` și `cancel()`.
  - CLI (`src/main.ts`): `aion-bridge [--port 47821] [--model claude-opus-5-5] [--auth auto|claude-code|api-key] [--fake [--code 12345678]] [--repo <cale>]`
    și `aion-bridge unpair`. `--code` e acceptat doar cu `--fake`. La pornire afișează: versiunea, sursa de
    autentificare, modelul, portul și „Codul de împerechere: 4821 7730 (valabil 5 minute)”.

**`prompt/system.md`** (în română): ești asistentul din Aion; răspunzi în română, scurt; înainte de o modificare
citești scena (`get_scene`); faci o cerere într-un singur `run_script` când se poate; unitățile sunt mm; spui la final
ce ai schimbat și că se anulează cu ⌘Z; nu pretinzi că ai făcut ceva ce o unealtă n-a confirmat; pentru fișiere
(salvare, export, deschidere) îi spui utilizatorului ce comandă să folosească.

- [ ] **Pasul 1: Verificarea autentificării (spike, fără commit).** Pe Mac-ul tău, cu Claude Code logat și fără
      `ANTHROPIC_API_KEY`, rulează un script de 10 rânduri cu `query({ prompt: "Spune „salut”.", options: { tools: [], settingSources: [], env } })`.
      Rezultatul așteptat: un mesaj `result` cu `subtype: "success"`. Dacă dă eroare de autentificare, pune
      `pathToClaudeCodeExecutable` pe ieșirea lui `which claude` și repetă; notează varianta care merge în
      `bridge/README.md`. Verifică tot aici câmpurile exacte ale mesajelor parțiale (`type: "stream_event"`, `event`) în
      referința `SDKPartialAssistantMessage` și ajustează `Conversation` după ele.
- [ ] **Pasul 2: Scrie testele care pică** (`queryFn` fals, care produce o secvență fixă de mesaje)

```ts
test("auto alege api-key doar cu cheie")                     // resolveAuth("auto", {ANTHROPIC_API_KEY:"x"}).mode === "api-key"; fără → "claude-code"
test("claude-code scoate cheile din mediul copilului")       // childEnv nu are ANTHROPIC_API_KEY / ANTHROPIC_AUTH_TOKEN
test("query primește doar uneltele aion")                    // opțiunile capturate: tools [], allowedTools == aionToolIds(), settingSources []
test("canUseTool refuză orice unealtă străină")              // canUseTool("Bash", …) → deny; ("mcp__aion__get_scene") → allow
test("text_delta devine chat.delta, result devine chat.done")
test("a doua tură reia sesiunea")                            // al doilea query are resume === session_id din prima
test("reset uită sesiunea")                                  // după reset(), resume lipsește
test("o tură în timpul alteia e refuzată")                   // Session: două chat.send → al doilea chat.error code "busy"
test("închiderea socket-ului oprește tura și uneltele")      // failAll și cancel apelate
test("FakeConversation rulează scenariul prin broker")       // send(...) → tool.call get_scene, run_script, capture_viewport, apoi chat.done
```

- [ ] **Pasul 3: Rulează** `npm test` → pică.
- [ ] **Pasul 4: Implementează** fișierele de mai sus și `README.md` (pornirea: `cd bridge && npm install && npm start`;
      `npm start -- --fake --code 12345678` pentru teste; `unpair`; nota despre autentificare din spec, secțiunea 2).
- [ ] **Pasul 5: Rulează** `npm test && npm run typecheck && npm run build` → trec.
- [ ] **Pasul 6: Verificare manuală cu un client simplu:** `npm start -- --fake --code 12345678`, apoi
      `npx wscat -c ws://127.0.0.1:47821 -o aion://desktop` cu `hello`, `pair`, `chat.send` → se văd `tool.call`-urile.
- [ ] **Pasul 7: Commit** `Bridge: conversația cu Agent SDK, autentificarea, modul --fake și CLI-ul`

---

### Task 7: Aplicația — `AssistantClient` (conexiune, împerechere, chat, unelte)

**Fișiere:**
- Creează: `render/include/aion/render/AssistantClient.h`, `render/src/AssistantClient.cpp`
- Modifică: `render/CMakeLists.txt` (sursele; `find_package(Qt6 REQUIRED COMPONENTS WebSockets)`, legat `PRIVATE Qt6::WebSockets`)

**Interfețe:**
- Consumă: codec-ul și `ToolResult` (Task 1), `normalizePairingCode`, `agentCommandAccess` (Task 2),
  `text::fuzzyMatch`, `ScriptEngine::run`, `ScriptApi::objects`, `Document` (`commandAllowed`, `workMode`,
  `contextPath`, `selectionSummary`, `modelMaterials`, `faceCount`, `edgeCount`, `undoName`),
  `Viewport::captureThumbnail`.
- Produce (QML_ELEMENT, `aion::render::AssistantClient`):
  - `enum State { Offline, Connecting, NeedsPairing, Ready, Busy }` (Q_ENUM)
  - proprietăți: `document`, `scripts` (`ScriptEngine*`), `viewport` (`Viewport*`), `commands` (`QVariantList` din
    Main.qml: `{ id, title, category, shortcut }`), `port` (implicit 47821), `autoConnect` (bool), `state`,
    `messages` (`QVariantList` de `{ role: "user"|"assistant"|"tool"|"error", text, status?: "running"|"ok"|"error" }`),
    `authLabel`, `model`, `pairingError`
  - `Q_INVOKABLE void start(); void stop(); bool pair(const QString& typed); void send(const QString& text); void cancel(); void newConversation(); void forgetPairing();`
  - `signals: void commandRequested(const QString& id);`
  - token-ul în `QSettings` (grupul `assistant`, cheia `token`); `QWebSocket` cu originea `aion://desktop`.

**Comportament:**
- `start()` → `Connecting` → la conectare `hello` (cu token dacă există) → `hello.ok paired` → `Ready`, altfel
  `NeedsPairing`. Conexiune pierdută → `Offline` și reîncercare după 1, 2, 5, apoi 10 s (cât `start` e activ).
- `send()` adaugă mesajul utilizatorului și un mesaj gol al asistentului, trimite `chat.send` (id nou), trece în `Busy`;
  `chat.delta` completează ultimul mesaj al asistentului; `chat.done` / `chat.error` → `Ready` (eroarea ca mesaj
  `error`; `unpaired` → șterge token-ul și trece în `NeedsPairing`).
- `tool.call` adaugă un rând `tool` cu `status: "running"`, execută unealta, trimite `tool.result`, pune `ok` / `error`:
  - `get_scene`: obiect JSON cu `workMode`, `contextPath`, `selection`, `objects` (primele 200 din `ScriptApi::objects`,
    plus `moreObjects`), `materials` (numele), `faces`, `edges`, `lastUndo`;
  - `list_commands`: comenzile cu `agentCommandAccess == Allowed` și `commandAllowed`, sortate după `fuzzyMatch`
    (toate dacă `query` lipsește), cel mult 60;
  - `run_command`: refuz cu motiv dacă politica sau modul nu permit; altfel `emit commandRequested(id)`;
  - `run_script`: `scripts->run(code, "Asistent: " + title)` → textul = ieșirea, sau `Eroare la linia N: …` cu `ok:false`;
  - `capture_viewport`: `captureThumbnail()` în base64.
  Rândul uneltei are un titlu omenesc: „Citește modelul”, „Caută comenzi”, „Comanda <titlu>”, „Scriptul „<title>””,
  „Privește viewport-ul”.

- [ ] **Pasul 1: Implementează clasa** (nu există teste Qt în proiect; logica pură e deja testată în Task 1–2).
- [ ] **Pasul 2: Compilează** `make build` → fără avertismente noi (proiectul are `-Werror` pe web).
- [ ] **Pasul 3: Verifică în Task 8** (acțiunile de script de acolo rulează clasa cap-coadă cu `--fake`).
- [ ] **Pasul 4: Commit** `Asistent: AssistantClient (WebSocket, împerechere, chat, uneltele Aion)`

---

### Task 8: Aplicația — panoul de chat, Preferințele, comanda și acțiunile de script

**Fișiere:**
- Creează: `app/qml/shell/AssistantPanel.qml`, `app/qml/shell/AssistantMessage.qml`
- Modifică: `app/qml/Main.qml` (instanța `AssistantClient` lângă `ScriptEngine`, cu
  `onCommandRequested: (id) => window.runCommand(id)`; comanda `{ id: "assistant.toggle", title: "Asistent", category:
  "Ajutor", shortcut: "⌘J", icon: "message-circle" }`; ramura în `runCommand`; acțiunile de script de mai jos; comanda
  ascunsă pe web când `WebFiles.available`), `app/qml/overlays/PreferencesPage.qml` (categoria „Asistent”),
  `app/qml/shell/AppMenuBar.qml` (meniul Ajutor), `app/CMakeLists.txt` (fișierele QML noi; iconița `message-circle`
  dacă lipsește din `resources/icons/`)

**Interfețe:**
- Consumă: `AssistantClient` (Task 7).
- Acțiuni noi în `AION_STARTUP`: `assistant.start`, `assistant.pair:12345678`, `assistant.send:<text>`,
  `debug.assistant` (scrie în consolă starea și mesajele ca JSON).

- [ ] **Pasul 1: Verifică că ⌘J e liberă** în `commands` din Main.qml și în harta scurtăturilor pe moduri
      (`window.shortcutOwners`); dacă nu e, alege o altă tastă și trece decizia în spec.
- [ ] **Pasul 2: Implementează panoul** după spec (secțiunea 4): stările, lista de mesaje (`ListView`, Markdown cu
      `Text.MarkdownText` pentru asistent), câmpul de text (Enter trimite, ⇧Enter rând nou), „Stop”, „Conversație nouă”,
      subsolul cu ⌘Z; codul în două câmpuri de câte 4 cifre sau unul cu masca `9999 9999`.
- [ ] **Pasul 3: Implementează categoria „Asistent”** din Preferințe (`SettingsGroup` / `SettingsRow`): starea și
      sursa, portul, „Conectează automat la pornire”, „Uită împerecherea”, instrucțiunile de pornire.
- [ ] **Pasul 4: Verificare cap-coadă cu bridge-ul fals.** Într-un terminal: `cd bridge && npm start -- --fake --code 12345678`.
      În altul:

```bash
AION_SCREENSHOT=/tmp/asistent.png AION_SCREENSHOT_DELAY_MS=6000 \
AION_STARTUP="assistant.start;wait:800;assistant.pair:12345678;wait:500;assistant.send:fă un cub;wait:2500;debug.assistant" \
build/mac-debug/app/Aion.app/Contents/MacOS/Aion
```

      Rezultatul așteptat: în consolă, starea `Ready` și mesajele `user`, trei rânduri `tool` cu `status: "ok"`,
      `assistant` „Am creat un cub de 1 m.”; în captură se vede cubul și panoul; ⌘Z (`keys:,ctrl+z` adăugat la final)
      îl scoate. Repetă cu `assistant.send:eroare` → rândul scriptului are `status: "error"`, modelul rămâne neschimbat.
      Repetă cu `AION_THEME=light`.
- [ ] **Pasul 5: Commit** `Asistent: panoul de chat (⌘J), Preferințe → Asistent, acțiunile de script`

---

### Task 9: Verificarea cu Claude, documentația și CI-ul

**Fișiere:**
- Creează: `docs/decisions/0041-asistent-ai.md`
- Modifică: `CLAUDE.md` (secțiunea despre asistent: fișierele, acțiunile de script, `--fake`), `docs/qa-checklist.md`
  (scenariile de mai jos), `PLAN.md` (o etapă nouă cu sarcinile bifate), `README.md` (cum pornești bridge-ul),
  `.github/workflows/ci.yml` (modulul `qtwebsockets` în toate pașii `install-qt-action`; job-ul `bridge`:
  `actions/setup-node` cu Node 20, `npm ci`, `npm run typecheck`, `npm test` în `bridge/`)

- [ ] **Pasul 1: Verificare reală, cu abonamentul tău:** `cd bridge && npm start`, împerechere din Preferințe, apoi
      în chat: „Ce face unealta Follow Me?” (răspuns fără unelte de modificare), „Fă o scară în spirală de 3 m”
      (un singur pas de undo „Asistent: …”, ⌘Z o scoate), „Ce vezi în viewport?” (folosește captura),
      „Salvează fișierul” (asistentul refuză și îți spune comanda). Notează timpii și eventualele probleme în
      ADR.
- [ ] **Pasul 2: Scenariile de robustețe** (în `qa-checklist.md`): oprești bridge-ul în mijlocul unei ture → chat-ul
      arată eroarea, panoul trece în „Oprit”, revine singur după repornire; `aion-bridge unpair` → aplicația cere un
      cod nou; ⌘Z în timpul unei ture → nimic nu crapă, tura continuă pe modelul actual; `curl` cu
      `-H "Origin: https://exemplu.ro"` spre port → 403.
- [ ] **Pasul 3: Scrie ADR 0041** (contextul, cele trei variante discutate — MCP extern, bridge, cheie API —, decizia,
      constrângerea de autentificare cu citatul, siguranța conexiunii, ce rămâne pentru publicare: Keychain, executabil
      descărcabil, web, aprobarea Anthropic sau `api-key`).
- [ ] **Pasul 4: Actualizează CI-ul** și verifică pe un push că job-urile macOS, Windows, web și `bridge` sunt verzi
      (`gh run watch`).
- [ ] **Pasul 5: Commit** `Asistent: ADR 0041, documentație, QA și CI (qtwebsockets, job-ul bridge)`
