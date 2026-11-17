# Airbase registry

`airbases.json` contains 34 initial public reference records from the supplied list. It contains stable identifiers, aliases, public reference coordinates where available, state/UT, source-provided command metadata, broad region/category labels, source attribution, and verification status. It contains no restricted operational information. Two records currently have no coordinate and remain unverified: Thoise and Nyoma.

Coordinate references are intended to be checked against public [OpenStreetMap](https://www.openstreetmap.org/) data, available under the Open Database License (ODbL) 1.0, and [OurAirports](https://ourairports.com/data/) data, released under a CC0/Public Domain dedication. Each record names its coordinate source. The registry keeps military air stations, shared-use airfields, training facilities, maintenance facilities, and naval air stations distinct; a civil airport is not treated as a military airbase merely because it is nearby.

Last registry review: 2026-10-14. `verified_public_reference` means a public map reference suitable for simulation initialization, not official confirmation of a current command assignment or an exact installation/runway point. Command values preserve the supplied list as source-provided metadata and are explicitly marked for current-assignment verification.

Coordinates are reference points for simulation initialization, not runway locations or claims about a real aircraft's position. New records must include a stable ID, source attribution, and a verification status. Unverifiable locations must use null coordinates and remain unavailable for prompt targeting until checked. Do not add operational, security, runway-level tactical, or force-deployment information.
