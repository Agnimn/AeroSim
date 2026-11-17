# Mapbox map configuration

The 2D map uses Qt Location's OSM plugin with Mapbox's 256-pixel Styles raster
tile endpoint. The default style is `mapbox/streets-v12`.

Configure a Mapbox public access token locally using either option:

1. Add this to the per-user `secrets.ini` file shown by the application error
   message:

   ```ini
   [Mapbox]
   AccessToken=pk.your_public_token_here
   StyleId=mapbox/streets-v12
   ```

2. Set the environment variables before starting the application:

   ```text
   MAPBOX_ACCESS_TOKEN=pk.your_public_token_here
   MAPBOX_STYLE_ID=mapbox/streets-v12
   ```

`MAPBOX_STYLE_ID` is optional and defaults to `mapbox/streets-v12`. The token is
URL-encoded in memory and is not written to logs. A missing token keeps the
OpenStreetMap tile fallback available.

Use a Mapbox public access token with URL/application restrictions and suitable
scopes. Any access token shipped in a desktop application can be extracted by
an end user; it must not be treated as a server-side secret.

Mapbox attribution is supplied to Qt Location through
`osm.mapping.custom.datacopyright` and remains visible through the map's
copyright display.
