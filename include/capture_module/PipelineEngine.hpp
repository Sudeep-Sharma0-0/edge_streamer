#pragma once

#include "config/AppConfig.hpp"
#include <gst/gst.h>
#include <string>

namespace capture {

class PipelineEngine {
public:
  explicit PipelineEngine(const config::CaptureConfig &config);
  ~PipelineEngine();

  bool BuildPipeline();
  bool SetState(GstState state);

  GstElement *GetPipeline() const { return pipeline_; }
  GstElement *GetAppSink() const { return appsink_; }

  std::string GenerateCameraString() const;
  std::string GenerateVideoString() const;

private:
  config::CaptureConfig config_;
  GstElement *pipeline_{nullptr};
  GstElement *appsink_{nullptr};
};

} // namespace capture
