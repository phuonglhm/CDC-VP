#ifndef FETCH_LOADER_REGISTRY_H
#define FETCH_LOADER_REGISTRY_H

// Simple global registry to expose a shared FetchFrameLoader to other
// components/tests without requiring explicit parameter passing.

struct FetchFrameLoader;

namespace fetch {

void register_loader(FetchFrameLoader* l);
FetchFrameLoader* get_loader();

} // namespace fetch

#endif
