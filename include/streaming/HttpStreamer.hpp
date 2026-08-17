#pragma once

#include "config/AppConfig.hpp"
#include "streaming/INetworkStreamer.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace streaming {

class HttpStreamer : public INetworkStreamer {
public:
  explicit HttpStreamer(const config::StreamingConfig &stream_cfg);
  ~HttpStreamer() override;

  bool Start() override;
  void SendFrame(const cv::Mat &frame) override;
  void Stop() override;
  bool IsRunning() const override { return is_running_; }

private:
  void ServerLoop();
  void ClientHandler(int client_fd);

  config::StreamingConfig config_;
  int server_fd_{-1};
  std::atomic<bool> is_running_{false};

  std::thread server_thread_;
  std::vector<std::thread> client_threads_;

  // Frame buffer synchronization
  std::mutex frame_mutex_;
  std::condition_variable frame_cv_;
  std::vector<uint8_t> latest_jpeg_buffer_;
  uint64_t frame_index_{0};

  // Connected client management
  std::mutex clients_mutex_;
  std::unordered_set<int> active_clients_;
};

} // namespace streaming
