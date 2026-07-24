add_rules("mode.debug", "mode.release")

set_languages("c++20")
add_defines("GLOG_USE_GLOG_EXPORT")

-- vcpkg-backed packages consumed via add_packages("vcpkg::...") in
-- src/phoenix_mcp/xmake.lua. xmake resolves these against $VCPKG_ROOT when
-- set (see PHOENIX_MCP_WITH_* env vars there), falling back to xmake-repo
-- otherwise; without add_requires() here xmake has no record of them at all.
add_requires("vcpkg::reflectcpp")
add_requires("vcpkg::spdlog")
add_requires("vcpkg::folly")
add_requires("vcpkg::glog")
add_requires("vcpkg::gflags")
add_requires("vcpkg::boost-context")
add_requires("vcpkg::libevent")
add_requires("vcpkg::double-conversion")
add_requires("vcpkg::drogon")
add_requires("vcpkg::trantor")
add_requires("vcpkg::jsoncpp")
add_requires("vcpkg::openssl")
add_requires("vcpkg::brotli")
add_requires("vcpkg::zlib")
add_requires("vcpkg::libuuid")
add_requires("vcpkg::c-ares")
add_requires("crow")

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