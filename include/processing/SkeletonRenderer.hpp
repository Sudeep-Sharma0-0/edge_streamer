#pragma once

#include "config/AppConfig.hpp"
#include "processing/InferenceTypes.hpp"
#include <opencv2/opencv.hpp>
#include <vector>

namespace processing {

class SkeletonRenderer {
public:
  static void Render(cv::Mat &image, const InferenceResult &result,
                     const config::VisualizationConfig &visual_cfg,
                     float min_keypoint_conf = 0.40f);

  static void Render(cv::Mat &image, const InferenceResult &result,
                     float min_keypoint_conf = 0.40f);

private:
  struct LimbConnection {
    int joint1;
    int joint2;
    cv::Scalar color;
  };

  static const std::vector<LimbConnection> &GetLimbConnections();
  static cv::Scalar GetKeypointColor(int joint_idx);
};

} // namespace processing
