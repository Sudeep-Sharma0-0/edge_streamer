#include "processing/TappasPipeline.hpp"
#include "config/AppConfig.hpp"
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <sstream>

namespace processing {

TappasPipeline::TappasPipeline(const config::CaptureConfig &capture_cfg,
                               const config::InferenceConfig &inf_cfg)
    : capture_cfg_(capture_cfg), inf_cfg_(inf_cfg) {}

TappasPipeline::~TappasPipeline() { Stop(); }

std::string TappasPipeline::BuildPipelineString() const {
  std::ostringstream ss;

  ss << "appsrc name=insink is-live=true do-timestamp=true block=false "
     << "caps=video/x-raw,format=RGB,width=" << capture_cfg_.width
     << ",height=" << capture_cfg_.height << ",framerate=" << capture_cfg_.fps
     << "/1 ! " << "videoconvert ! " << "videoscale ! "
     << "video/x-raw,format=RGB,width=640,height=640 ! "
     << "queue leaky=downstream max-size-buffers=1 ! "
     << "hailonet hef-path=" << inf_cfg_.model_path
     << " batch-size=" << inf_cfg_.batch_size << " force-writable=true ! "
     << "queue leaky=downstream max-size-buffers=1 ! "
     << "hailofilter so-path=" << inf_cfg_.tappas_postprocess_so;

  if (!inf_cfg_.tappas_function_name.empty()) {
    ss << " function-name=" << inf_cfg_.tappas_function_name;
  }
  ss << " qos=false ! " << "queue leaky=downstream max-size-buffers=1 ! "
     << "appsink name=outsink max-buffers=1 drop=true sync=false";

  return ss.str();
}

bool TappasPipeline::Initialize() {
  if (!gst_is_initialized()) {
    gst_init(nullptr, nullptr);
  }

  std::string pipeline_str = BuildPipelineString();
  std::cout << "[TappasPipeline] Initializing pipeline:\n"
            << pipeline_str << std::endl;

  GError *error = nullptr;
  pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);

  if (error || !pipeline_) {
    std::cerr << "[TappasPipeline] Parse error: "
              << (error ? error->message : "Unknown error") << std::endl;
    if (error)
      g_error_free(error);
    return false;
  }

  appsrc_ = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline_), "insink"));
  appsink_ = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipeline_), "outsink"));

  if (!appsrc_ || !appsink_) {
    std::cerr
        << "[TappasPipeline] Failed to bind appsrc or appsink elements.\n";
    return false;
  }

  return true;
}

bool TappasPipeline::PushFrame(const FrameBuffer &frame) {
  if (!appsrc_ || !frame.data || frame.size == 0)
    return false;

  GstBuffer *buffer = gst_buffer_new_allocate(nullptr, frame.size, nullptr);
  if (!buffer)
    return false;

  gst_buffer_fill(buffer, 0, frame.data, frame.size);
  GstFlowReturn ret = gst_app_src_push_buffer(appsrc_, buffer);

  return (ret == GST_FLOW_OK);
}

bool TappasPipeline::Start() {
  if (!pipeline_)
    return false;

  GstStateChangeReturn ret =
      gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    std::cerr << "[TappasPipeline] Failed to set PLAYING state.\n";
    return false;
  }

  start_time_ = std::chrono::high_resolution_clock::now();
  last_frame_time_ = start_time_;
  frame_counter_ = 0;
  return true;
}

void TappasPipeline::Stop() {
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    appsrc_ = nullptr;
    appsink_ = nullptr;
  }
}

bool TappasPipeline::ProcessFrame(BenchmarkMetrics &metrics) {
  if (!appsink_)
    return false;

  GstSample *sample =
      gst_app_sink_try_pull_sample(appsink_, 10000000); // 10ms timeout
  if (!sample)
    return false;

  auto now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> frame_delta =
      now - last_frame_time_;
  std::chrono::duration<double> elapsed_total = now - start_time_;

  last_frame_time_ = now;
  frame_counter_++;

  metrics.frame_latency_ms = frame_delta.count();
  metrics.frame_count = frame_counter_;
  metrics.average_fps = (elapsed_total.count() > 0)
                            ? (frame_counter_ / elapsed_total.count())
                            : 0.0;

  gst_sample_unref(sample);
  return true;
}

} // namespace processing
