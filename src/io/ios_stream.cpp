#include "io/ios_stream.hpp"
#include "io/capture_format.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <stdexcept>

namespace glint::io {

namespace {

// Keeps calling recv() until `size` bytes have actually arrived, retrying on both a
// partial read (the OS can deliver less than asked for even mid-stream) and EINTR. Returns
// false on an orderly close (recv returning 0) or a real socket error — both mean "no more
// data is coming," the same "source exhausted" signal capture_bundle.cpp's ifstream-backed
// reader gives for EOF.
bool recv_exact(int fd, void* dst, size_t size) {
  auto* bytes = static_cast<uint8_t*>(dst);
  size_t total = 0;
  while (total < size) {
    const ssize_t n = ::recv(fd, bytes + total, size - total, 0);
    if (n > 0) {
      total += static_cast<size_t>(n);
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace

IosStreamServer::IosStreamServer(uint16_t port) {
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error("failed to create listening socket");
  }

  // Without this, quickly restarting the app after a previous run can fail to bind with
  // "address already in use" while the OS still has the old socket in TIME_WAIT — an easy
  // thing to hit repeatedly during dev iteration, since restarting the app is exactly what
  // dev iteration is.
  const int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    ::close(listen_fd_);
    throw std::runtime_error("failed to bind to port " + std::to_string(port));
  }
  if (::listen(listen_fd_, /*backlog=*/1) < 0) {
    ::close(listen_fd_);
    throw std::runtime_error("failed to listen on port " + std::to_string(port));
  }

  worker_ = std::thread(&IosStreamServer::run, this);
}

IosStreamServer::~IosStreamServer() {
  // Closing the listen socket unblocks a still-pending accept() if no phone ever connected.
  if (listen_fd_ >= 0) {
    ::close(listen_fd_);
  }
  // Shutting down (not closing) the accepted socket unblocks a pending recv() from here,
  // without racing run()'s own close() of that same fd once its loop notices the shutdown.
  const int accepted = accept_fd_.load();
  if (accepted >= 0) {
    ::shutdown(accepted, SHUT_RDWR);
  }
  if (worker_.joinable()) {
    worker_.join();
  }
}

void IosStreamServer::run() {
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int fd =
      ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (fd < 0) {
    // Either a real error, or the destructor closed listen_fd_ to interrupt us during
    // shutdown with no phone ever connecting — either way, there's no session to run.
    std::lock_guard<std::mutex> lock(mutex_);
    session_ended_ = true;
    camera_ready_.notify_all();
    return;
  }
  accept_fd_.store(fd);

  const ByteReader read_exact = [fd](void* dst, size_t size) { return recv_exact(fd, dst, size); };

  try {
    const core::Camera camera = read_session_header(read_exact);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      camera_ = camera;
    }
    camera_ready_.notify_all();

    while (std::optional<core::Frame> frame =
               read_frame_record(read_exact, camera.width, camera.height)) {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_frames_.push_back(std::move(*frame));
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ios_stream: session ended with an error: %s\n", e.what());
  }

  ::close(fd);
  accept_fd_.store(-1);

  std::lock_guard<std::mutex> lock(mutex_);
  session_ended_ = true;
  camera_ready_.notify_all();
}

core::Camera IosStreamServer::wait_for_camera() {
  std::unique_lock<std::mutex> lock(mutex_);
  camera_ready_.wait(lock, [this] { return camera_.has_value() || session_ended_; });
  if (!camera_) {
    throw std::runtime_error("connection closed before a session header arrived");
  }
  return *camera_;
}

std::vector<core::Frame> IosStreamServer::drain_frames() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<core::Frame> drained(std::make_move_iterator(pending_frames_.begin()),
                                    std::make_move_iterator(pending_frames_.end()));
  pending_frames_.clear();
  return drained;
}

bool IosStreamServer::session_ended() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_ended_;
}

}  // namespace glint::io
