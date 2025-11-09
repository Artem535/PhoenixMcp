#pragma once

#include <string>
#include "abstract_server.h"
#include "../constants/constants.hpp"

namespace cnt = pxm::constants;
namespace cnt_t = pxm::constants::transport;

namespace pxm::server {


class StdioServer final : public AbstractServer {
public:
  StdioServer(const std::string& name, const std::string& description);

private:
  void start_server_() override;
};

} // namespace pxm::server