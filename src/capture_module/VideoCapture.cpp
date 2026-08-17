#include "capture_module/VideoCapture.hpp"
#include "capture_module/PipelineEngine.hpp"
#include <iostream>

namespace capture {

VideoCapture::VideoCapture() : config_(config::CaptureConfig{}) {}

VideoCapture::VideoCapture(const config::CaptureConfig &config)
    : config_(config) {}

VideoCapture::~VideoCapture() { Stop(); }

VideoCapture::VideoCapture(VideoCapture &&) noexcept = default;
VideoCapture &VideoCapture::operator=(VideoCapture &&) noexcept = default;

bool VideoCapture::Initialize(const config::CaptureConfig &config) {
  config_ = config;

  if (!gst_is_initialized()) {
    gst_init(nullptr, nullptr);
  }

  engine_ = std::make_unique<PipelineEngine>(config_);
  if (!engine_->BuildPipeline()) {
    std::cerr << "[ERROR] VideoCapture: Failed to build capture engine.\n";
    return false;
  }

  return true;
}

bool VideoCapture::Start() {
  if (!engine_) {
    std::cerr << "[ERROR] VideoCapture: Engine not initialized.\n";
    return false;
  }

  if (engine_->SetState(GST_STATE_PLAYING)) {
    is_running_ = true;
    std::cout << "[INFO] VideoCapture pipeline started successfully on "
              << config_.device << " (" << config_.width << "x"
              << config_.height << " @" << config_.fps << "fps)\n";
    return true;
  }

  return false;
}

void VideoCapture::Stop() {
  if (is_running_ && engine_) {
    engine_->SetState(GST_STATE_NULL);
    is_running_ = false;
    std::cout << "[INFO] VideoCapture pipeline stopped.\n";
  }
}

bool VideoCapture::IsRunning() const { return is_running_; }

GstElement *VideoCapture::GetPipelineElement() const {
  return engine_ ? engine_->GetPipeline() : nullptr;
}

GstElement *VideoCapture::GetAppSinkElement() const {
  return engine_ ? engine_->GetAppSink() : nullptr;
}

} // namespace capture
