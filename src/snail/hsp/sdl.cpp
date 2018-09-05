#include "../detail/sdl.hpp"
#include <iostream>
#include <unordered_map>
#include "../application.hpp"
#include "../color.hpp"
#include "../font.hpp"
#include "../input.hpp"
#include "../touch_input.hpp"
#include "../window.hpp"


namespace
{

struct FontCacheKey
{
    FontCacheKey(int size, elona::snail::Font::Style style)
        : size(size)
        , style(style)
    {
    }


    int size;
    elona::snail::Font::Style style;


    bool operator==(const FontCacheKey& other) const
    {
        return size == other.size && style == other.style;
    }
};

} // namespace



namespace std
{

template <>
struct hash<FontCacheKey>
{
    size_t operator()(const FontCacheKey& key) const
    {
        return hash<int>()(key.size * 100 + int(key.style));
    }
};

} // namespace std

namespace strutil
{

inline size_t byte_count(uint8_t c)
{
    if (c <= 0x7F)
        return 1;
    else if (c >= 0xc2 && c <= 0xdf)
        return 2;
    else if (c >= 0xe0 && c <= 0xef)
        return 3;
    else if (c >= 0xf0 && c <= 0xf7)
        return 4;
    else if (c >= 0xf8 && c <= 0xfb)
        return 5;
    else if (c >= 0xfc && c <= 0xfd)
        return 6;
    else
        return 1;
}

inline std::string
replace(const std::string& str, const std::string& from, const std::string& to)
{
    auto ret{str};
    std::string::size_type pos{};
    while ((pos = ret.find(from, pos)) != std::string::npos)
    {
        ret.replace(pos, from.size(), to);
        pos += to.size();
    }

    return ret;
}

} // namespace strutil

namespace elona
{
namespace snail
{
namespace hsp
{

namespace detail
{
struct TexBuffer
{
    ::SDL_Texture* texture = nullptr;
    int tex_width = 32;
    int tex_height = 32;
    snail::Color color{0, 0, 0, 255};
    int x = 0;
    int y = 0;
    int mode = 2;
};


int current_buffer;
std::vector<TexBuffer> tex_buffers;
::SDL_Texture* tmp_buffer;
::SDL_Texture* tmp_buffer_slow;
::SDL_Texture* android_display_region;

TexBuffer& current_tex_buffer()
{
    return tex_buffers[current_buffer];
}

void setup_buffers()
{
    // Default buffer for high-frequency texture copies.
    detail::tmp_buffer = snail::detail::enforce_sdl(::SDL_CreateTexture(
        Application::instance().get_renderer().ptr(),
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        1024,
        1024));

    // Slow buffer for texture copies larger than 1024 pixels.
    // The only real places where this gets used seem to be when
    // rendering the message box and when rendering fullscreen
    // backgrounds.
    detail::tmp_buffer_slow = snail::detail::enforce_sdl(::SDL_CreateTexture(
        Application::instance().get_renderer().ptr(),
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        // The assumption here is that it's pointless to copy a texture
        // larger than the size of the screen, because the player wouldn't
        // see the rest of the texture. That should save some cycles on less
        // powerful GPUs.
        std::max(1024, Application::instance().width()),
        std::max(1024, Application::instance().height())));

    if (Application::is_android)
    {
        // Output texture for Android. This is so the game window can
        // be placed such that it covers only part of the actual
        // screen, or scaled up and down.
        detail::android_display_region =
            snail::detail::enforce_sdl(::SDL_CreateTexture(
                Application::instance().get_renderer().ptr(),
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_TARGET,
                Application::instance().width(),
                Application::instance().height()));
    }

    Application::instance().register_finalizer([]() {
        ::SDL_DestroyTexture(detail::tmp_buffer);
        ::SDL_DestroyTexture(detail::tmp_buffer_slow);
    });
}

::SDL_Texture* get_tmp_buffer(int width, int height)
{
    if (width > 1024 || height > 1024)
    {
        return tmp_buffer_slow;
    }
    else
    {
        return tmp_buffer;
    }
}


void set_blend_mode()
{
    switch (current_tex_buffer().mode)
    {
    case 0:
    case 1:
        Application::instance().get_renderer().set_blend_mode(BlendMode::none);
        break;
    case 2:
    case 3:
        Application::instance().get_renderer().set_blend_mode(BlendMode::blend);
        break;
    case 4:
        Application::instance().get_renderer().set_blend_mode(BlendMode::blend);
        break;
    case 5:
        Application::instance().get_renderer().set_blend_mode(BlendMode::add);
        break;
    case 6:
        Application::instance().get_renderer().set_blend_mode(BlendMode::blend);
        break;
    default: break;
    }
}


} // namespace detail

namespace mesbox_detail
{


struct MessageBox
{
    MessageBox(std::string& buffer, int keywait, bool text)
        : buffer(buffer)
        , keywait(keywait)
        , text(text)
    {
    }


    void update()
    {
        auto& input = Input::instance();
        buffer += input.pop_text();

        if (text)
        {
            if (!input.is_ime_active())
            {
                if (input.was_pressed_just_now(Key::enter)
                    || input.was_pressed_just_now(Key::keypad_enter))
                {
                    // New line.
                    buffer += '\n';
                }
                else if (input.was_pressed_just_now(Key::escape))
                {
                    // A tab character indicates input was canceled.
                    buffer += '\t';
                }
                else if (input.is_pressed(Key::backspace) && !buffer.empty())
                {
                    if (backspace_held_frames == 0
                        || (backspace_held_frames > 15
                            && backspace_held_frames % 2 == 0))
                    {
                        // Delete the last character.
                        size_t last_byte_count{};
                        for (size_t i = 0; i < buffer.size();)
                        {
                            const auto byte = strutil::byte_count(buffer[i]);
                            last_byte_count = byte;
                            i += byte;
                        }
                        buffer.erase(
                            buffer.size() - last_byte_count, last_byte_count);
                    }
                    backspace_held_frames++;
                }
                else if (
                    input.is_pressed(Key::key_v) && input.is_pressed(Key::ctrl))
                {
                    // Paste.
                    std::unique_ptr<char, decltype(&::SDL_free)> text_ptr{
                        ::SDL_GetClipboardText(), ::SDL_free};

                    buffer +=
                        strutil::replace(text_ptr.get(), u8"\r\n", u8"\n");
                }
                else if (!input.is_pressed(Key::backspace))
                {
                    backspace_held_frames = 0;
                }
            }
        }
        else
        {
            if (input.is_pressed(Key::enter, keywait)
                || input.is_pressed(Key::keypad_enter, keywait))
            {
                // New line.
                buffer += '\n';
            }
        }
    }


private:
    std::string& buffer;
    int keywait;
    bool text;
    int backspace_held_frames{};
};

std::vector<std::unique_ptr<MessageBox>> message_boxes;

} // namespace mesbox_detail

namespace font_detail
{
std::unordered_map<FontCacheKey, Font> font_cache;
}

// TODO place all this in detail

int timeGetTime()
{
    return ::SDL_GetTicks();
}

void mes(const std::string& text)
{
    constexpr size_t tab_width = 4;

    auto copy = text;
    for (auto i = copy.find('\t'); i != std::string::npos; i = copy.find('\t'))
    {
        copy.replace(i, 1, tab_width, ' ');
    }

    if (copy.size() >= 25 /* TODO */)
    {
        Application::instance().get_renderer().render_multiline_text(
            copy,
            detail::current_tex_buffer().x,
            detail::current_tex_buffer().y,
            detail::current_tex_buffer().color);
    }
    else
    {
        Application::instance().get_renderer().render_text(
            copy,
            detail::current_tex_buffer().x,
            detail::current_tex_buffer().y,
            detail::current_tex_buffer().color);
    }
}

void mesbox(std::string& buffer, int keywait, bool text)
{
    mesbox_detail::message_boxes.emplace_back(
        std::make_unique<mesbox_detail::MessageBox>(buffer, keywait, text));

    if (Application::is_android && text)
    {
        Input::instance().show_soft_keyboard();
    }
}

void picload(BasicImage& img, int mode)
{
    if (mode == 0)
    {
        buffer(detail::current_buffer, img.width(), img.height());
    }
    const auto save = Application::instance().get_renderer().blend_mode();
    Application::instance().get_renderer().set_blend_mode(BlendMode::none);
    Application::instance().get_renderer().render_image(
        img, detail::current_tex_buffer().x, detail::current_tex_buffer().y);

    Application::instance().get_renderer().set_blend_mode(save);
}

void pos(int x, int y)
{
    detail::current_tex_buffer().x = x;
    detail::current_tex_buffer().y = y;
}

static void redraw_android()
{
    auto& renderer = Application::instance().get_renderer();
    Rect pos = Application::instance().window_pos();

    renderer.set_render_target(nullptr);
    renderer.clear();
    renderer.render_image(
        detail::android_display_region, pos.x, pos.y, pos.width, pos.height);

    auto itr = font_detail::font_cache.find({14, Font::Style::regular});
    if (itr != std::end(font_detail::font_cache))
    {
        renderer.set_font(itr->second);
    }
    TouchInput::instance().draw_quick_actions();
}

void redraw()
{
    ::SDL_Texture* target = nullptr;
    if (Application::is_android)
    {
        target = detail::android_display_region;
    }

    auto& renderer = Application::instance().get_renderer();
    const auto save = renderer.render_target();
    renderer.set_render_target(target);
    renderer.set_draw_color(snail::Color{0, 0, 0, 255});
    renderer.clear();
    renderer.render_image(detail::tex_buffers[0].texture, 0, 0);

    if (Application::is_android)
    {
        redraw_android();
    }

    renderer.present();
    renderer.set_render_target(save);
}

void set_color_mod(int r, int g, int b, int window_id)
{
    window_id = window_id == -1 ? detail::current_buffer : window_id;

    r = clamp(r, 0, 255);
    g = clamp(g, 0, 255);
    b = clamp(b, 0, 255);
    snail::detail::enforce_sdl(::SDL_SetTextureColorMod(
        detail::tex_buffers[window_id].texture,
        uint8_t(r),
        uint8_t(g),
        uint8_t(b)));
}

void onkey_0()
{
    mesbox_detail::message_boxes.erase(
        std::end(mesbox_detail::message_boxes) - 1);

    if (Application::is_android)
    {
        Input::instance().hide_soft_keyboard();
    }
}

namespace await_detail
{
uint32_t last_await;
}

void await(int msec)
{
    Application::instance().proc_event();
    if (mesbox_detail::message_boxes.back())
        mesbox_detail::message_boxes.back()->update();

    const auto now = ::SDL_GetTicks();
    if (await_detail::last_await == 0)
    {
        await_detail::last_await = now;
    }
    const auto delta = now - await_detail::last_await;
    if (size_t(msec) > delta)
    {
        ::SDL_Delay(msec - delta);
    }
    await_detail::last_await = now;
}

void boxf(int x, int y, int width, int height, const snail::Color& color)
{
    const auto save_color = detail::current_tex_buffer().color;
    detail::current_tex_buffer().color = color;
    Application::instance().get_renderer().set_draw_color(color);
    if (color == snail::Color{0, 0, 0, 0})
    {
        Application::instance().get_renderer().set_blend_mode(BlendMode::none);
    }
    else
    {
        Application::instance().get_renderer().set_blend_mode(BlendMode::blend);
    }
    Application::instance().get_renderer().fill_rect(x, y, width, height);
    detail::current_tex_buffer().color = save_color;
}

void boxf(const snail::Color& color)
{
    boxf(
        0,
        0,
        detail::current_tex_buffer().tex_width,
        detail::current_tex_buffer().tex_height,
        color);
}

void buffer(int window_id, int width, int height)
{
    // Cannot create zero-width or zero-height texture.
    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    if (size_t(window_id) >= detail::tex_buffers.size())
    {
        detail::tex_buffers.resize(window_id + 1);
    }
    if (auto texture = detail::tex_buffers[window_id].texture)
    {
        int img_width;
        int img_height;
        snail::detail::enforce_sdl(
            ::SDL_QueryTexture(texture, NULL, NULL, &img_width, &img_height));
        if (width == img_width && height == img_height)
        {
            gsel(window_id);
            Application::instance().get_renderer().clear();
            return;
        }
        else
        {
            ::SDL_DestroyTexture(detail::tex_buffers[window_id].texture);
        }
    }
    detail::tex_buffers[window_id] = {
        snail::detail::enforce_sdl(::SDL_CreateTexture(
            Application::instance().get_renderer().ptr(),
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET,
            width,
            height)),
        width,
        height,
    };

    Application::instance().register_finalizer(
        [ptr = detail::tex_buffers[window_id].texture]() {
            ::SDL_DestroyTexture(ptr);
        });

    const auto save = Application::instance().get_renderer().render_target();
    Application::instance().get_renderer().set_render_target(
        detail::tex_buffers[window_id].texture);
    Application::instance().get_renderer().set_draw_color({0, 0, 0, 0});
    Application::instance().get_renderer().clear();
    Application::instance().get_renderer().set_render_target(save);

    gsel(window_id);
}

void color(int r, int g, int b)
{
    detail::current_tex_buffer().color = {
        static_cast<uint8_t>(clamp(r, 0, 255)),
        static_cast<uint8_t>(clamp(g, 0, 255)),
        static_cast<uint8_t>(clamp(b, 0, 255)),
        detail::current_tex_buffer().color.a,
    };
    Application::instance().get_renderer().set_draw_color(
        detail::current_tex_buffer().color);
}

void font(int size, Font::Style style, const fs::path& filepath)
{
    auto& renderer = Application::instance().get_renderer();
    if (renderer.font().size() == size && renderer.font().style() == style)
        return;

    const auto itr = font_detail::font_cache.find({size, style});
    if (itr != std::end(font_detail::font_cache))
    {
        renderer.set_font(itr->second);
    }
    else
    {
        const auto inserted = font_detail::font_cache.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(size, style),
            std::forward_as_tuple(filepath, size, style));
        renderer.set_font(inserted.first->second);
    }
}



void gcopy(
    int window_id,
    int src_x,
    int src_y,
    int src_width,
    int src_height,
    int dst_width,
    int dst_height)
{
    auto&& renderer = snail::Application::instance().get_renderer();

    detail::set_blend_mode();
    snail::detail::enforce_sdl(::SDL_SetTextureAlphaMod(
        detail::tex_buffers[window_id].texture,
        detail::current_tex_buffer().color.a));

    int dst_x = detail::current_tex_buffer().x;
    int dst_y = detail::current_tex_buffer().y;
    dst_width = dst_width == -1 ? src_width : dst_width;
    dst_height = dst_height == -1 ? src_height : dst_height;

    if (src_x < 0)
    {
        int excess = -src_x;
        dst_x += excess;
        dst_width -= excess;
    }
    if (src_y < 0)
    {
        int excess = -src_y;
        dst_y += excess;
        dst_height -= excess;
    }
    if (src_x + src_width >= detail::tex_buffers[window_id].tex_width)
    {
        int excess =
            src_x + src_width - detail::tex_buffers[window_id].tex_width;
        dst_width -= excess;
    }
    if (src_y + src_height >= detail::tex_buffers[window_id].tex_height)
    {
        int excess =
            src_y + src_height - detail::tex_buffers[window_id].tex_height;
        dst_height -= excess;
    }

    if (window_id == detail::current_buffer)
    {
        auto tmp_buffer = detail::get_tmp_buffer(dst_width, dst_height);
        renderer.set_render_target(tmp_buffer);
        if (window_id >= 10)
        {
            const auto save = renderer.blend_mode();
            renderer.set_blend_mode(BlendMode::none);
            renderer.set_draw_color({0, 0, 0, 0});
            renderer.set_blend_mode(save);
        }
        renderer.clear();
        renderer.render_image(
            detail::tex_buffers[window_id].texture,
            src_x,
            src_y,
            src_width,
            src_height,
            0,
            0,
            dst_width,
            dst_height);
        gsel(window_id);
        renderer.render_image(
            tmp_buffer, 0, 0, dst_width, dst_height, dst_x, dst_y);
    }
    else
    {
        renderer.render_image(
            detail::tex_buffers[window_id].texture,
            src_x,
            src_y,
            src_width,
            src_height,
            dst_x,
            dst_y,
            dst_width,
            dst_height);
    }
}



int ginfo(int type)
{
    switch (type)
    {
    case 0: return 0; // mouse x
    case 1: return 0; // mouse y
    case 2: return 0; // active window id
    case 3: return detail::current_buffer; // target window id
    case 4: return 0; // window x1
    case 5: return 0; // window y1
    case 6: return Application::instance().width(); // window x2
    case 7: return Application::instance().height(); // window y2
    case 8: return 0; // window scroll x
    case 9: return 0; // window scroll y
    case 10: return Application::instance().width(); // window width
    case 11: return Application::instance().height(); // window height
    case 12:
        return detail::current_tex_buffer().tex_width; // window client width
    case 13:
        return detail::current_tex_buffer().tex_height; // window client height
    case 14: return 0; // font width
    case 15: return 0; // font height
    case 16: return detail::current_tex_buffer().color.r; // current color r
    case 17: return detail::current_tex_buffer().color.g; // current color g
    case 18: return detail::current_tex_buffer().color.b; // current color b
    case 19: return 0; // color mode
    case 20: return 0; // resolution x
    case 21: return 0; // resolution y
    case 22: return detail::current_tex_buffer().x; // current position x
    case 23: return detail::current_tex_buffer().y; // current position y
    default: throw std::logic_error("Bad ginfo type");
    }
}



void gmode(int mode, int alpha)
{
    detail::current_tex_buffer().mode = mode;
    detail::set_blend_mode();

    detail::current_tex_buffer().color.a = clamp(alpha, 0, 255);
}



void grotate(
    int window_id,
    int src_x,
    int src_y,
    int src_width,
    int src_height,
    double angle)
{
    grotate(
        window_id,
        src_x,
        src_y,
        src_width,
        src_height,
        src_width,
        src_height,
        angle);
}



void grotate(
    int window_id,
    int src_x,
    int src_y,
    int src_width,
    int src_height,
    int dst_width,
    int dst_height,
    double angle)
{
    assert(window_id != detail::current_buffer);

    detail::set_blend_mode();
    snail::detail::enforce_sdl(::SDL_SetTextureAlphaMod(
        detail::tex_buffers[window_id].texture,
        detail::current_tex_buffer().color.a));

    ::SDL_Rect src_rect{
        src_x,
        src_y,
        src_width,
        src_height,
    };
    ::SDL_Rect dst_rect{
        detail::current_tex_buffer().x - dst_width / 2,
        detail::current_tex_buffer().y - dst_height / 2,
        dst_width,
        dst_height,
    };

    switch (Application::instance().get_renderer().blend_mode())
    {
    case BlendMode::none:
        snail::detail::enforce_sdl(::SDL_SetTextureBlendMode(
            detail::tex_buffers[window_id].texture, ::SDL_BLENDMODE_NONE));
        break;
    case BlendMode::blend:
        snail::detail::enforce_sdl(::SDL_SetTextureBlendMode(
            detail::tex_buffers[window_id].texture, ::SDL_BLENDMODE_BLEND));
        break;
    case BlendMode::add:
        snail::detail::enforce_sdl(::SDL_SetTextureBlendMode(
            detail::tex_buffers[window_id].texture, ::SDL_BLENDMODE_ADD));
        break;
    }
    snail::detail::enforce_sdl(::SDL_RenderCopyEx(
        Application::instance().get_renderer().ptr(),
        detail::tex_buffers[window_id].texture,
        &src_rect,
        &dst_rect,
        rad2deg(angle),
        nullptr,
        ::SDL_FLIP_NONE));
}



void gsel(int window_id)
{
    detail::current_buffer = window_id;
    Application::instance().get_renderer().set_render_target(
        detail::current_tex_buffer().texture);
}



void line(int x1, int y1, int x2, int y2, const snail::Color& color)
{
    auto&& renderer = snail::Application::instance().get_renderer();
    renderer.set_draw_color(color);
    renderer.render_line(x1, y1, x2, y2);
    renderer.set_draw_color({0, 0, 0});
}



static void title_android(const std::string& display_mode)
{
    Application::instance().set_display_mode(
        Application::instance().get_default_display_mode());
    Application::instance().set_fullscreen_mode(
        Window::FullscreenMode::fullscreen);
    Application::instance().set_subwindow_display_mode(display_mode);
}



void title(
    const std::string& title_str,
    const std::string& display_mode,
    Window::FullscreenMode fullscreen_mode)
{
    Application::instance().initialize(title_str);

    if (Application::is_android)
    {
        title_android(display_mode);
    }
    else if (display_mode != "__unknown__")
    {
        if (display_mode != "")
        {
            Application::instance().set_display_mode(display_mode);
        }
        Application::instance().set_fullscreen_mode(fullscreen_mode);
    }

    detail::setup_buffers();
    Application::instance().register_finalizer(
        [&]() { font_detail::font_cache.clear(); });
    buffer(
        0, Application::instance().width(), Application::instance().height());
}

} // namespace hsp
} // namespace snail
} // namespace elona
