#pragma once

#include "config/AppConfig.hpp"
#include "streaming/INetworkStreamer.hpp"
#include "streaming/Serializer.hpp"
#include <atomic>
#include <cstdint>
#include <opencv2/core.hpp>

namespace streaming {

class RtpStreamer : public INetworkStreamer {
public:
  explicit RtpStreamer(const config::StreamingConfig &stream_cfg);
  ~RtpStreamer() override;

  bool Start() override;

  void SendFrame(const cv::Mat &frame) override;

  void NotifyEmergencyFall(uint64_t timestamp_ms,
                           const std::vector<Detection> &detections) override;

  void Stop() override;

  bool IsRunning() const override;

  bool CheckRemoteResolve() override;

private:
  config::StreamingConfig config_;
  std::atomic<bool> is_running_{false};
  uint32_t frame_counter_{0};
  cv::Mat active_frame_;
};

} // namespace streaming
