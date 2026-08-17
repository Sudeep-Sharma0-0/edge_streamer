#pragma once

#include "config/AppConfig.hpp"
#include "streaming/INetworkStreamer.hpp"
#include <memory>

namespace streaming {

class StreamerFactory {
public:
  static std::unique_ptr<INetworkStreamer>
  Create(const config::AppConfig &app_config);
};

} // namespace streaming
