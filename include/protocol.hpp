#pragma once
#define port 3200
#define PKTSIZE 64
#define VSIZEW 1920
#define VSIZEH 1080

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

class videoThreadParams {
public:
  AVFrame *frame;
  AVPacket *pkt;
  AVCodecContext *ctx;
  videoThreadParams(const videoThreadParams& x){
    pkt = av_packet_clone(x.pkt);
    frame = av_frame_clone(x.frame);
    ctx = x.ctx;
  }
videoThreadParams(){
    frame = nullptr;
    pkt=nullptr;
    ctx=nullptr;
  }
};
