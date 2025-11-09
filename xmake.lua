add_rules("mode.debug", "mode.release")

set_languages("c++20")

includes("src/phoenix_mcp/xmake.lua")

target("PhoenixMcp")
    set_kind("binary")
    add_deps("phoenix_mcp")
    add_files("src/*.cpp")
