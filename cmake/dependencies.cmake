find_package(rxcpp CONFIG REQUIRED)
get_target_property(_rxcpp_include_dir rxcpp INTERFACE_INCLUDE_DIRECTORIES)
get_filename_component(_rxcpp_include_root "${_rxcpp_include_dir}" DIRECTORY)
set_property(TARGET rxcpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_rxcpp_include_root}")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)
find_package(boost_url CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
