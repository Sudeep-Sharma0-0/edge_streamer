#pragma once

#include "config/AppConfig.hpp"
#include <cstdint>
#include <functional>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <vector>

namespace processing {

struct ImageFrame {
  std::vector<uint8_t> pixel_data;
  int width = 0;
  int height = 0;
  int channels = 3;
  uint64_t timestamp_ms = 0;
  std::string format = "RGB";
};

class FrameSink {
public:
  using FrameCallback = std::function<void(const ImageFrame &)>;

  FrameSink();
  explicit FrameSink(const config::CaptureConfig &config);
  ~FrameSink();

  FrameSink(const FrameSink &) = delete;
  FrameSink &operator=(const FrameSink &) = delete;
  FrameSink(FrameSink &&) noexcept;
  FrameSink &operator=(FrameSink &&) noexcept;

  bool AttachToAppSink(GstElement *appsink);
  void Detach();
  void SetFrameCallback(FrameCallback callback);

private:
  static GstFlowReturn OnNewSampleThunk(GstAppSink *sink, gpointer user_data);
  static GstFlowReturn OnNewPrerollThunk(GstAppSink *sink, gpointer user_data);
  static void OnEosThunk(GstAppSink *sink, gpointer user_data);

  GstFlowReturn HandleNewSample(GstAppSink *sink);

  config::CaptureConfig config_;
  GstElement *appsink_{nullptr};
  FrameCallback frame_callback_;
};

} // namespace processing
