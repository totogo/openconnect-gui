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
#include "StaleVpnState.h"

#include "logger.h"

#include <QProcess>
#include <QRegularExpression>

#include <net/if.h>
#include <unistd.h>

namespace {

const QLatin1String scutilPath("/usr/sbin/scutil");
/* Same interface naming that openconnect_setup_tun_device() accepts on macOS. */
const QLatin1String tunServicePattern("State:/Network/Service/utun[0-9]*/(IPv4|DNS)");

QString runScutil(const QString& commands, int* exitCode)
{
    QProcess scutil;
    scutil.start(scutilPath, QStringList());
    if (!scutil.waitForStarted(5000)) {
        *exitCode = -1;
        return QString();
    }
    scutil.write(commands.toUtf8());
    scutil.closeWriteChannel();
    if (!scutil.waitForFinished(10000)) {
        scutil.kill();
        *exitCode = -1;
        return QString();
    }
    *exitCode = scutil.exitStatus() == QProcess::NormalExit ? scutil.exitCode() : -1;
    return QString::fromUtf8(scutil.readAllStandardOutput());
}

bool interfaceExists(const QString& name)
{
    return if_nametoindex(name.toUtf8().constData()) != 0;
}

} // namespace

QStringList StaleVpnState::parseScutilKeys(const QString& scutilListOutput)
{
    /*   subKey [0] = State:/Network/Service/utun5/DNS   */
    static const QRegularExpression line(QStringLiteral("^\\s*subKey \\[\\d+\\] = (\\S+)\\s*$"),
        QRegularExpression::MultilineOption);
    QStringList keys;
    auto it = line.globalMatch(scutilListOutput);
    while (it.hasNext())
        keys << it.next().captured(1);
    return keys;
}

QStringList StaleVpnState::staleTunServiceKeys(const QStringList& keys,
    const std::function<bool(const QString&)>& interfaceExists)
{
    static const QRegularExpression tunKey(
        QStringLiteral("^State:/Network/Service/(utun\\d+)/(IPv4|DNS)$"));
    QStringList stale;
    for (const QString& key : keys) {
        const auto match = tunKey.match(key);
        if (!match.hasMatch())
            continue;
        if (!interfaceExists(match.captured(1)))
            stale << key;
    }
    return stale;
}

QStringList StaleVpnState::findStaleTunServiceKeys()
{
    int exitCode = 0;
    const QString output = runScutil(QStringLiteral("list %1\n").arg(tunServicePattern), &exitCode);
    if (exitCode != 0) {
        Logger::instance().addMessage(
            QStringLiteral("Could not list VPN network state with scutil (exit code %1)").arg(exitCode),
            Logger::MessageType::DEBUG);
        return QStringList();
    }
    return staleTunServiceKeys(parseScutilKeys(output), interfaceExists);
}

int StaleVpnState::cleanupStaleTunServiceState()
{
    const QStringList stale = findStaleTunServiceKeys();
    if (stale.isEmpty())
        return 0;

    if (geteuid() != 0) {
        Logger::instance().addMessage(
            QStringLiteral("Stale VPN network state left by a previous session (%1); "
                           "root privileges are required to remove it")
                .arg(stale.join(QLatin1String(", "))));
        return 0;
    }

    QString commands = QStringLiteral("open\n");
    for (const QString& key : stale)
        commands += QStringLiteral("remove %1\n").arg(key);
    commands += QStringLiteral("close\n");

    int exitCode = 0;
    runScutil(commands, &exitCode);
    if (exitCode != 0) {
        Logger::instance().addMessage(
            QStringLiteral("Failed to remove stale VPN network state (%1), scutil exit code %2")
                .arg(stale.join(QLatin1String(", ")))
                .arg(exitCode));
        return 0;
    }

    Logger::instance().addMessage(
        QStringLiteral("Removed stale VPN network state left by a previous session: %1")
            .arg(stale.join(QLatin1String(", "))));
    return stale.size();
}
