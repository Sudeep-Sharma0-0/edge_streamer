#include "streaming/RtpStreamer.hpp"
#include "streaming/Serializer.hpp"
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <ottersec/api.h>
#include <vector>

namespace streaming {

RtpStreamer::RtpStreamer(const config::StreamingConfig &stream_cfg)
    : config_(stream_cfg), frame_counter_(0), is_running_(false) {}

RtpStreamer::~RtpStreamer() { Stop(); }

bool RtpStreamer::Start() {
  if (is_running_) {
    std::cout
        << "[INFO] Streamer is already running. Ignoring duplicate Start().\n";
    return true;
  }

  OtterHandshakeConfig auth_cfg = {0};
  auth_cfg.handshake_port = 8443;
  auth_cfg.auth_timeout_ms = 5000;
  auth_cfg.cert_path = "/home/cloud9/sudip/edge_streamer/certs/server.crt";
  auth_cfg.key_path = "/home/cloud9/sudip/edge_streamer/certs/server.key";

  if (ottersec_init(&auth_cfg) != 0) {
    std::cerr << "[ERROR] Failed to initialize OtterSec.\n";
    return false;
  }

  if (ottersec_start_control_server(8080) != 0) {
    std::cerr << "[ERROR] Failed to start OtterSec Control Server.\n";
    return false;
  }

  is_running_ = true;
  std::cout
      << "[INFO] OtterSec initialized and operating in Silent Tripwire mode.\n";

  return true;
}

void RtpStreamer::SendFrame(const cv::Mat &frame) {
  if (!is_running_ || frame.empty())
    return;

  frame_counter_++;

  if (frame.cols != 640 || frame.rows != 640) {
    cv::resize(frame, active_frame_, cv::Size(640, 640));
  } else {
    active_frame_ = frame.clone();
  }

  cv::cvtColor(active_frame_, active_frame_, cv::COLOR_BGR2RGB);

  OtterFrameBuffer out_frame = {0};
  out_frame.width = 640;
  out_frame.height = 640;
  out_frame.surface_ptr = active_frame_.data;

  ottersec_push_frame(&out_frame);
}

void RtpStreamer::NotifyEmergencyFall(
    uint64_t timestamp_ms,
    const std::vector<streaming::Detection> &detections) {
  if (!is_running_)
    return;

  std::cout
      << "[INFO] Fall Detected! Triggering OtterSec emergency protocol.\n";

  std::vector<OtterDetection> otter_dets;
  std::vector<std::vector<OtterKeypoint>> all_kpts;

  all_kpts.reserve(detections.size());

  for (const auto &d : detections) {
    std::vector<OtterKeypoint> kpts;
    kpts.reserve(d.keypoints.size());
    for (const auto &k : d.keypoints) {
      kpts.push_back({k.x, k.y, k.confidence});
    }

    all_kpts.push_back(std::move(kpts));

    OtterDetection od = {d.track_id,
                         d.confidence,
                         d.x_min,
                         d.y_min,
                         d.width,
                         d.height,
                         all_kpts.back().data(),
                         all_kpts.back().size()};

    otter_dets.push_back(od);
  }

  OtterInferenceResult result = {0};
  result.timestamp_ms = timestamp_ms;
  result.fall_detected = 1;
  result.detections = otter_dets.data();
  result.num_detections = otter_dets.size();

  ottersec_send_telemetry(&result);

  OtterFallEvent event = {0};
  event.fall_detected = 1;

  if (!detections.empty()) {
    event.confidence = detections[0].confidence;
    event.bbox_x = static_cast<int>(detections[0].x_min);
    event.bbox_y = static_cast<int>(detections[0].y_min);
    event.bbox_w = static_cast<int>(detections[0].width);
    event.bbox_h = static_cast<int>(detections[0].height);
  }

  ottersec_notify_fall(&event);
}

bool RtpStreamer::IsRunning() const { return is_running_; }

void RtpStreamer::Stop() {
  if (is_running_) {
    is_running_ = false;
    ottersec_stop_control_server();
    ottersec_terminate();
  }
}

bool RtpStreamer::CheckRemoteResolve() {
  return ottersec_check_remote_resolve() == 1;
}

} // namespace streaming
