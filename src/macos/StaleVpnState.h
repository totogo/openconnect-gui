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
#pragma once

#include <QStringList>

#include <functional>

/*
 * Recovery from an abnormal exit while a VPN was connected (macOS).
 *
 * On macOS vpnc-script configures the tunnel through the SystemConfiguration
 * dynamic store: it creates State:/Network/Service/<utunN>/IPv4 (with
 * OverridePrimary) and State:/Network/Service/<utunN>/DNS. Those keys are only
 * removed by vpnc-script when libopenconnect runs it with reason=disconnect.
 * If the process dies instead (crash, kill -9, power loss of the GUI process),
 * the utun interface disappears but the keys stay behind, so configd keeps
 * treating the dead tunnel as the primary service: system DNS points at the
 * VPN resolvers and no default route is reinstalled. The machine has no
 * working DNS or Internet access until reboot.
 *
 * At start-up (running as root) we look for such keys whose utun interface no
 * longer exists and remove them, which lets configd recompute the primary
 * service, DNS and default route.
 */
namespace StaleVpnState {

/* Parse the output of `scutil` "list <pattern>" into the listed keys. */
QStringList parseScutilKeys(const QString& scutilListOutput);

/* From the State:/Network/Service/<utunN>/{IPv4,DNS} keys, return those whose
 * interface does not exist according to `interfaceExists`. Keys that do not
 * name a utun interface are ignored. */
QStringList staleTunServiceKeys(const QStringList& keys,
    const std::function<bool(const QString& interfaceName)>& interfaceExists);

/* Return the stale keys currently present in the dynamic store (read-only). */
QStringList findStaleTunServiceKeys();

/* Remove stale keys from the dynamic store. Requires root; returns the number
 * of keys removed (0 when nothing was stale or we are not root). */
int cleanupStaleTunServiceState();

} // namespace StaleVpnState
