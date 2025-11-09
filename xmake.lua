add_rules("mode.debug", "mode.release")

set_languages("c++20")

includes("src/phoenix_mcp/xmake.lua")

add_requires("vcpkg::reflectcpp")
add_requires("vcpkg::yyjson")
add_requires("vcpkg::spdlog")

target("PhoenixMcp")
    set_kind("binary")
    add_deps("phoenix_mcp")
    add_files("src/*.cpp")
    add_packages("vcpkg::reflectcpp", "vcpkg::yyjson", "vcpkg::spdlog")
