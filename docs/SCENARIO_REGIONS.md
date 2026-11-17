# Scenario region placement

`North India` uses a conservative polygon approximation in
`core/ScenarioEngine.cpp`. It is intended for scenario placement rather than
legal or cartographic boundary display. The approximation covers the northern
Indian states and union territories represented by this application, including
Jammu and Kashmir and Ladakh, while keeping candidate placement away from
Pakistan, Afghanistan, Nepal, Bhutan, and China.

The polygon is deliberately conservative because this repository does not yet
contain an authoritative India state/union-territory boundary dataset. It must
be replaced with licensed, authoritative geometry before the region is used for
operational or geographic claims.

The current approximate vertex extent is 24.8-35.0 degrees north and
75.0-82.0 degrees east. The interior is intentionally narrower than the
international boundary in places so candidate points do not spill into
neighboring countries.

Every generated coordinate is checked for valid latitude/longitude ranges,
membership in the requested region polygon, and any requested radius. Placement
uses bounded deterministic candidates and fails if the requested entity count
cannot be placed inside those constraints.
