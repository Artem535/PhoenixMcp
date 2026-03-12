add_rules("mode.release", "mode.debug")

set_languages("c++20")
add_defines("GLOG_USE_GLOG_EXPORT")

add_requires("vcpkg::reflectcpp 0.22.0")
add_requires("vcpkg::yyjson")
add_requires("vcpkg::spdlog")
add_requires("crow")
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

target("phoenix_mcp")
	set_kind("static")
	add_files("*/*.cpp", {public=true})
	add_packages("vcpkg::reflectcpp",
	"vcpkg::yyjson",
	"vcpkg::spdlog",
	"crow",
	"vcpkg::folly",
	"vcpkg::glog",
	"vcpkg::gflags",
	"vcpkg::boost-context",
	"vcpkg::libevent",
	"vcpkg::double-conversion",
	"vcpkg::drogon",
	"vcpkg::trantor",
	"vcpkg::jsoncpp",
	"vcpkg::openssl",
	"vcpkg::brotli",
	"vcpkg::zlib",
	"vcpkg::libuuid",
	"vcpkg::c-ares")
