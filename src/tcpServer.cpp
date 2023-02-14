#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/error.hpp>
#include <boost/range.hpp>
#include <boost/thread/thread.hpp>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "../include/protocol.hpp"
#include "../include/tcpServer.hpp"

tcpServer *tcpServer::instance = nullptr;

std::deque<boost::asio::mutable_buffer> send_buff;

void tcpServer::encode_send(videoThreadParams *video_param) {
  int ret;

  ret = avcodec_send_frame(video_param->ctx, video_param->frame);
  if (ret < 0) {
    fprintf(stderr, "Error sending a frame for encoding\n");
    exit(1);
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(video_param->ctx, video_param->pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return;
    else if (ret < 0) {
      fprintf(stderr, "Error during encoding\n");
      exit(1);
    }

    tcpServer::send_frame(video_param);
    av_packet_unref(video_param->pkt);
  }
}

int tcpServer::send_frame(videoThreadParams *video_param) {
  std::stringstream header_stream;

  header_stream << video_param->frame->width << 'x'
                << video_param->frame->height << " "
                << std::to_string(video_param->pkt->buf->size) << 'e'
                << std::endl;

  std::flush(header_stream);

  std::string header = header_stream.str();
  header.append(PKTSIZE - header.size(), '0');

  boost::system::error_code ignored_error;

  auto send = std::make_unique<std::array<boost::asio::const_buffer, 2>>(
      std::array<boost::asio::const_buffer, 2>{
          boost::asio::buffer(header, PKTSIZE),
          boost::asio::buffer(video_param->pkt->buf->data,
                              video_param->pkt->buf->size)});

  // io_context.restart();
  // boost::asio::async_write(
  //     *socket, *send, [](boost::system::error_code ec, std::size_t) {
  //       if (ec)
  //         std::cout << "Lambda Send: " << ec.message() << std::endl;
  //     });
  // try {
  //   io_context.run();
  // } catch (std::exception e) {
  //   std::cout << e.what() << std::endl;
  // }

  boost::asio::write(*socket, *send, ignored_error);
  return 0;
}
