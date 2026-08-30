#include "config/AppConfig.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace config {

AppConfig AppConfig::Default() { return AppConfig{}; }

bool AppConfig::Validate(std::vector<std::string> &errors) const {
  errors.clear();

  if (capture.type == "camera" && capture.device.empty()) {
    errors.push_back("Capture device path cannot be empty.");
  } else if (capture.type == "file" && capture.location.empty()) {
    errors.push_back(
        "A video file is needed if file capture type is selected.");
  }
  if (capture.width <= 0 || capture.height <= 0) {
    errors.push_back("Capture dimensions must be positive integers.");
  }
  if (capture.fps <= 0 || capture.fps > 120) {
    errors.push_back("Capture FPS must be within range [1, 120].");
  }

  if ((inference.backend == InferenceBackend::NATIVE_HAILO ||
       inference.backend == InferenceBackend::TAPPAS) &&
      inference.model_path.empty()) {
    errors.push_back(
        "Model path must be specified when using NATIVE_HAILO or TAPPAS "
        "backend.");
  }
  if (inference.backend == InferenceBackend::TAPPAS &&
      inference.tappas_postprocess_so.empty()) {
    errors.push_back(
        "TAPPAS post-processing library path (--tappas-so) cannot be empty.");
  }
  if (inference.confidence_threshold < 0.0f ||
      inference.confidence_threshold > 1.0f) {
    errors.push_back("Confidence threshold must be within range [0.0, 1.0].");
  }
  if (inference.iou_threshold < 0.0f || inference.iou_threshold > 1.0f) {
    errors.push_back("IoU threshold must be within range [0.0, 1.0].");
  }
  if (inference.keypoint_threshold < 0.0f ||
      inference.keypoint_threshold > 1.0f) {
    errors.push_back("Keypoint threshold must be within range [0.0, 1.0].");
  }
  if (inference.enabled_scales > 7 || inference.enabled_scales < 1) {
    errors.push_back(
        "Invalid scales given. Please enable at least 1 and at most 3 scales.");
  }

  if (streaming.enabled) {
    if (streaming.port < 1 || streaming.port > 65535) {
      errors.push_back("Streamer port must be within range [1, 65535].");
    }
    if (streaming.protocol == StreamProtocol::UDP_RTP &&
        streaming.host.empty()) {
      errors.push_back("Target host IP cannot be empty for UDP streaming.");
    }
    if (streaming.jpeg_quality < 1 || streaming.jpeg_quality > 100) {
      errors.push_back("JPEG quality must be within range [1, 100].");
    }
  }

  return errors.empty();
}

static std::string EnabledScalesToString(uint8_t mask) {
  if (mask == 0)
    return "None (0)";
  if (mask == 7)
    return "All [80x80, 40x40, 20x20] (7)";

  std::string result;
  if (mask & config::SCALE_80)
    result += "80x80 ";
  if (mask & config::SCALE_40)
    result += "40x40 ";
  if (mask & config::SCALE_20)
    result += "20x20 ";

  return result + "(" + std::to_string(static_cast<int>(mask)) + ")";
}

std::string AppConfig::ToString() const {
  std::ostringstream oss;
  oss << "\n┌───────────────────────[ EDGE STREAMER CONFIG "
         "]───────────────────────┐\n";
  oss << " <Capture>\n";
  oss << "   1. Device:            " << capture.device << "\n";
  oss << "   2. Resolution:        " << capture.width << "x" << capture.height
      << " @ " << capture.fps << " FPS\n";
  oss << "   3. Pixel Format:      " << capture.pixel_format << "\n";
  oss << "   4. Source Type:       " << capture.type << "\n";
  if (capture.type != "camera") {
    oss << "   a. File Source:       " << capture.location << "\n";
  }

  oss << " <Inference>\n";
  oss << "   1. Backend:           ";
  switch (inference.backend) {
  case InferenceBackend::NATIVE_HAILO:
    oss << "Native Hailo RT C++\n";
    break;
  case InferenceBackend::TAPPAS:
    oss << "GStreamer TAPPAS (hailonet + hailofilter)\n";
    break;
  case InferenceBackend::MOCK:
    oss << "Mock Engine\n";
    break;
  }
  oss << "   2. Model Path:        " << inference.model_path << "\n";
  if (inference.backend == InferenceBackend::TAPPAS) {
    oss << "   a. Postprocess SO:    " << inference.tappas_postprocess_so
        << "\n";
  }
  oss << "   3. Confidence Thresh: " << inference.confidence_threshold << "\n";
  oss << "   4. IoU NMS Thresh:    " << inference.iou_threshold << "\n";
  oss << "   5. Keypoint Thresh:   " << inference.keypoint_threshold << "\n";
  oss << "   6. Enabled Scales:    "
      << EnabledScalesToString(inference.enabled_scales) << "\n";

  oss << " <Streaming>\n";
  oss << "   1. Status:            "
      << (streaming.enabled ? "Enabled" : "Disabled") << "\n";
  if (streaming.enabled) {
    oss << "   a. Protocol:          "
        << (streaming.protocol == StreamProtocol::HTTP_MJPEG
                ? "HTTP MJPEG Server"
                : "UDP/RTP H.264")
        << "\n";
    oss << "   b. Target Host/Port: " << streaming.host << ":" << streaming.port
        << "\n";
    oss << "   c. Bitrate / Quality:" << streaming.bitrate_kbps << " kbps / Q"
        << streaming.jpeg_quality << "\n";
  }

  oss << " <Visualization>\n";
  oss << "   1. Overlay:           "
      << (visual.enable_overlay ? "Enabled" : "Disabled") << "\n";
  oss << "   2. Mode:              "
      << (visual.skeleton_only ? "Skeleton Only (Black Canvas)"
                               : "Full Video Feed Overlay")
      << "\n";

  oss << "============================================================\n";
  return oss.str();
}

} // namespace config
