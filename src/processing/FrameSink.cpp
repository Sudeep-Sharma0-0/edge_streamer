#include "processing/FrameSink.hpp"
#include <chrono>
#include <iostream>

namespace processing {

FrameSink::FrameSink() : config_(config::CaptureConfig{}) {}

FrameSink::FrameSink(const config::CaptureConfig &config) : config_(config) {}

FrameSink::~FrameSink() { Detach(); }

FrameSink::FrameSink(FrameSink &&other) noexcept
    : config_(other.config_), appsink_(other.appsink_),
      frame_callback_(std::move(other.frame_callback_)) {
  other.appsink_ = nullptr;
}

FrameSink &FrameSink::operator=(FrameSink &&other) noexcept {
  if (this != &other) {
    Detach();
    config_ = other.config_;
    appsink_ = other.appsink_;
    frame_callback_ = std::move(other.frame_callback_);
    other.appsink_ = nullptr;
  }
  return *this;
}

bool FrameSink::AttachToAppSink(GstElement *appsink) {
  if (!appsink) {
    std::cerr << "[ERROR] FrameSink: Target appsink element is null.\n";
    return false;
  }

  Detach();
  appsink_ = GST_ELEMENT(gst_object_ref(appsink));

  gst_app_sink_set_max_buffers(GST_APP_SINK(appsink_), 1);
  gst_app_sink_set_drop(GST_APP_SINK(appsink_), TRUE);
  gst_app_sink_set_emit_signals(GST_APP_SINK(appsink_), FALSE);

  GstAppSinkCallbacks callbacks{};
  callbacks.eos = OnEosThunk;
  callbacks.new_preroll = OnNewPrerollThunk;
  callbacks.new_sample = OnNewSampleThunk;

  gst_app_sink_set_callbacks(GST_APP_SINK(appsink_), &callbacks, this, nullptr);

  std::cout << "[INFO] FrameSink attached to GStreamer appsink pipeline.\n";
  return true;
}

void FrameSink::Detach() {
  if (appsink_) {
    GstAppSinkCallbacks callbacks{};
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink_), &callbacks, nullptr,
                               nullptr);

    gst_object_unref(appsink_);
    appsink_ = nullptr;
  }
}

void FrameSink::SetFrameCallback(FrameCallback callback) {
  frame_callback_ = std::move(callback);
}

GstFlowReturn FrameSink::OnNewSampleThunk(GstAppSink *sink,
                                          gpointer user_data) {
  auto *self = static_cast<FrameSink *>(user_data);
  return self ? self->HandleNewSample(sink) : GST_FLOW_ERROR;
}

GstFlowReturn FrameSink::OnNewPrerollThunk(GstAppSink * /*sink*/,
                                           gpointer /*user_data*/) {
  return GST_FLOW_OK;
}

void FrameSink::OnEosThunk(GstAppSink * /*sink*/, gpointer /*user_data*/) {
  std::cout << "[INFO] FrameSink: End of Stream (EOS) received.\n";
}

GstFlowReturn FrameSink::HandleNewSample(GstAppSink *sink) {
  GstSample *sample = gst_app_sink_pull_sample(sink);
  if (!sample) {
    return GST_FLOW_ERROR;
  }

  GstBuffer *buffer = gst_sample_get_buffer(sample);
  if (!buffer) {
    gst_sample_unref(sample);
    return GST_FLOW_ERROR;
  }

  GstCaps *caps = gst_sample_get_caps(sample);
  int frame_width = config_.width;
  int frame_height = config_.height;

  if (caps) {
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(structure, "width", &frame_width);
    gst_structure_get_int(structure, "height", &frame_height);
  }

  uint64_t timestamp_ms = 0;
  if (GST_BUFFER_PTS_IS_VALID(buffer)) {
    timestamp_ms = GST_BUFFER_PTS(buffer) / GST_MSECOND;
  } else {
    timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }

  GstMapInfo map;
  if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
    if (frame_callback_) {
      ImageFrame frame;
      frame.width = frame_width;
      frame.height = frame_height;
      frame.channels = 3;
      frame.timestamp_ms = timestamp_ms;
      frame.format = config_.pixel_format;

      frame.pixel_data.assign(map.data, map.data + map.size);

      frame_callback_(frame);
    }
    gst_buffer_unmap(buffer, &map);
  }

  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

} // namespace processing
