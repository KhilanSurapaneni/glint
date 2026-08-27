#pragma once

#include "core/types.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace glint::io {

// Accepts one live capture connection from the iOS app (ARSessionManager.swift's
// `startCaptureLive`) and buffers incoming frames for a consumer — the render loop in
// main.cpp — to drain at its own pace. The actual accept()/recv() work runs on a background
// thread so construction and drain_frames() never block the caller; only wait_for_camera()
// blocks, since nothing downstream can size its GPU buffers before the session header
// arrives. Reuses capture_format.cpp's parsing exactly as capture_bundle.cpp does — this
// class only supplies the byte source (a TCP socket instead of a file).
class IosStreamServer {
 public:
  // Binds and starts listening on `port` immediately (throws if the socket can't be
  // created/bound/listened on), then spawns the background thread that waits for one
  // connection and reads its frames.
  explicit IosStreamServer(uint16_t port);

  // Unblocks and joins the background thread, whatever state it's in — a pending accept() or
  // a pending recv() are both interrupted so shutdown never hangs waiting on the phone.
  ~IosStreamServer();

  IosStreamServer(const IosStreamServer&) = delete;
  IosStreamServer& operator=(const IosStreamServer&) = delete;

  // Blocks until the phone connects and its session header arrives. Throws if the connection
  // closes before that happens (e.g. the phone never showed up, or sent something invalid).
  core::Camera wait_for_camera();

  // Non-blocking: returns every frame received since the last call, oldest first. Empty if
  // none have arrived since — including after the session has ended, at which point it'll
  // always be empty.
  std::vector<core::Frame> drain_frames();

  // True once the connection has closed and no more frames will ever arrive.
  bool session_ended() const;

 private:
  void run();  // background-thread entry point: accept, then read frames until the source ends

  int listen_fd_ = -1;
  std::atomic<int> accept_fd_{-1};
  std::thread worker_;

  mutable std::mutex mutex_;
  std::condition_variable camera_ready_;
  std::optional<core::Camera> camera_;
  std::deque<core::Frame> pending_frames_;
  bool session_ended_ = false;
};

}  // namespace glint::io
