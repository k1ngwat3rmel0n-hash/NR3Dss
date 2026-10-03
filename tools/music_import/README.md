# v0.015 music import

The supplied UnityFS music bundle contains 30 streamed Unity AudioClip objects.
`source_music_manifest.json` records the recovered names, duration, source sample
rates, and exact FSB5 resource offsets/sizes.

v0.015 enables the 3DS NDSP playback path with a tiny original synthesized test
loop. The source songs remain in FSB5 and are not redistributed in this package;
they need offline FSB5 -> 3DS-friendly ADPCM/PCM conversion first.
