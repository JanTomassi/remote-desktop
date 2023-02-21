#include <SDL.h>
#include <stdint.h>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/basic_streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/streambuf.hpp>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <opencv2/highgui.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <xdo.h>
}

#include "../include/protocol.hpp"

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

/**
 * @brief decode ffmpeg package in frame
 * @param[in]  *dec_ctx Context to send packet to decode
 * @param[out] *frame single image frame return from decoded packet
 * @param[in]  *pkt packet to decoded
 **/
void decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
  int                ret;
  static SwsContext *conversion;

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

    int     width  = frame->width;
    int     height = frame->height;
    cv::Mat image(height, width, CV_8UC3);
    int     cvLinesizes[1];

    cvLinesizes[0] = image.step1();
    if (conversion == NULL)
      conversion = sws_getContext(width, height, (AVPixelFormat)frame->format, width, height,
                                  AVPixelFormat::AV_PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data, cvLinesizes);

    cv::imshow("clientVideo", image);
    cv::waitKey(1);
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
    c->width     = VSIZEW;
    c->height    = VSIZEH;
    c->time_base = (AVRational){1, 30};
    c->framerate = (AVRational){30, 1};
    c->pix_fmt   = AV_PIX_FMT_YUV444P;

    frame->width  = VSIZEW;
    frame->height = VSIZEH;
    frame->format = AV_PIX_FMT_YUV444P;
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
    c->width     = width;
    c->height    = height;
    c->time_base = (AVRational){1, 30};
    c->framerate = (AVRational){30, 1};
    c->pix_fmt   = AV_PIX_FMT_YUV444P;

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
  cv::namedWindow("clientVideo", cv::WINDOW_FULLSCREEN | cv::WINDOW_GUI_NORMAL | cv::WINDOW_FREERATIO);
  av_log_set_level(AV_LOG_INFO);

  xdo_t *x = xdo_new(NULL);

  try {
    for (;;) {
      int mouse_x = 0, mouse_y = 0, mouse_screen_num = 0;
      xdo_get_mouse_location(x, &mouse_x, &mouse_y, &mouse_screen_num);
      std::cout << "x: " << mouse_x << "\ty: " << mouse_y << "\tsn: " << mouse_screen_num << std::endl;

      std::unique_ptr<boost::asio::streambuf> receive_buffer = std::make_unique<boost::asio::streambuf>();

      // Retrive 64 bytes header from socket
      std::function<void()> fReadHeader = [&]() { _EndpointContext.readHeader(*receive_buffer); };

      // Parsing header
      std::function<void()> fParseHeader = [&]() {
        _DecodeContext.header_data = parse_header(*receive_buffer);
        av_new_packet(_DecodeContext.pkt, _DecodeContext.header_data.image_size_bytes);
      };
      _EndpointContext.runIfNoError(fReadHeader, fParseHeader);

      // Retrive packet from socket
      std::function<void()> fReadPkt = [&]() {
        boost::asio::mutable_buffer packetData(_DecodeContext.pkt->data, _DecodeContext.header_data.image_size_bytes);

        _EndpointContext.readPacket(packetData, _DecodeContext.header_data.image_size_bytes);
      };

      // Decode AV packet
      std::function<void()> fDecodePkt = [&]() {
        decode_pkt(_DecodeContext.c, _DecodeContext.frame, _DecodeContext.pkt);
        av_packet_unref(_DecodeContext.pkt);
      };  //< Frunction to run on packet read success
      _EndpointContext.runIfNoError(fReadPkt, fDecodePkt);
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
