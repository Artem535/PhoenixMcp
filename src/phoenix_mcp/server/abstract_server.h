//
// Created by artem.d on 09.11.2025.
//

#pragma once
#include <string>
#include "../constants/constants.hpp"

namespace cnt = pxm::constants;
namespace cnt_t = pxm::constants::transport;


namespace pxm::server {
class AbstractServer {
public:
  AbstractServer(std::string name, std::string desc,
                 const cnt_t::TransportType& transport =
                     cnt_t::TransportType::Stdio);

  virtual ~AbstractServer() = default;

  int start_server();

private:
  std::string name_;
  std::string desc_;
  cnt_t::TransportType transport_;

  virtual void start_server_();
};
};