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

/*
 * Work around a crash in Qt's Cocoa platform plugin on macOS 27.
 *
 * QCocoaSystemTrayIcon::emitActivated() reads NSApp.currentEvent.clickCount
 * to work out why the tray icon was activated. On macOS 27 the status item
 * is driven by gesture recognizers / FrontBoardServices scene actions, so at
 * that point the current event is no longer a mouse event, and AppKit raises
 * NSInternalInconsistencyException ("Invalid message sent to event ...").
 * The exception escapes into Qt's event loop and the process aborts. When
 * that happens while a VPN is up, libopenconnect never runs vpnc-script with
 * reason=disconnect, so the machine is left with the VPN's DNS / primary
 * interface configured and no working default route.
 *
 * Qt fixed this in qtbase change I48b082fcd0778c22eec71c4c96c22b812ec9fc6d
 * ("macOS: Don't assume the current event is a mouse event in the tray icon",
 * picked to 6.8, 6.11 and 6.12). Until the minimum supported Qt contains that
 * fix, we patch the two selectors of Qt's private QStatusItemDelegate at
 * runtime so they are skipped when the current event is not a mouse event.
 * Qt would report QSystemTrayIcon::Unknown in that case, which MainWindow
 * ignores anyway.
 *
 * Must be called after the QApplication has been constructed (that is when
 * the cocoa platform plugin is loaded) and before any QSystemTrayIcon is shown.
 * Returns true when the guard was installed, false when the Qt classes were
 * not found (for example a Qt with the upstream fix that renamed them).
 */
bool installMacOSTrayIconWorkaround();
