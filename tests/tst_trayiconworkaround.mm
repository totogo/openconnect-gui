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

/*
 * Regression test for the macOS 27 tray-icon crash (Qt 6.11.2 and older):
 * QCocoaSystemTrayIcon::emitActivated() asks NSApp.currentEvent for its
 * clickCount while the current event is not a mouse event, AppKit raises
 * NSInternalInconsistencyException and the process aborts.
 *
 * The test reproduces the situation without a human clicking the menu bar:
 * it makes a real non-mouse event the application's current event and then
 * posts the NSMenuDidBeginTrackingNotification for the tray menu, which is
 * what AppKit does when the status item menu opens. Without the guard the
 * process dies with SIGABRT; with it the test exits 0.
 *
 * Run with --without-guard to see the failure on an affected Qt.
 */
#import <AppKit/AppKit.h>

#include "macos/TrayIconWorkaround.h"

#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    bool withGuard = true;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--without-guard") == 0)
            withGuard = false;

    if (withGuard && !installMacOSTrayIconWorkaround()) {
        fprintf(stderr, "FAIL: tray-icon guard could not be installed (Qt %s)\n", qVersion());
        return 1;
    }

    QMenu menu;
    menu.addAction("Connect to...");
    QSystemTrayIcon tray;
    tray.setContextMenu(&menu);
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::black);
    tray.setIcon(QIcon(pixmap));
    tray.show();

    int activations = 0;
    QObject::connect(&tray, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason) { ++activations; });

    int result = 1;
    QTimer::singleShot(200, [&]() {
        /* 1. A real, non-mouse event becomes NSApp.currentEvent. */
        NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint modifierFlags:0 timestamp:0
                                        windowNumber:0 context:nil subtype:0 data1:0 data2:0];
        [NSApp postEvent:event atStart:YES];
        [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
                              inMode:NSDefaultRunLoopMode dequeue:YES];
        if (NSApp.currentEvent.type != NSEventTypeApplicationDefined) {
            fprintf(stderr, "FAIL: could not make a non-mouse event current\n");
            result = 1;
            app.quit();
            return;
        }
        /* 2. What AppKit posts when the status-item menu starts tracking. */
        [NSNotificationCenter.defaultCenter postNotificationName:NSMenuDidBeginTrackingNotification
                                                          object:menu.toNSMenu()];
        fprintf(stderr, "PASS: tray menu tracking with a non-mouse current event did not abort\n");
        if (activations != 0) {
            fprintf(stderr, "FAIL: expected no activation for a non-mouse event, got %d\n", activations);
            result = 1;
            app.quit();
            return;
        }

        /* 3. With a real mouse event current, the activation must still reach Qt. */
        NSEvent* click = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(1, 1)
                                       modifierFlags:0 timestamp:0 windowNumber:0 context:nil
                                         eventNumber:1 clickCount:1 pressure:0];
        [NSApp postEvent:click atStart:YES];
        [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
                              inMode:NSDefaultRunLoopMode dequeue:YES];
        [NSNotificationCenter.defaultCenter postNotificationName:NSMenuDidBeginTrackingNotification
                                                          object:menu.toNSMenu()];
        if (activations != 1) {
            fprintf(stderr, "FAIL: expected one activation for a mouse event, got %d\n", activations);
            result = 1;
            app.quit();
            return;
        }
        fprintf(stderr, "PASS: mouse-event activation still reaches QSystemTrayIcon::activated\n");
        result = 0;
        app.quit();
    });
    app.exec();
    return result;
}
