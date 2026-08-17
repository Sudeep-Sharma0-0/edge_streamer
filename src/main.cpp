#include "capture_module/VideoCapture.hpp"
#include "cli/Gluppy.hpp"
#include "config/AppConfig.hpp"
#include "processing/FrameSink.hpp"
#include "processing/HailoInferenceEngine.hpp"
#include "processing/SkeletonRenderer.hpp"
#include "streaming/StreamerFactory.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <thread>

std::atomic<bool> g_running(true);

void SignalHandler(int) { g_running = false; }

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
  if (app_config.inference.backend == config::InferenceBackend::HAILO_NPU) {
    inference_engine = std::make_unique<processing::HailoInferenceEngine>();
  } else {
  }

  if (!inference_engine->Initialize(app_config.inference)) {
    std::cerr << "[FATAL] Failed to initialize inference engine backend.\n";
    return -1;
  }

  auto streamer = streaming::StreamerFactory::Create(app_config);
  if (streamer && !streamer->Start()) {
    std::cerr << "[FATAL] Failed to start network streamer.\n";
    return -1;
  }

  capture::VideoCapture video_capture;
  if (!video_capture.Initialize(app_config.capture)) {
    std::cerr << "[FATAL] Failed to initialize video capture module.\n";
    return -1;
  }

  processing::FrameSink frame_sink(app_config.capture);
  frame_sink.SetFrameCallback([&](const processing::ImageFrame &frame) {
    processing::FrameBuffer buffer{.data = frame.pixel_data.data(),
                                   .size = frame.pixel_data.size(),
                                   .width = frame.width,
                                   .height = frame.height,
                                   .timestamp_ms = frame.timestamp_ms};

    processing::InferenceResult result = inference_engine->Infer(buffer);

    /*TODO
     * Later we have to add the capability to call another AI model here. Take
     * the points and frame provided by POSE detector AI and feed it to a Fall
     * detector AI.
     */
    if (!result.detections.empty()) {
      std::cout << "[PIPELINE] Detections: " << result.detections.size()
                << " | Inference Latency: " << result.inference_time_ms
                << " ms\n";
    }

    /*TODO
     * Later the following section will be change to only start the Streamer
     * when there is a fall detected. Before streaming there will also be a call
     * to the face masker and video encrypter.
     */
    if (app_config.visual.enable_overlay && streamer && streamer->IsRunning()) {
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

  std::cout << "[INFO] EdgeStreamer pipeline started. Press Ctrl+C to exit.\n";

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\n[INFO] Shutting down EdgeStreamer pipeline...\n";
  video_capture.Stop();
  frame_sink.Detach();
  if (streamer) {
    streamer->Stop();
  }

  std::cout << "[INFO] Clean exit completed.\n";
  return 0;
}
