#pragma once

#include <string>
#include <vector>

namespace config {

enum class StreamProtocol { UDP_RTP, HTTP_MJPEG, NONE };

enum class InferenceBackend { HAILO_NPU, MOCK };

struct CaptureConfig {
  std::string device = "/dev/video0";
  std::string type = "camera";
  std::string location = "videos/test.mp4";
  int width = 640;
  int height = 640;
  int fps = 30;
  std::string pixel_format = "RGB";
};

struct InferenceConfig {
  InferenceBackend backend = InferenceBackend::HAILO_NPU;
  std::string model_path = "models/yolov8m_pose.hef";
  float confidence_threshold = 0.50f;
  float iou_threshold = 0.45f;
  float keypoint_threshold = 0.40f;
};

struct StreamingConfig {
  StreamProtocol protocol = StreamProtocol::HTTP_MJPEG;
  std::string host = "127.0.0.1";
  int port = 5777;
  int bitrate_kbps = 2000;
  int jpeg_quality = 75;
  bool enabled = true;
};

struct VisualizationConfig {
  bool enable_overlay = true;
  bool skeleton_only = false;
  bool draw_bounding_box = true;
  bool draw_skeleton = true;
  bool draw_keypoints = true;
  int line_thickness = 2;
  int joint_radius = 4;
};

struct AppConfig {
  CaptureConfig capture;
  InferenceConfig inference;
  StreamingConfig streaming;
  VisualizationConfig visual;

  bool Validate(std::vector<std::string> &validation_errors) const;

  std::string ToString() const;

  static AppConfig Default();
};

} // namespace config
