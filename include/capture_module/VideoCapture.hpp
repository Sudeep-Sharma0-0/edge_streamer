#pragma once

#include "config/AppConfig.hpp"
#include <gst/gst.h>
#include <memory>
#include <string>

namespace capture {

class PipelineEngine; // Forward declaration

class VideoCapture {
public:
  VideoCapture();
  explicit VideoCapture(const config::CaptureConfig &config);
  ~VideoCapture();

  // Non-copyable, movable
  VideoCapture(const VideoCapture &) = delete;
  VideoCapture &operator=(const VideoCapture &) = delete;
  VideoCapture(VideoCapture &&) noexcept;
  VideoCapture &operator=(VideoCapture &&) noexcept;

  // Lifecycle methods
  bool Initialize(const config::CaptureConfig &config);
  bool Start();
  void Stop();
  bool IsRunning() const;

  // Access the underlying GstElement (pipeline) to attach FrameSink
  GstElement *GetPipelineElement() const;
  GstElement *GetAppSinkElement() const;

  const config::CaptureConfig &GetConfig() const { return config_; }

private:
  config::CaptureConfig config_;
  std::unique_ptr<PipelineEngine> engine_;
  bool is_running_{false};
};

} // namespace capture
