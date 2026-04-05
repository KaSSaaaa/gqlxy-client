#pragma once

#include <boost/url.hpp>

#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

namespace gqlxy::internal {

struct ParsedUrl {
    bool tls;
    std::string host;
    std::string port;
    std::string target;
};

ParsedUrl ParseUrl(const std::string& url, const std::vector<boost::urls::scheme>& allowedSchemes);
ParsedUrl ParseHttpUrl(const std::string& url);
ParsedUrl ParseWsUrl(const std::string& url);

}
