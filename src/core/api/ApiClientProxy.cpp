#include "ApiClientProxy.h"

namespace OpenC3::Core::Api {

ApiClientProxy::ApiClientProxy()
    : current_{&nullClient_}
{}

void ApiClientProxy::swap(ICosmosApiClient* newClient) noexcept
{
    current_.store(newClient ? newClient : &nullClient_,
                   std::memory_order_release);
}

void ApiClientProxy::reset() noexcept
{
    current_.store(&nullClient_, std::memory_order_release);
}

void ApiClientProxy::configure(const ApiConfig& config)
{
    current_.load(std::memory_order_acquire)->configure(config);
}

const ApiConfig& ApiClientProxy::config() const noexcept
{
    return current_.load(std::memory_order_acquire)->config();
}

bool ApiClientProxy::connect()
{
    return current_.load(std::memory_order_acquire)->connect();
}

void ApiClientProxy::disconnect()
{
    current_.load(std::memory_order_acquire)->disconnect();
}

bool ApiClientProxy::isConnected() const noexcept
{
    return current_.load(std::memory_order_acquire)->isConnected();
}

std::string ApiClientProxy::lastError() const
{
    return current_.load(std::memory_order_acquire)->lastError();
}

ApiResponse ApiClientProxy::get(
    const std::string& path, const QueryParams& params)
{
    return current_.load(std::memory_order_acquire)->get(path, params);
}

ApiResponse ApiClientProxy::post(
    const std::string& path, const std::string& jsonBody, const QueryParams& params)
{
    return current_.load(std::memory_order_acquire)->post(path, jsonBody, params);
}

ApiResponse ApiClientProxy::postForm(
    const std::string& path, const QueryParams& formFields, const QueryParams& params)
{
    return current_.load(std::memory_order_acquire)
                   ->postForm(path, formFields, params);
}

ApiResponse ApiClientProxy::postFile(
    const std::string& path,
    const std::string& fieldName,
    const std::string& localFilePath,
    const QueryParams& extraFields,
    const QueryParams& params)
{
    return current_.load(std::memory_order_acquire)
                   ->postFile(path, fieldName, localFilePath, extraFields, params);
}

ApiResponse ApiClientProxy::del(
    const std::string& path, const QueryParams& params)
{
    return current_.load(std::memory_order_acquire)->del(path, params);
}

} // namespace OpenC3::Core::Api
