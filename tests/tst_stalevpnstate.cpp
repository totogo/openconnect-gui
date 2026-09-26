/*
 * This file is part of openconnect-gui.
 *
 * openconnect-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "macos/StaleVpnState.h"

#include <QtTest>

#include <net/if.h>

class TestStaleVpnState : public QObject {
    Q_OBJECT

private slots:
    void parsesScutilListOutput()
    {
        const QString output = QStringLiteral(
            "  subKey [0] = State:/Network/Service/utun5/DNS\n"
            "  subKey [1] = State:/Network/Service/utun5/IPv4\n"
            "  subKey [12] = State:/Network/Service/utun12/IPv4\n");
        QCOMPARE(StaleVpnState::parseScutilKeys(output),
            QStringList({ "State:/Network/Service/utun5/DNS",
                "State:/Network/Service/utun5/IPv4",
                "State:/Network/Service/utun12/IPv4" }));
    }

    void emptyOutputGivesNoKeys()
    {
        QVERIFY(StaleVpnState::parseScutilKeys(QString()).isEmpty());
        QVERIFY(StaleVpnState::parseScutilKeys(QStringLiteral("  no key\n")).isEmpty());
    }

    void keepsKeysOfLiveInterfaces()
    {
        const QStringList keys { "State:/Network/Service/utun5/DNS", "State:/Network/Service/utun5/IPv4" };
        const auto stale = StaleVpnState::staleTunServiceKeys(keys, [](const QString&) { return true; });
        QVERIFY(stale.isEmpty());
    }

    void reportsKeysOfVanishedInterfaces()
    {
        /* What is left behind when the GUI aborts while connected on utun5. */
        const QStringList keys {
            "State:/Network/Service/utun5/DNS",
            "State:/Network/Service/utun5/IPv4",
            "State:/Network/Service/utun7/IPv4",
        };
        const auto stale = StaleVpnState::staleTunServiceKeys(keys,
            [](const QString& iface) { return iface == QLatin1String("utun7"); });
        QCOMPARE(stale, QStringList({ "State:/Network/Service/utun5/DNS", "State:/Network/Service/utun5/IPv4" }));
    }

    void liveStoreNeverReportsExistingInterfaces()
    {
        /* Reads the real dynamic store (no root needed). Whatever it reports
         * must name a utun interface that does not exist on this machine. */
        for (const QString& key : StaleVpnState::findStaleTunServiceKeys()) {
            const QString iface = key.section(QLatin1Char('/'), 3, 3);
            QVERIFY2(if_nametoindex(iface.toUtf8().constData()) == 0, qPrintable(key));
        }
    }

    void ignoresNonTunServiceKeys()
    {
        const QStringList keys {
            "State:/Network/Service/6362B5F7-F43F-45BA-A98D-DEEF2F0043DD/DNS",
            "State:/Network/Service/en0/IPv4",
            "State:/Network/Service/utun5/Proxies",
            "Setup:/Network/Service/utun5/DNS",
        };
        const auto stale = StaleVpnState::staleTunServiceKeys(keys, [](const QString&) { return false; });
        QVERIFY(stale.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestStaleVpnState)
#include "tst_stalevpnstate.moc"
