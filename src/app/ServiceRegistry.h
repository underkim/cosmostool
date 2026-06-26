#pragma once

#include <any>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace OpenC3::App {

/// Lightweight dependency injection container.
///
/// Holds shared_ptr instances indexed by type. Every service is registered
/// once (singleton lifetime within the application). Resolving an unregistered
/// type throws std::runtime_error at startup, catching wiring mistakes early.
///
/// Usage:
///   ServiceRegistry registry;
///   registry.registerInstance<IFoo>(std::make_shared<FooImpl>(...));
///   auto foo = registry.resolve<IFoo>();  // shared_ptr<IFoo>
class ServiceRegistry {
public:
    template<typename TInterface>
    void registerInstance(std::shared_ptr<TInterface> instance)
    {
        const std::type_index idx(typeid(TInterface));
        if (services_.count(idx))
            throw std::runtime_error(
                std::string("ServiceRegistry: already registered: ") +
                typeid(TInterface).name());
        services_[idx] = std::move(instance);
    }

    template<typename TInterface>
    [[nodiscard]] std::shared_ptr<TInterface> resolve() const
    {
        const std::type_index idx(typeid(TInterface));
        auto it = services_.find(idx);
        if (it == services_.end())
            throw std::runtime_error(
                std::string("ServiceRegistry: not registered: ") +
                typeid(TInterface).name());
        return std::static_pointer_cast<TInterface>(
            std::any_cast<std::shared_ptr<TInterface>>(it->second));
    }

private:
    std::unordered_map<std::type_index, std::any> services_;
};

} // namespace OpenC3::App
