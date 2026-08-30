#include "cli/Gluppy.hpp"
#include "config/AppConfig.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace cli {

Gluppy::Gluppy(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    args_.emplace_back(argv[i]);
  }
}

bool Gluppy::HasFlag(const std::string &flag, const std::string &alt) const {
  for (const auto &arg : args_) {
    if (arg == flag || (!alt.empty() && arg == alt)) {
      return true;
    }
  }
  return false;
}

std::string Gluppy::GetOption(const std::string &flag, const std::string &alt,
                              const std::string &default_val) const {
  for (size_t i = 0; i < args_.size(); ++i) {
    if (args_[i] == flag || (!alt.empty() && args_[i] == alt)) {
      if (i + 1 < args_.size() && args_[i + 1][0] != '-') {
        return args_[i + 1];
      }
    }
  }
  return default_val;
}

Gluppy::ParseResult Gluppy::Parse(config::AppConfig &config) {
  if (HasFlag("-h", "--help")) {
    return ParseResult::HELP_REQUESTED;
  }

  try {
    config.capture.device = GetOption("-d", "--device", config.capture.device);
    config.capture.width = std::stoi(
        GetOption("-W", "--width", std::to_string(config.capture.width)));
    config.capture.height = std::stoi(
        GetOption("-H", "--height", std::to_string(config.capture.height)));
    config.capture.fps =
        std::stoi(GetOption("-f", "--fps", std::to_string(config.capture.fps)));
    config.capture.type = GetOption("-t", "--type", config.capture.type);
    if (config.capture.type != "camera") {
      config.capture.location =
          GetOption("-L", "--location", config.capture.location);
    }

    std::string backend_opt = GetOption("-B", "--backend", "native");
    if (backend_opt == "tappas") {
      config.inference.backend = config::InferenceBackend::TAPPAS;
    } else if (backend_opt == "native" || backend_opt == "hailo") {
      config.inference.backend = config::InferenceBackend::NATIVE_HAILO;
    } else if (backend_opt == "mock") {
      config.inference.backend = config::InferenceBackend::MOCK;
    } else {
      std::cerr << "[CLI ERROR] Unknown backend option '" << backend_opt
                << "'. Supported: native, tappas, mock\n";
      return ParseResult::ERROR;
    }

    config.inference.model_path =
        GetOption("-m", "--model", config.inference.model_path);
    config.inference.tappas_postprocess_so =
        GetOption("--tappas-so", "", config.inference.tappas_postprocess_so);
    config.inference.confidence_threshold = std::stof(GetOption(
        "-c", "--conf", std::to_string(config.inference.confidence_threshold)));
    config.inference.iou_threshold = std::stof(GetOption(
        "-i", "--iou", std::to_string(config.inference.iou_threshold)));
    config.inference.keypoint_threshold = std::stof(
        GetOption("-k", "--kp-conf",
                  std::to_string(config.inference.keypoint_threshold)));
    config.inference.enabled_scales = static_cast<uint8_t>(std::stof(GetOption(
        "-Es", "--scales",
        std::to_string(static_cast<int>(config.inference.enabled_scales)))));

    std::string proto = GetOption("-s", "--stream", "mjpeg");
    if (proto == "udp") {
      config.streaming.protocol = config::StreamProtocol::UDP_RTP;
    } else if (proto == "mjpeg" || proto == "http") {
      config.streaming.protocol = config::StreamProtocol::HTTP_MJPEG;
    } else if (proto == "none") {
      config.streaming.protocol = config::StreamProtocol::NONE;
      config.streaming.enabled = false;
    }

    config.streaming.host = GetOption("--host", "", config.streaming.host);
    config.streaming.port = std::stoi(
        GetOption("-p", "--port", std::to_string(config.streaming.port)));
    config.streaming.bitrate_kbps = std::stoi(GetOption(
        "-b", "--bitrate", std::to_string(config.streaming.bitrate_kbps)));
    config.streaming.jpeg_quality = std::stoi(GetOption(
        "-q", "--quality", std::to_string(config.streaming.jpeg_quality)));

    config.visual.skeleton_only = HasFlag("--skeleton-only", "-S");

  } catch (const std::exception &e) {
    std::cerr << "[CLI ERROR] Failed to parse numeric arguments: " << e.what()
              << "\n";
    return ParseResult::ERROR;
  }

  std::vector<std::string> validation_errors;
  if (!config.Validate(validation_errors)) {
    std::cerr << "[CONFIG ERROR] Invalid configuration parameters:\n";
    for (const auto &err : validation_errors) {
      std::cerr << "  * " << err << "\n";
    }
    return ParseResult::ERROR;
  }

  return ParseResult::SUCCESS;
}

void Gluppy::PrintHelp(const std::string &binary_name) const {
  std::cout << "\n┌───────────────────────[ EDGE STREAMER HELP "
               "]───────────────────────┐\n";
  std::cout << "Usage:\n";
  std::cout << "  " << binary_name << " [options]\n\n";

  std::cout << "Capture Options:\n";
  std::cout << "  -d, --device PATH    V4L2 capture device path (default: "
               "/dev/video0)\n";
  std::cout << "  -W, --width INT      Capture frame width (default: 640)\n";
  std::cout << "  -H, --height INT     Capture frame height (default: 640)\n";
  std::cout
      << "  -f, --fps INT        Target capture frame rate (default: 30)\n";
  std::cout << "  -t, --type TYPE      Type of capture source: 'camera', "
               "'file' (default: camera)\n";
  std::cout << "  -L, --location PATH  Path to video file if type=file "
               "(default: 'videos/test.mp4')\n\n";

  std::cout << "Inference Options:\n";
  std::cout << "  -B, --backend TYPE   Inference backend: 'native', 'tappas', "
               "or 'mock' (default: native)\n";
  std::cout << "  -m, --model PATH     Path to Hailo compiled HEF model "
               "(default: models/yolov8m_pose.hef)\n";
  std::cout
      << "      --tappas-so PATH Path to TAPPAS postprocess shared object\n";
  std::cout << "  -c, --conf FLOAT     Detection confidence threshold "
               "(default: 0.50)\n";
  std::cout << "  -i, --iou FLOAT      NMS IoU threshold (default: 0.45)\n";
  std::cout << "  -k, --kp-conf FLOAT  Keypoint confidence threshold (default: "
               "0.40)\n";
  std::cout << "  -Es, --scales INT    Enabled scale mask (1 through 7)\n\n";

  std::cout << "Streaming Options:\n";
  std::cout << "  -s, --stream TYPE    Protocol: 'mjpeg', 'udp', or 'none' "
               "(default: mjpeg)\n";
  std::cout << "      --host IP        Target host IP for UDP streaming "
               "(default: 127.0.0.1)\n";
  std::cout
      << "  -p, --port INT       Streaming network port (default: 5777)\n";
  std::cout << "  -b, --bitrate INT    Encoder bitrate in kbps for H.264 "
               "(default: 2000)\n";
  std::cout << "  -q, --quality INT    JPEG compression quality 1-100 "
               "(default: 75)\n\n";

  std::cout << "Visualization Options:\n";
  std::cout << "  -S, --skeleton-only  Render skeleton on black canvas\n\n";

  std::cout << "General Options:\n";
  std::cout << "  -h, --help           Display this help manual\n";
}

} // namespace cli
