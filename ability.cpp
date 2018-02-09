#include "ability.hpp"
#include "cat.hpp"
#include "range.hpp"

using namespace elona;



namespace
{


// FIXME: DO NOT USE A GLOBAL VARIABLE!
std::unordered_map<int, ability_data>* storage_ptr;


int define(lua_State* state)
{
    int argc = lua_gettop(state);
    if (argc != 2)
        throw 0;

    const char* id = luaL_checklstring(state, 1, nullptr);
    if (!id)
        throw 0;

#define FIELD_I(name) \
    lua_getfield(state, 2, #name); \
    int name = luaL_checkinteger(state, -1);

    FIELD_I(related_basic_attribute);
    FIELD_I(sdataref1);
    FIELD_I(cost);
    FIELD_I(sdataref3);
    FIELD_I(sdataref4);

#undef FIELD_I

    storage_ptr->emplace(
        std::stoi(id), // TODO
        ability_data{
            std::stoi(id),
            related_basic_attribute,
            sdataref1,
            cost,
            sdataref3,
            sdataref4,
        });

    return 0;
}


} // namespace



namespace elona
{



ability_db::ability_db()
{
    lua_State* state = luaL_newstate();
    luaL_openlibs(state);
    cat::register_function(state, "define", &define);
    storage_ptr = &storage;
    cat::load(state, fs::u8path(u8"../data/ability.lua"));
    storage_ptr = nullptr;
    lua_close(state);

    storage[0] = {};
}



const ability_data& ability_db::operator[](int id) const
{
    const auto itr = storage.find(id);
    if (itr == std::end(storage))
        return storage.at(0);
    else
        return itr->second;
}



skill_data::skill_data()
    : storage(245, std::vector<ability>(600))
{
}



void skill_data::clear(int cc)
{
    range::fill(storage[cc], ability{});
}



void skill_data::copy(int tc, int cc)
{
    range::copy(storage[cc], std::begin(storage[tc]));
}



// Serialize the entire data from offset
std::unique_ptr<char[]> skill_data::serialize(int offset) const
{
    std::unique_ptr<char[]> buf{
        new char[(std::size(storage) - offset) * sizeof(int) * 1200]};
    std::fill(
        buf.get(),
        buf.get() + (std::size(storage) - offset) * sizeof(int) * 1200,
        0);
    for (size_t i = offset; i < std::size(storage); ++i)
    {
        for (size_t j = 0; j < 1200; ++j)
        {
            int value;
            if (j < 600)
            {
                value = storage[i][j].current_level;
            }
            else
            {
                value = storage[i][j - 600].original_level * 1'000'000
                    + storage[i][j - 600].experience * 1'000
                    + storage[i][j - 600].potential;
            }
            *reinterpret_cast<int*>(
                buf.get() + (i - offset) * sizeof(int) * 1200
                + j * sizeof(int)) = value;
        }
    }
    return std::move(buf);
}



void skill_data::deserialize(
    std::unique_ptr<char[]> raw_data,
    int size,
    int offset)
{
    for (size_t i = size_t(offset);; ++i)
    {
        for (size_t j = 0; j < 1200; ++j)
        {
            int value = *reinterpret_cast<int*>(
                raw_data.get() + (i - offset) * sizeof(int) * 1200
                + j * sizeof(int));
            if (j < 600)
            {
                storage[i][j].current_level = value;
            }
            else
            {
                storage[i][j - 600].original_level = value / 1'000'000;
                storage[i][j - 600].experience = value % 1'000'000 / 1'000;
                storage[i][j - 600].potential = value % 1'000;
            }
        }

        size -= 1200 * sizeof(int);
        if (size == 0)
            return;
    }
}


} // namespace elona