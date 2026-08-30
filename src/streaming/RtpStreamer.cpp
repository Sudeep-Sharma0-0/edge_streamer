#include "streaming/RtpStreamer.hpp"
#include <iostream>

namespace streaming {

RtpStreamer::RtpStreamer(const config::StreamingConfig &stream_cfg)
    : config_(stream_cfg) {
  gst_init(nullptr, nullptr);
}

RtpStreamer::~RtpStreamer() { Stop(); }

bool RtpStreamer::Start() {
  std::string pipeline_str =
      "appsrc name=mysrc is-live=true do-timestamp=true format=time ! "
      "videoconvert ! "
      "video/x-raw,format=I420 ! "
      "x264enc tune=zerolatency speed-preset=ultrafast bframes=0 "
      "key-int-max=30 bitrate=2000 ! "
      "rtph264pay config-interval=1 pt=96 ! "
      "udpsink host=" +
      config_.host + " port=" + std::to_string(config_.port) + " sync=false";

  GError *err = nullptr;
  pipeline_ = gst_parse_launch(pipeline_str.c_str(), &err);
  if (err) {
    std::cerr << "[ERROR] GStreamer pipeline parse failed: " << err->message
              << "\n";
    g_error_free(err);
    return false;
  }

  appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "mysrc");

  GstCaps *caps =
      gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR",
                          "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 640,
                          "framerate", GST_TYPE_FRACTION, 30, 1, nullptr);
  g_object_set(G_OBJECT(appsrc_), "caps", caps, nullptr);
  gst_caps_unref(caps);

  gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  is_running_ = true;

  std::cout << "[INFO] Native GStreamer RTP initialized. Target: "
            << config_.host << ":" << config_.port << "\n";
  return true;
}

void RtpStreamer::SendFrame(const cv::Mat &frame) {
  if (!is_running_ || frame.empty() || !appsrc_)
    return;

  cv::Mat strict_frame;
  if (frame.cols != 640 || frame.rows != 640) {
    cv::resize(frame, strict_frame, cv::Size(640, 640));
  } else {
    strict_frame = frame;
  }

  guint size = strict_frame.total() * strict_frame.elemSize();
  GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);

  gst_buffer_fill(buffer, 0, strict_frame.data, size);

  GstFlowReturn ret;
  g_signal_emit_by_name(appsrc_, "push-buffer", buffer, &ret);
  gst_buffer_unref(buffer);
}

void RtpStreamer::Stop() {
  is_running_ = false;
  if (pipeline_) {
    gst_element_send_event(pipeline_, gst_event_new_eos());
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
  if (appsrc_) {
    gst_object_unref(appsrc_);
    appsrc_ = nullptr;
  }
}

} // namespace streaming
