#pragma once
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
}

#include "protocol.hpp"

class tcpServerAV {
 private:
  boost::asio::io_context                       io_context;
  boost::asio::ip::tcp::acceptor               *acceptor;
  std::unique_ptr<boost::asio::ip::tcp::socket> socket;
  // std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work;

  static tcpServerAV *instance;

  tcpServerAV() {
    acceptor = new boost::asio::ip::tcp::acceptor(io_context,
                                                  boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT_AV));

    socket = std::make_unique<boost::asio::ip::tcp::socket>(acceptor->accept(io_context));

    // work =
    // std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(boost::asio::make_work_guard(io_context));

    std::cout << "Success" << std::endl;
  }

 public:
  static tcpServerAV *getInstance() {
    if (instance == nullptr) {
      instance = new tcpServerAV();
    }
    return instance;
  }

  int  send_frame(videoThreadParams *video_param);
  void encode_send(videoThreadParams *video_param);
};
