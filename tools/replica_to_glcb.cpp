// Dev tool only — re-encodes an already-loaded Replica scene into a real .glcb file (see
// docs/CAPTURE_FORMAT.md) using the exact same encode logic src/io/capture_bundle.cpp's
// reader is tested against (tests/test_capture_bundle.cpp), giving this tool's output a
// stronger claim to being "real" than a one-off hand-rolled encoder would.
#include "io/capture_format.hpp"
#include "io/dataset.hpp"

#include <cstdio>
#include <fstream>
#include <limits>
#include <string>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <replica_scene_dir> <output.glcb> [max_frames]\n", argv[0]);
    return 1;
  }

  const std::string scene_dir = argv[1];
  const std::string output_path = argv[2];
  const size_t max_frames = argc >= 4 ? static_cast<size_t>(std::stoul(argv[3]))
                                       : std::numeric_limits<size_t>::max();

  try {
    const glint::io::ReplicaScene scene = glint::io::load_replica_scene(scene_dir, max_frames);

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
      std::fprintf(stderr, "failed to open %s for writing\n", output_path.c_str());
      return 1;
    }

    const glint::io::ByteWriter write_exact = [&out](const void* src, size_t size) {
      out.write(static_cast<const char*>(src), static_cast<std::streamsize>(size));
    };

    glint::io::write_session_header(write_exact, scene.camera);
    for (const glint::core::Frame& frame : scene.frames) {
      glint::io::write_frame_record(write_exact, frame, scene.camera.width, scene.camera.height);
    }

    std::fprintf(stderr, "wrote %zu frames to %s\n", scene.frames.size(), output_path.c_str());
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }

  return 0;
}
