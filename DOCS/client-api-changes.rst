Introduction
============

This file lists all changes that can cause compatibility issues when using
universalvideoplayer through the client API (libuniversalvideoplayer and ``client.h``). Since the client API
interfaces to input handling (commands, properties) as well as command line
options, you should also look at ``interface-changes.rst``.

Normally, changes to the C API that are incompatible to previous iterations
receive a major version bump (i.e. the first version number is increased),
while C API additions bump the minor version (i.e. the second number is
increased). Changes to properties/commands/options may also lead to a minor
version bump, in particular if they are incompatible.

The version number is the same as used for UNIVERSALVIDEOPLAYER_CLIENT_API_VERSION (see
``client.h`` how to convert between major/minor version numbers and the flat
32 bit integer).

Also, read the section ``Compatibility`` in ``client.h``, and compatibility.rst.

Options, commands, properties
=============================

Changes to these are not listed here, but in ``interface-changes.rst``. (Before
client API version 1.17, they were listed here partially.)

This listing includes changes to the bare C API and behavior only, not what
you can access with them.

API changes
===========

::

 --- universalvideoplayer 0.42.0 ---
 2.6    - add UNIVERSALVIDEOPLAYER_RENDER_API_TYPE_D3D11 render API backend, along with
          UNIVERSALVIDEOPLAYER_RENDER_PARAM_D3D11_INIT_PARAMS and UNIVERSALVIDEOPLAYER_RENDER_PARAM_D3D11_FBO,
          and a new render_d3d11.h header.
 --- universalvideoplayer 0.40.0 ---
 2.5    - Deprecate UNIVERSALVIDEOPLAYER_RENDER_PARAM_AMBIENT_LIGHT. no replacement.
 --- universalvideoplayer 0.39.0 ---
 2.4    - universalvideoplayer_render_param with the UNIVERSALVIDEOPLAYER_RENDER_PARAM_ICC_PROFILE argument no
          longer has incorrect assumptions about memory allocation and can be
          correctly used.
 --- universalvideoplayer 0.38.0 ---
 2.3    - partially revert the changes from API version 1.27, remove libuniversalvideoplayer as
          the default VO and move it to the bottom of the auto-probing order.
          This restores the prior behavior on all platforms other than macOS,
          but still auto selects libuniversalvideoplayer/cocoa-cb on macOS if it was built with
          support for cocoa-cb.
 --- universalvideoplayer 0.37.0 ---
 2.2    - add universalvideoplayer_time_ns()
 --- universalvideoplayer 0.36.0 ---
 2.1    - add universalvideoplayer_del_property()
 --- universalvideoplayer 0.35.0 ---
 2.0    - remove headers/functions of the obsolete opengl_cb API
        - remove universalvideoplayer_opengl_init_params.extra_exts field
        - remove deprecated universalvideoplayer_detach_destroy. Use universalvideoplayer_destroy instead.
        - remove obsolete universalvideoplayer_suspend and universalvideoplayer_resume
        - remove deprecated SCRIPT_INPUT_DISPATCH, PAUSE and UNPAUSE, TRACKS_CHANGED
          TRACK_SWITCHED, METADATA_UPDATE, CHAPTER_CHANGE events
 --- universalvideoplayer 0.33.0 ---
 1.109  - add UNIVERSALVIDEOPLAYER_RENDER_API_TYPE_SW and related (software rendering API)
        - deactivate the opengl_cb API (always fails to initialize now)
          The opengl_cb API was deprecated over 2 years ago. Use the render API
          instead.
 1.108  - Deprecate UNIVERSALVIDEOPLAYER_EVENT_IDLE
        - add universalvideoplayer_event_start_file
        - add the following fields to universalvideoplayer_event_end_file: playlist_entry_id,
          playlist_insert_id, playlist_insert_num_entries
        - add universalvideoplayer_event_to_node()
        - add universalvideoplayer_client_id()
 1.107  - Remove the deprecated qthelper.hpp. This was obviously not part of the
          libuniversalvideoplayer API, only an "additionally" provided helper, thus this is not
          considered an API change. If you are maintaining a project that relies
          on this header, you can simply download this file and adjust the
          include statement to use it instead:

            https://raw.githubusercontent.com/yashhasan570/Universal Video Player/v0.32.0/libuniversalvideoplayer/qthelper.hpp

          It is a good idea to write better wrappers for your use, though.
 --- universalvideoplayer 0.31.0 ---
 1.107  - Deprecate UNIVERSALVIDEOPLAYER_EVENT_TICK
 --- universalvideoplayer 0.30.0 ---
 1.106  - Add cancel_fn to universalvideoplayer_stream_cb_info
 1.105  - Fix deadlock problems with UNIVERSALVIDEOPLAYER_RENDER_PARAM_ADVANCED_CONTROL and if
          the "vd-lavc-dr" option is enabled (which it is by default).
          There were no actual API changes.
          API users on older API versions and universalvideoplayer releases should set
          "vd-lavc-dr" to "no" to avoid these issues.
          API users must still adhere to the tricky rules documented in render.h
          to avoid other deadlocks.
 1.104  - Deprecate struct universalvideoplayer_opengl_drm_params. Replaced by universalvideoplayer_opengl_drm_params_v2
        - Deprecate UNIVERSALVIDEOPLAYER_RENDER_PARAM_DRM_DISPLAY. Replaced by UNIVERSALVIDEOPLAYER_RENDER_PARAM_DRM_DISPLAY_V2.
 1.103  - redo handling of async commands
        - add universalvideoplayer_event_command and make it possible to return values from
          commands issued with universalvideoplayer_command_async() or universalvideoplayer_command_node_async()
        - add universalvideoplayer_abort_async_command()
 1.102  - rename struct universalvideoplayer_opengl_drm_osd_size to universalvideoplayer_opengl_drm_draw_surface_size
        - rename UNIVERSALVIDEOPLAYER_RENDER_PARAM_DRM_OSD_SIZE to UNIVERSALVIDEOPLAYER_RENDER_PARAM_DRM_DRAW_SURFACE_SIZE
 --- universalvideoplayer 0.29.0 ---
 1.101  - add UNIVERSALVIDEOPLAYER_RENDER_PARAM_ADVANCED_CONTROL and related API
        - add UNIVERSALVIDEOPLAYER_RENDER_PARAM_NEXT_FRAME_INFO and related symbols
        - add UNIVERSALVIDEOPLAYER_RENDER_PARAM_BLOCK_FOR_TARGET_TIME
        - add UNIVERSALVIDEOPLAYER_RENDER_PARAM_SKIP_RENDERING
        - add universalvideoplayer_render_context_get_info()
 1.100  - bump API number to avoid confusion with universalvideoplayer release versions
        - actually apply the GL_MP_MPGetNativeDisplay change for the new render
          API. This also means compatibility for anything but x11 and wayland
          through the old opengl-cb GL_MP_MPGetNativeDisplay method is now
          unsupported.
        - deprecate universalvideoplayer_get_wakeup_pipe(). It's complex, but easy to replace
          using normal API (just set a wakeup callback to a function which
          writes to a pipe).
        - add a 1st class hook API, which replaces the hacky universalvideoplayer_command()
          based one. The old API is deprecated and will be removed soon. The
          old API was never meant to be stable, while the new API is.
 1.29   - the behavior of universalvideoplayer_terminate_destroy() and universalvideoplayer_detach_destroy()
          changes subtly (see documentation in the header file). In particular,
          universalvideoplayer_detach_destroy() will not leave the player running in all
          situations anymore (it gets closer to refcounting).
        - rename universalvideoplayer_detach_destroy() to universalvideoplayer_destroy() (the old function will
          remain valid as deprecated alias)
        - add universalvideoplayer_create_weak_client(), which makes use of above changes
        - UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN is now returned exactly once if a universalvideoplayer_handle
          should terminate, instead of spamming the event queue with this event
 1.28   - deprecate the render opengl_cb API, and replace it with render.h
          and render_gl.h. The goal is allowing support for APIs other than
          OpenGL. The old API is emulated with the new API.
          Likewise, the "opengl-cb" VO is renamed to "libuniversalvideoplayer".
          universalvideoplayer_get_sub_api() is deprecated along the opengl_cb API.
          The new API is relatively similar, but not the same. The rough
          equivalents are:
            universalvideoplayer_opengl_cb_init_gl => universalvideoplayer_render_context_create
            universalvideoplayer_opengl_cb_set_update_callback => universalvideoplayer_render_context_set_update_callback
            universalvideoplayer_opengl_cb_draw => universalvideoplayer_render_context_render
            universalvideoplayer_opengl_cb_report_flip => universalvideoplayer_render_context_report_swap
            universalvideoplayer_opengl_cb_uninit_gl => universalvideoplayer_render_context_free
          The VO opengl-cb is also renamed to "libuniversalvideoplayer".
          Also, the GL_MP_MPGetNativeDisplay pseudo extension is not used by the
          render API anymore, and the old opengl-cb API only handles the "x11"
          and "wl" names anymore. Support for everything else has been removed.
          The new render API uses proper API parameters, e.g. for X11 you pass
          UNIVERSALVIDEOPLAYER_RENDER_PARAM_X11_DISPLAY directly.
        - deprecate the qthelper.hpp header file. This provided some C++ helper
          utility functions for Qt with use of libuniversalvideoplayer. There is no reason to
          keep this in the universalvideoplayer git repository, nor to make it part of the libuniversalvideoplayer
          API. If you're using this header, you can safely copy it into your
          project - it uses only libuniversalvideoplayer public API. Alternatively, it could be
          maintained in a separate repository by interested parties.
 1.27   - make opengl-cb the default VO. This causes a subtle behavior change
          if the API user called universalvideoplayer_opengl_cb_init_gl(), but does not set
          the "vo" option. Before, it would still have used another VO (like
          on the CLI, e.g. vo=gpu). Now it'll behave as if vo=opengl-cb was
          used.
 --- universalvideoplayer 0.28.0 ---
 1.26   - remove glMPGetNativeDisplay("drm") support
        - add universalvideoplayer_opengl_cb_window_pos and universalvideoplayer_opengl_cb_drm_params and
          support via glMPGetNativeDisplay() for using it
        - make --stop-playback-on-init-failure=no the default in libuniversalvideoplayer (just
          like in universalvideoplayer CLI)
 --- universalvideoplayer 0.27.0 ---
 1.25   - remove setting "no-" options via universalvideoplayer_set_option*(). (See corresponding
          deprecation in 0.23.0.)
 --- universalvideoplayer 0.25.0 ---
 1.24   - add a UNIVERSALVIDEOPLAYER_ENABLE_DEPRECATED preprocessor symbol, which can be defined
          by the user to exclude deprecated API symbols from the C headers
 --- universalvideoplayer 0.23.0 ---
 1.24   - the deprecated universalvideoplayer_suspend() and universalvideoplayer_resume() APIs now do nothing.
 --- universalvideoplayer 0.22.0 ---
 1.23   - deprecate setting "no-" options via universalvideoplayer_set_option*(). For example,
          instead of "no-video=" you should set "video=no".
        - do not override the SIGPIPE signal handler anymore. This was done as
          workaround for the FFmpeg TLS code, which has been fixed long ago.
        - deprecate universalvideoplayer_suspend() and universalvideoplayer_resume(). They will be stubbed out
          in universalvideoplayer 0.23.0.
        - make universalvideoplayer_set_property() work to some degree before universalvideoplayer_initialize().
          It can now be used instead of universalvideoplayer_set_option().
        - semi-deprecate universalvideoplayer_set_option()/universalvideoplayer_set_option_string(). You should
          use universalvideoplayer_set_property() instead. There are some deprecated properties
          which conflict with some options (see client.h remarks on
          universalvideoplayer_set_option()), for which universalvideoplayer_set_option() might still be required.
          In future universalvideoplayer releases, the conflicting deprecated options/properties
          will be removed, and universalvideoplayer_set_option() will internally translate API
          calls to universalvideoplayer_set_property().
        - qthelper.hpp: deprecate get_property_variant, set_property_variant,
          set_option_variant, command_variant, and replace them with
          get_property, set_property, command.
 --- universalvideoplayer 0.19.0 ---
 1.22   - add stream_cb API for custom protocols
 --- universalvideoplayer 0.18.1 ---
 ----   - remove "status" log level from universalvideoplayer_request_log_messages() docs. This
          is 100% equivalent to "v". The behavior is still the same, thus no
          actual API change.
 --- universalvideoplayer 0.18.0 ---
 1.21   - universalvideoplayer_set_property() changes behavior with UNIVERSALVIDEOPLAYER_FORMAT_NODE. Before this
          change it rejected universalvideoplayer_nodes with format==UNIVERSALVIDEOPLAYER_FORMAT_STRING if the
          property was not a string or did not have special mechanisms in place
          the function failed. Now it always invokes the option string parser,
          and universalvideoplayer_node with a basic data type works exactly as if the function
          is invoked with that type directly. This new behavior is equivalent
          to universalvideoplayer_set_option().
          This also affects the mp.set_property_native() Lua function.
        - generally, setting choice options/properties with "yes"/"no" options
          can now be set as UNIVERSALVIDEOPLAYER_FORMAT_FLAG
        - reading a choice property as UNIVERSALVIDEOPLAYER_FORMAT_NODE will now return a
          UNIVERSALVIDEOPLAYER_FORMAT_FLAG value if the choice is "yes" (true) or "no" (false)
          This implicitly affects Lua and JSON IPC interfaces as well.
        - big changes to vo-cmdline on vo_opengl and vo_opengl_hq (but not
          vo_opengl_cb): options are now normally not reset, but applied on top
          of the current options. The special undocumented value "-" still
          works, but now resets all options to before any vo-cmdline command
          has been called.
 --- universalvideoplayer 0.12.0 ---
 1.20   - deprecate "GL_MP_D3D_interfaces"/"glMPGetD3DInterface", and introduce
          "GL_MP_MPGetNativeDisplay"/"glMPGetNativeDisplay" (this is a
          backwards-compatible rename)
 --- universalvideoplayer 0.11.0 ---
 --- universalvideoplayer 0.10.0 ---
 1.19   - add "GL_MP_D3D_interfaces" pseudo extension to make it possible to
          use DXVA2 in OpenGL fullscreen mode in some situations
        - universalvideoplayer_request_log_messages() now accepts "terminal-default" as parameter
 1.18   - add UNIVERSALVIDEOPLAYER_END_FILE_REASON_REDIRECT, and change behavior of
          UNIVERSALVIDEOPLAYER_EVENT_END_FILE accordingly
        - a bunch of interface-changes.rst changes
 1.17   - universalvideoplayer_initialize() now blocks SIGPIPE (details see client.h)
 --- universalvideoplayer 0.9.0 ---
 1.16   - add universalvideoplayer_opengl_cb_report_flip()
        - introduce universalvideoplayer_opengl_cb_draw() and deprecate universalvideoplayer_opengl_cb_render()
        - add UNIVERSALVIDEOPLAYER_FORMAT_BYTE_ARRAY
 1.15   - universalvideoplayer_initialize() will now load config files. This requires setting
          the "config" and "config-dir" options. In particular, it will load
          universalvideoplayer.conf.
        - minor backwards-compatible change to the "seek" and "screenshot"
          commands (new flag syntax, old additional args deprecated)
 --- universalvideoplayer 0.8.0 ---
 1.14   - add universalvideoplayer_wait_async_requests()
        - the --msg-level option changes its native type from a flat string to
          a key-value list (setting/reading the option as string still works)
 1.13   - add UNIVERSALVIDEOPLAYER_EVENT_QUEUE_OVERFLOW
 1.12   - add class Handle to qthelper.hpp
        - improve opengl_cb.h API uninitialization behavior, and fix the qml
          example
        - add universalvideoplayer_create_client() function
 1.11   - add OpenGL rendering interop API - allows an application to combine
          its own and universalvideoplayer's OpenGL rendering
          Warning: this API is not stable yet - anything in opengl_cb.h might
                   be changed in completely incompatible ways in minor API bumps
 --- universalvideoplayer 0.7.0 ---
 1.10   - deprecate/disable everything directly related to script_dispatch
          (most likely affects nobody)
 1.9    - add enum universalvideoplayer_end_file_reason for universalvideoplayer_event_end_file.reason
        - add UNIVERSALVIDEOPLAYER_END_FILE_REASON_ERROR and the universalvideoplayer_event_end_file.error field
          for slightly better error reporting on playback failure
        - add --stop-playback-on-init-failure option, and make it the default
          behavior for libuniversalvideoplayer only
        - add qthelper.hpp set_option_variant()
        - mark the following events as deprecated:
            UNIVERSALVIDEOPLAYER_EVENT_TRACKS_CHANGED
            UNIVERSALVIDEOPLAYER_EVENT_TRACK_SWITCHED
            UNIVERSALVIDEOPLAYER_EVENT_PAUSE
            UNIVERSALVIDEOPLAYER_EVENT_UNPAUSE
            UNIVERSALVIDEOPLAYER_EVENT_METADATA_UPDATE
            UNIVERSALVIDEOPLAYER_EVENT_CHAPTER_CHANGE
          They are handled better with universalvideoplayer_observe_property() as mentioned in
          the documentation comments. They are not removed and still work.
 1.8    - add qthelper.hpp
 1.7    - add universalvideoplayer_command_node(), universalvideoplayer_command_node_async()
 1.6    - modify "core-idle" property behavior
        - UNIVERSALVIDEOPLAYER_EVENT_LOG_MESSAGE now always sends complete lines
        - introduce numeric log levels (universalvideoplayer_log_level)
 --- universalvideoplayer 0.6.0 ---
 1.5    - change in X11 and "--wid" behavior again. The previous change didn't
          work as expected, and now the behavior can be explicitly controlled
          with the "input-x11-keyboard" option. This is only a temporary
          measure until XEmbed is implemented and confirmed working.
          Note: in 1.6, "input-x11-keyboard" was renamed to "input-vo-keyboard",
          although the old option name still works.
 1.4    - subtle change in X11 and "--wid" behavior
          (this change was added to 0.5.2, and broke some things, see #1090)
 --- universalvideoplayer 0.5.0 ---
 1.3    - add UNIVERSALVIDEOPLAYER_MAKE_VERSION()
 1.2    - remove "stream-time-pos" property (no replacement)
 1.1    - remap dvdnav:// to dvd://
        - add "--cache-file", "--cache-file-size"
        - add "--colormatrix-primaries" (and property)
        - add "primaries" sub-field to image format properties
        - add "playback-time" property
        - extend the "--start" option; a leading "+", which was previously
          insignificant is now significant
        - add "cache-free" and "cache-used" properties
        - macOS: the "coreaudio" AO spdif code is split into a separate AO
 --- universalvideoplayer 0.4.0 ---
 1.0    - the API is declared stable
