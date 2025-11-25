//
// Created by artem.d on 09.11.2025.
//

#pragma once

#include <string>


namespace pxm::server {
/**
 * @brief Abstract base class for transport implementations
 *
 * This class defines the interface for different transport mechanisms
 * that can be used to communicate with the MCP server. Concrete implementations
 * should inherit from this class and provide specific read/write functionality.
 */
class AbstractTransport {
public:
  /**
   * @brief Virtual destructor for proper cleanup of derived classes
   */
  virtual ~AbstractTransport() = default;

  /**
   * @brief Reads a message from the transport
   *
   * This method should be implemented by concrete transport classes
   * to read data from their specific source (e.g., stdin, network socket, etc.)
   *
   * @return std::string The received message
   */
  virtual std::string read_msg() = 0;

  /**
   * @brief Writes a message to the transport
   *
   * This method should be implemented by concrete transport classes
   * to write data to their specific destination (e.g., stdout, network socket, etc.)
   *
   * @param msg The message to be sent
   */
  virtual void write_msg(const std::string& msg) = 0;
};
}