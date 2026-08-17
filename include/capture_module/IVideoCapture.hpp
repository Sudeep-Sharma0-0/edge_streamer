#pragma once

#include <gst/gst.h>

namespace capture {
class IVideoCapture {
public:
  IVideoCapture() = default;
  virtual ~IVideoCapture() = default;
  virtual bool StartCapturePass(GstElement *pipeline,
                                GstElement *next_element) = 0;
};
} // namespace capture
