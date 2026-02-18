//
// Created by artem.d on 09.11.2025.
//
#pragma once

#include "i_transport.h"

namespace pxm::server {
class StdioTransport final : public ITransport {
public:
  int run(Handler on_message) override;
};
} // namespace pxm::server
