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
#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "TrayIconWorkaround.h"

#include "logger.h"

#include <QString>

namespace {

/* Mirrors qt_mac_isMouseEvent() in Qt's cocoa plugin: the event types for
 * which -[NSEvent clickCount] is defined. */
bool isMouseEvent(NSEvent* event)
{
    if (!event)
        return false;
    switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
        return true;
    default:
        return false;
    }
}

void logSkipped(SEL selector)
{
    Logger::instance().addMessage(
        QString("Tray icon: skipped Qt's %1 because the current event (type %2) is not a mouse event")
            .arg(QString::fromUtf8(sel_getName(selector)))
            .arg(static_cast<unsigned long>(NSApp.currentEvent ? NSApp.currentEvent.type : 0)),
        Logger::MessageType::DEBUG);
}

/* Replace `selector` on `cls` with a version that only forwards to the
 * original implementation when NSApp.currentEvent is a mouse event. */
bool guardNoArgSelector(Class cls, SEL selector)
{
    Method method = class_getInstanceMethod(cls, selector);
    if (!method)
        return false;
    using Fn = void (*)(id, SEL);
    const Fn original = reinterpret_cast<Fn>(method_getImplementation(method));
    IMP guarded = imp_implementationWithBlock(^(id self) {
        if (!isMouseEvent(NSApp.currentEvent)) {
            logSkipped(selector);
            return;
        }
        original(self, selector);
    });
    method_setImplementation(method, guarded);
    return true;
}

bool guardOneArgSelector(Class cls, SEL selector)
{
    Method method = class_getInstanceMethod(cls, selector);
    if (!method)
        return false;
    using Fn = void (*)(id, SEL, id);
    const Fn original = reinterpret_cast<Fn>(method_getImplementation(method));
    IMP guarded = imp_implementationWithBlock(^(id self, id argument) {
        if (!isMouseEvent(NSApp.currentEvent)) {
            logSkipped(selector);
            return;
        }
        original(self, selector, argument);
    });
    method_setImplementation(method, guarded);
    return true;
}

} // namespace

bool installMacOSTrayIconWorkaround()
{
    static bool installed = false;
    if (installed)
        return true;

    /* Qt's private delegate class; see qtbase/src/plugins/platforms/cocoa/qcocoasystemtrayicon.mm */
    Class delegate = objc_getClass("QStatusItemDelegate");
    if (!delegate) {
        Logger::instance().addMessage(
            QLatin1String("Tray icon: QStatusItemDelegate not found; Qt tray-icon guard not installed"),
            Logger::MessageType::DEBUG);
        return false;
    }

    /* Both selectors call QCocoaSystemTrayIcon::emitActivated(). */
    const bool clicked = guardNoArgSelector(delegate, sel_registerName("statusItemClicked"));
    const bool tracking = guardOneArgSelector(delegate, sel_registerName("statusItemMenuBeganTracking:"));

    installed = clicked || tracking;
    Logger::instance().addMessage(
        QString("Tray icon: Qt tray-icon guard installed (statusItemClicked=%1, statusItemMenuBeganTracking=%2)")
            .arg(clicked)
            .arg(tracking),
        Logger::MessageType::DEBUG);
    return installed;
}
