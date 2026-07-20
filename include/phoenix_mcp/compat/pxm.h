#ifndef PHOENIX_MCP_COMPAT_PXM_H_
#define PHOENIX_MCP_COMPAT_PXM_H_

#include "phoenix_mcp/core/runtime.h"
#include "phoenix_mcp/protocol/message_types.h"
#include "phoenix_mcp/server/mcp_request_handler.h"
#include "phoenix_mcp/server/mcp_server.h"
#include "phoenix_mcp/server/server.h"
#include "phoenix_mcp/tool_registry/tool_registry.h"
#include "phoenix_mcp/tool_registry/utils.h"
#include "phoenix_mcp/transport/abstract_transport.h"
#include "phoenix_mcp/transport/i_transport.h"
#include "phoenix_mcp/transport/stdio_transport.h"

namespace [[deprecated("Use namespace phoenix_mcp instead.")]] pxm {
using namespace phoenix_mcp;
}

#endif  // PHOENIX_MCP_COMPAT_PXM_H_
