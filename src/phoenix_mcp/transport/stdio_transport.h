//
// Created by artem.d on 09.11.2025.
//
#pragma once
#include "abstract_transport.h"

class StdioTransport final : public AbstractTransport {
public:
  std::string read_msg() override;

  void write_msg(const std::string& msg) override;
};