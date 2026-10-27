#include "SessionBus.h"

namespace vp
{
namespace
{
std::mutex& registryMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::map<std::string, std::weak_ptr<SessionBus>>& registry()
{
    static std::map<std::string, std::weak_ptr<SessionBus>> map;
    return map;
}
} // namespace

std::shared_ptr<SessionBus> getSessionBus (const std::string& id)
{
    std::lock_guard<std::mutex> lock (registryMutex());
    auto& entries = registry();

    if (auto it = entries.find (id); it != entries.end())
    {
        if (auto existing = it->second.lock())
            return existing;
    }

    auto bus = std::make_shared<SessionBus>();
    entries[id] = bus;
    return bus;
}

void releaseSessionBus (const std::string& id)
{
    std::lock_guard<std::mutex> lock (registryMutex());
    registry().erase (id);
}

} // namespace vp
