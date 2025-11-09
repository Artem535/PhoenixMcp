add_rules("mode.release", "mode.debug")

set_languages("c++20")

add_requires("vcpkg::reflectcpp")
add_requires("vcpkg::yyjson")
add_requires("vcpkg::nlohmann-json")
add_requires("vcpkg::spdlog")

target("phoenix_mcp")
	set_kind("static")
	add_files("*/*.cpp", {public=true})
	add_packages("vcpkg::reflectcpp", "vcpkg::yyjson", "vcpkg::nlohmann-json", "vcpkg::spdlog")
