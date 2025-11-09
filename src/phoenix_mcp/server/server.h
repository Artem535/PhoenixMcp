//
// Created by artem.d on 09.11.2025.
//

#pragma once
#include <memory>
#include <string>
#include "../constants/constants.hpp"
#include "../transport/abstract_transport.h"

namespace cnt = pxm::constants;
namespace cnt_t = pxm::constants::transport;


namespace pxm::server {
/**
 * @brief Main server class that handles MCP protocol communication
 *
 * The Server class is responsible for initializing and running the MCP server,
 * managing transport connections, and processing incoming requests.
 */
class Server {
public:
  /**
   * @brief Construct a new Server object
   *
   * @param name Server name for identification
   * @param desc Server description
   * @param transport Unique pointer to transport implementation for communication
   */
  Server(std::string name, std::string desc,
         std::unique_ptr<AbstractTransport> transport);

  /**
   * @brief Start the server and begin processing requests
   *
   * Initializes the server and starts listening for incoming messages
   * through the configured transport mechanism.
   *
   * @return int Exit code (0 for success, non-zero for error)
   */
  int start_server();

private:
  std::string name_;                                   ///< Server name
  std::string desc_;                                   ///< Server description
  std::unique_ptr<AbstractTransport> transport_;       ///< Transport mechanism for communication

  /**
   * @brief Internal implementation of server startup logic
   *
   * This method contains the core server initialization and message processing loop.
   * It is called by start_server() and should not be called directly.
   */
  void start_server_();
};
};
