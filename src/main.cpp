#include "capture_module/VideoCapture.hpp"
#include "cli/Gluppy.hpp"
#include "config/AppConfig.hpp"
#include "processing/FrameSink.hpp"
#include "processing/HailoInferenceEngine.hpp"
#include "processing/SkeletonRenderer.hpp"
#include "processing/TappasInferenceEngine.hpp"
#include "streaming/Serializer.hpp"
#include "streaming/StreamerFactory.hpp"
#include "streaming/TCPServer.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <thread>

std::atomic<bool> g_running(true);

void SignalHandler(int) { g_running = false; }

static std::string BackendToString(config::InferenceBackend backend) {
  switch (backend) {
  case config::InferenceBackend::NATIVE_HAILO:
    return "NATIVE_HAILO";
  case config::InferenceBackend::TAPPAS:
    return "TAPPAS";
  case config::InferenceBackend::MOCK:
    return "MOCK";
  default:
    return "UNKNOWN";
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  config::AppConfig app_config = config::AppConfig::Default();
  cli::Gluppy cli_parser(argc, argv);

  cli::Gluppy::ParseResult parse_res = cli_parser.Parse(app_config);
  if (parse_res == cli::Gluppy::ParseResult::HELP_REQUESTED) {
    cli_parser.PrintHelp(argv[0]);
    return 0;
  }
  if (parse_res == cli::Gluppy::ParseResult::ERROR) {
    return -1;
  }

  std::cout << app_config.ToString() << std::endl;

  std::unique_ptr<processing::IInferenceEngine> inference_engine;
  switch (app_config.inference.backend) {
  case config::InferenceBackend::NATIVE_HAILO:
    std::cout << "[INFO] Initializing Native Hailo RT C++ backend...\n";
    inference_engine = std::make_unique<processing::HailoInferenceEngine>();
    break;
  case config::InferenceBackend::TAPPAS:
    std::cout << "[INFO] Initializing GStreamer TAPPAS plugin backend...\n";
    inference_engine = std::make_unique<processing::TappasInferenceEngine>();
    break;
  case config::InferenceBackend::MOCK:
    std::cerr << "[FATAL] Mock backend selected but not configured.\n";
    return -1;
  }

  if (!inference_engine ||
      !inference_engine->Initialize(app_config.inference)) {
    std::cerr << "[FATAL] Failed to initialize inference engine backend ("
              << BackendToString(app_config.inference.backend) << ").\n";
    return -1;
  }

  auto streamer = streaming::StreamerFactory::Create(app_config);
  if (streamer && !streamer->Start()) {
    std::cerr << "[FATAL] Failed to start network streamer.\n";
    return -1;
  }

  streaming::TcpProtoServer proto_server;
  if (!proto_server.Start(8080)) {
    std::cerr << "[FATAL] Failed to start Protobuf TCP server on port 8080.\n";
    return -1;
  }
  std::cout << "[INFO] Protobuf TCP server listening on port 8080...\n";

  capture::VideoCapture video_capture;
  if (!video_capture.Initialize(app_config.capture)) {
    std::cerr << "[FATAL] Failed to initialize video capture module.\n";
    return -1;
  }

  auto pipeline_start_time = std::chrono::steady_clock::now();

  static int frame_counter = 0;
  static float accum_cam_delay = 0.0f;
  static float accum_post_kpt = 0.0f;
  static float accum_npu = 0.0f;
  static float accum_cpu = 0.0f;
  static float accum_render = 0.0f;
  static float accum_post_total = 0.0f;
  static float accum_e2e = 0.0f;
  static size_t accum_detections = 0;
  static auto window_start_time = std::chrono::steady_clock::now();

  processing::FrameSink frame_sink(app_config.capture);
  frame_sink.SetFrameCallback([&](const processing::ImageFrame &frame) {
    auto t_callback_start = std::chrono::steady_clock::now();

    if (frame_counter == 0) {
      window_start_time = t_callback_start;
    }

    auto steady_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             t_callback_start - pipeline_start_time)
                             .count();

    float camera_capture_delay_ms = 0.0f;
    if (frame.timestamp_ms > 0 && steady_now_ms >= frame.timestamp_ms) {
      camera_capture_delay_ms =
          static_cast<float>(steady_now_ms - frame.timestamp_ms);
    }

    processing::FrameBuffer buffer{.data = frame.pixel_data.data(),
                                   .size = frame.pixel_data.size(),
                                   .width = frame.width,
                                   .height = frame.height,
                                   .timestamp_ms = frame.timestamp_ms};

    processing::InferenceResult result = inference_engine->Infer(buffer);
    auto t_keypoints_ready = std::chrono::steady_clock::now();

    auto t_proto_start = std::chrono::steady_clock::now();

    streaming::InferenceResult pose_res;
    pose_res.timestamp_ms = frame.timestamp_ms;
    for (const auto &det : result.detections) {
      streaming::Detection p_det;
      p_det.track_id = -1;
      p_det.confidence = det.confidence;
      p_det.x_min = det.x_min;
      p_det.y_min = det.y_min;
      p_det.width = det.width;
      p_det.height = det.height;

      for (const auto &kp : det.keypoints) {
        p_det.keypoints.push_back({kp.x, kp.y, kp.confidence});
      }
      pose_res.detections.push_back(p_det);
    }

    std::string proto_payload = streaming::PoseSerializer::Serialize(pose_res);
    proto_server.Broadcast(proto_payload);

    float post_capture_to_keypoints_ms =
        std::chrono::duration<float, std::milli>(t_keypoints_ready -
                                                 t_callback_start)
            .count();

    float render_and_stream_ms = 0.0f;

    if (app_config.visual.enable_overlay && streamer && streamer->IsRunning()) {
      auto t_render_start = std::chrono::steady_clock::now();
      cv::Mat frame_to_render;

      if (app_config.visual.skeleton_only) {
        frame_to_render = cv::Mat::zeros(frame.height, frame.width, CV_8UC3);
      } else {
        cv::Mat frame_rgb(frame.height, frame.width, CV_8UC3,
                          const_cast<uint8_t *>(frame.pixel_data.data()));
        cv::cvtColor(frame_rgb, frame_to_render, cv::COLOR_RGB2BGR);
      }

      processing::SkeletonRenderer::Render(
          frame_to_render, result, app_config.visual,
          app_config.inference.keypoint_threshold);

      streamer->SendFrame(frame_to_render);

      auto t_stream_end = std::chrono::steady_clock::now();
      render_and_stream_ms = std::chrono::duration<float, std::milli>(
                                 t_stream_end - t_render_start)
                                 .count();
    }

    auto t_callback_end = std::chrono::steady_clock::now();

    float total_post_capture_ms = std::chrono::duration<float, std::milli>(
                                      t_callback_end - t_callback_start)
                                      .count();

    float total_end_to_end_ms = camera_capture_delay_ms + total_post_capture_ms;

    accum_cam_delay += camera_capture_delay_ms;
    accum_post_kpt += post_capture_to_keypoints_ms;
    accum_npu += result.npu_time_ms;
    accum_cpu += result.postprocess_time_ms;
    accum_render += render_and_stream_ms;
    accum_post_total += total_post_capture_ms;
    accum_e2e += total_end_to_end_ms;
    accum_detections += result.detections.size();
    frame_counter++;

    if (frame_counter >= 30) {
      auto window_end_time = std::chrono::steady_clock::now();
      float window_duration_sec =
          std::chrono::duration<float>(window_end_time - window_start_time)
              .count();
      float effective_fps =
          (window_duration_sec > 0.0f) ? (30.0f / window_duration_sec) : 0.0f;

      std::cout << std::fixed << std::setprecision(2);
      std::cout << "\n┌──────────────────[ BENCHMARK: "
                << BackendToString(app_config.inference.backend)
                << " (30-FRAME AVG) ]──────────────────┐\n"
                << " Throughput (FPS):             " << effective_fps
                << " FPS\n"
                << " Avg Detections Found:         "
                << (static_cast<float>(accum_detections) / 30.0f) << "\n"
                << " 1. Camera Driver Delay:       "
                << (accum_cam_delay / 30.0f) << " ms\n"
                << " 2. Post-Capture to Keypoints: " << (accum_post_kpt / 30.0f)
                << " ms\n"
                << "    ├── NPU Hardware Latency:  " << (accum_npu / 30.0f)
                << " ms\n"
                << "    └── CPU Postprocess Time:  " << (accum_cpu / 30.0f)
                << " ms\n"
                << " 3. Render & Stream Latency:   " << (accum_render / 30.0f)
                << " ms\n"
                << " ----------------------------------------------------------"
                   "--------------\n"
                << " [METRIC] Camera -> Keypoints: "
                << ((accum_cam_delay + accum_post_kpt) / 30.0f) << " ms\n"
                << " [METRIC] Post-Capture Total:  "
                << (accum_post_total / 30.0f) << " ms\n"
                << " [METRIC] Total End-to-End:    " << (accum_e2e / 30.0f)
                << " ms\n"
                << "==========================================================="
                   "==============\n";

      frame_counter = 0;
      accum_cam_delay = 0.0f;
      accum_post_kpt = 0.0f;
      accum_npu = 0.0f;
      accum_cpu = 0.0f;
      accum_render = 0.0f;
      accum_post_total = 0.0f;
      accum_e2e = 0.0f;
      accum_detections = 0;
    }
  });

  if (!frame_sink.AttachToAppSink(video_capture.GetAppSinkElement())) {
    std::cerr << "[FATAL] Failed to attach FrameSink to appsink.\n";
    return -1;
  }

  if (!video_capture.Start()) {
    std::cerr << "[FATAL] Failed to start video capture pipeline.\n";
    return -1;
  }

  std::cout << "[INFO] EdgeStreamer pipeline started running on backend ["
            << BackendToString(app_config.inference.backend)
            << "]. Press Ctrl+C to exit.\n";

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\n[INFO] Shutting down EdgeStreamer pipeline...\n";
  video_capture.Stop();
  frame_sink.Detach();
  if (streamer) {
    streamer->Stop();
  }

  std::cout << "\n[INFO] Shutting down EdgeStreamer pipeline...\n";
  proto_server.Stop();
  video_capture.Stop();
  frame_sink.Detach();

  std::cout << "[INFO] Clean exit completed.\n";
  return 0;
}
