#pragma once

// Structs shared between C++ and Metal Shading Language go here. MSL alignment rules differ
// from C++ (e.g. float3 is 16-byte aligned/sized, not 12) — see CLAUDE.md §8. Every struct
// added to this file needs a static_assert(sizeof/alignof) on the C++ side and a round-trip
// check in tests/test_layout.cpp before it's trusted. Nothing crosses the boundary yet.
