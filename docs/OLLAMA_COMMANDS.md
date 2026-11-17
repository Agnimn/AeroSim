# Local Ollama commands

AeroSim can interpret the prompt field with a locally running Ollama server. The
default endpoint is `http://127.0.0.1:11434` and the default model is
`qwen2.5-coder:7b`. Override them with `AEROSIM_OLLAMA_ENDPOINT` and
`AEROSIM_OLLAMA_MODEL`. The generation context defaults to 2048 tokens and can
be overridden with the positive-integer environment variable
`AEROSIM_OLLAMA_NUM_CTX`; no cloud service is used. The endpoint setting is the
service root; AeroSim appends `/api/generate`, so do not include that path in
the configured URL.

The application sends `/api/generate` with `stream: false` and requires the
model response to be one JSON object. A complete JSON code fence is accepted
as a transport wrapper, but prose or partial JSON is rejected. The supported
schema includes:

```json
{
  "operation": "CREATE_FORMATION",
  "count": 7,
  "entity_type": "aircraft",
  "formation_type": "V"
}
```

Supported operations include `ADD_ENTITIES`, `CREATE_FORMATION`,
`MOVE_ENTITY`, `MOVE_FORMATION`, `SET_ROUTE`, `SCHEDULE_TAKEOFF`,
`CANCEL_EVENT`, `SHOW_AIRBASES`, `QUERY_SCENARIO`, `STOP_AIRCRAFT`,
`STOP_ALL_AIRCRAFT`, `PAUSE_AIRCRAFT`, `RESUME_AIRCRAFT`,
`START_AIRCRAFT`, `REMOVE_AIRCRAFT`, `REMOVE_LAST_AIRCRAFT`,
`REMOVE_ALL_AIRCRAFT`, `STOP_FORMATION`, `RESUME_FORMATION`, and
`DISBAND_FORMATION`. `MOVE_ENTITY`,
`SET_ROUTE`, `CANCEL_EVENT`, `STOP_AIRCRAFT`, `PAUSE_AIRCRAFT`,
`RESUME_AIRCRAFT`, and `START_AIRCRAFT` require `entity_id`;
`REMOVE_AIRCRAFT` also requires `entity_id`; `REMOVE_LAST_AIRCRAFT` requires
an integer `count`. `REMOVE_ALL_AIRCRAFT` and `STOP_ALL_AIRCRAFT` do not
require an entity ID. `MOVE_FORMATION` requires `destination` and may include
`formation_type` or `"all_formations": true`. The formation stop, resume, and
disband operations can use the same optional selectors. Singular formation
commands are rejected when more than one formation matches.
`SCHEDULE_TAKEOFF` additionally requires `time` in `HH:MM` format and may
include `origin` and `destination`. `SHOW_AIRBASES` requires `region`.

Examples:

```json
{"operation":"REMOVE_ALL_AIRCRAFT"}
{"operation":"REMOVE_AIRCRAFT","entity_id":"AERO001"}
{"operation":"REMOVE_LAST_AIRCRAFT","count":3}
{"operation":"STOP_ALL_AIRCRAFT"}
{"operation":"MOVE_FORMATION","formation_type":"V","destination":"Chennai"}
{"operation":"RESUME_FORMATION"}
{"operation":"DISBAND_FORMATION"}
```

Aircraft removal only removes scenario aircraft. It preserves airbase markers
and other scenario entities, filters scheduled events for removed aircraft,
and clears formation membership when a formation member is removed.

The returned object is validated in C++ and converted to an existing
scenario-engine command. The model never mutates entities or simulation state
directly. Successful model commands are reported as `OLLAMA GENERATED JSON`;
an outage fallback is reported as `DETERMINISTIC PARSER EXECUTED`. HTTP errors
are distinguished from connection/timeout failures. Request URL, method,
content type, request body, HTTP status, and response body are written to the
application log.

The request timeout is 180 seconds to allow model initialization. To exercise
the model without allowing outage fallback, set
`AEROSIM_DISABLE_DETERMINISTIC_FALLBACK=1`. When Ollama is unavailable, the
command is rejected instead of being passed to the deterministic parser.
