#include "capture_module/PipelineEngine.hpp"
#include <iostream>
#include <sstream>

namespace capture {

PipelineEngine::PipelineEngine(const config::CaptureConfig &config)
    : config_(config) {}

PipelineEngine::~PipelineEngine() {
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
}

std::string PipelineEngine::GenerateCameraString() const {
  std::ostringstream ss;

  ss << "v4l2src device=" << config_.device << " do-timestamp=true ! ";

  ss << "image/jpeg,framerate=" << config_.fps << "/1 ! ";
  ss << "jpegdec ! ";

  ss << "videoconvert ! ";
  ss << "videoscale ! ";
  ss << "video/x-raw,format=RGB,width=" << config_.width
     << ",height=" << config_.height << " ! ";

  ss << "appsink name=outsink max-buffers=1 drop=true sync=false";

  return ss.str();
}

std::string PipelineEngine::GenerateVideoString() const {
  std::ostringstream ss;

  ss << "filesrc location=" << config_.location << " ! ";
  ss << "decodebin ! ";

  ss << "videoconvert ! ";
  ss << "videorate ! ";
  ss << "video/x-raw,framerate=" << config_.fps << "/1 ! ";
  ss << "videoscale ! ";

  ss << "video/x-raw,format=RGB,width=" << config_.width
     << ",height=" << config_.height << " ! ";

  ss << "appsink name=outsink max-buffers=1 drop=true sync=true";

  return ss.str();
}

bool PipelineEngine::BuildPipeline() {
  std::string pipeline_str;
  if (config_.type == "camera") {
    pipeline_str = GenerateCameraString();
  } else {
    pipeline_str = GenerateVideoString();
  }
  std::cout << "[INFO] Building GStreamer Capture Pipeline:\n  gst-launch-1.0 "
            << pipeline_str << "\n";

  GError *error = nullptr;
  pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);

  if (error || !pipeline_) {
    std::cerr << "[ERROR] GStreamer pipeline creation failed: "
              << (error ? error->message : "Unknown error") << "\n";
    if (error)
      g_error_free(error);
    return false;
  }

  appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "outsink");
  if (!appsink_) {
    std::cerr << "[ERROR] Failed to find 'outsink' appsink in pipeline.\n";
    return false;
  }

  return true;
}

bool PipelineEngine::SetState(GstState state) {
  if (!pipeline_)
    return false;

  GstStateChangeReturn ret = gst_element_set_state(pipeline_, state);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    std::cerr << "[ERROR] Failed to change GStreamer pipeline state.\n";
    return false;
  }
  return true;
}

} // namespace capture
