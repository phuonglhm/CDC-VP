#include "../include/fetch_loader_registry.h"

namespace {
    static FetchFrameLoader* g_loader = nullptr;
}

namespace fetch {

void register_loader(FetchFrameLoader* l) {
    g_loader = l;
}

FetchFrameLoader* get_loader() {
    return g_loader;
}

} // namespace fetch
