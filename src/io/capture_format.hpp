#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <functional>
#include <optional>

namespace glint::io {

// Reads exactly `size` bytes into `dst`. Returns true if it delivered all of them, false if
// the byte source ran out first — for a file this is EOF, for a live socket it's the
// connection closing. Implementations must retry internally on partial reads (a socket
// recv() can legitimately return fewer bytes than asked for without the connection being
// over); this contract lets the parsing code below stay identical for both transports, per
// docs/CAPTURE_FORMAT.md's "one format, one parser" design.
using ByteReader = std::function<bool(void* dst, size_t size)>;

// Writes `size` bytes from `src` to wherever bytes go (a file, a socket) — the encode-side
// mirror of ByteReader, used by tools/replica_to_glcb.cpp today and by the iOS app's own
// Swift encoder conceptually (CaptureBundleFormat.swift), though the two encoders don't share
// code across the language boundary.
using ByteWriter = std::function<void(const void* src, size_t size)>;

// Parses the 28-byte session header (magic, version, camera intrinsics). Throws
// std::runtime_error if the reader fails or the magic/version don't match — unlike a frame
// record, a session is never expected to be zero bytes, so any failure here is a real error,
// not a normal end.
core::Camera read_session_header(const ByteReader& read);

// Parses one frame record (pose + JPEG RGB + raw depth) for a `width`x`height` session.
// Returns std::nullopt once the byte source is exhausted *before* a new record starts — the
// normal, expected way a session ends. A failure partway through an already-started record
// (the source running out mid-frame) is treated the same way: per
// docs/CAPTURE_FORMAT.md, a dropped connection mid-frame just means the session ended
// abnormally, not that the caller needs a separate error path for it.
std::optional<core::Frame> read_frame_record(const ByteReader& read, int width, int height);

// Writes the session header — the encode-side mirror of read_session_header.
void write_session_header(const ByteWriter& write, const core::Camera& camera);

// Writes one frame record — the encode-side mirror of read_frame_record. `frame.rgb` is
// encoded to JPEG at a fixed quality; `frame.depth` is written raw.
void write_frame_record(const ByteWriter& write, const core::Frame& frame, int width, int height);

}  // namespace glint::io
