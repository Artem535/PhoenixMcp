add_rules("mode.debug", "mode.release")

set_languages("c++20")
add_defines("GLOG_USE_GLOG_EXPORT")

-- vcpkg-backed packages consumed via add_packages("vcpkg::...") in
-- src/phoenix_mcp/xmake.lua. xmake resolves these against $VCPKG_ROOT when
-- set (see PHOENIX_MCP_WITH_* env vars there), falling back to xmake-repo
-- otherwise; without add_requires() here xmake has no record of them at all.
--
-- NOTE: xmake's vcpkg package manager cannot currently resolve these in a
-- repo that also has its own vcpkg.json (as this one does): classic mode
-- runs `vcpkg install <pkg>` with cwd = repo root and vcpkg refuses because
-- it auto-detects the manifest; xmake's manifest-mode workaround installs
-- successfully but then fails its own post-install verification, which
-- calls `vcpkg depend-info <pkg>` with a positional package name — invalid
-- in manifest mode ("does not support individual package arguments") — so
-- xmake reports the package as not found regardless. Filed upstream:
-- https://github.com/xmake-io/xmake/issues (see CI workflow comment for the
-- issue link). The xmake-linux CI job is marked informational
-- (continue-on-error) until this is fixed upstream.
local vcpkg_packages = {
    "reflectcpp", "spdlog", "folly", "glog", "gflags", "boost-context",
    "libevent", "double-conversion", "drogon", "trantor", "jsoncpp",
    "openssl", "brotli", "zlib", "libuuid", "c-ares",
}
for _, name in ipairs(vcpkg_packages) do
    add_requires("vcpkg::" .. name)
end
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