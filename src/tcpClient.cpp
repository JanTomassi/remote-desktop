#include <SDL.h>
#include <SDL3/SDL.h>
#include <stdint.h>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/thread.hpp>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_surface.h"
#include "SDL_video.h"

extern "C" {
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <xdo.h>
}

#include "../include/protocol.hpp"

struct client_SDL {
  SDL_Window   *window        = NULL;
  SDL_Surface  *screenSurface = NULL;
  SDL_Renderer *renderer      = NULL;
  SDL_Texture  *bmp           = NULL;
  SDL_Surface  *surf          = NULL;
  SwsContext   *conversion    = NULL;
};

client_SDL client_SDL;

uint64_t timeing() {
  struct timeval tv;

  gettimeofday(&tv, NULL);

  return ((uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec) / 1000;
}

/**
 * @brief parse the input stream
 * @param[in] &buffer this is a buffer containing width, hight and
 * byte size this part si always 64 byte max. It is followed by the ffmpeg
 * package
 * @return this struct conain width, height and image size
 **/
image_metadata_t parse_header(boost::asio::streambuf &buffer) {
  std::string data_buff_str = std::string(boost::asio::buffer_cast<const char *>(buffer.data()));

  int x_pos    = data_buff_str.find("x");
  int end_pos  = data_buff_str.find(' ');
  int end_size = data_buff_str.find('e', end_pos);

  image_metadata_t meta_data;
  meta_data.width            = std::stoi(data_buff_str.substr(0, x_pos));
  meta_data.height           = std::stoi(data_buff_str.substr(x_pos + 1, end_pos));
  meta_data.image_size_bytes = std::stoi(data_buff_str.substr(end_pos + 1, end_size));

  return meta_data;
}

bool init_show() {
  // Initialization flag
  bool success = true;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    success = false;
  } else {
    int res;
    // Create window
    res = SDL_CreateWindowAndRenderer(1920, 1080, SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_RESIZABLE, &client_SDL.window,
                                      &client_SDL.renderer);
    if (res < 0) {
      printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
      success = false;
    } else {
      // Get window surface
      client_SDL.bmp  = SDL_CreateTexture(client_SDL.renderer, SDL_PIXELFORMAT_BGR24,
                                          SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, 1920, 1080);
      client_SDL.surf = SDL_GetWindowSurface(client_SDL.window);
      SDL_SetSurfaceRLE(client_SDL.surf, 1);
    }
  }

  return success;
}

/**
 * @brief decode ffmpeg package in frame
 * @param[in]  *dec_ctx Context to send packet to decode
 * @param[out] *frame single image frame return from decoded packet
 * @param[in]  *pkt packet to decoded
 **/
void decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
  int ret;

  ret = avcodec_send_packet(dec_ctx, pkt);
  if (ret < 0) {
    fprintf(stderr, "Error sending a packet for decoding\n");
    exit(1);
  }

  while (ret >= 0) {
    ret = avcodec_receive_frame(dec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0) {
      fprintf(stderr, "Error during decoding\n");
      exit(1);
    }

    int width  = frame->width;
    int height = frame->height;

    if (client_SDL.conversion == NULL)
      client_SDL.conversion = sws_getContext(width, height, (AVPixelFormat)frame->format, width, height,
                                             AVPixelFormat::AV_PIX_FMT_RGB32, SWS_POINT, NULL, NULL, NULL);

    AVFrame *bgrFrame = av_frame_alloc();

    av_image_alloc(bgrFrame->data, bgrFrame->linesize, width, height, AVPixelFormat::AV_PIX_FMT_RGB32, 1);

    SDL_LockSurface(client_SDL.surf);
    sws_scale(client_SDL.conversion, frame->data, frame->linesize, 0, height,
              (uint8_t *const *)&client_SDL.surf->pixels, &client_SDL.surf->pitch);
    SDL_UnlockSurface(client_SDL.surf);

    SDL_UpdateWindowSurface(client_SDL.window);
  }
}

void th_send_xdo() {
  xdo_t *x       = xdo_new(NULL);
  int    mouse_x = 0, mouse_y = 0, mouse_screen_num = 0;

  xdo_get_mouse_location(x, &mouse_x, &mouse_y, &mouse_screen_num);

  for (;;) {
    xdo_wait_for_mouse_move_from(x, mouse_x, mouse_y);
    xdo_get_mouse_location(x, &mouse_x, &mouse_y, &mouse_screen_num);
    std::cout << "x: " << mouse_x << "\ty: " << mouse_y << "\tsn: " << mouse_screen_num << std::endl;
  }
}

/**
 * @brief struct to manage ffmpeg decode context
 **/
struct _Decode {
  AVPacket        *pkt   = av_packet_alloc();
  AVFrame         *frame = av_frame_alloc();
  const AVCodec   *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  AVCodecContext  *c     = avcodec_alloc_context3(codec);
  image_metadata_t header_data;

  /**
   * @brief Consturctor with macro defined width and height */
  _Decode() {
    c->width        = VSIZEW;
    c->height       = VSIZEH;
    c->framerate    = (AVRational){15, 1};
    c->thread_count = 4;
    c->thread_type  = FF_THREAD_SLICE;
    // av_opt_set(c->priv_data, "preset", "ultrafast", 0);
    // av_opt_set(c->priv_data, "tune", "zerolatency", 0);

    frame->width  = VSIZEW;
    frame->height = VSIZEH;
    frame->format = AV_PIX_FMT_YUV422P;
    frame->pts    = 0;

    if (avcodec_open2(c, codec, NULL) < 0) {
      fprintf(stderr, "Could not open codec\n");
      exit(1);
    }
  }
  /**
   * @brief Constructor with parameterized width and height
   * @param width of the receved frame
   * @param height of the receved frame
   **/
  _Decode(uint16_t width, uint16_t height) {
    c->width        = width;
    c->height       = height;
    c->thread_count = 8;
    c->thread_type  = FF_THREAD_FRAME;
    // c->time_base = (AVRational){1, 60};
    // c->framerate = (AVRational){60, 1};

    frame->width  = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV444P;
    frame->pts    = 0;
  }
  _Decode(const _Decode &)            = default;
  _Decode(_Decode &&)                 = default;
  _Decode &operator=(const _Decode &) = default;
  _Decode &operator=(_Decode &&)      = default;
};

/**
 * @brief stuct to manage tcp endpoint */
class _Endpoint {
 public:
  boost::asio::io_service        io_service;
  boost::asio::ip::tcp::endpoint end_point;
  boost::asio::ip::tcp::socket   socket;

  /**
   * @brief wrapper of asio read for header
   * @param[out] rReceiveBuffer buffer where to write data */
  void readHeader(boost::asio::streambuf &rReceiveBuffer) {
    size_t header_size = 64;
    boost::asio::read(socket, rReceiveBuffer, boost::asio::transfer_exactly(header_size), mTcpError);
  }

  /**
   * @brief wrapper of asio read for packet
   * @param[out] rPacket asio::buffer pointing to ffmpeg packet data
   * @param[in] byteSize how big is the rPacket in bytes */
  void readPacket(boost::asio::mutable_buffer &rPacket, size_t byteSize) {
    boost::asio::read(socket, rPacket, boost::asio::transfer_exactly(byteSize), mTcpError);
  }
  /**
   * @brief run read funtion then test if the asio read return any error and
   * then run parser function
   * @param[in] read lambda function that wrap the asio read function
   * @param[in] parser lambda function for parsing/decoding read data */
  void runIfNoError(std::function<void()> read, std::function<void()> parser) {
    read();
    if (mTcpError && (mTcpError != boost::asio::error::eof)) {
      std::cout << "first receive failed: " << mTcpError.message() << std::endl;
    } else {
      parser();
    }
  }

  _Endpoint() : socket(io_service) {
    end_point = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("192.168.0.1"), 3200);
    socket.connect(end_point);
  };
  _Endpoint(const _Endpoint &)            = delete;
  _Endpoint(_Endpoint &&)                 = delete;
  _Endpoint &operator=(const _Endpoint &) = delete;
  _Endpoint &operator=(_Endpoint &&)      = delete;

 private:
  boost::system::error_code mTcpError;
};

int main() {
  _Decode   _DecodeContext;
  _Endpoint _EndpointContext;

  av_log_set_level(AV_LOG_INFO);

  // boost::thread xdo_client(th_send_xdo);

  try {
    init_show();
    for (;;) {
      std::unique_ptr<boost::asio::streambuf> receive_buffer = std::make_unique<boost::asio::streambuf>();

      // Retrive 64 bytes header from socket
      // std::function<void()> fReadHeader = [&]() {
      _EndpointContext.readHeader(*receive_buffer);
      //};

      // Parsing header
      // std::function<void()> fParseHeader = [&]() {
      _DecodeContext.header_data = parse_header(*receive_buffer);
      av_new_packet(_DecodeContext.pkt, _DecodeContext.header_data.image_size_bytes);
      //};
      //_EndpointContext.runIfNoError(fReadHeader, fParseHeader);

      // Retrive packet from socket
      // std::function<void()> fReadPkt = [&]() {
      boost::asio::mutable_buffer packetData(_DecodeContext.pkt->data, _DecodeContext.header_data.image_size_bytes);

      _EndpointContext.readPacket(packetData, _DecodeContext.header_data.image_size_bytes);
      //};

      // Decode AV packet
      // std::function<void()> fDecodePkt = [&]() {
      decode_pkt(_DecodeContext.c, _DecodeContext.frame, _DecodeContext.pkt);
      av_packet_unref(_DecodeContext.pkt);
      // };  //< Frunction to run on packet read success
      // _EndpointContext.runIfNoError(fReadPkt, fDecodePkt);
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
