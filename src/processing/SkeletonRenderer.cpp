#include "processing/SkeletonRenderer.hpp"

namespace processing {

const std::vector<SkeletonRenderer::LimbConnection> &
SkeletonRenderer::GetLimbConnections() {
  static const std::vector<LimbConnection> kLimbs = {
      // Facial features (Head - Magenta)
      {0, 1, cv::Scalar(255, 0, 255)},
      {0, 2, cv::Scalar(255, 0, 255)},
      {1, 3, cv::Scalar(255, 0, 255)},
      {2, 4, cv::Scalar(255, 0, 255)},

      // Torso (Central - Green)
      {5, 6, cv::Scalar(0, 255, 0)},
      {5, 11, cv::Scalar(0, 255, 0)},
      {6, 12, cv::Scalar(0, 255, 0)},
      {11, 12, cv::Scalar(0, 255, 0)},

      // Left Arm (Cyan)
      {5, 7, cv::Scalar(255, 255, 0)},
      {7, 9, cv::Scalar(255, 255, 0)},

      // Right Arm (Orange/Yellow)
      {6, 8, cv::Scalar(0, 165, 255)},
      {8, 10, cv::Scalar(0, 165, 255)},

      // Left Leg (Blue/Cyan)
      {11, 13, cv::Scalar(255, 200, 0)},
      {13, 15, cv::Scalar(255, 200, 0)},

      // Right Leg (Red/Orange)
      {12, 14, cv::Scalar(0, 100, 255)},
      {14, 16, cv::Scalar(0, 100, 255)}};
  return kLimbs;
}

cv::Scalar SkeletonRenderer::GetKeypointColor(int joint_idx) {
  if (joint_idx == 0)
    return cv::Scalar(0, 255, 255);
  if (joint_idx % 2 == 1)
    return cv::Scalar(255, 255, 0);
  return cv::Scalar(0, 140, 255);
}

void SkeletonRenderer::Render(cv::Mat &image, const InferenceResult &result,
                              float min_keypoint_conf) {
  config::VisualizationConfig default_cfg;
  Render(image, result, default_cfg, min_keypoint_conf);
}

void SkeletonRenderer::Render(cv::Mat &image, const InferenceResult &result,
                              const config::VisualizationConfig &visual_cfg,
                              float min_keypoint_conf) {
  if (!visual_cfg.enable_overlay || image.empty()) {
    return;
  }

  const int frame_w = image.cols;
  const int frame_h = image.rows;

  const float MODEL_DIM = 640.0f;
  const float scale = std::min(MODEL_DIM / frame_w, MODEL_DIM / frame_h);
  const float pad_x = (MODEL_DIM - (frame_w * scale)) / 2.0f;
  const float pad_y = (MODEL_DIM - (frame_h * scale)) / 2.0f;

  auto get_true_pt = [&](const auto &kp) -> cv::Point {
    float norm_x = kp.GetPixelX(10000) / 10000.0f;
    float norm_y = kp.GetPixelY(10000) / 10000.0f;

    int true_x = static_cast<int>(((norm_x * MODEL_DIM) - pad_x) / scale);
    int true_y = static_cast<int>(((norm_y * MODEL_DIM) - pad_y) / scale);
    return cv::Point(true_x, true_y);
  };

  for (const auto &det : result.detections) {

    if (visual_cfg.draw_bounding_box) {
      int x = static_cast<int>(det.x_min * frame_w);
      int y = static_cast<int>(det.y_min * frame_h);
      int w = static_cast<int>(det.width * frame_w);
      int h = static_cast<int>(det.height * frame_h);

      x = std::max(0, std::min(x, frame_w - 1));
      y = std::max(0, std::min(y, frame_h - 1));
      w = std::max(0, std::min(w, frame_w - x));
      h = std::max(0, std::min(h, frame_h - y));

      cv::rectangle(image, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0),
                    visual_cfg.line_thickness, cv::LINE_AA);
    }

    if (visual_cfg.draw_keypoints) {
      for (size_t i = 0; i < det.keypoints.size(); ++i) {
        if (i >= 17)
          break;

        const auto &kp = det.keypoints[i];
        if (kp.confidence >= min_keypoint_conf) {
          cv::Point center = get_true_pt(kp);

          cv::circle(image, center, visual_cfg.joint_radius,
                     GetKeypointColor(static_cast<int>(i)), -1, cv::LINE_AA);

          cv::circle(image, center, std::max(1, visual_cfg.joint_radius / 2),
                     cv::Scalar(255, 255, 255), -1, cv::LINE_AA);
        }
      }
    }
  }
}

} // namespace processing
