//
// Created by artem.d on 09.11.2025.
//
#pragma once
#include <spdlog/spdlog.h>
#include "abstract_transport.h"

namespace pxm::server {
class StdioTransport final : public AbstractTransport {
public:
  std::string read_msg() override;

  void write_msg(const std::string& msg) override;
};
}