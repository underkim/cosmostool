#include "HttpCosmosApiClient.h"

#include "core/api/CosmosRoutes.h"
#include "core/logging/Logger.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <memory>
#include <utility>

namespace OpenC3::ViewModels::Api {

using Core::Api::ApiConfig;
using Core::Api::ApiResponse;
using Core::Api::QueryParams;

namespace {

constexpr const char* kUserAgent = "OpenC3DevToolkit";

/// Strips one trailing slash so baseUrl + path never yields a double slash.
QString normalizeBaseUrl(const std::string& raw)
{
    QString url = QString::fromStdString(raw).trimmed();
    while (url.endsWith('/'))
        url.chop(1);
    return url;
}

} // namespace

HttpCosmosApiClient::HttpCosmosApiClient(const ApiConfig& config)
    : config_(config)
{}

void HttpCosmosApiClient::configure(const ApiConfig& config)
{
    config_ = config;
    // Credentials or endpoint changed, so the previous successful probe no
    // longer says anything about the new target - force a fresh connect().
    connected_.store(false, std::memory_order_release);
    setLastError({});
}

const ApiConfig& HttpCosmosApiClient::config() const noexcept
{
    return config_;
}

bool HttpCosmosApiClient::connect()
{
    // Probe an *authenticated* route rather than just checking reachability.
    // /openc3-api/ping would answer even with a bad password, which would turn
    // a wrong credential into a confusing 401 on the user's first real action
    // instead of an immediate, obvious connection failure. Listing scopes is
    // cheap and goes through the same authorize() path every other call does.
    auto response = get(Core::Api::Routes::kScopes, {});

    if (response) {
        connected_.store(true, std::memory_order_release);
        Logging::Logger::info("[CosmosApi] Connected to {} (scope {})",
                              config_.baseUrl, config_.scope);
        return true;
    }

    connected_.store(false, std::memory_order_release);

    // 401/403 is overwhelmingly a wrong OPENC3_API_PASSWORD, and the raw
    // "HTTP 401" that fromHttp() produces gives the user nothing to act on.
    if (response.statusCode == 401 || response.statusCode == 403) {
        setLastError(
            "Authentication rejected by COSMOS (HTTP "
            + std::to_string(response.statusCode)
            + "). Check the API password - it must match OPENC3_API_PASSWORD "
              "in the COSMOS .env file.");
    } else if (response.statusCode == 0) {
        setLastError("Could not reach the COSMOS API at " + config_.baseUrl
                     + ": " + response.errorMessage);
    } else {
        setLastError("COSMOS API returned HTTP "
                     + std::to_string(response.statusCode)
                     + " for " + std::string(Core::Api::Routes::kScopes));
    }
    Logging::Logger::warn("[CosmosApi] Connect failed: {}", lastError());
    return false;
}

void HttpCosmosApiClient::disconnect()
{
    connected_.store(false, std::memory_order_release);
}

bool HttpCosmosApiClient::isConnected() const noexcept
{
    return connected_.load(std::memory_order_acquire);
}

std::string HttpCosmosApiClient::lastError() const
{
    const std::lock_guard<std::mutex> lock(lastErrorMutex_);
    return lastError_;
}

void HttpCosmosApiClient::setLastError(std::string message)
{
    const std::lock_guard<std::mutex> lock(lastErrorMutex_);
    lastError_ = std::move(message);
}

QString HttpCosmosApiClient::buildUrl(
    const std::string& path, const QueryParams& params) const
{
    QUrl url(normalizeBaseUrl(config_.baseUrl) + QString::fromStdString(path));

    QUrlQuery query;
    // COSMOS's authorize() raises "Scope is required" when scope is absent, so
    // it is attached here once for every route instead of at each call site.
    if (!config_.scope.empty())
        query.addQueryItem("scope", QString::fromStdString(config_.scope));

    for (const auto& [key, value] : params)
        query.addQueryItem(QString::fromStdString(key),
                           QString::fromStdString(value));

    url.setQuery(query);
    return url.toString();
}

QNetworkRequest HttpCosmosApiClient::makeRequest(
    const std::string& path,
    const QueryParams& params,
    const char*        contentType) const
{
    QNetworkRequest request{QUrl(buildUrl(path, params))};

    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Accept", "application/json");

    // COSMOS Core sends the API password as the bare Authorization header
    // value - NOT "Bearer <token>". OpenC3Authentication#token returns
    // ENV['OPENC3_API_PASSWORD'] verbatim and ignores its include_bearer
    // argument, and AuthModel.verify() compares the header against the stored
    // password directly. Adding a "Bearer " prefix here makes every request
    // fail authorization. Enterprise/Keycloak is the case that needs the
    // prefix, and it is not supported in this release.
    if (!config_.password.empty())
        request.setRawHeader("Authorization",
                             QByteArray::fromStdString(config_.password));

    if (contentType != nullptr)
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QString::fromLatin1(contentType));

    if (config_.allowInsecureTls && request.url().scheme() == "https") {
        QSslConfiguration ssl = request.sslConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(ssl);
    }

    return request;
}

ApiResponse HttpCosmosApiClient::awaitReply(
    QNetworkReply* reply, int timeoutMs, const std::string& what)
{
    if (reply == nullptr)
        return ApiResponse::fail("Failed to create network request for " + what);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // QNetworkAccessManager's own transfer timeout only covers stalled
    // transfers, so an explicit deadline guards the total exchange and
    // guarantees this call cannot block a worker thread indefinitely.
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timer.start(timeoutMs);

    loop.exec();
    timer.stop();

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload        = reply->readAll();
    const auto       netError       = reply->error();
    const QString    netErrorString = reply->errorString();
    reply->deleteLater();

    if (timedOut) {
        auto result = ApiResponse::fail(
            what + " timed out after " + std::to_string(timeoutMs) + " ms");
        setLastError(result.errorMessage);
        return result;
    }

    // A transport-level failure (host unreachable, TLS refused, connection
    // reset) never produces an HTTP status, so it must be reported as such
    // rather than as "HTTP 0" - that distinction is what lets the UI tell
    // "COSMOS isn't running" apart from "COSMOS said no".
    if (netError != QNetworkReply::NoError && status == 0) {
        auto result = ApiResponse::fail(netErrorString.toStdString());
        setLastError(result.errorMessage);
        return result;
    }

    auto result = ApiResponse::fromHttp(status, QString::fromUtf8(payload).toStdString());
    if (!result)
        setLastError(result.errorMessage);
    return result;
}

ApiResponse HttpCosmosApiClient::send(
    Verb               verb,
    const std::string& path,
    const QueryParams& params,
    const QByteArray&  body,
    const char*        contentType)
{
    const QNetworkRequest request = makeRequest(path, params, contentType);

    // Manager and event loop are both local to the calling thread; see the
    // threading note on the class declaration for why nothing is cached.
    QNetworkAccessManager manager;
    QNetworkReply*        reply = nullptr;

    switch (verb) {
    case Verb::Get:    reply = manager.get(request);            break;
    case Verb::Post:   reply = manager.post(request, body);     break;
    case Verb::Delete: reply = manager.deleteResource(request); break;
    }

    return awaitReply(reply, config_.requestTimeoutMs, "Request to " + path);
}

ApiResponse HttpCosmosApiClient::get(
    const std::string& path, const QueryParams& params)
{
    return send(Verb::Get, path, params, {}, nullptr);
}

ApiResponse HttpCosmosApiClient::post(
    const std::string& path, const std::string& jsonBody, const QueryParams& params)
{
    return send(Verb::Post, path, params,
                QByteArray::fromStdString(jsonBody), "application/json");
}

ApiResponse HttpCosmosApiClient::postForm(
    const std::string& path,
    const QueryParams& formFields,
    const QueryParams& params)
{
    QUrlQuery form;
    for (const auto& [key, value] : formFields)
        form.addQueryItem(QString::fromStdString(key),
                          QString::fromStdString(value));

    return send(Verb::Post, path, params,
                form.toString(QUrl::FullyEncoded).toUtf8(),
                "application/x-www-form-urlencoded");
}

ApiResponse HttpCosmosApiClient::postFile(
    const std::string& path,
    const std::string& fieldName,
    const std::string& localFilePath,
    const QueryParams& extraFields,
    const QueryParams& params)
{
    const QString localPath = QString::fromStdString(localFilePath);

    // Checked up front so a missing/unreadable .gem reports a clear local
    // error instead of silently uploading an empty part and coming back as a
    // puzzling server-side failure.
    auto file = std::make_unique<QFile>(localPath);
    if (!file->exists())
        return ApiResponse::fail("File not found: " + localFilePath);
    if (!file->open(QIODevice::ReadOnly))
        return ApiResponse::fail("Could not open file for upload: " + localFilePath);

    auto multiPart = std::make_unique<QHttpMultiPart>(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("application/octet-stream"));
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant(QString(R"(form-data; name="%1"; filename="%2")")
                     .arg(QString::fromStdString(fieldName),
                          QFileInfo(localPath).fileName())));
    filePart.setBodyDevice(file.get());
    // The multipart takes ownership of the QFile so it outlives this scope
    // and stays readable until the upload completes.
    file->setParent(multiPart.get());
    (void)file.release();
    multiPart->append(filePart);

    for (const auto& [key, value] : extraFields) {
        QHttpPart textPart;
        textPart.setHeader(
            QNetworkRequest::ContentDispositionHeader,
            QVariant(QString(R"(form-data; name="%1")")
                         .arg(QString::fromStdString(key))));
        textPart.setBody(QByteArray::fromStdString(value));
        multiPart->append(textPart);
    }

    const QNetworkRequest request = makeRequest(path, params, nullptr);

    QNetworkAccessManager manager;
    QNetworkReply*        reply = manager.post(request, multiPart.get());
    if (reply != nullptr) {
        multiPart->setParent(reply);
        (void)multiPart.release();
    }

    // Uploading a plugin .gem is far slower than a JSON call, so the deadline
    // is scaled up rather than reusing requestTimeoutMs directly.
    return awaitReply(reply, config_.requestTimeoutMs * 4, "Upload to " + path);
}

ApiResponse HttpCosmosApiClient::del(
    const std::string& path, const QueryParams& params)
{
    return send(Verb::Delete, path, params, {}, nullptr);
}

} // namespace OpenC3::ViewModels::Api
