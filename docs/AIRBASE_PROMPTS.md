# Airbase prompt validation

The registry currently contains 34 public reference records from the supplied initial list. Two entries remain unverified and have no coordinates, so they are retained for auditability but cannot be selected or used for generation. Coordinates are reference points for simulation initialization; generated aircraft use a deterministic 0.5 nautical mile simulation-only spawn radius around the registered point and are not placed on a claimed runway.

Manual checks using the prompt field:

1. `Generate 3 aircraft at Hindon Air Force Station` creates 3 entities, selects Hindon, and shows the airbase in the status panel.
2. `Generate 3 aircraft at hindon` resolves to the same `in-hindon-afs` ID.
3. Select a marker, then run `Generate 10 aircraft at the selected airbase`.
4. `Generate 2 aircraft at each configured airbase in North India` creates two entities around each verified configured record in that region.
5. `Generate 3 aircraft at an unconfigured station` reports that the airbase is not configured.
6. `Generate 3 aircraft at AFS` reports that the airbase name is ambiguous.
7. Inspect generated markers after steps 1, 3, and 4: every initial position is valid, distinct, and within 0.5 nautical miles of its registry reference point.
8. `Create 3 aircraft in North India` continues to use the existing regional placement path.
9. Toggle `Airbases`, pan/zoom, select an aircraft, and verify play, pause, reset, speed, trails, routes, conflict overlays, and 3D selection remain available. The QtTest target also covers registry loading, India-wide display, aliases, unknown/ambiguous errors, count limits, placement constraints, regional generation, and simulation controls.

`Show airbases in North India` filters the registry marker layer without creating a simulation scenario.
