#include "streaming/HttpStreamer.hpp"
#include "config/AppConfig.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <opencv2/imgcodecs.hpp>
#include <sys/socket.h>
#include <unistd.h>

namespace streaming {

HttpStreamer::HttpStreamer(const config::StreamingConfig &stream_cfg)
    : config_(stream_cfg) {}

HttpStreamer::~HttpStreamer() { Stop(); }

bool HttpStreamer::Start() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::cerr << "[ERROR] HttpStreamer: Failed to create socket.\n";
    return false;
  }

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(config_.port);

  if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&server_addr),
           sizeof(server_addr)) < 0) {
    std::cerr << "[ERROR] HttpStreamer: Failed to bind to port " << config_.port
              << ".\n";
    close(server_fd_);
    return false;
  }

  if (listen(server_fd_, 10) < 0) {
    std::cerr << "[ERROR] HttpStreamer: Listen failed.\n";
    close(server_fd_);
    return false;
  }

  is_running_ = true;
  server_thread_ = std::thread(&HttpStreamer::ServerLoop, this);

  std::cout << "[INFO] HTTP MJPEG Server running at http://0.0.0.0:"
            << config_.port << " (Quality: " << config_.jpeg_quality << "%)\n";
  return true;
}

void HttpStreamer::ServerLoop() {
  while (is_running_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd_, reinterpret_cast<struct sockaddr *>(&client_addr),
               &client_len);
    if (client_fd < 0) {
      if (!is_running_)
        break;
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      active_clients_.insert(client_fd);
    }

    client_threads_.emplace_back(&HttpStreamer::ClientHandler, this, client_fd);
  }
}

void HttpStreamer::ClientHandler(int client_fd) {
  char req_buffer[1024] = {0};
  read(client_fd, req_buffer, sizeof(req_buffer) - 1);

  std::string http_response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Server: EdgeStreamer/1.0\r\n"
      "Cache-Control: no-cache, no-store, must-revalidate, pre-check=0, "
      "post-check=0, max-age=0\r\n"
      "Pragma: no-cache\r\n"
      "Content-Type: multipart/x-mixed-replace; "
      "boundary=--frameboundary\r\n\r\n";

  if (send(client_fd, http_response.c_str(), http_response.size(),
           MSG_NOSIGNAL) < 0) {
    close(client_fd);
    return;
  }

  uint64_t last_sent_index = 0;

  while (is_running_) {
    std::vector<uint8_t> jpeg_data;

    {
      std::unique_lock<std::mutex> lock(frame_mutex_);
      frame_cv_.wait(lock, [this, &last_sent_index]() {
        return !is_running_ || frame_index_ > last_sent_index;
      });

      if (!is_running_)
        break;

      jpeg_data = latest_jpeg_buffer_;
      last_sent_index = frame_index_;
    }

    if (jpeg_data.empty())
      continue;

    std::string boundary_header = "----frameboundary\r\n"
                                  "Content-Type: image/jpeg\r\n"
                                  "Content-Length: " +
                                  std::to_string(jpeg_data.size()) + "\r\n\r\n";

    if (send(client_fd, boundary_header.c_str(), boundary_header.size(),
             MSG_NOSIGNAL) < 0)
      break;
    if (send(client_fd, reinterpret_cast<const char *>(jpeg_data.data()),
             jpeg_data.size(), MSG_NOSIGNAL) < 0)
      break;
    if (send(client_fd, "\r\n", 2, MSG_NOSIGNAL) < 0)
      break;
  }

  close(client_fd);
  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_clients_.erase(client_fd);
  }
}

void HttpStreamer::SendFrame(const cv::Mat &frame) {
  if (!is_running_ || frame.empty())
    return;

  std::vector<uint8_t> encoded_buffer;
  std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, config_.jpeg_quality};

  if (!cv::imencode(".jpg", frame, encoded_buffer, params)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    latest_jpeg_buffer_ = std::move(encoded_buffer);
    ++frame_index_;
  }
  frame_cv_.notify_all();
}

void HttpStreamer::Stop() {
  if (!is_running_.exchange(false))
    return;

  frame_cv_.notify_all();

  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }

  {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int fd : active_clients_) {
      shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    active_clients_.clear();
  }

  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  for (auto &t : client_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  client_threads_.clear();
}

} // namespace streaming
