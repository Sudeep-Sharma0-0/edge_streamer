#include "streaming/TCPServer.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace streaming {

TcpProtoServer::TcpProtoServer() : server_fd_(-1), running_(false) {}

TcpProtoServer::~TcpProtoServer() { Stop(); }

bool TcpProtoServer::Start(int port) {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    return false;
  }

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&address),
           sizeof(address)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  if (listen(server_fd_, 5) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  running_ = true;
  accept_thread_ = std::thread(&TcpProtoServer::AcceptLoop, this);
  return true;
}

void TcpProtoServer::Stop() {
  running_ = false;
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  std::lock_guard<std::mutex> lock(clients_mutex_);
  for (int client_fd : clients_) {
    close(client_fd);
  }
  clients_.clear();
}

void TcpProtoServer::Broadcast(const std::string &payload) {
  if (payload.empty()) {
    return;
  }

  uint32_t len = htonl(static_cast<uint32_t>(payload.size()));

  std::lock_guard<std::mutex> lock(clients_mutex_);
  clients_.erase(
      std::remove_if(clients_.begin(), clients_.end(),
                     [&](int fd) {
                       if (send(fd, &len, sizeof(len), MSG_NOSIGNAL) <= 0) {
                         return true;
                       }
                       if (send(fd, payload.data(), payload.size(),
                                MSG_NOSIGNAL) <= 0) {
                         return true;
                       }
                       return false;
                     }),
      clients_.end());
}

void TcpProtoServer::AcceptLoop() {
  while (running_) {
    sockaddr_in client_addr{};
    socklen_t addrlen = sizeof(client_addr);
    int client_fd =
        accept(server_fd_, reinterpret_cast<struct sockaddr *>(&client_addr),
               &addrlen);
    if (client_fd >= 0) {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      clients_.push_back(client_fd);
    }
  }
}

} // namespace streaming
