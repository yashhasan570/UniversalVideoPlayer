EMBEDDING INTO OTHER PROGRAMS (LIBUNIVERSALVIDEOPLAYER)
======================================

universalvideoplayer can be embedded into other programs as video/audio playback backend. The
recommended way to do so is using libuniversalvideoplayer. See ``include/universalvideoplayer/client.h`` in the universalvideoplayer
source code repository. This provides a C API. Bindings for other languages
might be available (see wiki).

Since libuniversalvideoplayer merely allows access to underlying mechanisms that can control
universalvideoplayer, further documentation is spread over a few places:

- https://github.com/yashhasan570/Universal Video Player/blob/master/include/universalvideoplayer/client.h
- https://universalvideoplayer.io/manual/master/#options
- https://universalvideoplayer.io/manual/master/#list-of-input-commands
- https://universalvideoplayer.io/manual/master/#properties
- https://github.com/yashhasan570/Universal Video Player-examples/tree/master/libuniversalvideoplayer

C PLUGINS
=========

You can write C plugins for universalvideoplayer. These use the libuniversalvideoplayer API, although they do not
use the libuniversalvideoplayer library itself.

They are enabled by default if compiler supports linking with the ``-rdynamic``
flag on Linux/BSD platforms. On Windows they are always enabled.

C plugins location
------------------

C plugins are put into the universalvideoplayer scripts directory in its config directory
(see the `FILES`_ section for details). They must have a ``.so`` or ``.dll``
file extension. They can also be explicitly loaded with the ``--script`` option.

API
---

A C plugin must export the following function::

    int universalvideoplayer_open_cplugin(universalvideoplayer_handle *handle)

The plugin function will be called on loading time. This function must not
return as long as your plugin is loaded (it runs in its own thread). The
``handle`` will be deallocated as soon as the plugin function returns.

The return value is interpreted as error status. A value of ``0`` is
interpreted as success, while ``-1`` signals an error. In the latter case,
the player prints an uninformative error message that loading failed.

Return values other than ``0`` and ``-1`` are reserved, and trigger undefined
behavior.

Within the plugin function, you can call libuniversalvideoplayer API functions. The ``handle``
is created by ``universalvideoplayer_create_client()`` (or actually an internal equivalent),
and belongs to you. You can call ``universalvideoplayer_wait_event()`` to wait for things
happening, and so on. However do not call ``universalvideoplayer_destroy()`` or
``universalvideoplayer_terminate_destroy()`` on this handle.

Note that the player might block until your plugin calls ``universalvideoplayer_wait_event()``
for the first time. This gives you a chance to install initial hooks etc.
before playback begins.

The details are quite similar to Lua scripts.

Linkage to libuniversalvideoplayer
-----------------

The current implementation requires that your plugins are **not** linked against
libuniversalvideoplayer. What your plugins use are not symbols from a libuniversalvideoplayer binary, but
symbols from the universalvideoplayer host binary.

On Windows to make symbols from the host binary available, you have to define
UNIVERSALVIDEOPLAYER_CPLUGIN_DYNAMIC_SYM when compiling cplugin. This will load symbols
dynamically, before calling ``universalvideoplayer_open_cplugin()``.

Examples
--------

See:

- https://github.com/yashhasan570/Universal Video Player-examples/tree/master/cplugins
