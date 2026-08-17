#include "processing/HailoInferenceEngine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace processing {

namespace {

inline float ComputeIoU(const Detection &a, const Detection &b) {
  float x1 = std::max(a.x_min, b.x_min);
  float y1 = std::max(a.y_min, b.y_min);
  float x2 = std::min(a.x_min + a.width, b.x_min + b.width);
  float y2 = std::min(a.y_min + a.height, b.y_min + b.height);

  float inter = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
  float total = (a.width * a.height) + (b.width * b.height) - inter;
  return (total <= 0.0f) ? 0.0f : (inter / total);
}

inline float DecodeDFL(const float *bins) {
  float max_v = *std::max_element(bins, bins + 16);
  float sum = 0.0f;
  float exp_bins[16];

  for (int i = 0; i < 16; ++i) {
    exp_bins[i] = std::exp(bins[i] - max_v);
    sum += exp_bins[i];
  }

  float dist = 0.0f;
  for (int i = 0; i < 16; ++i) {
    dist += (exp_bins[i] / sum) * i;
  }
  return dist;
}

struct ScaleHead {
  int grid;
  int stride;
  const char *dfl_layer;
  const char *cls_layer;
  const char *kpt_layer;
};

constexpr ScaleHead kScales[3] = {
    {80, 8, "yolov8m_pose/conv59", "yolov8m_pose/conv60",
     "yolov8m_pose/conv61"},
    {40, 16, "yolov8m_pose/conv75", "yolov8m_pose/conv76",
     "yolov8m_pose/conv77"},
    {20, 32, "yolov8m_pose/conv90", "yolov8m_pose/conv91",
     "yolov8m_pose/conv92"}};

} // namespace

HailoInferenceEngine::HailoInferenceEngine()
    : config_(config::InferenceConfig{}) {}

HailoInferenceEngine::HailoInferenceEngine(
    const config::InferenceConfig &config)
    : config_(config) {}

HailoInferenceEngine::~HailoInferenceEngine() = default;

bool HailoInferenceEngine::Initialize(const std::string &model_path,
                                      float conf_thresh, float iou_thresh) {
  config_.model_path = model_path;
  config_.confidence_threshold = conf_thresh;
  config_.iou_threshold = iou_thresh;
  return Initialize(config_);
}

bool HailoInferenceEngine::Initialize(const config::InferenceConfig &config) {
  config_ = config;

  auto vdevice_exp = hailort::VDevice::create();
  if (!vdevice_exp) {
    std::cerr << "[ERROR] HailoEngine: Failed to create VDevice: "
              << vdevice_exp.status() << "\n";
    return false;
  }
  vdevice_ = vdevice_exp.release();

  auto infer_model_exp = vdevice_->create_infer_model(config_.model_path);
  if (!infer_model_exp) {
    std::cerr << "[ERROR] HailoEngine: Failed to load HEF: "
              << config_.model_path << " (" << infer_model_exp.status()
              << ")\n";
    return false;
  }
  infer_model_ = infer_model_exp.release();

  return ConfigureStreams();
}

bool HailoInferenceEngine::ConfigureStreams() {
  if (!infer_model_)
    return false;

  auto input_names = infer_model_->get_input_names();

  if (input_names.empty())
    return false;

  auto input_stream = infer_model_->input(input_names[0]);
  input_stream->set_format_type(HAILO_FORMAT_TYPE_UINT8);
  input_stream->set_format_order(HAILO_FORMAT_ORDER_NHWC);

  output_buffers_.clear();
  for (const auto &name : infer_model_->get_output_names()) {
    auto output_stream = infer_model_->output(name);
    output_stream->set_format_type(HAILO_FORMAT_TYPE_FLOAT32);
    output_stream->set_format_order(HAILO_FORMAT_ORDER_NHWC);
    output_buffers_[name] =
        std::vector<uint8_t>(output_stream->get_frame_size());
  }

  auto configured_exp = infer_model_->configure();
  if (!configured_exp)
    return false;
  configured_infer_model_ =
      std::make_unique<hailort::ConfiguredInferModel>(configured_exp.release());

  auto bindings_exp = configured_infer_model_->create_bindings();
  if (!bindings_exp)
    return false;
  bindings_ = std::make_unique<hailort::ConfiguredInferModel::Bindings>(
      bindings_exp.release());

  for (const auto &name : infer_model_->get_output_names()) {
    auto status = bindings_->output(name)->set_buffer(hailort::MemoryView(
        output_buffers_[name].data(), output_buffers_[name].size()));
    if (status != HAILO_SUCCESS)
      return false;
  }

  is_initialized_ = true;
  std::cout << "[INFO] HailoInferenceEngine initialized: " << config_.model_path
            << "\n";
  return true;
}

InferenceResult HailoInferenceEngine::Infer(const FrameBuffer &frame) {
  auto start_time = std::chrono::steady_clock::now();
  InferenceResult result{.timestamp_ms = frame.timestamp_ms};

  if (!is_initialized_) {
    std::cerr
        << "[ERROR] HailoEngine: Inference called before initialization.\n";
    return result;
  }

  auto status = bindings_->input(infer_model_->get_input_names()[0])
                    ->set_buffer(hailort::MemoryView(
                        const_cast<uint8_t *>(frame.data), frame.size));
  if (status != HAILO_SUCCESS)
    return result;

  status =
      configured_infer_model_->run(*bindings_, std::chrono::milliseconds(1000));
  if (status != HAILO_SUCCESS)
    return result;

  std::vector<Detection> candidates;
  for (const auto &scale : kScales) {
    DecodeScale(scale.grid, scale.stride,
                reinterpret_cast<const float *>(
                    output_buffers_[scale.dfl_layer].data()),
                reinterpret_cast<const float *>(
                    output_buffers_[scale.cls_layer].data()),
                reinterpret_cast<const float *>(
                    output_buffers_[scale.kpt_layer].data()),
                candidates);
  }

  ApplyNMS(candidates, result.detections);

  result.inference_time_ms = std::chrono::duration<float, std::milli>(
                                 std::chrono::steady_clock::now() - start_time)
                                 .count();

  return result;
}

void HailoInferenceEngine::DecodeScale(int grid_size, int stride,
                                       const float *bbox_dfl,
                                       const float *cls_scores,
                                       const float *kpts,
                                       std::vector<Detection> &proposals) {
  constexpr float kInvImgSize = 1.0f / 640.0f;

  for (int row = 0; row < grid_size; ++row) {
    for (int col = 0; col < grid_size; ++col) {
      int grid_idx = row * grid_size + col;

      float score = cls_scores[grid_idx];
      if (score < config_.confidence_threshold)
        continue;

      const float *cell_dfl = bbox_dfl + (grid_idx * 64);
      float left = DecodeDFL(cell_dfl + 0) * stride;
      float top = DecodeDFL(cell_dfl + 16) * stride;
      float right = DecodeDFL(cell_dfl + 32) * stride;
      float bottom = DecodeDFL(cell_dfl + 48) * stride;

      float cx = (col + 0.5f) * stride;
      float cy = (row + 0.5f) * stride;
      float x1 = cx - left;
      float y1 = cy - top;
      float x2 = cx + right;
      float y2 = cy + bottom;

      Detection det;
      det.class_id = 0;
      det.label = "person";
      det.confidence = score;
      det.x_min = std::clamp(x1 * kInvImgSize, 0.0f, 1.0f);
      det.y_min = std::clamp(y1 * kInvImgSize, 0.0f, 1.0f);
      det.width = std::clamp((x2 - x1) * kInvImgSize, 0.0f, 1.0f - det.x_min);
      det.height = std::clamp((y2 - y1) * kInvImgSize, 0.0f, 1.0f - det.y_min);

      const float *cell_kpts = kpts + (grid_idx * 51);
      det.keypoints.reserve(17);

      for (int kp_i = 0; kp_i < 17; ++kp_i) {
        float raw_x = cell_kpts[kp_i * 3 + 0];
        float raw_y = cell_kpts[kp_i * 3 + 1];
        float conf = cell_kpts[kp_i * 3 + 2];

        float px = (raw_x * 2.0f - 0.5f + col) * stride;
        float py = (raw_y * 2.0f - 0.5f + row) * stride;

        det.keypoints.push_back({.x = std::clamp(px * kInvImgSize, 0.0f, 1.0f),
                                 .y = std::clamp(py * kInvImgSize, 0.0f, 1.0f),
                                 .confidence = conf});
      }

      proposals.push_back(std::move(det));
    }
  }
}

void HailoInferenceEngine::ApplyNMS(std::vector<Detection> &candidates,
                                    std::vector<Detection> &output) {
  if (candidates.empty())
    return;

  std::sort(candidates.begin(), candidates.end(),
            [](const Detection &a, const Detection &b) {
              return a.confidence > b.confidence;
            });

  std::vector<bool> suppressed(candidates.size(), false);
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (suppressed[i])
      continue;
    output.push_back(candidates[i]);

    for (size_t j = i + 1; j < candidates.size(); ++j) {
      if (!suppressed[j] &&
          ComputeIoU(candidates[i], candidates[j]) > config_.iou_threshold) {
        suppressed[j] = true;
      }
    }
  }
}

} // namespace processing
