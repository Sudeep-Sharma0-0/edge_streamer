#pragma once
#include "config/AppConfig.hpp"
#include "streaming/INetworkStreamer.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>

namespace streaming {

class RtpStreamer : public INetworkStreamer {
public:
  explicit RtpStreamer(const config::StreamingConfig &stream_cfg);
  ~RtpStreamer() override;

  bool Start() override;
  void SendFrame(const cv::Mat &frame) override;
  void Stop() override;

  bool IsRunning() const override { return is_running_; }

private:
  config::StreamingConfig config_;
  bool is_running_ = false;

  GstElement *pipeline_ = nullptr;
  GstElement *appsrc_ = nullptr;
  GstClockTime timestamp_ = 0;
};

} // namespace streaming
