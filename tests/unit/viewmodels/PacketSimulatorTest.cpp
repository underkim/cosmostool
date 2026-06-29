#include "viewmodels/packettools/PacketSimulator.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QUdpSocket>
#include <gtest/gtest.h>

using namespace OpenC3::ViewModels;

namespace {

void ensureQtApplication()
{
    if (QCoreApplication::instance()) return;

    static int argc = 1;
    static char appName[] = "opencosmos_tests";
    static char* argv[] = { appName, nullptr };
    static QCoreApplication app(argc, argv);
}

} // namespace

TEST(PacketSimulatorTest, ReportsInvalidUdpBindAddress)
{
    ensureQtApplication();
    PacketSimulator simulator;
    QSignalSpy errors(&simulator, &PacketSimulator::errorOccurred);

    EXPECT_FALSE(simulator.startUdpListener(QStringLiteral("not-an-ip-address"), 45555));

    ASSERT_EQ(errors.count(), 1);
    EXPECT_TRUE(errors.takeFirst().at(0).toString().contains(QStringLiteral("bind address")));
}

TEST(PacketSimulatorTest, ReportsInvalidHexPayload)
{
    ensureQtApplication();
    PacketSimulator simulator;
    QSignalSpy errors(&simulator, &PacketSimulator::errorOccurred);

    EXPECT_FALSE(simulator.sendUdpHex(QStringLiteral("127.0.0.1"), 45556,
                                      QStringLiteral("ABC")));

    ASSERT_EQ(errors.count(), 1);
    EXPECT_TRUE(errors.takeFirst().at(0).toString().contains(QStringLiteral("even number")));
}

TEST(PacketSimulatorTest, ReceivesUdpHexPayload)
{
    ensureQtApplication();
    PacketSimulator simulator;
    QSignalSpy received(&simulator, &PacketSimulator::packetReceived);
    QSignalSpy started(&simulator, &PacketSimulator::started);

    ASSERT_TRUE(simulator.startUdpListener(QStringLiteral("127.0.0.1"), 0));
    ASSERT_EQ(started.count(), 1);

    const QString startedText = started.takeFirst().at(0).toString();
    const int colon = startedText.lastIndexOf(':');
    ASSERT_GT(colon, 0);
    bool ok = false;
    const quint16 port = startedText.mid(colon + 1).toUShort(&ok);
    ASSERT_TRUE(ok);
    ASSERT_GT(port, 0);

    QUdpSocket sender;
    const QByteArray payload = QByteArray::fromHex("01024142ff");
    ASSERT_EQ(sender.writeDatagram(payload, QHostAddress::LocalHost, port), payload.size());

    ASSERT_TRUE(received.wait(1000));
    ASSERT_EQ(received.count(), 1);
    const QList<QVariant> args = received.takeFirst();
    EXPECT_EQ(args.at(0).toString(), QStringLiteral("UDP"));
    EXPECT_TRUE(args.at(2).toString().contains(QStringLiteral("01 02 41 42 FF")));
    EXPECT_EQ(args.at(3).toString(), QStringLiteral("..AB."));
}
