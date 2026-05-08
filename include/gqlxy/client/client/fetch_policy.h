#pragma once

namespace gqlxy {

enum class FetchPolicy {
    CacheFirst,
    NetworkOnly,
    CacheAndNetwork,
    NoCache
};

}
