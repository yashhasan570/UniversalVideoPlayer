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

#include "smtc.h"

#include <chrono>
#include <format>
#include <utility>

#include <windows.h>
#include <systemmediatransportcontrolsinterop.h>
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic warning "-Wtemplate-body"
#endif
#include <winrt/Windows.Foundation.h>
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
#endif
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

extern "C" {
#include "common/msg.h"
#include "misc/node.h"
#include "misc/path_utils.h"
#include "osdep/threads.h"
#include "player/client.h"
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define WM_MP_EVENT (WM_USER + 1)

using namespace std::chrono_literals;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

struct universalvideoplayer_deleter {
    void operator()(void *ptr) const {
        universalvideoplayer_free(ptr);
    }
};
using mp_string = std::unique_ptr<char, universalvideoplayer_deleter>;

struct mp_node : universalvideoplayer_node {
    mp_node() = default;
    ~mp_node() {
        if (valid)
            universalvideoplayer_free_node_contents(static_cast<universalvideoplayer_node *>(this));
    }
    mp_node(const mp_node &) = delete;
    mp_node& operator=(const mp_node &) = delete;
    mp_node& operator=(mp_node &&) = delete;
    mp_node(mp_node &&o) noexcept
        : universalvideoplayer_node(static_cast<universalvideoplayer_node &>(o)),
          valid(std::exchange(o.valid, false)) {}

    bool valid = false;
};

template<universalvideoplayer_format F> struct mp_fmt;
template<> struct mp_fmt<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>   { using type = int; };
template<> struct mp_fmt<UNIVERSALVIDEOPLAYER_FORMAT_INT64>  { using type = int64_t; };
template<> struct mp_fmt<UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE> { using type = double; };
template<> struct mp_fmt<UNIVERSALVIDEOPLAYER_FORMAT_NODE>   { using type = mp_node; };

template<universalvideoplayer_format F>
static inline std::optional<typename mp_fmt<F>::type>
mp_get_property(universalvideoplayer_handle *universalvideoplayer, const char *name)
{
    typename mp_fmt<F>::type val;
    if (universalvideoplayer_get_property(universalvideoplayer, name, F, &val) != UNIVERSALVIDEOPLAYER_ERROR_SUCCESS)
        return std::nullopt;
    if constexpr (F == UNIVERSALVIDEOPLAYER_FORMAT_NODE)
        val.valid = true;
    return val;
}

template<typename T> struct mp_fmt_e;
template<> struct mp_fmt_e<int> { static constexpr universalvideoplayer_format value = UNIVERSALVIDEOPLAYER_FORMAT_FLAG; };
template<> struct mp_fmt_e<bool> { static constexpr universalvideoplayer_format value = UNIVERSALVIDEOPLAYER_FORMAT_FLAG; };
template<> struct mp_fmt_e<int64_t> { static constexpr universalvideoplayer_format value = UNIVERSALVIDEOPLAYER_FORMAT_INT64; };
template<> struct mp_fmt_e<double> { static constexpr universalvideoplayer_format value = UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE; };

template<typename T>
static inline int mp_set_property(universalvideoplayer_handle *universalvideoplayer, const char *name, T &&val)
{
    using val_t = std::remove_reference_t<T>;
    typename mp_fmt<mp_fmt_e<val_t>::value>::type universalvideoplayer_val = std::forward<T>(val);
    return universalvideoplayer_set_property(universalvideoplayer, name, mp_fmt_e<val_t>::value, &universalvideoplayer_val);
}

struct smtc_ctx {
    mp_log *log;
    universalvideoplayer_handle *universalvideoplayer;
    CO_MTA_USAGE_COOKIE mta{ nullptr };
    SystemMediaTransportControls smtc{ nullptr };
    IAsyncOperation<FileProperties::StorageItemThumbnail> thumb_async{ nullptr };
    std::atomic_bool close{ false };
    std::atomic<HWND> hwnd{ nullptr };

    ~smtc_ctx() {
        thumb_async = nullptr;
        smtc = nullptr;
        if (mta)
            CoDecrementMTAUsage(mta);
    }
};

static void update_state(SystemMediaTransportControls &smtc, universalvideoplayer_handle *universalvideoplayer)
{
    auto closed = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "idle-active");
    if (!closed.value_or(false)) {
        auto paused = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "pause");
        smtc.PlaybackStatus(paused.value_or(true) ? MediaPlaybackStatus::Paused : MediaPlaybackStatus::Playing);
        smtc.IsPlayEnabled(true);
        smtc.IsPauseEnabled(true);
        smtc.IsStopEnabled(true);
        auto ch_index = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter");
        auto ch_count = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter-list/count");
        auto pl_count = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "playlist-count");
        smtc.IsNextEnabled(pl_count > 1 || ch_count > ch_index.value_or(0));
        smtc.IsPreviousEnabled(pl_count > 1 || ch_index > 0);
        smtc.IsRewindEnabled(true);
    } else {
        smtc.PlaybackStatus(MediaPlaybackStatus::Closed);
        smtc.IsPlayEnabled(false);
        smtc.IsPauseEnabled(false);
        smtc.IsStopEnabled(false);
        smtc.IsNextEnabled(false);
        smtc.IsPreviousEnabled(false);
        smtc.IsRewindEnabled(false);
    }

    auto shuffle = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "shuffle");
    smtc.ShuffleEnabled(shuffle.value_or(false));

    auto speed = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE>(universalvideoplayer, "speed");
    smtc.PlaybackRate(speed.value_or(1.0));

    mp_string loop_file_opt{ universalvideoplayer_get_property_string(universalvideoplayer, "loop-file") };
    bool loop_file = loop_file_opt && strcmp(loop_file_opt.get(), "no");
    mp_string loop_playlist_opt{ universalvideoplayer_get_property_string(universalvideoplayer, "loop-playlist") };
    bool loop_playlist = loop_playlist_opt && strcmp(loop_playlist_opt.get(), "no");
    if (loop_file) {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::Track);
    } else if (loop_playlist) {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::List);
    } else {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::None);
    }

    auto pos = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE>(universalvideoplayer, "time-pos");
    auto duration = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE>(universalvideoplayer, "duration");

    if (!pos || !duration)
        return;

    SystemMediaTransportControlsTimelineProperties tl;
    tl.StartTime(0s);
    tl.MinSeekTime(0s);
    tl.Position(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*pos)));
    tl.MaxSeekTime(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*duration)));
    tl.EndTime(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*duration)));
    smtc.UpdateTimelineProperties(tl);
}

static void update_thumbnail(SystemMediaTransportControls &smtc, smtc_ctx &ctx)
{
    auto track_list = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_NODE>(ctx.universalvideoplayer, "track-list");
    if (!track_list)
        return;
    mp_assert(track_list->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY);
    auto list = track_list->u.list;
    const char *thumbnail = nullptr;
    mp_string filepath{ universalvideoplayer_get_property_string(ctx.universalvideoplayer, "path") };
    // Get first albumart or image from tracks
    for (int i = 0; i < list->num; ++i) {
        universalvideoplayer_node *img = node_map_get(list->values + i, "image");
        if (!img || img->format != UNIVERSALVIDEOPLAYER_FORMAT_FLAG || !img->u.flag)
            continue;

        // If this is the only track selected and image, try to use it directly
        if (list->num == 1)
            thumbnail = filepath.get();

        universalvideoplayer_node *file = node_map_get(list->values + i, "external-filename");
        if (!file || file->format != UNIVERSALVIDEOPLAYER_FORMAT_STRING || !file->u.string)
            continue;

        // Select first image found
        if (!thumbnail)
            thumbnail = file->u.string;

        universalvideoplayer_node *art = node_map_get(list->values + i, "albumart");
        if (!art || art->format != UNIVERSALVIDEOPLAYER_FORMAT_FLAG || !art->u.flag)
            continue;

        // Select first albumart found
        thumbnail = file->u.string;
        break;
    }

    try {
        if (thumbnail) {
            mp_string normalized_path{ mp_normalize_path(nullptr, thumbnail) };
            MP_TRACE(&ctx, "Using thumbnail: %s\n", normalized_path.get());
            auto hstr = winrt::to_hstring(normalized_path.get());
            RandomAccessStreamReference stream_ref{ nullptr };
            if (mp_is_url(bstr0(thumbnail))) {
                stream_ref = RandomAccessStreamReference::CreateFromUri(Uri(hstr));
            } else {
                auto storage_file = StorageFile::GetFileFromPathAsync(hstr);
                stream_ref = RandomAccessStreamReference::CreateFromFile(storage_file.get());
            }
            if (!stream_ref)
                return;
            auto updater = smtc.DisplayUpdater();
            updater.Thumbnail(stream_ref);
        } else {
            if (filepath && !mp_is_url(bstr0(filepath.get()))) {
                mp_string path{ mp_normalize_path(nullptr, filepath.get()) };
                MP_TRACE(&ctx, "Generating thumbnail for: %s\n", path.get());
                auto storage_file = StorageFile::GetFileFromPathAsync(winrt::to_hstring(path.get()));

                if (ctx.thumb_async)
                    ctx.thumb_async.Cancel();
                ctx.thumb_async = storage_file.get().GetThumbnailAsync(FileProperties::ThumbnailMode::SingleItem, 240);

                ctx.thumb_async.Completed([&ctx](auto &&async, const AsyncStatus &status) {
                    if (status != AsyncStatus::Completed || ctx.close)
                        return;
                    auto stream_ref = RandomAccessStreamReference::CreateFromStream(async.GetResults());
                    auto updater = ctx.smtc.DisplayUpdater();
                    updater.Thumbnail(stream_ref);
                    updater.Update();
                });
            }
        }
    } catch (const winrt::hresult_error& e) {
        MP_VERBOSE(&ctx, "%s: 0x%x - %ls\n", __func__, int32_t(e.code()), e.message().c_str());
    }
}

static void update_metadata(SystemMediaTransportControls &smtc, smtc_ctx &ctx)
{
    auto *universalvideoplayer = ctx.universalvideoplayer;
    auto updater = smtc.DisplayUpdater();
    updater.ClearAll();

    auto image_opt = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "current-tracks/video/image");
    bool video = bool(image_opt);
    bool image = image_opt.value_or(false);
    auto audio = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "current-tracks/audio/selected");

    if (!video && !image && !audio)
        return;

    update_thumbnail(smtc, ctx);

    mp_string title{ universalvideoplayer_get_property_osd_string(universalvideoplayer, "media-title") };
    if (video && !image) {
        updater.Type(MediaPlaybackType::Video);
        const auto &props = updater.VideoProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
        auto ch_index = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter").value_or(-1);
        if (ch_index >= 0) {
            mp_string ch_title {
                universalvideoplayer_get_property_string(universalvideoplayer, std::format("chapter-list/{}/title", ch_index).c_str())
            };
            if (ch_title) {
                auto ch_count = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter-list/count").value_or(0);
                props.Subtitle(winrt::to_hstring(std::format("{} ({}/{})", ch_title.get(), ch_index + 1, ch_count)));
            }
        }
    } else if (image && !audio) {
        updater.Type(MediaPlaybackType::Image);
        const auto &props = updater.ImageProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
    } else {
        updater.Type(MediaPlaybackType::Music);
        const auto &props = updater.MusicProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
        if (mp_string str{ universalvideoplayer_get_property_string(universalvideoplayer, "metadata/by-key/Album_Artist") })
            props.AlbumArtist(winrt::to_hstring(str.get()));
        if (mp_string str{ universalvideoplayer_get_property_string(universalvideoplayer, "metadata/by-key/Album") })
            props.AlbumTitle(winrt::to_hstring(str.get()));
        if (mp_string str{ universalvideoplayer_get_property_string(universalvideoplayer, "metadata/by-key/Album_Track_Count") })
            props.AlbumTrackCount(std::atoi(str.get()));
        if (mp_string str{ universalvideoplayer_get_property_string(universalvideoplayer, "metadata/by-key/Artist") })
            props.Artist(winrt::to_hstring(str.get()));
        if (mp_string str{ universalvideoplayer_get_property_string(universalvideoplayer, "metadata/by-key/Track") })
            props.TrackNumber(std::atoi(str.get()));
    }

    updater.Update();
}

static void handle_mp_event(smtc_ctx *ctx, universalvideoplayer_event *event)
{
    if (!ctx || !ctx->smtc || !ctx->universalvideoplayer || ctx->close)
        return;

    try {
        update_state(ctx->smtc, ctx->universalvideoplayer);
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE) {
            auto &prop = *static_cast<universalvideoplayer_event_property *>(event->data);
            if (!strcmp(prop.name, "time-pos") || !strcmp(prop.name, "duration"))
                return;
            if (!strcmp(prop.name, "media-controls")) {
                mp_assert(prop.format == UNIVERSALVIDEOPLAYER_FORMAT_FLAG);
                ctx->smtc.IsEnabled(*static_cast<int *>(prop.data));
                return;
            }
        }
        update_metadata(ctx->smtc, *ctx);
    } catch (const winrt::hresult_error& e) {
        MP_VERBOSE(ctx, "%s: 0x%x - %ls\n", __func__, int32_t(e.code()), e.message().c_str());
    }
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DESTROY)
        PostQuitMessage(0);

    smtc_ctx *ctx = reinterpret_cast<smtc_ctx *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_MP_EVENT:
        handle_mp_event(ctx, reinterpret_cast<universalvideoplayer_event *>(lParam));
        return 0;
    case WM_SETFOCUS:
        if (!ctx)
            return 0;
        if (auto wid { mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(ctx->universalvideoplayer, "window-id") })
            SetFocus(HWND(*wid));
        return 0;
    case WM_ACTIVATE:
        if (!ctx)
            return 0;
        if (auto wid { mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(ctx->universalvideoplayer, "window-id") }) {
            if (IsIconic(HWND(*wid)))
                ShowWindow(HWND(*wid), SW_RESTORE);
            BringWindowToTop(HWND(*wid));
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static MP_THREAD_VOID win_event_loop_fn(void *arg)
{
    mp_thread_set_name("smtc/win");
    auto &ctx = *static_cast<smtc_ctx *>(arg);
    auto *universalvideoplayer = ctx.universalvideoplayer;

    WNDCLASS wc = {
        .lpfnWndProc = WindowProc,
        .hInstance = HINSTANCE(&__ImageBase),
        .hIcon = LoadIconW(HINSTANCE(&__ImageBase), L"IDI_ICON1"),
        .lpszClassName = L"universalvideoplayer-smtc"
    };
    RegisterClassW(&wc);

    try {
        // Dummy window is used to allow SMTC to work also in audio only mode,
        // where VO may not be created.
        ctx.hwnd = CreateWindowExW(0, wc.lpszClassName, L"universalvideoplayer smtc",
                                   WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                   nullptr, nullptr, wc.hInstance, nullptr);
        if (!ctx.hwnd)
            winrt::throw_last_error();

        winrt::check_hresult(CoIncrementMTAUsage(&ctx.mta));

        SystemMediaTransportControls &smtc = ctx.smtc;
        auto interop = winrt::get_activation_factory<SystemMediaTransportControls,
                                                     ISystemMediaTransportControlsInterop>();
        HRESULT hr = interop->GetForWindow(ctx.hwnd,
                                           winrt::guid_of<SystemMediaTransportControls>(),
                                           winrt::put_abi(smtc));
        if (FAILED(hr))
            winrt::throw_hresult(hr);
        SetWindowLongPtrW(ctx.hwnd, GWLP_USERDATA, LONG_PTR(&ctx));

        smtc.IsEnabled(mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_FLAG>(universalvideoplayer, "media-controls").value_or(0));

        smtc.ButtonPressed([&](const SystemMediaTransportControls &,
                               const SystemMediaTransportControlsButtonPressedEventArgs &args) {
            switch (args.Button()) {
            case SystemMediaTransportControlsButton::Play:
                mp_set_property(universalvideoplayer, "pause", false);
                break;
            case SystemMediaTransportControlsButton::Pause:
                mp_set_property(universalvideoplayer, "pause", true);
                break;
            case SystemMediaTransportControlsButton::Stop:
                universalvideoplayer_command_string(universalvideoplayer, "stop");
                break;
            case SystemMediaTransportControlsButton::Next: {
                auto ch_index = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter").value_or(0);
                auto ch_count = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter-list/count");
                // universalvideoplayer allows to jump past last chapter
                universalvideoplayer_command_string(universalvideoplayer, ch_index < ch_count ? "add chapter 1" : "playlist-next");
                break;
            }
            case SystemMediaTransportControlsButton::Previous: {
                auto ch_index = mp_get_property<UNIVERSALVIDEOPLAYER_FORMAT_INT64>(universalvideoplayer, "chapter");
                universalvideoplayer_command_string(universalvideoplayer, ch_index > 0 ? "add chapter -1" : "playlist-prev");
                break;
            }
            default:
                break;
            }
        });
        smtc.PlaybackPositionChangeRequested([&](const SystemMediaTransportControls &,
                                                 const PlaybackPositionChangeRequestedEventArgs &args) {
            auto position = args.RequestedPlaybackPosition();
            auto pos = std::chrono::duration_cast<std::chrono::duration<double>>(position).count();
            mp_set_property(universalvideoplayer, "time-pos", pos);
        });
        smtc.PlaybackRateChangeRequested([&](const SystemMediaTransportControls &,
                                             const PlaybackRateChangeRequestedEventArgs &args) {
            mp_set_property(universalvideoplayer, "speed", args.RequestedPlaybackRate());
        });
        smtc.ShuffleEnabledChangeRequested([&](const SystemMediaTransportControls &,
                                               const ShuffleEnabledChangeRequestedEventArgs &args) {
            mp_set_property(universalvideoplayer, "shuffle", args.RequestedShuffleEnabled());
        });
        smtc.AutoRepeatModeChangeRequested([&](const SystemMediaTransportControls &,
                                               const AutoRepeatModeChangeRequestedEventArgs &args) {
            bool loop_file = false, loop_playlist = false;
            switch (args.RequestedAutoRepeatMode()) {
                case MediaPlaybackAutoRepeatMode::Track:
                    loop_file = true;
                    break;
                case MediaPlaybackAutoRepeatMode::List:
                    loop_playlist = true;
                    break;
                case MediaPlaybackAutoRepeatMode::None:
                    break;
            }
            mp_set_property(universalvideoplayer, "loop-file", loop_file);
            mp_set_property(universalvideoplayer, "loop-playlist", loop_playlist);
        });

        MSG msg;
        while (!ctx.close) {
            BOOL ret = GetMessageW(&msg, nullptr, 0, 0);
            if (!ret)
                break;
            if (ret == -1)
                winrt::throw_last_error();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } catch (const winrt::hresult_error& e) {
        MP_ERR(&ctx, "%s: 0x%x - %ls\n", __func__, int32_t(e.code()), e.message().c_str());
    }

    ctx.close = true;
    universalvideoplayer_wakeup(universalvideoplayer);
    HWND hwnd = ctx.hwnd;
    ctx.hwnd = nullptr;
    if (ctx.thumb_async)
        ctx.thumb_async.Cancel();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, HINSTANCE(&__ImageBase));

    MP_THREAD_RETURN();
}

static MP_THREAD_VOID universalvideoplayer_event_loop_fn(void *arg)
{
    mp_thread_set_name("smtc/universalvideoplayer");
    auto universalvideoplayer = static_cast<universalvideoplayer_handle *>(arg);
    smtc_ctx ctx = {
        .log = mp_client_get_log(universalvideoplayer),
        .universalvideoplayer = universalvideoplayer
    };

    // Nothing is created until the controls are enabled.
    universalvideoplayer_observe_property(universalvideoplayer, 0, "media-controls", UNIVERSALVIDEOPLAYER_FORMAT_FLAG);
    for (;;) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(universalvideoplayer, -1);
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN)
            goto done;
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE) {
            auto &prop = *static_cast<universalvideoplayer_event_property *>(event->data);
            if (prop.format == UNIVERSALVIDEOPLAYER_FORMAT_FLAG && *static_cast<int *>(prop.data))
                break;
        }
    }

    // Create a dedicated window and event loop. We could use the universalvideoplayer main window,
    // but it is not always available, especially in audio-only/console mode.
    mp_thread win_event_loop;
    if (mp_thread_create(&win_event_loop, win_event_loop_fn, &ctx)) {
        MP_ERR(&ctx, "Failed to create window event thread!\n");
        goto done;
    }

    // It is recommended that you keep the system controls in sync with your
    // media playback by updating these properties approximately every 5 seconds
    // during playback and again whenever the state of playback changes, such as
    // pausing or seeking to a new position.
    // https://learn.microsoft.com/windows/uwp/audio-video-camera/system-media-transport-controls
    // For simplicity we observe time-pos and duration as integers, so we get
    // update every second, faster than recommended, but should be fine.

    universalvideoplayer_observe_property(universalvideoplayer, 0, "current-tracks", UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "duration", UNIVERSALVIDEOPLAYER_FORMAT_INT64);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "idle-active", UNIVERSALVIDEOPLAYER_FORMAT_FLAG);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "media-title", UNIVERSALVIDEOPLAYER_FORMAT_STRING);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "metadata", UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "pause", UNIVERSALVIDEOPLAYER_FORMAT_FLAG);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "shuffle", UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "speed", UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "time-pos", UNIVERSALVIDEOPLAYER_FORMAT_INT64);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "track-list", UNIVERSALVIDEOPLAYER_FORMAT_NODE);
    // TODO: Options are not observable, fix me!
    universalvideoplayer_observe_property(universalvideoplayer, 0, "loop-file", UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE);
    universalvideoplayer_observe_property(universalvideoplayer, 0, "loop-playlist", UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE);

    while (!ctx.close) {
        universalvideoplayer_event *event = universalvideoplayer_wait_event(universalvideoplayer, -1);
        if (ctx.close)
            break;
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_SHUTDOWN) {
            ctx.close = true;
            HWND hwnd = ctx.hwnd;
            if (hwnd)
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        }
        if (event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PROPERTY_CHANGE ||
            event->event_id == UNIVERSALVIDEOPLAYER_EVENT_PLAYBACK_RESTART)
        {
            HWND hwnd = ctx.hwnd;
            if (hwnd)
                SendMessageW(hwnd, WM_MP_EVENT, 0, LPARAM(event));
        }
    }
    mp_thread_join(win_event_loop);

done:
    universalvideoplayer_destroy(universalvideoplayer);
    MP_THREAD_RETURN();
}

void mp_smtc_init(universalvideoplayer_handle *universalvideoplayer)
{
    mp_thread universalvideoplayer_event_loop;
    if (!mp_thread_create(&universalvideoplayer_event_loop, universalvideoplayer_event_loop_fn, universalvideoplayer))
        mp_thread_detach(universalvideoplayer_event_loop);
    else
        universalvideoplayer_destroy(universalvideoplayer);
}
