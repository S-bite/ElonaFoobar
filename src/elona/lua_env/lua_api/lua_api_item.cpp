#include "lua_api_item.hpp"
#include "../../calc.hpp"
#include "../../character.hpp"
#include "../../data/types/type_item.hpp"
#include "../../enchantment.hpp"
#include "../../item.hpp"
#include "../../itemgen.hpp"
#include "../../lua_env/enums/enums.hpp"
#include "../interface.hpp"



namespace elona
{
namespace lua
{

int LuaApiItem::count()
{
    return inv_sum(-1);
}

bool LuaApiItem::has_enchantment(const LuaItemHandle handle, int id)
{
    auto& item_ref = lua::lua->get_handle_manager().get_ref<Item>(handle);
    return encfindspec(item_ref.index, id);
}

std::string
LuaApiItem::itemname(LuaItemHandle handle, int number, bool needs_article)
{
    auto& item_ref = lua::lua->get_handle_manager().get_ref<Item>(handle);
    return elona::itemname(item_ref.index, number, needs_article ? 0 : 1);
}

sol::optional<LuaItemHandle> LuaApiItem::create_with_id(
    const Position& position,
    const std::string& id)
{
    return LuaApiItem::create_with_id_xy(position.x, position.y, id);
}

sol::optional<LuaItemHandle>
LuaApiItem::create_with_id_xy(int x, int y, const std::string& id)
{
    return LuaApiItem::create_xy(
        x, y, lua::lua->get_state()->create_table_with("id", id));
}

sol::optional<LuaItemHandle> LuaApiItem::create_with_number(
    const Position& position,
    const std::string& id,
    int number)
{
    return LuaApiItem::create_with_number_xy(
        position.x, position.y, id, number);
}

sol::optional<LuaItemHandle> LuaApiItem::create_with_number_xy(
    int x,
    int y,
    const std::string& id,
    int number)
{
    return LuaApiItem::create_xy(
        x,
        y,
        lua::lua->get_state()->create_table_with("id", id, "number", number));
}

sol::optional<LuaItemHandle> LuaApiItem::create(
    const Position& position,
    sol::table args)
{
    return LuaApiItem::create_xy(position.x, position.y, args);
}

sol::optional<LuaItemHandle>
LuaApiItem::create_xy(int x, int y, sol::table args)
{
    int id = 0;
    int slot = -1;
    int number = 0;
    objlv = 0;
    fixlv = Quality::none;

    if (auto it = args.get<sol::optional<bool>>("nostack"))
    {
        nostack = *it ? 1 : 0;
    }

    if (auto it = args.get<sol::optional<int>>("mode"))
    {
        mode = *it;
    }

    if (auto it = args.get<sol::optional<int>>("number"))
    {
        number = *it;
    }

    if (auto it = args.get<sol::optional<int>>("slot"))
    {
        slot = *it;
    }

    // Random objlv
    if (auto it = args.get<sol::optional<int>>("level"))
    {
        objlv = calcobjlv(*it);
    }

    // Random fixlv
    if (auto it = args.get<sol::optional<std::string>>("quality"))
    {
        fixlv = calcfixlv(LuaEnums::QualityTable.ensure_from_string(*it));
    }

    // Clears flttypemajor and flttypeminor.
    flt(objlv, fixlv);

    // Exact objlv
    if (auto it = args.get<sol::optional<int>>("objlv"))
    {
        objlv = *it;
    }

    // Exact fixlv
    if (auto it = args.get<sol::optional<std::string>>("fixlv"))
    {
        fixlv = LuaEnums::QualityTable.ensure_from_string(*it);
    }

    if (auto it = args.get<sol::optional<int>>("flttypemajor"))
    {
        flttypemajor = *it;
    }

    if (auto it = args.get<sol::optional<int>>("flttypeminor"))
    {
        flttypeminor = *it;
    }

    if (auto it = args.get<sol::optional<std::string>>("fltn"))
    {
        fltn(*it);
    }

    if (auto it = args.get<sol::optional<std::string>>("id"))
    {
        auto data = the_item_db[*it];
        if (!data)
        {
            throw sol::error("No such item " + *it);
        }
        id = data->id;
    }

    if (itemcreate(slot, id, x, y, number) != 0)
    {
        LuaItemHandle handle =
            lua::lua->get_handle_manager().get_handle(inv[ci]);
        return handle;
    }
    else
    {
        return sol::nullopt;
    }
}

int LuaApiItem::memory(int type, const std::string& id)
{
    if (type < 0 || type > 2)
    {
        return 0;
    }

    auto data = the_item_db[id];
    if (!data)
    {
        return 0;
    }

    return itemmemory(type, data->id);
}

sol::optional<LuaItemHandle> LuaApiItem::stack(
    int inventory_id,
    LuaItemHandle handle)
{
    if (inventory_id < -1 || inventory_id > ELONA_MAX_CHARACTERS)
    {
        return sol::nullopt;
    }

    auto& item_ref = lua::lua->get_handle_manager().get_ref<Item>(handle);

    int tibk = ti;
    item_stack(inventory_id, item_ref.index);
    auto& item = inv[ti];
    ti = tibk;

    if (item.number() == 0)
    {
        return sol::nullopt;
    }

    return lua::handle(item);
}

int LuaApiItem::trade_rate(LuaItemHandle handle)
{
    auto& item_ref = lua::lua->get_handle_manager().get_ref<Item>(handle);

    // Item must be in the cargo category.
    if (the_item_db[item_ref.id]->category != 92000)
    {
        return 0;
    }

    return trate(item_ref.param1);
}

void LuaApiItem::bind(sol::table& api_table)
{
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, count);
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, has_enchantment);
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, itemname);
    api_table.set_function(
        "create",
        sol::overload(
            LuaApiItem::create,
            LuaApiItem::create_xy,
            LuaApiItem::create_with_id,
            LuaApiItem::create_with_id_xy,
            LuaApiItem::create_with_number,
            LuaApiItem::create_with_number_xy));
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, memory);
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, stack);
    LUA_API_BIND_FUNCTION(api_table, LuaApiItem, trade_rate);
}

} // namespace lua
} // namespace elona
