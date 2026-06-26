#pragma once

#include <any>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace OpenC3::Core::Events {

/// Thread-safe publish/subscribe event bus.
///
/// Events are plain value types (structs). Subscribers receive a const-ref.
/// Handlers are called synchronously on the publishing thread.
///
/// Example:
///   struct ContainerStartedEvent { std::string containerId; };
///
///   bus.subscribe<ContainerStartedEvent>([](const auto& e) {
///       // handle e.containerId
///   });
///   bus.publish(ContainerStartedEvent{"cosmos_redis"});
///
/// No inheritance required for event types.
class EventBus {
public:
    template<typename TEvent>
    using Handler = std::function<void(const TEvent&)>;

    template<typename TEvent>
    void subscribe(Handler<TEvent> handler)
    {
        std::lock_guard lock(mutex_);
        auto& slot = handlers_[std::type_index(typeid(TEvent))];
        slot.emplace_back(
            [h = std::move(handler)](const std::any& e) {
                h(std::any_cast<const TEvent&>(e));
            });
    }

    template<typename TEvent>
    void publish(const TEvent& event)
    {
        // Take a snapshot so subscribers can themselves publish/subscribe
        std::vector<std::function<void(const std::any&)>> snapshot;
        {
            std::lock_guard lock(mutex_);
            auto it = handlers_.find(std::type_index(typeid(TEvent)));
            if (it != handlers_.end())
                snapshot = it->second;
        }
        const std::any boxed = event;
        for (auto& h : snapshot)
            h(boxed);
    }

    void clear()
    {
        std::lock_guard lock(mutex_);
        handlers_.clear();
    }

private:
    std::mutex mutex_;
    std::unordered_map<
        std::type_index,
        std::vector<std::function<void(const std::any&)>>
    > handlers_;
};

} // namespace OpenC3::Core::Events
