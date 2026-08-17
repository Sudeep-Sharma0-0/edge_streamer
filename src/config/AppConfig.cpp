#include "config/AppConfig.hpp"
#include <sstream>

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

  if (inference.backend == InferenceBackend::HAILO_NPU &&
      inference.model_path.empty()) {
    errors.push_back(
        "Model path must be specified when using HAILO_NPU backend.");
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

std::string AppConfig::ToString() const {
  std::ostringstream oss;
  oss << "\n┌───────────────────────[ EDGE STREAMER CONFIG "
         "]───────────────────────┐\n";
  oss << " <Capture>\n";
  oss << "   1. Device:           " << capture.device << "\n";
  oss << "   2. Resolution:       " << capture.width << "x" << capture.height
      << " @ " << capture.fps << " FPS\n";
  oss << "   3. Pixel Format:     " << capture.pixel_format << "\n";
  oss << "   4. Source Type:      " << capture.type << "\n";
  if (capture.type != "camera") {
    oss << "   a. File Source:      " << capture.location << "\n";
  }

  oss << " <Inference>\n";
  oss << "   1. Backend:          "
      << (inference.backend == InferenceBackend::HAILO_NPU ? "Hailo-8 / 8L NPU"
                                                           : "")
      << "\n";
  oss << "   2. Model Path:       " << inference.model_path << "\n";
  oss << "   3. Confidence Thresh:" << inference.confidence_threshold << "\n";
  oss << "   4. IoU NMS Thresh:   " << inference.iou_threshold << "\n";
  oss << "   5. Keypoint Thresh:  " << inference.keypoint_threshold << "\n";

  oss << " <Streaming>\n";
  oss << "   1. Status:           "
      << (streaming.enabled ? "Enabled" : "Disabled") << "\n";
  if (streaming.enabled) {
    oss << "   a. Protocol:         "
        << (streaming.protocol == StreamProtocol::HTTP_MJPEG
                ? "HTTP MJPEG Server"
                : "UDP/RTP H.264")
        << "\n";
    oss << "   b. Target Host/Port: " << streaming.host << ":" << streaming.port
        << "\n";
    oss << "   c. Bitrate / Quality:" << streaming.bitrate_kbps << " kbps / Q"
        << streaming.jpeg_quality << "\n";

    oss << " <Visualization>\n";
    oss << "   1. Overlay:          "
        << (visual.enable_overlay ? "Enabled" : "Disabled") << "\n";
    oss << "   2. Mode:             "
        << (visual.skeleton_only ? "Skeleton Only (Black Background)"
                                 : "Full Video Feed Overlay")
        << "\n";
  }
  oss << "============================================================\n";
  return oss.str();
}

} // namespace config
