#import <AVFoundation/AVFoundation.h>

#include "MediaAccess.hpp"

static void Request(AVMediaType type, bool wanted, void (^next)(BOOL)) {
  if (!wanted) {
    next(NO);
    return;
  }
  [AVCaptureDevice requestAccessForMediaType:type completionHandler:next];
}

void RequestSystemMediaAccess(bool video, bool audio,
                              std::function<void(bool, bool)> done) {
  // Ask one after the other so macOS shows the two prompts in turn.
  Request(AVMediaTypeVideo, video, ^(BOOL video_ok) {
    Request(AVMediaTypeAudio, audio, ^(BOOL audio_ok) {
      done(video_ok, audio_ok);
    });
  });
}
