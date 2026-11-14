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
import IOKit.hidsystem

extension NSDeviceDescriptionKey {
    static let screenNumber = NSDeviceDescriptionKey("NSScreenNumber")
}

extension NSScreen {
    public var displayID: CGDirectDisplayID {
        return deviceDescription[.screenNumber] as? CGDirectDisplayID ?? 0
    }

    public var serialNumber: String {
        return String(CGDisplaySerialNumber(displayID))
    }

    public var name: String {
        guard let regex = try? NSRegularExpression(pattern: " \\(\\d+\\)$", options: .caseInsensitive) else {
            return localizedName
        }
        return regex.stringByReplacingMatches(
            in: localizedName,
            range: NSRange(location: 0, length: localizedName.count),
            withTemplate: ""
        )
    }

    public var uniqueName: String {
        return name + " (\(serialNumber))"
    }
}

extension NSEvent.ModifierFlags {
    public static let optionLeft: NSEvent.ModifierFlags = .init(rawValue: UInt(NX_DEVICELALTKEYMASK))
    public static let optionRight: NSEvent.ModifierFlags = .init(rawValue: UInt(NX_DEVICERALTKEYMASK))
}

extension String {
    func isUrl() -> Bool {
        guard let regex = try? NSRegularExpression(pattern: "^(https?|ftp)://[^\\s/$.?#].[^\\s]*$",
                                                   options: .caseInsensitive) else {
            return false
        }
        let isUrl = regex.numberOfMatches(in: self,
                                     options: [],
                                       range: NSRange(location: 0, length: self.count))
        return isUrl > 0
    }
}

extension mp_keymap {
    init(_ f: Int, _ t: Int32) {
        self.init(from: Int32(f), to: t)
    }
}

extension universalvideoplayer_event_id: CustomStringConvertible {
    public var description: String {
        switch self {
        case UNIVERSALVIDEOPLAYER_EVENT_NONE: return "UNIVERSALVIDEOPLAYER_EVENT_NONE"
        case UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN: return "UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN"
        case UNIVERSALVIDEOPLAYER_EVENT_LOG_MESSAGE: return "UNIVERSALVIDEOPLAYER_EVENT_LOG_MESSAGE"
        case UNIVERSALVIDEOPLAYER_EVENT_GET_PROPERTY_REPLY: return "UNIVERSALVIDEOPLAYER_EVENT_GET_PROPERTY_REPLY"
        case UNIVERSALVIDEOPLAYER_EVENT_SET_PROPERTY_REPLY: return "UNIVERSALVIDEOPLAYER_EVENT_SET_PROPERTY_REPLY"
        case UNIVERSALVIDEOPLAYER_EVENT_COMMAND_REPLY: return "UNIVERSALVIDEOPLAYER_EVENT_COMMAND_REPLY"
        case UNIVERSALVIDEOPLAYER_EVENT_START_FILE: return "UNIVERSALVIDEOPLAYER_EVENT_START_FILE"
        case UNIVERSALVIDEOPLAYER_EVENT_END_FILE: return "UNIVERSALVIDEOPLAYER_EVENT_END_FILE"
        case UNIVERSALVIDEOPLAYER_EVENT_FILE_LOADED: return "UNIVERSALVIDEOPLAYER_EVENT_FILE_LOADED"
        case UNIVERSALVIDEOPLAYER_EVENT_IDLE: return "UNIVERSALVIDEOPLAYER_EVENT_IDLE"
        case UNIVERSALVIDEOPLAYER_EVENT_TICK: return "UNIVERSALVIDEOPLAYER_EVENT_TICK"
        case UNIVERSALVIDEOPLAYER_EVENT_CLIENT_MESSAGE: return "UNIVERSALVIDEOPLAYER_EVENT_CLIENT_MESSAGE"
        case UNIVERSALVIDEOPLAYER_EVENT_VIDEO_RECONFIG: return "UNIVERSALVIDEOPLAYER_EVENT_VIDEO_RECONFIG"
        case UNIVERSALVIDEOPLAYER_EVENT_AUDIO_RECONFIG: return "UNIVERSALVIDEOPLAYER_EVENT_AUDIO_RECONFIG"
        case UNIVERSALVIDEOPLAYER_EVENT_SEEK: return "UNIVERSALVIDEOPLAYER_EVENT_SEEK"
        case UNIVERSALVIDEOPLAYER_EVENT_PLAYBACK_RESTART: return "UNIVERSALVIDEOPLAYER_EVENT_PLAYBACK_RESTART"
        case UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE: return "UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE"
        case UNIVERSALVIDEOPLAYER_EVENT_QUEUE_OVERFLOW: return "UNIVERSALVIDEOPLAYER_EVENT_QUEUE_OVERFLOW"
        case UNIVERSALVIDEOPLAYER_EVENT_HOOK: return "UNIVERSALVIDEOPLAYER_EVENT_HOOK"
        default: return "UNIVERSALVIDEOPLAYER_EVENT_" + String(self.rawValue)
        }
    }
}

extension Bool {
    init(_ int32: Int32) {
        self.init(int32 != 0)
    }
}

extension Int32 {
    init(_ bool: Bool) {
        self.init(bool ? 1 : 0)
    }
}

extension MTLPixelFormat {
    public var name: String {
        switch self {
        case .bgra8Unorm: return "bgra8Unorm"
        case .bgra8Unorm_srgb: return "bgra8Unorm_srgb"
        case .rgba16Float: return "rgba16Float"
        case .rgb10a2Unorm: return "rgb10a2Unorm"
        case .bgr10a2Unorm: return "bgr10a2Unorm"
        default: break
        }

#if HAVE_MACOS_11_FEATURES
        if #available(macOS 11.0, *) {
            switch self {
            case .bgra10_xr: return "bgra10_xr"
            case .bgra10_xr_srgb: return "bgra10_xr_srgb"
            case .bgr10_xr: return "bgr10_xr"
            case .bgr10_xr_srgb: return "bgr10_xr_srgb"
            default: break
            }
        }
#endif

        return "raw pixel format " + String(self.rawValue)
    }
}

extension CGColorSpace {
    public var longName: String {
        let description = String(describing: self)
        guard let colorSpaceName = self.name as? String,
              let regex = try? NSRegularExpression(pattern: ".*\\((.*)\\)", options: .caseInsensitive),
              let result = regex.firstMatch(in: description, options: [], range: NSRange(location: 0, length: description.count)),
              let range = Range(result.range(at: 1), in: description) else { return description }

        let nameList = description[range].components(separatedBy: "; ").filter { !$0.hasPrefix("kCGColorSpace") }
        let simpleName = colorSpaceName.replacingOccurrences(of: "kCGColorSpace", with: "")
        return "\(simpleName) (\(nameList.joined(separator: ", ")))"
    }
}
