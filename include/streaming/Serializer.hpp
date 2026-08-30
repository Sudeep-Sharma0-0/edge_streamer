#ifndef POSE_SERIALIZER_HPP
#define POSE_SERIALIZER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace streaming {

struct Keypoint {
  float x{0.0f};
  float y{0.0f};
  float confidence{0.0f};
};

struct Detection {
  int32_t track_id{-1};
  float confidence{0.0f};
  float x_min{0.0f};
  float y_min{0.0f};
  float width{0.0f};
  float height{0.0f};
  std::vector<Keypoint> keypoints;
};

struct InferenceResult {
  uint64_t timestamp_ms{0};
  std::vector<Detection> detections;
};

class PoseSerializer {
public:
  static std::string Serialize(const InferenceResult &result);
  static bool Deserialize(const std::string &binary_buffer,
                          InferenceResult &out_result);
};

} // namespace streaming

#endif
