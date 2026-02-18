#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace pxm::server {

class ITransport {
public:
  using Handler = std::function<std::optional<std::string>(std::string_view)>;

  virtual ~ITransport() = default;
  virtual int run(Handler on_message) = 0;
};

} // namespace pxm::server
