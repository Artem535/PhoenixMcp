add_rules("mode.release", "mode.debug")

set_languages("c++20")
add_defines("GLOG_USE_GLOG_EXPORT")

local vcpkg_root = os.getenv("VCPKG_ROOT")
if vcpkg_root then
    add_includedirs(path.join(vcpkg_root, "installed", "x64-linux", "include"))
end

local with_otel = os.getenv("PHOENIX_MCP_WITH_OTEL") == "1"
local with_drogon = os.getenv("PHOENIX_MCP_WITH_DROGON") ~= "0"
local with_crow = os.getenv("PHOENIX_MCP_WITH_CROW") ~= "0"

-- Mirror of the CMake module graph.
-- Core    : runtime, constants (no public header — private only)
-- Protocol: message_types.h (header-only)
-- Server  : server, session, request_handler, tool_registry
-- Client  : header-only stub (MCP Client implementation deferred)
-- The following CMake modules have no Xmake mirror:
--   Host         — MCP Host role, deferred
--   TransportHttp— base HTTP types, deferred
--   TransportDrogon— see phoenix_mcp_transport_drogon below
--   TransportCrow  — see phoenix_mcp_transport_crow below
--   Telemetry    — see phoenix_mcp_telemetry below
--   Testing      — CMake/CTest only

target("phoenix_mcp_core")
    set_kind("static")
    add_files("core/error.cc")
    add_files("runtime/runtime.cpp")
    add_includedirs(".", "../../include", {public = true})
    add_packages("vcpkg::folly", "vcpkg::spdlog", {public = true})

target("phoenix_mcp_protocol")
    set_kind("headeronly")
    add_includedirs("../../include", {public = true})
    add_deps("phoenix_mcp_core")

target("phoenix_mcp_server")
    set_kind("static")
    add_files("server/*.cpp")
    add_files("tool_registry/tool_registry.cpp")
    add_includedirs(".", "../../include", {public = true})
    add_deps("phoenix_mcp_protocol")
    add_packages("vcpkg::reflectcpp", "vcpkg::folly", "vcpkg::spdlog", {public = true})

target("phoenix_mcp_client")
    set_kind("headeronly")
    add_includedirs("../../include", {public = true})
    add_deps("phoenix_mcp_protocol")

target("phoenix_mcp_transport_stdio")
    set_kind("static")
    add_files("transport/stdio_transport.cpp")
    add_includedirs(".", "../../include", {public = true})
    add_deps("phoenix_mcp_core")
    add_packages("vcpkg::folly", "vcpkg::spdlog", {public = true})

if with_drogon then
    target("phoenix_mcp_transport_drogon")
        set_kind("static")
        add_files("transport/drogon_transport.cpp")
        add_includedirs(".", "../../include", {public = true})
        add_deps("phoenix_mcp_core")
        add_packages("vcpkg::folly", "vcpkg::spdlog", "vcpkg::drogon", {public = true})
else
    -- Drogon transport is optional. To enable, set PHOENIX_MCP_WITH_DROGON=1.
    -- Xmake does not provide Drogon-dependent targets by default.
end

if with_crow then
    target("phoenix_mcp_transport_crow")
        set_kind("static")
        add_files("transport/crow_transport.cpp")
        add_includedirs(".", "../../include", {public = true})
        add_deps("phoenix_mcp_core")
        add_packages("vcpkg::folly", "vcpkg::spdlog", "crow", {public = true})
else
    -- Crow transport is optional. To enable, set PHOENIX_MCP_WITH_CROW=1.
    -- Xmake does not provide Crow-dependent targets by default.
end

if with_otel then
    target("phoenix_mcp_telemetry")
        set_kind("headeronly")
        add_includedirs("../../include", {public = true})
        add_deps("phoenix_mcp_core")
        add_packages("opentelemetry-cpp", {public = true})
        add_defines("PXM_WITH_OTEL=1", {public = true})
    else
        add_defines("PXM_WITH_OTEL=0", {public = true})
    end

-- Legacy monolithic target (kept for backward compatibility)
target("phoenix_mcp")
    set_kind("static")
    add_deps("phoenix_mcp_core", "phoenix_mcp_protocol", "phoenix_mcp_server",
             "phoenix_mcp_transport_stdio")
    if with_drogon then
        add_deps("phoenix_mcp_transport_drogon")
    end
    if with_crow then
        add_deps("phoenix_mcp_transport_crow")
    end
    if with_otel then
        add_deps("phoenix_mcp_telemetry")
    end