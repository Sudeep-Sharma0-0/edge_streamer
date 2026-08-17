#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace processing {

struct Keypoint {
  float x = 0.0f;
  float y = 0.0f;
  float confidence = 0.0f;

  int GetPixelX(int image_width) const {
    return static_cast<int>(x * image_width);
  }

  int GetPixelY(int image_height) const {
    return static_cast<int>(y * image_height);
  }
};

struct Detection {
  int class_id = 0;
  std::string label = "person";
  float confidence = 0.0f;
  float x_min = 0.0f;
  float y_min = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  std::vector<Keypoint> keypoints;
};

struct InferenceResult {
  uint64_t timestamp_ms = 0;
  float inference_time_ms = 0.0f;
  std::vector<Detection> detections;
};

} // namespace processing
