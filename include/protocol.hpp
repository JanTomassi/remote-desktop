#pragma once
//! @brief tcp socket port for AV packet
#include <cstdint>
#define PORT_AV 3200
#define PORT_XDO 3201
#define PKTSIZE 64
#define VSIZEW 1920
#define VSIZEH 1080

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>

#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
}

class videoThreadParams {
 public:
  AVFrame        *frame;
  AVPacket       *pkt;
  AVCodecContext *ctx;
  videoThreadParams(const videoThreadParams &x) {
    pkt   = av_packet_clone(x.pkt);
    frame = av_frame_clone(x.frame);
    ctx   = x.ctx;
  }
  videoThreadParams() {
    frame = nullptr;
    pkt   = nullptr;
    ctx   = nullptr;
  }
};

struct image_metadata_t {
  int    width            = 0;
  int    height           = 0;
  size_t image_size_bytes = 0;
};

typedef struct mouse {
  uint16_t x = 0;
  uint16_t y = 0;
  enum key { LEFT, MID, RIGHT, UP, DOWN };
  key key;
} mouse_t;
typedef struct keyboard {
  bool ctrl  = false;
  bool shift = false;
  bool alt   = false;
  bool mod   = false;
  char key   = false;
} keyboard_t;
struct xdo_packet_t {
  mouse_t    mouse;
  keyboard_t key;
};
