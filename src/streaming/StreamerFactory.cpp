#include "streaming/StreamerFactory.hpp"
#include "streaming/HttpStreamer.hpp"
#include "streaming/RtpStreamer.hpp"
#include <iostream>

namespace streaming {

std::unique_ptr<INetworkStreamer>
StreamerFactory::Create(const config::AppConfig &app_config) {
  if (!app_config.streaming.enabled ||
      app_config.streaming.protocol == config::StreamProtocol::NONE) {
    std::cout << "[INFO] Streaming output is disabled.\n";
    return nullptr;
  }

  switch (app_config.streaming.protocol) {
  case config::StreamProtocol::HTTP_MJPEG:
    return std::make_unique<HttpStreamer>(app_config.streaming);
  case config::StreamProtocol::UDP_RTP:
    return std::make_unique<RtpStreamer>(app_config.streaming);

  default:
    std::cerr << "[WARN] Unknown streaming protocol requested.\n";
    return nullptr;
  }
}

} // namespace streaming
