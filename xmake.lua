add_rules("mode.debug", "mode.release")

set_languages("c++20")
add_defines("GLOG_USE_GLOG_EXPORT")

includes("src/phoenix_mcp/xmake.lua")

-- PhoenixMcp top-level binary (main.cpp)
target("PhoenixMcp")
    set_default(false)
    set_kind("binary")
    add_deps("phoenix_mcp")
    add_files("src/*.cpp")

-- Examples
target("using_tool_registry")
    set_default(false)
    set_kind("binary")
    add_deps("phoenix_mcp_server")
    add_files("examples/using_tool_registry/*.cpp")
    add_includedirs("include")

target("create_server")
    set_default(false)
    set_kind("binary")
    add_deps("phoenix_mcp_server", "phoenix_mcp_transport_stdio")
    add_files("examples/create_server/*.cpp")
    add_includedirs("include")

local with_drogon = os.getenv("PHOENIX_MCP_WITH_DROGON") ~= "0"
if with_drogon then
    target("create_server_http")
        set_default(false)
        set_kind("binary")
        add_deps("phoenix_mcp_server", "phoenix_mcp_transport_drogon")
        add_files("examples/create_server_http/*.cpp")
        add_includedirs("include")
end