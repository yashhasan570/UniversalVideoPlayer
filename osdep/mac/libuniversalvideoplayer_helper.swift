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
import OpenGL.GL
import OpenGL.GL3

let glDummy: @convention(c) () -> Void = {}

class LibuniversalvideoplayerHelper {
    var log: LogHelper
    var universalvideoplayer: OpaquePointer?
    var universalvideoplayerRenderContext: OpaquePointer?
    var fbo: GLint = 1
    let uninitLock = NSLock()

    init(_ universalvideoplayer: OpaquePointer, _ log: LogHelper) {
        self.universalvideoplayer = universalvideoplayer
        self.log = log
    }

    func initRender() {
        let advanced: CInt = 1
        let api = UnsafeMutableRawPointer(mutating: (UNIVERSALVIDEOPLAYER_RENDER_API_TYPE_OPENGL as NSString).utf8String)
        let pAddress = universalvideoplayer_opengl_init_params(get_proc_address: getProcAddress,
                                              get_proc_address_ctx: nil)

        TypeHelper.withUnsafeMutableRawPointers([pAddress, advanced]) { (pointers: [UnsafeMutableRawPointer?]) in
            var params: [universalvideoplayer_render_param] = [
                universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_API_TYPE, data: api),
                universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_OPENGL_INIT_PARAMS, data: pointers[0]),
                universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_ADVANCED_CONTROL, data: pointers[1]),
                universalvideoplayer_render_param()
            ]

            if universalvideoplayer_render_context_create(&universalvideoplayerRenderContext, universalvideoplayer, &params) < 0 {
                log.error("Render context init has failed.")
                exit(1)
            }
        }
    }

    let getProcAddress: (@convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Int8>?)
        -> UnsafeMutableRawPointer?) = { (_ ctx: UnsafeMutableRawPointer?, name: UnsafePointer<Int8>?) -> UnsafeMutableRawPointer? in
        let symbol: CFString = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII)
        let identifier = CFBundleGetBundleWithIdentifier("com.apple.opengl" as CFString)
        let addr = CFBundleGetFunctionPointerForName(identifier, symbol)

        if symbol as String == "glFlush" {
            return unsafeBitCast(glDummy, to: UnsafeMutableRawPointer.self)
        }

        return addr
    }

    func setRenderUpdateCallback(_ callback: @escaping universalvideoplayer_render_update_fn, context object: AnyObject) {
        if universalvideoplayerRenderContext == nil {
            log.warning("Init universalvideoplayer render context first.")
        } else {
            universalvideoplayer_render_context_set_update_callback(universalvideoplayerRenderContext, callback, TypeHelper.bridge(obj: object))
        }
    }

    func setRenderControlCallback(_ callback: @escaping mp_render_cb_control_fn, context object: AnyObject) {
        if universalvideoplayerRenderContext == nil {
            log.warning("Init universalvideoplayer render context first.")
        } else {
            mp_render_context_set_control_callback(universalvideoplayerRenderContext, callback, TypeHelper.bridge(obj: object))
        }
    }

    func reportRenderFlip() {
        if universalvideoplayerRenderContext == nil { return }
        universalvideoplayer_render_context_report_swap(universalvideoplayerRenderContext)
    }

    func isRenderUpdateFrame() -> Bool {
        uninitLock.lock()
        if universalvideoplayerRenderContext == nil {
            uninitLock.unlock()
            return false
        }
        let flags: UInt64 = universalvideoplayer_render_context_update(universalvideoplayerRenderContext)
        uninitLock.unlock()
        return flags & UInt64(UNIVERSALVIDEOPLAYER_RENDER_UPDATE_FRAME.rawValue) > 0
    }

    func drawRender(_ surface: NSSize, _ depth: GLint, _ ctx: CGLContextObj, skip: Bool = false) {
        uninitLock.lock()
        if universalvideoplayerRenderContext != nil {
            var i: GLint = 0
            let flip: CInt = 1
            let skip: CInt = skip ? 1 : 0
            let ditherDepth = depth
            glGetIntegerv(GLenum(GL_DRAW_FRAMEBUFFER_BINDING), &i)
            // CAOpenGLLayer has ownership of FBO zero yet can return it to us,
            // so only utilize a newly received FBO ID if it is nonzero.
            fbo = i != 0 ? i : fbo

            let data = universalvideoplayer_opengl_fbo(fbo: Int32(fbo),
                                        w: Int32(surface.width),
                                        h: Int32(surface.height),
                          internal_format: 0)

            TypeHelper.withUnsafeMutableRawPointers([data, flip, ditherDepth, skip]) { (pointers: [UnsafeMutableRawPointer?]) in
                var params: [universalvideoplayer_render_param] = [
                    universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_OPENGL_FBO, data: pointers[0]),
                    universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_FLIP_Y, data: pointers[1]),
                    universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_DEPTH, data: pointers[2]),
                    universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_SKIP_RENDERING, data: pointers[3]),
                    universalvideoplayer_render_param()
                ]
                universalvideoplayer_render_context_render(universalvideoplayerRenderContext, &params)
            }
        } else {
            glClearColor(0, 0, 0, 1)
            glClear(GLbitfield(GL_COLOR_BUFFER_BIT))
        }

        if !skip { CGLFlushDrawable(ctx) }

        uninitLock.unlock()
    }

    func setRenderICCProfile(_ profile: NSColorSpace) {
        if universalvideoplayerRenderContext == nil { return }
        guard var iccData = profile.iccProfileData else {
            log.warning("Invalid ICC profile data.")
            return
        }
        iccData.withUnsafeMutableBytes { (ptr: UnsafeMutableRawBufferPointer) in
            guard let baseAddress = ptr.baseAddress, ptr.count > 0 else { return }

            let u8Ptr = baseAddress.assumingMemoryBound(to: UInt8.self)
            var icc = universalvideoplayer_byte_array(data: u8Ptr, size: ptr.count)
            withUnsafeMutableBytes(of: &icc) { (ptr: UnsafeMutableRawBufferPointer) in
                let params = universalvideoplayer_render_param(type: UNIVERSALVIDEOPLAYER_RENDER_PARAM_ICC_PROFILE, data: ptr.baseAddress)
                universalvideoplayer_render_context_set_parameter(universalvideoplayerRenderContext, params)
            }
        }
    }

    func uninit() {
        universalvideoplayer_render_context_set_update_callback(universalvideoplayerRenderContext, nil, nil)
        mp_render_context_set_control_callback(universalvideoplayerRenderContext, nil, nil)
        uninitLock.lock()
        universalvideoplayer_render_context_free(universalvideoplayerRenderContext)
        universalvideoplayerRenderContext = nil
        universalvideoplayer_destroy(universalvideoplayer)
        universalvideoplayer = nil
        uninitLock.unlock()
    }
}
