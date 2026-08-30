#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace streaming {

class TcpProtoServer {
public:
  TcpProtoServer();
  ~TcpProtoServer();

  bool Start(int port);
  void Stop();
  void Broadcast(const std::string &payload);

private:
  void AcceptLoop();

  int server_fd_{-1};
  std::atomic<bool> running_{false};
  std::thread accept_thread_;
  std::vector<int> clients_;
  std::mutex clients_mutex_;
};

} // namespace streaming
