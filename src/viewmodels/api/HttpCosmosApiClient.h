#pragma once

#include "core/api/ICosmosApiClient.h"

#include <QNetworkRequest>
#include <QString>

#include <atomic>
#include <mutex>
#include <string>

class QNetworkReply;

namespace OpenC3::ViewModels::Api {

/// Qt Network implementation of Core::Api::ICosmosApiClient.
///
/// WHY THIS LIVES IN viewmodels/ RATHER THAN core/
/// The architecture forbids Qt in core/ and services/, which is what keeps
/// those layers unit-testable without a GUI. The *interface*
/// (Core::Api::ICosmosApiClient) is therefore pure C++20 and lives in core/,
/// while this concrete Qt-based implementation lives here - exactly the
/// arrangement PluginTemplateEngine established. Services depend only on the
/// interface, so they stay Qt-free and mockable; only the composition root
/// ever names this class.
///
/// THREADING
/// Every method blocks until the exchange completes, matching
/// ICommandExecutor's synchronous contract, so callers marshal it onto a
/// worker thread with QtConcurrent::run just as they already do for shell
/// commands. A QNetworkAccessManager is bound to the thread that created it,
/// and QtConcurrent hands work to arbitrary pool threads, so this class
/// deliberately does NOT hold a long-lived manager: each request builds a
/// manager plus a local QEventLoop on the calling thread. That costs a little
/// per call, but these are user-initiated operations, not a hot path, and it
/// removes an entire class of cross-thread misuse.
class HttpCosmosApiClient final : public Core::Api::ICosmosApiClient {
public:
    HttpCosmosApiClient() = default;
    explicit HttpCosmosApiClient(const Core::Api::ApiConfig& config);

    void configure(const Core::Api::ApiConfig& config) override;
    [[nodiscard]] const Core::Api::ApiConfig& config() const noexcept override;

    [[nodiscard]] bool connect()    override;
    void               disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] std::string lastError() const override;

    [[nodiscard]] Core::Api::ApiResponse get(
        const std::string&             path,
        const Core::Api::QueryParams&  params) override;

    [[nodiscard]] Core::Api::ApiResponse post(
        const std::string&             path,
        const std::string&             jsonBody,
        const Core::Api::QueryParams&  params) override;

    [[nodiscard]] Core::Api::ApiResponse postForm(
        const std::string&             path,
        const Core::Api::QueryParams&  formFields,
        const Core::Api::QueryParams&  params) override;

    [[nodiscard]] Core::Api::ApiResponse postFile(
        const std::string&             path,
        const std::string&             fieldName,
        const std::string&             localFilePath,
        const Core::Api::QueryParams&  extraFields,
        const Core::Api::QueryParams&  params) override;

    [[nodiscard]] Core::Api::ApiResponse del(
        const std::string&             path,
        const Core::Api::QueryParams&  params) override;

private:
    enum class Verb { Get, Post, Delete };

    /// Builds the absolute URL, appending the configured scope plus any
    /// caller-supplied query parameters.
    [[nodiscard]] QString buildUrl(
        const std::string& path, const Core::Api::QueryParams& params) const;

    /// Builds a request carrying the standard headers, auth, and TLS policy.
    /// Pass nullptr for contentType on verbs that send no body.
    [[nodiscard]] QNetworkRequest makeRequest(
        const std::string&            path,
        const Core::Api::QueryParams& params,
        const char*                   contentType) const;

    /// Blocks until the reply finishes, aborting at `timeoutMs`, and maps the
    /// outcome onto ApiResponse. Consumes and schedules deletion of `reply`.
    [[nodiscard]] Core::Api::ApiResponse awaitReply(
        QNetworkReply*     reply,
        int                timeoutMs,
        const std::string& what);

    /// Runs one exchange to completion on the calling thread.
    /// `body` is ignored for Get/Delete.
    [[nodiscard]] Core::Api::ApiResponse send(
        Verb                          verb,
        const std::string&            path,
        const Core::Api::QueryParams& params,
        const QByteArray&             body,
        const char*                   contentType);

    void setLastError(std::string message);

    Core::Api::ApiConfig config_;
    std::atomic<bool>    connected_{false};

    mutable std::mutex lastErrorMutex_;
    std::string        lastError_;
};

} // namespace OpenC3::ViewModels::Api
