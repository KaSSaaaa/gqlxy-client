#pragma once

#include <boost/url.hpp>

#include <ranges>
#include <string>
#include <vector>

namespace gqlxy::internal {

struct Url {
    bool tls;
    std::string host;
    std::string port;
    std::string target;
};

Url ParseUrl(const std::string& url, const std::vector<boost::urls::scheme>& allowedSchemes);
Url ParseHttpUrl(const std::string& url);
Url ParseWsUrl(const std::string& url);

}
