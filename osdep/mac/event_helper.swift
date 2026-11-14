/*
 * This file is part of universalvideoplayer.
 *
 * universalvideoplayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * universalvideoplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with universalvideoplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

import Cocoa

protocol EventSubscriber: AnyObject {
    var uid: Int { get }
    func handle(event: EventHelper.Event)
}

extension EventSubscriber {
    var uid: Int { return Int(bitPattern: ObjectIdentifier(self)) }
}

extension EventHelper {
    typealias WakeupCallback = (@convention(c) (UnsafeMutableRawPointer?) -> Void)?

    struct Event {
        var id: String {
            return name + (name.starts(with: "UNIVERSALVIDEOPLAYER_EVENT_") ? "" : String(format.rawValue))
        }
        var idReset: String {
            return name + (name.starts(with: "UNIVERSALVIDEOPLAYER_EVENT_") ? "" : String(UNIVERSALVIDEOPLAYER_FORMAT_NONE.rawValue))
        }
        let name: String
        let format: universalvideoplayer_format
        let string: String?
        let bool: Bool?
        let int: Int64?
        let double: Double?
        let array: [Any?]

        init(
            name: String = "",
            format: universalvideoplayer_format = UNIVERSALVIDEOPLAYER_FORMAT_NONE,
            string: String? = nil,
            bool: Bool? = nil,
            int: Int64? = nil,
            double: Double? = nil,
            array: [Any?] = []

        ) {
            self.name = name
            self.format = format
            self.string = string
            self.bool = bool
            self.int = int
            self.double = double
            self.array = array
        }
    }
}

class EventHelper {
    unowned let appHub: AppHub
    var universalvideoplayer: OpaquePointer?
    var events: [String: [Int: EventSubscriber]] = [:]

    init?(_ appHub: AppHub, _ universalvideoplayer: OpaquePointer) {
        if !appHub.isApplication {
            universalvideoplayer_destroy(universalvideoplayer)
            return nil
        }

        self.appHub = appHub
        self.universalvideoplayer = universalvideoplayer
        universalvideoplayer_set_wakeup_callback(universalvideoplayer, wakeup, TypeHelper.bridge(obj: self))
    }

    func subscribe(_ subscriber: EventSubscriber, event: Event) {
        guard let universalvideoplayer = universalvideoplayer else { return }

        if !event.name.isEmpty {
            if !events.keys.contains(event.idReset) {
                events[event.idReset] = [:]
            }
            if !events.keys.contains(event.id) {
                universalvideoplayer_observe_property(universalvideoplayer, 0, event.name, event.format)
                events[event.id] = [:]
            }
            events[event.idReset]?[subscriber.uid] = subscriber
            events[event.id]?[subscriber.uid] = subscriber
        }
    }

    let wakeup: WakeupCallback = { ( ctx ) in
        let event = unsafeBitCast(ctx, to: EventHelper.self)
        DispatchQueue.main.async { event.eventLoop() }
    }

    func eventLoop() {
        while let universalvideoplayer = universalvideoplayer, let event = universalvideoplayer_wait_event(universalvideoplayer, 0) {
            if event.pointee.event_id == UNIVERSALVIDEOPLAYER_EVENT_NONE { break }
            handle(event: event)
        }
    }

    func handle(event: UnsafeMutablePointer<universalvideoplayer_event>) {
        switch event.pointee.event_id {
        case UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE:
            handle(property: event)
        default:
            for (_, subscriber) in events[String(describing: event.pointee.event_id)] ?? [:] {
                subscriber.handle(event: .init(name: String(describing: event.pointee.event_id)))
            }
        }

        if event.pointee.event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN {
            universalvideoplayer_destroy(universalvideoplayer)
            universalvideoplayer = nil
        }
    }

    func handle(property universalvideoplayerEvent: UnsafeMutablePointer<universalvideoplayer_event>) {
        let pData = OpaquePointer(universalvideoplayerEvent.pointee.data)
        guard let property = UnsafePointer<universalvideoplayer_event_property>(pData)?.pointee else {
            return
        }

        let name = String(cString: property.name)
        let format = property.format
        for (_, subscriber) in events[name + String(format.rawValue)] ?? [:] {
            var event: Event?
            switch format {
            case UNIVERSALVIDEOPLAYER_FORMAT_STRING:
                event = .init(name: name, format: format, string: TypeHelper.toString(property.data))
            case UNIVERSALVIDEOPLAYER_FORMAT_FLAG:
                event = .init(name: name, format: format, bool: TypeHelper.toBool(property.data))
            case UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE:
                event = .init(name: name, format: format, double: TypeHelper.toDouble(property.data))
            case UNIVERSALVIDEOPLAYER_FORMAT_INT64:
                event = .init(name: name, format: format, int: TypeHelper.toInt(property.data))
            case UNIVERSALVIDEOPLAYER_FORMAT_NODE:
                let node = TypeHelper.toNode(property.data)
                event = .init(
                    name: name,
                    format: format,
                    string: TypeHelper.nodeToString(node),
                    bool: TypeHelper.nodeToBool(node),
                    int: TypeHelper.nodeToInt(node),
                    double: TypeHelper.nodeToDouble(node),
                    array: TypeHelper.nodeToArray(node)
                )
            case UNIVERSALVIDEOPLAYER_FORMAT_NONE:
                event = .init(name: name, format: format)
            default: break
            }

            if let e = event { subscriber.handle(event: e) }
        }
    }
}
