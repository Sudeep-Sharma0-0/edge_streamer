#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace config {

enum class StreamProtocol { UDP_RTP, HTTP_MJPEG, NONE };

enum class InferenceBackend { NATIVE_HAILO, TAPPAS, MOCK };

struct CaptureConfig {
  std::string device = "/dev/video0";
  std::string type = "camera";
  std::string location = "videos/test.mp4";
  int width = 640;
  int height = 640;
  int fps = 30;
  std::string pixel_format = "RGB";
};

enum ScaleFlags : uint8_t {
  SCALE_NONE = 0,
  SCALE_80 = 1 << 0,
  SCALE_40 = 1 << 1,
  SCALE_20 = 1 << 2,
  SCALE_ALL = SCALE_80 | SCALE_40 | SCALE_20
};

struct InferenceConfig {
  InferenceBackend backend = InferenceBackend::NATIVE_HAILO;
  std::string model_path = "models/yolov8m_pose.hef";
  std::string tappas_postprocess_so =
      "/usr/lib/aarch64-linux-gnu/hailo/tappas/post_processes/"
      "libyolov8pose_post.so";
  std::string tappas_function_name = "filter";
  float confidence_threshold = 0.50f;
  float iou_threshold = 0.45f;
  float keypoint_threshold = 0.40f;
  int batch_size{1};
  uint8_t enabled_scales{SCALE_ALL};
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
