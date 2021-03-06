#include "main_menu.hpp"
#include "../util/fileutil.hpp"
#include "../util/strutil.hpp"
#include "../version.hpp"
#include "audio.hpp"
#include "browser.hpp"
#include "character_making.hpp"
#include "config/config.hpp"
#include "ctrl_file.hpp"
#include "i18n.hpp"
#include "input.hpp"
#include "keybind/keybind.hpp"
#include "macro.hpp"
#include "main_menu.hpp"
#include "menu.hpp"
#include "random.hpp"
#include "ui.hpp"
#include "variables.hpp"



namespace elona
{

struct Release
{
    Release(
        const std::string& version,
        const std::string& date,
        const std::string& content)
        : version(version)
        , date(date)
        , content(content)
    {
    }



    std::string title() const
    {
        return version + u8" (" + date + u8")";
    }



    std::string version;
    std::string date;
    std::string content;
};



struct Changelog
{
public:
    using iterator = std::vector<Release>::const_iterator;


    void load(const fs::path& changelog_path)
    {
        for (const auto& line : fileutil::read_by_line(changelog_path))
        {
            if (strutil::starts_with(line, u8"## "))
            {
                // ## [version] - date
                const auto open_bracket_pos = line.find('[');
                const auto closing_bracket_pos = line.find(']');
                const auto hyphen_bracket_pos = line.find('-');
                const auto version = line.substr(
                    open_bracket_pos + 1,
                    closing_bracket_pos - open_bracket_pos - 1);
                const auto date = line.substr(hyphen_bracket_pos + 2);
                releases.emplace_back(version, date, "");
            }
            else if (!releases.empty())
            {
                releases.back().content += line + '\n';
            }
        }
    }



    const Release& at(size_t index) const
    {
        return releases.at(index);
    }



    iterator begin() const
    {
        return std::begin(releases);
    }



    iterator end() const
    {
        return std::end(releases);
    }



private:
    // The first element is the latest. The last, the oldest.
    std::vector<Release> releases;
};



bool main_menu_loop()
{
    MainMenuResult result = main_menu_wrapper();
    bool finished = false;
    while (!finished)
    {
        switch (result)
        {
        case MainMenuResult::main_title_menu:
            result = main_menu_wrapper();
            break;
        case MainMenuResult::initialize_game: finished = true; break;
        case MainMenuResult::finish_elona:
            finish_elona();
            finished = true;
            break;
        default: assert(0); break;
        }
    }

    return result == MainMenuResult::initialize_game;
}



MainMenuResult main_title_menu()
{
    mode = 10;
    lomiaseaster = 0;

    play_music("core.mcOpening");

    cs = 0;
    cs_bk = -1;
    keyrange = 6;
    pagesize = 0;

    load_background_variants(2);

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"title.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);

    font(13 - en * 2);
    mes(20, 20, u8"Elona version 1.22  Developed by Noa"s, {255, 255, 255});

    mes(20,
        38,
        u8"  Variant foobar version "s + latest_version.short_string() +
            u8"  Developed by KI",
        {255, 255, 255});

    if (jp)
    {
        mes(20,
            56,
            u8"Contributor MSL / View the credits for more"s,
            {255, 255, 255});
    }
    else
    {
        mes(20,
            56,
            u8"Contributor f1r3fly, Sunstrike, Schmidt, Elvenspirit / View the credits for more"s,
            {255, 255, 255});
        mes(20, 76, u8"Omake/OO translator: Doorknob", {255, 255, 255});
        mes(20, 96, u8"Cutscenes translator: AnnaBannana", {255, 255, 255});
    }

    windowshadow = 1;
    ui_display_window(
        jp ? u8"冒険の道標" : u8"Starting Menu",
        strhint1,
        80,
        winposy(308, 1),
        320,
        320);
    cmbg = 4;
    x = ww / 5 * 4;
    y = wh - 80;
    gmode(2, 50);
    gcopy_c(
        2,
        cmbg / 2 * 180,
        cmbg % 2 * 300,
        180,
        300,
        wx + 160,
        wy + wh / 2,
        x,
        y);
    gmode(2);

    std::vector<std::string> items = {
        u8"Restore an Adventurer",
        i18n::s.get("core.locale.main_menu.title_menu.continue"),
        u8"Generate an Adventurer",
        i18n::s.get("core.locale.main_menu.title_menu.new"),
        u8"Incarnate an Adventurer",
        i18n::s.get("core.locale.main_menu.title_menu.incarnate"),
        u8"About",
        i18n::s.get("core.locale.main_menu.title_menu.about"),
        u8"Options",
        i18n::s.get("core.locale.main_menu.title_menu.options"),
        u8"Exit",
        i18n::s.get("core.locale.main_menu.title_menu.exit"),
    };

    gsel(3);
    picload(filesystem::dir::graphic() / u8"deco_title.bmp", 960, 96, false);
    gsel(0);
    gmode(0);
    gcopy(4, 0, 0, windoww, windowh, 0, 0);
    gmode(2);

    while (true)
    {
        if (Config::instance().autodisable_numlock)
        {
            snail::Input::instance().disable_numlock();
        }

        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_listbk();
        for (int cnt = 0; cnt < 6; ++cnt)
        {
            x = wx + 40;
            y = cnt * 35 + wy + 50;
            display_customkey(key_select(cnt), x, y);
            if (en)
            {
                font(14 - en * 2);
                cs_list(cs == cnt, items.at(cnt * 2 + 1), x + 40, y + 1);
            }
            else
            {
                font(11 - en * 2);
                mes(x + 40, y - 4, items.at(cnt * 2));
                font(13 - en * 2);
                cs_list(cs == cnt, items.at(cnt * 2 + 1), x + 40, y + 8);
            }
        }
        cs_bk = cs;

        redraw();

        int index{};
        cursor_check_ex(index);

        if (index == 1)
        {
            snd("core.ok1");
            geneuse = "";
            return MainMenuResult::main_menu_new_game;
        }
        if (index == 0)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_continue;
        }
        if (index == 2)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_incarnate;
        }
        if (index == 3)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_about;
        }
        if (index == 4)
        {
            snd("core.ok1");
            set_option();
            return MainMenuResult::main_title_menu;
        }
        if (index == 5)
        {
            snd("core.ok1");
            return MainMenuResult::finish_elona;
        }
    }
}



MainMenuResult main_menu_wrapper()
{
    // Start off in the title menu.
    MainMenuResult result = main_title_menu();
    bool finished = false;
    while (!finished)
    {
        switch (result)
        {
            // Main menu

        case MainMenuResult::main_menu_incarnate:
            result = main_menu_incarnate();
            break;
        case MainMenuResult::main_menu_continue:
            result = main_menu_continue();
            break;
        case MainMenuResult::main_menu_new_game:
            result = main_menu_new_game();
            break;
        case MainMenuResult::main_menu_about: result = main_menu_about(); break;
        case MainMenuResult::main_menu_about_changelogs:
            result = main_menu_about_changelogs();
            break;
        case MainMenuResult::main_menu_about_license:
            result = main_menu_about_license();
            break;
        case MainMenuResult::main_menu_about_credits:
            result = main_menu_about_credits();
            break;
        case MainMenuResult::main_title_menu:
            // Loop back to the start.
            result = MainMenuResult::main_title_menu;
            finished = true;
            break;

            // Character making

        case MainMenuResult::character_making_select_race:
            result = character_making_select_race();
            break;
        case MainMenuResult::character_making_select_sex:
            result = character_making_select_sex();
            break;
        case MainMenuResult::character_making_select_sex_looped:
            result = character_making_select_sex(false);
            break;
        case MainMenuResult::character_making_select_class:
            result = character_making_select_class();
            break;
        case MainMenuResult::character_making_select_class_looped:
            result = character_making_select_class(false);
            break;
        case MainMenuResult::character_making_role_attributes:
            result = character_making_role_attributes();
            break;
        case MainMenuResult::character_making_role_attributes_looped:
            result = character_making_role_attributes(false);
            break;
        case MainMenuResult::character_making_select_feats:
            result = character_making_select_feats();
            break;
        case MainMenuResult::character_making_select_alias:
            result = character_making_select_alias();
            break;
        case MainMenuResult::character_making_select_alias_looped:
            result = character_making_select_alias(false);
            break;
        case MainMenuResult::character_making_customize_appearance:
            result = character_making_customize_appearance();
            break;
        case MainMenuResult::character_making_final_phase:
            result = character_making_final_phase();
            break;

            // Finished initializing, start the game.

        case MainMenuResult::initialize_game:
            result = MainMenuResult::initialize_game;
            finished = true;
            break;
        case MainMenuResult::finish_elona:
            result = MainMenuResult::finish_elona;
            finished = true;
            break;
        default: assert(0); break;
        }
    }
    return result;
}



MainMenuResult main_menu_new_game()
{
    if (Config::instance().wizard)
    {
        game_data.wizard = 1;
    }
    if (geneuse != ""s)
    {
        load_gene_files();
    }
    rc = 0;
    mode = 1;
    cm = 1;
    gsel(4);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    load_background_variants(2);
    gsel(3);
    picload(filesystem::dir::graphic() / u8"deco_cm.bmp", 960, 96, false);
    gsel(0);
    return MainMenuResult::character_making_select_race;
}



MainMenuResult main_menu_continue()
{
    int save_data_count = 0;
    int index = 0;
    cs = 0;
    cs_bk = -1;
    page = 0;
    pagesize = 5;
    keyrange = 0;

    for (const auto& entry : filesystem::dir_entries(
             filesystem::dir::save(), filesystem::DirEntryRange::Type::dir))
    {
        s = filepathutil::to_utf8_path(entry.path().filename());
        const auto header_filepath = filesystem::dir::save(s) / u8"header.txt";
        if (!fs::exists(header_filepath))
        {
            continue;
        }

        // the number of bytes read by bload depends on the size of
        // the string passed in, so make it 100 bytes long.
        playerheader = std::string(100, '\0');
        bload(header_filepath, playerheader);

        list(0, save_data_count) = save_data_count;
        listn(0, save_data_count) = s;
        listn(1, save_data_count) = ""s + playerheader;
        key_list(save_data_count) = key_select(save_data_count);
        ++save_data_count;
    }
    listmax = save_data_count;

    while (true)
    {
    savegame_change_page:
        cs_bk = -1;
        pagemax = (listmax - 1) / pagesize;
        if (page < 0)
        {
            page = pagemax;
        }
        else if (page > pagemax)
        {
            page = 0;
        }

        clear_background_in_continue();
        ui_draw_caption(
            i18n::s.get("core.locale.main_menu.continue.which_save"));
        windowshadow = 1;
    savegame_draw_page:
        ui_display_window(
            i18n::s.get("core.locale.main_menu.continue.title"),
            i18n::s.get("core.locale.main_menu.continue.key_hint") + strhint2 +
                strhint3b,
            (windoww - 440) / 2 + inf_screenx,
            winposy(288, 1),
            440,
            288);
        cs_listbk();
        keyrange = 0;
        for (int cnt = 0, cnt_end = pagesize; cnt < cnt_end; ++cnt)
        {
            index = pagesize * page + cnt;
            if (index >= listmax)
            {
                break;
            }
            x = wx + 20;
            y = cnt * 40 + wy + 50;
            display_key(x + 20, y - 2, cnt);
            font(11 - en * 2);
            mes(x + 48, y - 4, listn(0, index));
            font(13 - en * 2);
            cs_list(cs == cnt, listn(1, index), x + 48, y + 8);
            ++keyrange;
        }
        cs_bk = cs;
        if (save_data_count == 0)
        {
            font(14 - en * 2);
            mes(wx + 140,
                wy + 120,
                i18n::s.get("core.locale.main_menu.continue.no_save"));
        }
        redraw();

        int cursor{};
        auto action = cursor_check_ex(cursor);

        p = -1;
        for (int cnt = 0, cnt_end = (pagesize); cnt < cnt_end; ++cnt)
        {
            index = pagesize * page + cnt;
            if (index >= listmax)
            {
                break;
            }
            if (cursor == cnt)
            {
                p = list(0, index);
                break;
            }
        }
        if (p != -1)
        {
            playerid = listn(0, p);
            snd("core.ok1");
            mode = 3;
            return MainMenuResult::initialize_game;
        }
        if (ginfo(2) == 0)
        {
            if (save_data_count != 0)
            {
                if (getkey(snail::Key::backspace))
                {
                    p = list(0, cs);
                    playerid = listn(0, p);
                    ui_draw_caption(i18n::s.get(
                        "core.locale.main_menu.continue.delete", playerid));
                    if (!yes_no())
                    {
                        return MainMenuResult::main_menu_continue;
                    }
                    ui_draw_caption(i18n::s.get(
                        "core.locale.main_menu.continue.delete2", playerid));
                    if (yes_no())
                    {
                        snd("core.ok1");
                        ctrl_file(FileOperation::save_game_delete);
                    }
                    return MainMenuResult::main_menu_continue;
                }
            }
        }
        if (action == "next_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                ++page;
                goto savegame_change_page;
            }
        }
        if (action == "previous_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                --page;
                goto savegame_change_page;
            }
        }
        if (action == "cancel")
        {
            return MainMenuResult::main_title_menu;
        }
        goto savegame_draw_page;
    }
}



MainMenuResult main_menu_incarnate()
{
    cs = 0;
    cs_bk = -1;
    gsel(4);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gsel(0);
    gmode(0);
    gcopy(4, 0, 0, windoww, windowh, 0, 0);
    gmode(2);
    ui_draw_caption(i18n::s.get("core.locale.main_menu.incarnate.which_gene"));
    keyrange = 0;
    listmax = 0;
    for (const auto& entry : filesystem::dir_entries(
             filesystem::dir::save(), filesystem::DirEntryRange::Type::dir))
    {
        s = filepathutil::to_utf8_path(entry.path().filename());
        const auto gene_header_filepath =
            filesystem::dir::save(s) / u8"gene_header.txt";
        if (!fs::exists(gene_header_filepath))
        {
            continue;
        }
        bload(gene_header_filepath, playerheader);
        list(0, listmax) = listmax;
        listn(0, listmax) = s;
        listn(1, listmax) = ""s + playerheader;
        key_list(listmax) = key_select(listmax);
        ++keyrange;
        ++listmax;
    }
    windowshadow = 1;

    while (1)
    {
        ui_display_window(
            i18n::s.get("core.locale.main_menu.incarnate.title"),
            strhint3b,
            (windoww - 440) / 2 + inf_screenx,
            winposy(288, 1),
            440,
            288);
        cs_listbk();
        for (int cnt = 0, cnt_end = (listmax); cnt < cnt_end; ++cnt)
        {
            x = wx + 20;
            y = cnt * 40 + wy + 50;
            display_key(x + 20, y - 2, cnt);
            font(11 - en * 2);
            mes(x + 48, y - 4, listn(0, cnt));
            font(13 - en * 2);
            cs_list(cs == cnt, listn(1, cnt), x + 48, y + 8);
        }
        cs_bk = cs;
        if (listmax == 0)
        {
            font(14 - en * 2);
            mes(wx + 140,
                wy + 120,
                i18n::s.get("core.locale.main_menu.incarnate.no_gene"));
        }
        redraw();

        int cursor{};
        auto command = cursor_check_ex(cursor);

        p = -1;
        for (int cnt = 0, cnt_end = (keyrange); cnt < cnt_end; ++cnt)
        {
            if (cnt == cursor)
            {
                p = list(0, cnt);
                break;
            }
        }
        if (p != -1)
        {
            snd("core.ok1");
            geneuse = listn(0, p);
            playerid = listn(0, p);
            return MainMenuResult::main_menu_new_game;
        }
        if (command == "cancel")
        {
            return MainMenuResult::main_title_menu;
        }
    }
    return MainMenuResult::main_title_menu;
}



MainMenuResult main_menu_about()
{
    cs = 0;
    cs_bk = -1;
    keyrange = 5;
    listmax = 5;

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);

    ui_draw_caption("Elona foobar " + latest_version.short_string());

    windowshadow = 1;
    ui_display_window(
        i18n::s.get("core.locale.main_menu.about.title"),
        strhint3b,
        (windoww - 440) / 2 + inf_screenx,
        winposy(288, 1),
        440,
        288);

    s(0) = i18n::s.get("core.locale.main_menu.about.vanilla_homepage");
    s(1) = i18n::s.get("core.locale.main_menu.about.foobar_homepage");
    s(2) = i18n::s.get("core.locale.main_menu.about.foobar_changelog");
    s(3) = i18n::s.get("core.locale.main_menu.about.license");
    s(4) = i18n::s.get("core.locale.main_menu.about.credits");

    gsel(0);

    while (true)
    {
        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_listbk();
        for (int cnt = 0; cnt < 5; ++cnt)
        {
            const auto x = wx + 40;
            const auto y = cnt * 35 + wy + 50;
            display_key(x, y, cnt);
            font(14 - en * 2);
            cs_list(cs == cnt, s(cnt), x + 40, y + 1);
        }
        cs_bk = cs;

        redraw();

        int index{};
        auto action = cursor_check_ex(index);

        if (index == 0)
        {
            snd("core.ok1");
            if (jp)
            {
                open_browser("http://ylvania.org/jp");
            }
            else
            {
                open_browser("http://ylvania.org/en");
            }
        }
        if (index == 1)
        {
            snd("core.ok1");
            open_browser("https://elonafoobar.com");
        }
        if (index == 2)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_about_changelogs;
        }
        if (index == 3)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_about_license;
        }
        if (index == 4)
        {
            snd("core.ok1");
            return MainMenuResult::main_menu_about_credits;
        }

        if (action == "cancel")
        {
            return MainMenuResult::main_title_menu;
        }
    }
}



void main_menu_about_one_changelog(const Release& release)
{
    std::vector<std::string> changes;
    if (release.content.empty())
    {
        changes.push_back("");
    }
    else
    {
        const size_t text_width = 75 - en * 15;
        constexpr size_t lines_per_page = 16;

        std::vector<std::string> buffer;
        range::copy(
            strutil::split_lines(release.content), std::back_inserter(buffer));
        for (auto&& line : buffer)
        {
            // talk_conv only accepts single line text, so you need to split by
            // line.
            talk_conv(line, text_width);
        }
        size_t line_count = 0;
        for (const auto& lines : buffer)
        {
            for (const auto& line : strutil::split_lines(lines))
            {
                if (line_count == 0)
                {
                    changes.push_back(line + '\n');
                }
                else
                {
                    changes.back() += line + '\n';
                }
                ++line_count;
                if (line_count == lines_per_page)
                {
                    line_count = 0;
                }
            }
        }
    }
    pagemax = changes.size() - 1;

    page = 0;

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);
    gsel(0);

    ui_draw_caption(i18n::s.get("core.locale.main_menu.about_changelog.title"));

    while (true)
    {
    savegame_change_page:
        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_bk = -1;
        if (page < 0)
        {
            page = pagemax;
        }
        else if (page > pagemax)
        {
            page = 0;
        }

        windowshadow = 1;
    savegame_draw_page:
        ui_display_window(
            release.title(),
            strhint2 + strhint3b,
            (windoww - 600) / 2 + inf_screenx,
            winposy(425, 1),
            600,
            425);

        font(13);
        mes(wx + 20, wy + 30, changes.at(page));

        redraw();

        int cursor{};
        auto action = cursor_check_ex(cursor);
        (void)cursor;

        if (action == "next_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                ++page;
                goto savegame_change_page;
            }
        }
        if (action == "previous_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                --page;
                goto savegame_change_page;
            }
        }
        if (action == "cancel")
        {
            return;
        }
        goto savegame_draw_page;
    }
}



MainMenuResult main_menu_about_changelogs()
{
    int index = 0;
    cs = 0;
    cs_bk = -1;
    page = 0;
    pagesize = 5;
    keyrange = 0;

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);
    gsel(0);

    ui_draw_caption(i18n::s.get("core.locale.main_menu.about_changelog.title"));

    Changelog changelog;
    if (jp)
    {
        changelog.load(filesystem::path("../CHANGELOG-jp.md"));
    }
    else
    {
        changelog.load(filesystem::path("../CHANGELOG.md"));
    }

    listmax = 0;
    for (const auto& release : changelog)
    {
        list(0, listmax) = listmax;
        listn(0, listmax) = release.title();
        key_list(listmax) = key_select(listmax);
        ++listmax;
    }

    while (true)
    {
    savegame_change_page:
        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_bk = -1;
        pagemax = (listmax - 1) / pagesize;
        if (page < 0)
        {
            page = pagemax;
        }
        else if (page > pagemax)
        {
            page = 0;
        }

        windowshadow = 1;
    savegame_draw_page:
        ui_display_window(
            "",
            strhint2 + strhint3b,
            (windoww - 440) / 2 + inf_screenx,
            winposy(288, 1),
            440,
            288);
        cs_listbk();
        keyrange = 0;
        for (int cnt = 0, cnt_end = pagesize; cnt < cnt_end; ++cnt)
        {
            index = pagesize * page + cnt;
            if (index >= listmax)
            {
                break;
            }
            x = wx + 20;
            y = cnt * 40 + wy + 30;
            display_key(x + 20, y, cnt);
            font(13 - en * 2);
            cs_list(cs == cnt, listn(0, index), x + 48, y);
            ++keyrange;
        }
        cs_bk = cs;
        redraw();

        int cursor{};
        auto action = cursor_check_ex(cursor);

        p = -1;
        for (int cnt = 0, cnt_end = (pagesize); cnt < cnt_end; ++cnt)
        {
            index = pagesize * page + cnt;
            if (index >= listmax)
            {
                break;
            }
            if (cursor == cnt)
            {
                p = list(0, index);
                break;
            }
        }
        if (p != -1)
        {
            snd("core.ok1");
            const auto page_save = page;
            const auto pagemax_save = pagemax;
            main_menu_about_one_changelog(changelog.at(p));
            page = page_save;
            pagemax = pagemax_save;
            goto savegame_change_page;
        }
        if (action == "next_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                ++page;
                goto savegame_change_page;
            }
        }
        if (action == "previous_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                --page;
                goto savegame_change_page;
            }
        }
        if (action == "cancel")
        {
            return MainMenuResult::main_menu_about;
        }
        goto savegame_draw_page;
    }
}



MainMenuResult main_menu_about_license()
{
    std::vector<std::string> license_text_lines;
    range::copy(
        fileutil::read_by_line(filesystem::path("../LICENSE.txt")),
        std::back_inserter(license_text_lines));

    std::vector<std::string> license_pages;

    if (license_text_lines.empty())
    {
        license_pages.push_back("");
    }
    else
    {
        const size_t text_width = 75 - en * 15;
        constexpr size_t lines_per_page = 16;

        for (auto&& line : license_text_lines)
        {
            // talk_conv only accepts single line text, so you need to split by
            // line.
            talk_conv(line, text_width);
        }
        size_t line_count = 0;
        for (const auto& lines : license_text_lines)
        {
            for (const auto& line : strutil::split_lines(lines))
            {
                if (line_count == 0)
                {
                    license_pages.push_back(line + '\n');
                }
                else
                {
                    license_pages.back() += line + '\n';
                }
                ++line_count;
                if (line_count == lines_per_page)
                {
                    line_count = 0;
                }
            }
        }
    }
    pagemax = license_pages.size() - 1;

    page = 0;

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);
    gsel(0);

    ui_draw_caption(i18n::s.get("core.locale.main_menu.about.license"));

    while (true)
    {
    savegame_change_page:
        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_bk = -1;
        if (page < 0)
        {
            page = pagemax;
        }
        else if (page > pagemax)
        {
            page = 0;
        }

        windowshadow = 1;
    savegame_draw_page:
        ui_display_window(
            "",
            strhint2 + strhint3b,
            (windoww - 600) / 2 + inf_screenx,
            winposy(425, 1),
            600,
            425);

        font(13);
        mes(wx + 20, wy + 30, license_pages.at(page));

        redraw();

        int cursor{};
        auto action = cursor_check_ex(cursor);
        (void)cursor;

        if (action == "next_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                ++page;
                goto savegame_change_page;
            }
        }
        if (action == "previous_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                --page;
                goto savegame_change_page;
            }
        }
        if (action == "cancel")
        {
            return MainMenuResult::main_menu_about;
        }
        goto savegame_draw_page;
    }
}



MainMenuResult main_menu_about_credits()
{
    std::vector<std::string> credits_text_lines;
    range::copy(
        fileutil::read_by_line(filesystem::path("../CREDITS.txt")),
        std::back_inserter(credits_text_lines));

    std::vector<std::string> credits_pages;

    if (credits_text_lines.empty())
    {
        credits_pages.push_back("");
    }
    else
    {
        const size_t text_width = 75 - en * 15;
        constexpr size_t lines_per_page = 16;

        for (auto&& line : credits_text_lines)
        {
            // talk_conv only accepts single line text, so you need to split by
            // line.
            talk_conv(line, text_width);
        }
        size_t line_count = 0;
        for (const auto& lines : credits_text_lines)
        {
            for (const auto& line : strutil::split_lines(lines))
            {
                if (line_count == 0)
                {
                    credits_pages.push_back(line + '\n');
                }
                else
                {
                    credits_pages.back() += line + '\n';
                }
                ++line_count;
                if (line_count == lines_per_page)
                {
                    line_count = 0;
                }
            }
        }
    }
    pagemax = credits_pages.size() - 1;

    page = 0;

    gsel(4);
    gmode(0);
    picload(filesystem::dir::graphic() / u8"void.bmp", 0, 0, false);
    gcopy(4, 0, 0, 800, 600, 0, 0, windoww, windowh);
    gmode(2);
    gsel(0);

    ui_draw_caption(i18n::s.get("core.locale.main_menu.about.credits"));

    while (true)
    {
    savegame_change_page:
        gmode(0);
        gcopy(4, 0, 0, windoww, windowh, 0, 0);
        gmode(2);

        cs_bk = -1;
        if (page < 0)
        {
            page = pagemax;
        }
        else if (page > pagemax)
        {
            page = 0;
        }

        windowshadow = 1;
    savegame_draw_page:
        ui_display_window(
            "",
            strhint2 + strhint3b,
            (windoww - 600) / 2 + inf_screenx,
            winposy(425, 1),
            600,
            425);

        font(13);
        mes(wx + 20, wy + 30, credits_pages.at(page));

        redraw();

        int cursor{};
        auto action = cursor_check_ex(cursor);
        (void)cursor;

        if (action == "next_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                ++page;
                goto savegame_change_page;
            }
        }
        if (action == "previous_page")
        {
            if (pagemax != 0)
            {
                snd("core.pop1");
                --page;
                goto savegame_change_page;
            }
        }
        if (action == "cancel")
        {
            return MainMenuResult::main_menu_about;
        }
        goto savegame_draw_page;
    }
}

} // namespace elona
