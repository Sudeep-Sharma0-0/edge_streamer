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
  const auto &limbs = GetLimbConnections();

  const int OFFSET_X = 18;
  const int OFFSET_Y = 10;

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

      std::ostringstream label_ss;
      label_ss << det.label << " " << std::fixed << std::setprecision(2)
               << (det.confidence * 100.0f) << "%";
      std::string label_str = label_ss.str();

      int baseline = 0;
      cv::Size label_size = cv::getTextSize(label_str, cv::FONT_HERSHEY_SIMPLEX,
                                            0.45, 1, &baseline);
      int label_y = std::max(y, label_size.height + 4);

      cv::rectangle(image, cv::Point(x, label_y - label_size.height - 4),
                    cv::Point(x + label_size.width + 4, label_y + baseline - 2),
                    cv::Scalar(0, 255, 0), cv::FILLED);

      cv::putText(image, label_str, cv::Point(x + 2, label_y - 2),
                  cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1,
                  cv::LINE_AA);
    }

    auto get_calibrated_pt = [&](const auto &kp) -> cv::Point {
      int abs_x = kp.GetPixelX(frame_w) + OFFSET_X;
      int abs_y = kp.GetPixelY(frame_h) + OFFSET_Y;
      return cv::Point(abs_x, abs_y);
    };

    if (visual_cfg.draw_skeleton) {
      for (const auto &limb : limbs) {
        if (limb.joint1 >= static_cast<int>(det.keypoints.size()) ||
            limb.joint2 >= static_cast<int>(det.keypoints.size())) {
          continue;
        }

        const auto &kp1 = det.keypoints[limb.joint1];
        const auto &kp2 = det.keypoints[limb.joint2];

        if (kp1.confidence >= min_keypoint_conf &&
            kp2.confidence >= min_keypoint_conf) {

          cv::Point pt1 = get_calibrated_pt(kp1);
          cv::Point pt2 = get_calibrated_pt(kp2);

          cv::line(image, pt1, pt2, limb.color, visual_cfg.line_thickness,
                   cv::LINE_AA);
        }
      }
    }

    if (visual_cfg.draw_keypoints) {
      for (size_t i = 0; i < det.keypoints.size(); ++i) {
        const auto &kp = det.keypoints[i];
        if (kp.confidence >= min_keypoint_conf) {
          cv::circle(image, get_calibrated_pt(kp), visual_cfg.joint_radius,
                     GetKeypointColor(static_cast<int>(i)), -1, cv::LINE_AA);
        }
      }
    }
  }
}

} // namespace processing
