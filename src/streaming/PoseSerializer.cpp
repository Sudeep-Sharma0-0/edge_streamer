#include "pose_data.pb.h"
#include "streaming/Serializer.hpp"

namespace streaming {

std::string PoseSerializer::Serialize(const InferenceResult &result) {
  pose::InferenceResultMsg proto_result;
  proto_result.set_timestamp_ms(result.timestamp_ms);

  for (const auto &det : result.detections) {
    auto *proto_det = proto_result.add_detections();
    proto_det->set_track_id(det.track_id);
    proto_det->set_confidence(det.confidence);
    proto_det->set_x_min(det.x_min);
    proto_det->set_y_min(det.y_min);
    proto_det->set_width(det.width);
    proto_det->set_height(det.height);

    for (const auto &kp : det.keypoints) {
      auto *proto_kp = proto_det->add_keypoints();
      proto_kp->set_x(kp.x);
      proto_kp->set_y(kp.y);
      proto_kp->set_confidence(kp.confidence);
    }
  }

  std::string binary_output;
  if (!proto_result.SerializeToString(&binary_output)) {
    return "";
  }
  return binary_output;
}

bool PoseSerializer::Deserialize(const std::string &binary_buffer,
                                 InferenceResult &out_result) {
  pose::InferenceResultMsg proto_result;
  if (!proto_result.ParseFromString(binary_buffer)) {
    return false;
  }

  out_result.timestamp_ms = proto_result.timestamp_ms();
  out_result.detections.clear();
  out_result.detections.reserve(proto_result.detections_size());

  for (int i = 0; i < proto_result.detections_size(); ++i) {
    const auto &proto_det = proto_result.detections(i);
    Detection det;
    det.track_id = proto_det.track_id();
    det.confidence = proto_det.confidence();
    det.x_min = proto_det.x_min();
    det.y_min = proto_det.y_min();
    det.width = proto_det.width();
    det.height = proto_det.height();

    det.keypoints.reserve(proto_det.keypoints_size());
    for (int j = 0; j < proto_det.keypoints_size(); ++j) {
      const auto &proto_kp = proto_det.keypoints(j);
      Keypoint kp;
      kp.x = proto_kp.x();
      kp.y = proto_kp.y();
      kp.confidence = proto_kp.confidence();
      det.keypoints.push_back(kp);
    }

    out_result.detections.push_back(det);
  }

  return true;
}

} // namespace streaming
