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

#include "w32_register.h"

#include <windows.h>
#include <knownfolders.h>
#include <pathcch.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <common/msg.h>
#include <options/options.h>
#include <osdep/io.h>
#include <player/core.h>
#include <stream/stream.h>
#include <version.h>

#include "windows_utils.h"

#define UNIVERSALVIDEOPLAYER_NAME L"universalvideoplayer"
#define UNIVERSALVIDEOPLAYER_FRIENDLY_NAME L"universalvideoplayer media player"

#define KEY_AUTOPLAY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers"
#define KEY_UNIVERSALVIDEOPLAYER_APP L"Software\\Classes\\Applications\\" UNIVERSALVIDEOPLAYER_NAME L".exe"
#define KEY_UNIVERSALVIDEOPLAYER_APP_PATHS L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"
#define KEY_UNIVERSALVIDEOPLAYER_APP_PATH(suffix) KEY_UNIVERSALVIDEOPLAYER_APP_PATHS UNIVERSALVIDEOPLAYER_NAME suffix
#define KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES_APP L"Software\\Clients\\Media\\" UNIVERSALVIDEOPLAYER_NAME
#define KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES_APP L"\\Capabilities"
#define KEY_UNIVERSALVIDEOPLAYER_UNINSTALL L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" UNIVERSALVIDEOPLAYER_NAME

#define UNIVERSALVIDEOPLAYER_PROG_ID_PREFIX L"io.universalvideoplayer."
#define UNIVERSALVIDEOPLAYER_PROG_ID(h) UNIVERSALVIDEOPLAYER_PROG_ID_PREFIX h
#define KEY_UNIVERSALVIDEOPLAYER_PROG_ID(h) L"Software\\Classes\\" UNIVERSALVIDEOPLAYER_PROG_ID(h)

struct w32_register_opts {
    bool register_opt;
    char *rpath;
    bool unregister;
};

#define OPT_BASE_STRUCT struct w32_register_opts
const struct m_sub_options w32_register_conf = {
    .opts = (const struct m_option[]) {
        {"register", OPT_BOOL(register_opt)},
        {"register-rpath", OPT_STRING(rpath)},
        {"unregister", OPT_BOOL(unregister)},
        {0}
    },
    .size = sizeof(struct w32_register_opts)
};

static bool is_admin(void)
{
    PSID sid;
    SID_IDENTIFIER_AUTHORITY identifier_authority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&identifier_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
    {
        return false;
    }

    BOOL member = false;
    CheckTokenMembership(NULL, sid, &member);
    if (sid)
        FreeSid(sid);

    return member;
}

static void create_shortcut(struct mp_log *log, const wchar_t *target,
                            const wchar_t *shortcut)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to initialize COM: %s\n",
               mp_HRESULT_to_str(hr));
        return;
    }

    IShellLinkW *shell_link = NULL;
    IPersistFile *persist_file = NULL;
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void **)&shell_link);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to create IShellLinkW instance: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IShellLinkW_SetPath(shell_link, target);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to set target path: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPersistFile,
                                    (LPVOID *)&persist_file);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to get IPersistFile interface: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IPersistFile_Save(persist_file, shortcut, TRUE);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to save shortcut: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    mp_msg(log, MSGL_V, "Shortcut created: '%ls' -> '%ls'\n", shortcut, target);

done:
    SAFE_RELEASE(persist_file);
    SAFE_RELEASE(shell_link);
    CoUninitialize();
}

static void reg_add(struct mp_log *log, HKEY key, LPCWSTR sub_key, LPCWSTR name,
                    DWORD type, const BYTE *data, DWORD size)
{
    HKEY sub_key_handle;
    if (RegCreateKeyExW(key, sub_key, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &sub_key_handle,
                        NULL) != ERROR_SUCCESS)
    {
        mp_msg(log, MSGL_ERR, "Failed to create registry key\n");
        return;
    }

    if (RegSetValueExW(sub_key_handle, name, 0, type, data, size) != ERROR_SUCCESS)
        mp_msg(log, MSGL_ERR, "Failed to set registry value\n");

    RegCloseKey(sub_key_handle);
}

static void reg_add_str(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                        LPCWSTR name, LPCWSTR value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    mp_msg(log, MSGL_V, "Adding registry key: %ls\\%ls\\%ls -> '%ls'\n", root,
           sub_key, name ? name : L"(Default)", value);
    reg_add(log, key, sub_key, name, REG_SZ, (const BYTE *)value,
            (wcslen(value) + 1) * sizeof(WCHAR));
}

static void reg_add_dwr(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                        LPCWSTR name, DWORD value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    mp_msg(log, MSGL_V, "Adding registry key: %ls\\%ls\\%ls -> %lu\n", root,
           sub_key, name ? name : L"(Default)", value);
    reg_add(log, key, sub_key, name, REG_DWORD, (const BYTE *)&value,
            sizeof(DWORD));
}

static void reg_del(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                    LPCWSTR value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    const char *name    = value ? "value" : "key";
    mp_msg(log, MSGL_V, "Removing registry %s: %ls\\%ls%ls%ls\n", name, root,
           sub_key, value ? L"\\" : L"", value ? value : L"");

    LSTATUS status = value ? RegDeleteKeyValueW(key, sub_key, value)
                           : RegDeleteTreeW(key, sub_key);
    if (status != ERROR_SUCCESS) {
        mp_msg(log, status == ERROR_FILE_NOT_FOUND ? MSGL_DEBUG : MSGL_ERR,
               "Failed to delete registry %s: %ls\\%ls%ls%ls -> %s\n", name,
               root, sub_key, value ? L"\\" : L"", value ? value : L"",
               mp_HRESULT_to_str(HRESULT_FROM_WIN32(status)));
    }
}

static uint64_t get_file_size(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
        return 0;

    return ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
}

static wchar_t *w32_get_shortcut_path(struct mp_log *log)
{
    LPWSTR shortcut_path = NULL;
    LPWSTR programs_path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Programs, 0, NULL, &programs_path))) {
        PathAllocCombine(programs_path, UNIVERSALVIDEOPLAYER_NAME L".lnk",
                         PATHCCH_ALLOW_LONG_PATHS, &shortcut_path);
    } else {
        mp_msg(log, MSGL_ERR, "Failed to get Programs folder path.\n");
    }
    CoTaskMemFree(programs_path);
    return shortcut_path;
}

static void remove_registration(struct mp_log *log, HKEY root)
{
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".exe"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".com"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_APP, NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES_APP, NULL);
    reg_del(log, root, L"Software\\RegisteredApplications", UNIVERSALVIDEOPLAYER_NAME);

    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("bluray"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("cdda"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("dvd"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("dvda"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("file"), NULL);
    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url"), NULL);

    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlayCDAudioOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayCDAudioOnArrival",
            L"UniversalVideoPlayerPlayCDAudioOnArrival");
    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlayDVDMovieOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayDVDMovieOnArrival",
            L"UniversalVideoPlayerPlayDVDMovieOnArrival");
    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlayDVDAudioOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayDVDAudioOnArrival",
            L"UniversalVideoPlayerPlayDVDAudioOnArrival");
    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlayBluRayOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayBluRayOnArrival",
            L"UniversalVideoPlayerPlayBluRayOnArrival");

    wchar_t *shortcut_path = w32_get_shortcut_path(log);
    if (shortcut_path) {
        mp_msg(log, MSGL_V, "Removing shortcut: '%ls'\n", shortcut_path);
        if (!DeleteFileW(shortcut_path)) {
            DWORD err = GetLastError();
            mp_msg(log, err == ERROR_FILE_NOT_FOUND ? MSGL_DEBUG : MSGL_ERR,
                   "Failed to delete shortcut: '%ls' -> %s\n", shortcut_path,
                   mp_LastError_to_str());
        }
        LocalFree(shortcut_path);
    }

    reg_del(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, NULL);
}

#define REGISTER_AUTOPLAY_HANDLER(media, event, action)                        \
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID(media) L"\\shell\\open\\command",   \
                NULL, L"universalvideoplayer.exe " media L":// --" media L"-device=%L");        \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlay" event "OnArrival",         \
                L"Action", L"" action);                                        \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlay" event "OnArrival",         \
                L"DefaultIcon", icon_path);                                    \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlay" event "OnArrival",         \
                L"InvokeProgID", UNIVERSALVIDEOPLAYER_PROG_ID(media));                          \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlay" event "OnArrival",         \
                L"InvokeVerb", L"open");                                       \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\UniversalVideoPlayerPlay" event "OnArrival",         \
                L"Provider", UNIVERSALVIDEOPLAYER_FRIENDLY_NAME);                               \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\EventHandlers\\Play" event "OnArrival",       \
                L"UniversalVideoPlayerPlay" event "OnArrival", L"");

static void w32_register(struct MPContext *mpctx)
{
    void *tmp = talloc_new(NULL);
    struct mp_log *log = mp_log_new(tmp, mpctx->log, "win32");

    LPWSTR universalvideoplayer_path = talloc_array(tmp, wchar_t, MP_PATH_MAX);
    DWORD universalvideoplayer_path_len = GetModuleFileNameW(NULL, universalvideoplayer_path, MP_PATH_MAX);
    if (!universalvideoplayer_path_len || universalvideoplayer_path_len == MP_PATH_MAX) {
        mp_msg(log, MSGL_ERR, "Failed to get universalvideoplayer path.\n");
        return;
    }
    LPWSTR icon_path = talloc_array(tmp, wchar_t, universalvideoplayer_path_len + 3);
    memcpy(icon_path, universalvideoplayer_path, universalvideoplayer_path_len * sizeof(wchar_t));
    icon_path[universalvideoplayer_path_len + 0] = L',';
    icon_path[universalvideoplayer_path_len + 1] = L'0';
    icon_path[universalvideoplayer_path_len + 2] = L'\0';

    HKEY root = is_admin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    mp_msg(log, MSGL_INFO, "Registering universalvideoplayer for %s...\n",
           root == HKEY_LOCAL_MACHINE ? "all users" : "the current user");

    remove_registration(log, root);

    // Register universalvideoplayer as an application
    // <https://learn.microsoft.com/windows/win32/shell/app-registration>
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".exe"), NULL, universalvideoplayer_path);
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".exe"), L"UseUrl", 1);

    char *rpath = mpctx->opts->w32_register_opts->rpath;
    if (rpath) {
        wchar_t *rpath_w = mp_from_utf8(tmp, rpath);
        reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".exe"), L"Path", rpath_w);
        reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".com"), L"Path", rpath_w);
    }

    // Name of the "Open With" entry in the context menu
    // I prefer a simple "universalvideoplayer" here, so let's skip this.
    // reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP, L"FriendlyAppName", UNIVERSALVIDEOPLAYER_FRIENDLY_NAME);

    // Add shell open command
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP L"\\shell\\open\\command", NULL,
                L"universalvideoplayer.exe -- \"%L\"");

    // Register universalvideoplayer capabilities and handlers

    // Note that we do not register any extensions or protocols directly,
    // as universalvideoplayer does not own any. It is simply a media player that can open them.
    // Windows will prompt the user to choose the default application for each
    // file type encountered.

    // Register only a single URL and file handler; there is no need to
    // duplicate this for each supported protocol or file type.

    // Register URL handler
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url"), NULL, L"URL:universalvideoplayer");
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url"), L"EditFlags", FTA_Show);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url"), L"FriendlyTypeName",
                L"universalvideoplayer URL handler");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url") L"\\shell\\open\\command",
                NULL, L"universalvideoplayer.exe -- \"%L\"");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("url") L"\\shell\\open\\ddeexec",
                NULL, L"");

    // Register URL protocols
    char **safe_protocols = talloc_steal(tmp, stream_get_proto_list(true));
    mp_assert(safe_protocols);
    char *protocols_str = NULL;
    for (int i = 0; safe_protocols[i]; ++i) {
        mp_require(strlen(safe_protocols[i]) < 41);
        char *protocol_key = mp_tprintf(61, "Software\\Classes\\%s", safe_protocols[i]);
        wchar_t *protocol_key_w = mp_from_utf8(tmp, protocol_key);

        HKEY key;
        LRESULT res = RegOpenKeyExW(root, protocol_key_w, 0, KEY_READ, &key);
        if (res == ERROR_FILE_NOT_FOUND) {
            // Add protocol definition only if it doesn't already exist
            char *protocol_name = mp_tprintf(61, "URL:%s", safe_protocols[i]);
            reg_add_str(log, root, protocol_key_w, NULL, mp_from_utf8(tmp, protocol_name));
            reg_add_str(log, root, protocol_key_w, L"URL Protocol", L"");
            reg_add_dwr(log, root, protocol_key_w, L"EditFlags", FTA_Show);
        } else if (res == ERROR_SUCCESS) {
            RegCloseKey(key);
        }

        reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES L"\\URLAssociations",
                    mp_from_utf8(tmp, safe_protocols[i]),
                    UNIVERSALVIDEOPLAYER_PROG_ID("url"));
        if (!protocols_str) {
            protocols_str = talloc_strdup(tmp, safe_protocols[i]);
        } else {
            protocols_str = talloc_asprintf_append(protocols_str, ":%s",
                                                   safe_protocols[i]);
        }
    }
    if (protocols_str) {
        reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP_PATH(".exe"), L"SupportedProtocols",
                    mp_from_utf8(tmp, protocols_str));
    }

    // Register file handler. Without a name, so it will be identified by its extension.
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("file"), L"EditFlags", FTA_OpenIsSafe);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_PROG_ID("file") L"\\shell\\open\\command",
                NULL, L"universalvideoplayer.exe -- \"%L\"");

    // Register file associations
    char *disc_exts[] = {
#if HAVE_DVDNAV || HAVE_DVDA || HAVE_LIBBLURAY
        "iso",
#endif
#if HAVE_DVDNAV || HAVE_DVDA
        "ifo",
#endif
#if HAVE_LIBBLURAY
        "bdmv",
#endif
        NULL
    };
    char **exts_groups[] = {
#if HAVE_LIBARCHIVE
        mpctx->opts->archive_exts,
#endif
        mpctx->opts->audio_exts,
        disc_exts,
        mpctx->opts->image_exts,
        mpctx->opts->playlist_exts,
        mpctx->opts->video_exts,
    };
    for (int i = 0; i < MP_ARRAY_SIZE(exts_groups); ++i) {
        char **exts = exts_groups[i];
        for (int j = 0; exts && exts[j]; ++j) {
            wchar_t *ext = mp_from_utf8(tmp, mp_tprintf(10, ".%s", exts[j]));
            reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_APP L"\\SupportedTypes", ext, L"");
            reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES L"\\FileAssociations",
                        ext, UNIVERSALVIDEOPLAYER_PROG_ID("file"));
        }
    }

    // Connect above handlers to the application
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES, L"ApplicationName", UNIVERSALVIDEOPLAYER_NAME);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES, L"ApplicationDescription", UNIVERSALVIDEOPLAYER_FRIENDLY_NAME);

    // Register the application
    reg_add_str(log, root, L"Software\\RegisteredApplications", UNIVERSALVIDEOPLAYER_NAME, KEY_UNIVERSALVIDEOPLAYER_CAPABILITIES);

    // <https://learn.microsoft.com/windows/win32/shell/how-to-register-an-event-handler>

#if HAVE_CDDA
    // Register CDDA handler
    REGISTER_AUTOPLAY_HANDLER("cdda", "CDAudio", "Play CD-Audio");
#endif

#if HAVE_DVDNAV
    // Register DVD handler
    REGISTER_AUTOPLAY_HANDLER("dvd", "DVDMovie", "Play DVD movie");
#endif

#if HAVE_DVDA
    // Register DVD-Audio handler
    REGISTER_AUTOPLAY_HANDLER("dvda", "DVDAudio", "Play DVD-Audio");
#endif

#if HAVE_LIBBLURAY
    // Register Blu-ray handler
    REGISTER_AUTOPLAY_HANDLER("bluray", "BluRay", "Play Blu-ray movie");
#endif

    // Create a shortcut in the Start Menu
    // Note that this is required for SystemMediaTransportControls to detect the
    // app correctly. Which is quite stupid, but it's the only way to make it work.
    wchar_t *shortcut_path = w32_get_shortcut_path(log);
    if (shortcut_path) {
        create_shortcut(log, universalvideoplayer_path, shortcut_path);
        LocalFree(shortcut_path);
    }

    // Register uninstaller
    // <https://learn.microsoft.com/windows/win32/msi/uninstall-registry-key>
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"DisplayName", L"universalvideoplayer media player");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"DisplayIcon", icon_path);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"DisplayVersion", L"" VERSION);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"Version", L"" VERSION);
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"Language", 1033);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"UninstallString", L"universalvideoplayer.exe --no-config --unregister");
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"NoModify", 1);
    reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"NoRepair", 1);
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"Publisher", L"universalvideoplayer");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"HelpLink", L"https://universalvideoplayer.io/manual");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"Readme", L"https://universalvideoplayer.io/community");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"URLInfoAbout", L"https://universalvideoplayer.io");
    reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"URLUpdateInfo", L"https://universalvideoplayer.io/installation");

    uint64_t size = get_file_size(universalvideoplayer_path);
    if (size)
        reg_add_dwr(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"EstimatedSize", size / 1024);

    if (SUCCEEDED(PathCchRemoveFileSpec(universalvideoplayer_path, MP_PATH_MAX)))
        reg_add_str(log, root, KEY_UNIVERSALVIDEOPLAYER_UNINSTALL, L"InstallLocation", universalvideoplayer_path);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    mp_msg(log, MSGL_INFO, "universalvideoplayer has been successfully registered.\n");
    mp_msg(log, MSGL_INFO, "Note: The universalvideoplayer binary has not been copied or moved. Please keep it in the current directory.\n");
    mp_msg(log, MSGL_INFO, "If you move it, simply run this process again to re-register.\n");

    talloc_free(tmp);
}

static void w32_unregister(struct MPContext *mpctx)
{
    struct mp_log *log = mp_log_new(NULL, mpctx->log, "win32");

    HKEY root = is_admin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    mp_msg(log, MSGL_INFO, "Unregistering universalvideoplayer for %s...\n",
           root == HKEY_LOCAL_MACHINE ? "all users" : "the current user");

    remove_registration(log, root);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    mp_msg(log, MSGL_INFO, "universalvideoplayer has been successfully unregistered.\n");
    mp_msg(log, MSGL_INFO, "You may now safely delete the universalvideoplayer binary if desired.\n");

    talloc_free(log);
}

bool mp_w32_handle_register(struct MPContext *mpctx)
{
    struct w32_register_opts *opts = mpctx->opts->w32_register_opts;
    if (!opts->register_opt && !opts->unregister)
        return false;

    if (opts->register_opt) {
        w32_register(mpctx);
    } else if (opts->unregister) {
        w32_unregister(mpctx);
    }

    return true;
}
