#include "opencv2/highgui.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/completion_condition.hpp>
#include <iostream>
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
}

#include "protocol.hpp"

using boost::asio::ip::tcp;

// source https://www.programmerall.com/article/7721373696/
// source
// https://cppsecrets.com/users/14041151035752494957504952535764103109971051084699111109/Programming-in-C00-using-boostasio.php

struct image_metadata_t {
  int width;
  int height;
  size_t image_size_bytes;
};

image_metadata_t parse_header(boost::asio::streambuf &buffer) {

  std::string data_buff_str =
      std::string(boost::asio::buffer_cast<const char *>(buffer.data()));
  // cout << data_buff_str << endl;

  int x_pos = data_buff_str.find("x");
  int end_pos = data_buff_str.find(' ');
  int end_size = data_buff_str.find('e', end_pos);

  image_metadata_t meta_data;
  meta_data.width = std::stoi(data_buff_str.substr(0, x_pos));
  meta_data.height = std::stoi(data_buff_str.substr(x_pos + 1, end_pos));
  meta_data.image_size_bytes =
      std::stoi(data_buff_str.substr(end_pos + 1, end_size));

  return meta_data;
}

void decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
  int ret;
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

    int width = frame->width;
    int height = frame->height;
    cv::Mat image(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step1();
    if (conversion == NULL)
      conversion = sws_getContext(
          width, height, (AVPixelFormat)frame->format, width, height,
          AVPixelFormat::AV_PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data,
              cvLinesizes);

    cv::imshow("clientVideo", image);
    cv::waitKey(1);
  }
}

int main() {
  AVPacket *pkt = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  AVCodecContext *c = avcodec_alloc_context3(codec);
  image_metadata_t header_data;
  cv::namedWindow("clientVideo", cv::WINDOW_FULLSCREEN | cv::WINDOW_GUI_NORMAL |
                                     cv::WINDOW_FREERATIO);
  av_log_set_level(AV_LOG_INFO);

  c->width = VSIZEW;
  c->height = VSIZEH;
  c->time_base = (AVRational){1, 30};
  c->framerate = (AVRational){30, 1};
  c->pix_fmt = AV_PIX_FMT_YUV444P;

  frame->width = VSIZEW;
  frame->height = VSIZEH;
  frame->format = AV_PIX_FMT_YUV444P;
  frame->pts = 0;

  if (avcodec_open2(c, codec, NULL) < 0) {
    fprintf(stderr, "Could not open codec\n");
    exit(1);
  }

  try {
    boost::asio::io_service io_service;
    tcp::endpoint end_point(
        boost::asio::ip::address::from_string("192.168.0.1"), 3200);
    tcp::socket socket(io_service);
    socket.connect(end_point);
    boost::system::error_code ignored_error;
    // cv::namedWindow("client", cv::WINDOW_NORMAL | cv::WINDOW_GUI_NORMAL);

    for (;;) {
      boost::asio::streambuf receive_buffer;
      // Now we retrieve the message header of 64 bytes
      size_t header_size = 64;
      boost::asio::read(socket, receive_buffer,
                        boost::asio::transfer_exactly(header_size),
                        ignored_error);

      if (ignored_error && ignored_error != boost::asio::error::eof) {
        std::cout << "first receive failed: " << ignored_error.message()
                  << std::endl;
      } else {
        header_data = parse_header(receive_buffer);
        av_new_packet(pkt, header_data.image_size_bytes);

        auto recv = boost::asio::read(
            socket,
            boost::asio::buffer(pkt->data, header_data.image_size_bytes),
            boost::asio::transfer_exactly(header_data.image_size_bytes),
            ignored_error);
        // std::cout << "recv/expeted: " << recv << " / "
        //           << header_data.image_size_bytes << " -> "
        //           << (float)recv / header_data.image_size_bytes << std::endl;
        decode_pkt(c, frame, pkt);
        av_packet_unref(pkt);
      }
    }
  }

  catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
