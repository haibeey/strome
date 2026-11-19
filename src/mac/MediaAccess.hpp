#pragma once

#include <functional>

// Asks macOS for camera and/or microphone access, showing the system prompt
// the first time. `done` runs on an arbitrary thread with what was granted.
void RequestSystemMediaAccess(bool video, bool audio,
                              std::function<void(bool video_ok, bool audio_ok)> done);
