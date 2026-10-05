# Local aircraft model

Place the project's authorized, locally licensed aircraft model here as:

`aircraft.glb`

The viewer uses Qt Quick 3D AssetUtils `RuntimeLoader` with the embedded resource URL `qrc:/AeroSim/assets/aircraft/aircraft.glb`. When this file exists at configure time, the root CMake target embeds it for offline loading. Re-run CMake configuration in Qt Creator after adding or replacing the GLB.

No third-party aircraft model is bundled. Until one is supplied, the viewer displays an original low-poly development aircraft assembled from Qt Quick 3D 3D meshes. This is a visual placeholder, not a production aircraft asset.
