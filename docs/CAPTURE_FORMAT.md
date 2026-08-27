# Capture bundle format

The wire format `capture/ios/GlintCapture` writes and the C++ side reads — designed to work
identically over **two transports**: a complete file (for offline/remote capture, read by
`src/io/capture_bundle.cpp`) and a live TCP stream (for when the phone and Mac are on the same
network, read by `src/io/ios_stream.cpp`). One session = one header, followed by a sequence of
frame records, read until the byte source runs out — end-of-file for a file, connection-closed
(or an explicit stop signal) for a live socket. Same records either way; only how you know
"there are no more" differs.

Designed so its contents map directly onto `core::Camera`/`core::Frame` with no dataset-
specific conversion needed on the C++ side — unlike Replica, where we don't control the format,
here we do, so it's built to match our internal types exactly.

All multi-byte integers and floats are little-endian (matching both ARM64 and the Metal/x86
tooling this project already assumes).

## Session header (28 bytes, once, at the start)

| Field | Type | Bytes | Notes |
|---|---|---|---|
| `magic` | `char[4]` | 4 | Literal ASCII `"GLCB"` — lets a reader sanity-check it's actually looking at one of these before parsing further. |
| `version` | `uint32` | 4 | `1` for this revision. Bump on any future format change so old captures don't silently misparse under a newer reader. |
| `width` | `uint32` | 4 | Image width, pixels. |
| `height` | `uint32` | 4 | Image height, pixels. |
| `fx` | `float32` | 4 | Camera intrinsics — same shape as `core::Camera`. Taken from the first captured frame; treated as constant for the whole session. |
| `fy` | `float32` | 4 | |
| `cx` | `float32` | 4 | |
| `cy` | `float32` | 4 | |

No frame count here on purpose — see above. This is what changed from the first draft of this
spec, once live streaming came back into scope: a live session genuinely doesn't know its own
frame count in advance, and rather than support two different header shapes for two
transports, both just read frame records until there aren't any more.

## Frame record (repeated until the byte source ends)

| Field | Type | Bytes | Notes |
|---|---|---|---|
| `camera_to_world` | `float32[16]` | 64 | Row-major 4x4 pose matrix — same convention `io/dataset.cpp` already parses Replica's `traj.txt` with, so the reading code can look nearly identical. |
| `rgb_byte_length` | `uint32` | 4 | Size of the JPEG data that follows. |
| `rgb_data` | `uint8[rgb_byte_length]` | variable | One JPEG-encoded frame — decoded with `stb_image`, same as Replica's `frame*.jpg`. |
| `depth_byte_length` | `uint32` | 4 | Size of the depth data that follows — always `width * height * 4` for this version, but stored explicitly so the parser reads both variable-length sections the same uniform way. |
| `depth_data` | `float32[width * height]` | variable | Raw, uncompressed, row-major depth in **meters** — ARKit's native format already matches `core::Frame.depth`'s representation exactly; no scale factor, no conversion. |

## Shared code, not two implementations

Since one format now serves both transports, the actual "turn one `CapturedFrame` into these
bytes" logic (Swift side) and "parse these bytes into one `core::Frame`" logic (C++ side)
should each be written **once** and reused — encoding doesn't know or care whether its output
goes to a local file or a socket; parsing doesn't know or care whether its input came from an
`ifstream` or a `recv()` loop. Only the byte source/sink differs between `capture_bundle.cpp`
and `ios_stream.cpp`.

## Known limitation, accepted deliberately for now

Uncompressed depth means larger files/more bandwidth than a compressed format would give — not
solved here, since there's no real captured data yet to meaningfully test compression
trade-offs against. Worth revisiting once real capture sessions exist to size against.
