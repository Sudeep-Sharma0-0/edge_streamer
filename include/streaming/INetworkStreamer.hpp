#pragma once

#include "streaming/Serializer.hpp"
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>

namespace streaming {

class INetworkStreamer {
public:
  virtual ~INetworkStreamer() = default;

  virtual bool Start() = 0;
  virtual void SendFrame(const cv::Mat &frame) = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() const = 0;
  virtual void
  NotifyEmergencyFall(uint64_t timestamp_ms,
                      const std::vector<Detection> &detections) = 0;
  virtual bool CheckRemoteResolve() = 0;
};

} // namespace streaming
