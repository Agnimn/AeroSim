# AeroSim Generation and Entity Persistence Fix Report

Date: 2026-11-19

## 1. Bug 1 root cause

The parser classified ordinary `Create`, `Generate`, and `Make` prompts as replace operations. The Ollama
response could therefore be validated as the correct `ADD_ENTITIES` command but executed through a
replacement prompt, and entity/model state was also reset by replacement semantics.

The response validator also canonicalized model output before its original-intent comparison. This could
lose route intent or turn a malformed/incomplete Ollama response into a generic rejection. Operation
normalization was too strict in several places, and `entity_count`/location variants and lowercase
operation names were not normalized consistently.

## 2. Bug 2 root cause

Ordinary create/generation requests used replacement semantics instead of incremental add semantics.
The engine's generation commit path used `m_scenarioGenerated && (Add || CreateFormation)` for additive
updates, but the parser returned `Replace` for `Create` and `Generate`. Existing scenario entities were
therefore copied? or replaced according to the wrong operation.

The code did contain an incremental `EntityModel::setEntitiesIncremental` path and
`SimulationEngine::setScenario(..., preserveExistingState)`, but ordinary prompts never reliably reached
it. Replacements now require an explicit `Replace` or `Reset` prompt.

## 3. Route-related issue

The reproduction route clause:

`route one aircraft from Bengaluru to Chennai`

was not recognized by the existing route regex, which mainly recognized `send/route ... to <city>` forms.
The parser now recognizes the explicit `from ... to ...` form and retains `routeDestinations`.

For generated aircraft, only the first requested route destination is assigned to the first newly
generated aircraft. The remaining aircraft receive the normal placement route. The route role stores
latitude/longitude coordinates in `QGeoCoordinate(latitude, longitude)` order.

## 4. Files modified

- `ai/AIParser.cpp`: classify ordinary create/generate/make prompts as ADD; recognize the explicit
  Bengaluru-from/Chennai-to route wording.
- `ai/AICommand.cpp`: normalize operation names and accept `entity_count` as a count alias.
- `core/ScenarioEngine.cpp`: bounded Ollama correction retry, raw-response logging, exact mismatch
  reporting, explicit replacement execution, formation mismatch preservation, duplicate ID/callsign
  validation, invalid-route-coordinate validation, and transactional commit checks.
- `core/ScenarioEngine.h`: per-request Ollama retry counter.
- `tests/airbase_tests.cpp`: updated/additional route and replacement regression expectations.

## 5. Implementation

Normal `Add`, `Create`, `Generate`, and `Make` entity prompts now parse as additive operations.
Only `Replace` and `Reset` use reset/replacement behavior.

The engine builds a complete candidate scenario before committing it. It validates:

- entity count and position bounds;
- route destination resolution;
- route coordinates;
- duplicate IDs and callsigns;
- final entity, route, and model consistency.

The existing simulation engine is called with `preserveExistingState=true` for incremental updates,
preserving existing runtime state while adding only new entities.

Ollama responses are logged raw. If parsing or authoritative intent validation fails, one bounded
correction request is made while preserving the original prompt and exact validation failure. A second
failure rejects the response and leaves the scenario unchanged.

Deterministic fallback remains environment-controlled:

- `AEROSIM_DISABLE_DETERMINISTIC_FALLBACK=1` disables fallback;
- without that setting, unavailable Ollama keeps the existing deterministic-parser fallback behavior.

## 6. Automated test results

Build: passed with Qt 6.11.2 MinGW 64-bit Debug environment.

CTest: `100% tests passed, 0 tests failed` (`airbaseTests`, 22.01 seconds).

The suite includes deterministic/mock coverage for:

- 5 aircraft;
- incremental 5 -> 8 -> 12 -> 14;
- Bengaluru route generation;
- malformed Ollama responses;
- wrong model operation;
- duplicate IDs/callsigns;
- explicit replace/reset;
- paused and running simulation preservation;
- entity/model counter consistency.

## 7. Live Ollama test results

Server: `http://127.0.0.1:11434`

Model: `qwen2.5-coder:1.5b`

Live tests passed:

- workflow: 5, then 8, then 15, then 15 with a live Ollama response;
- exact reproduction: 5 aircraft in Bengaluru, exactly one route ending in Chennai.

The exact live route test logged a first response that omitted `route_destinations`, triggered the bounded
correction request, and then accepted the corrected response.

One additional live formation-management test for `Disband the formation.` remained model-dependent:
the 1.5b model emitted a malformed/wrong operation before exhausting the bounded correction, while the
same management behavior passed deterministic/mock coverage. This report does not claim that live case
passed.

All live Ollama tests used fallback disabled.

## 8. GUI verification

The actual GUI executable was launched with the Qt 6.11.2 environment. The process started successfully
and the main window was observed as `Airspace Scenario Generator`.

The window/process check is actual runtime verification, but it was not a scripted mouse/keyboard
interaction through the UI. Therefore it does not claim to have visually confirmed the reproduction
prompt in the GUI. Manual input through the running window is still recommended.

## 9. Before-and-after entity counts

- Automated replacement test: 2 -> 2 after explicit replacement.
- Automated mock ADD test: 2 -> 5.
- Automated mock South India ADD test: 2 -> 7.
- Live workflow: 0 -> 5 -> 8 -> 15 -> 15.
- Live exact route: 0 -> 5.

## 10. Remaining limitations

- Live behavior for some 1.5b model variants is still sensitive to formatting and schema aliases.
- The bounded correction is deliberately limited to one retry.
- General multi-waypoint route support exists in the route subsystem, but this fix only claims and tests
  the implemented single-destination add-route case.
- There was no successful end-to-end scripted mouse/keyboard GUI reproduction in this session.
