// Live-transport check for HttpCosmosApiClient.
//
// Not part of the unit suite: it stands up a real local HTTP server and drives
// the client's actual Qt Network code path end to end. The mock-based tests
// prove the proxy delegates correctly, but only a real socket can prove the
// things that actually break against COSMOS - that the Authorization header is
// sent WITHOUT a "Bearer " prefix (Core compares it verbatim), that scope is
// appended to every request, and that a multipart .gem upload is well formed.
#include "viewmodels/api/HttpCosmosApiClient.h"
#include "core/api/CosmosRoutes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <iostream>
#include <string>

using OpenC3::Core::Api::ApiConfig;
using OpenC3::ViewModels::Api::HttpCosmosApiClient;

namespace {

int failures = 0;

void check(bool condition, const std::string& label)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << label << "\n";
    if (!condition) ++failures;
}

/// Minimal one-shot HTTP server that captures the raw request and replies 200.
class CaptureServer : public QTcpServer {
public:
    QByteArray request;

    void incomingConnection(qintptr handle) override
    {
        auto* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            request += socket->readAll();
            // Good enough for these fixtures: reply once headers are complete
            // and, for uploads, the body has stopped arriving.
            if (!request.contains("\r\n\r\n")) return;
            const QByteArray body = R"({"status":"ok"})";
            QByteArray response =
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            socket->write(response);
            socket->flush();
            socket->disconnectFromHost();
        });
    }
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // ── Authorization header shape and scope injection ────────────────────────
    {
        CaptureServer server;
        if (!server.listen(QHostAddress::LocalHost, 0)) {
            std::cerr << "could not bind test server\n";
            return 2;
        }

        ApiConfig config;
        config.baseUrl  = "http://127.0.0.1:" + std::to_string(server.serverPort());
        config.password = "SuperSecret123";
        config.scope    = "DEFAULT";

        HttpCosmosApiClient client(config);
        const auto response = client.get(OpenC3::Core::Api::Routes::kPlugins, {});

        const std::string raw = server.request.toStdString();
        std::cout << "-- GET /openc3-api/plugins --\n";

        check(response.success, "2xx response is reported as success");
        check(response.body == R"({"status":"ok"})", "body is returned verbatim");
        check(raw.find("GET /openc3-api/plugins?scope=DEFAULT") != std::string::npos,
              "scope is appended to the query string automatically");
        check(raw.find("Authorization: SuperSecret123") != std::string::npos,
              "Authorization carries the raw password");
        check(raw.find("Bearer") == std::string::npos,
              "no Bearer prefix (Core verifies the header verbatim)");
    }

    // ── Multipart upload ──────────────────────────────────────────────────────
    {
        const QString gemPath = QDir::tempPath() + "/openc3-cosmos-probe.gem";
        QFile gem(gemPath);
        if (!gem.open(QIODevice::WriteOnly)) {
            std::cerr << "could not create fixture gem\n";
            return 2;
        }
        gem.write("GEMDATA");
        gem.close();

        CaptureServer server;
        if (!server.listen(QHostAddress::LocalHost, 0)) return 2;

        ApiConfig config;
        config.baseUrl  = "http://127.0.0.1:" + std::to_string(server.serverPort());
        config.password = "pw";
        config.scope    = "DEFAULT";

        HttpCosmosApiClient client(config);
        const auto response = client.postFile(
            OpenC3::Core::Api::Routes::kPlugins, "plugin",
            gemPath.toStdString(), {}, {});

        const std::string raw = server.request.toStdString();
        std::cout << "-- POST /openc3-api/plugins (multipart) --\n";

        check(response.success, "upload succeeds");
        check(raw.find("POST /openc3-api/plugins?scope=DEFAULT") != std::string::npos,
              "upload also carries scope");
        check(raw.find("multipart/form-data") != std::string::npos,
              "content type is multipart/form-data");
        check(raw.find(R"(name="plugin")") != std::string::npos,
              "file part is named 'plugin' (what plugins#create reads)");
        check(raw.find("openc3-cosmos-probe.gem") != std::string::npos,
              "original filename is preserved");
        check(raw.find("GEMDATA") != std::string::npos,
              "file contents reach the wire");

        QFile::remove(gemPath);
    }

    // ── Missing file is a clean local error ───────────────────────────────────
    {
        ApiConfig config;
        config.baseUrl  = "http://127.0.0.1:1";
        config.password = "pw";

        HttpCosmosApiClient client(config);
        const auto response =
            client.postFile("/openc3-api/plugins", "plugin", "/no/such.gem", {}, {});

        std::cout << "-- missing upload file --\n";
        check(!response.success, "missing file fails");
        check(response.statusCode == 0, "no HTTP status for a local failure");
        check(response.errorMessage.find("File not found") != std::string::npos,
              "error names the real cause");
    }

    // ── Unreachable endpoint is distinguishable from an HTTP error ────────────
    {
        ApiConfig config;
        config.baseUrl          = "http://127.0.0.1:1";
        config.password         = "pw";
        config.requestTimeoutMs = 3000;

        HttpCosmosApiClient client(config);
        const auto response = client.get(OpenC3::Core::Api::Routes::kScopes, {});

        std::cout << "-- unreachable endpoint --\n";
        check(!response.success, "connection refused fails");
        check(response.statusCode == 0,
              "statusCode 0 marks a transport failure, not an HTTP error");
        check(!client.isConnected(), "client does not claim to be connected");
    }

    std::cout << (failures == 0 ? "\nALL LIVE CHECKS PASSED\n"
                                : "\nLIVE CHECK FAILURES\n");
    return failures == 0 ? 0 : 1;
}
