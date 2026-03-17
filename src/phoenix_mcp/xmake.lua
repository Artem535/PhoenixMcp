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

local with_otel = os.getenv("PHOENIX_MCP_WITH_OTEL") == "1"

if with_otel then
    add_requires("opentelemetry-cpp")
end

local vcpkg_root = os.getenv("VCPKG_ROOT")
if vcpkg_root then
    add_includedirs(path.join(vcpkg_root, "installed", "x64-linux", "include"))
end

target("phoenix_mcp")
	set_kind("static")
	add_files("*/*.cpp", {public=true})
	add_includedirs(".", {public = true})
	add_packages("vcpkg::reflectcpp", {public = true})
	add_packages("vcpkg::yyjson", {public = true})
	add_packages("vcpkg::spdlog", {public = true})
	add_packages("crow", {public = true})
	add_packages("vcpkg::folly", {public = true})
	add_packages("vcpkg::glog", {public = true})
	add_packages("vcpkg::gflags", {public = true})
	add_packages("vcpkg::boost-context", {public = true})
	add_packages("vcpkg::libevent", {public = true})
	add_packages("vcpkg::double-conversion", {public = true})
	add_packages("vcpkg::drogon", {public = true})
	add_packages("vcpkg::trantor", {public = true})
	add_packages("vcpkg::jsoncpp", {public = true})
	add_packages("vcpkg::openssl", {public = true})
	add_packages("vcpkg::brotli", {public = true})
	add_packages("vcpkg::zlib", {public = true})
	add_packages("vcpkg::libuuid", {public = true})
	add_packages("vcpkg::c-ares", {public = true})
    if with_otel then
        add_packages("opentelemetry-cpp", {public = true})
        add_defines("PXM_WITH_OTEL=1", {public = true})
    else
        add_defines("PXM_WITH_OTEL=0", {public = true})
    end
