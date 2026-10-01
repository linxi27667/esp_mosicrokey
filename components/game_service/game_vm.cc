#include "game_internal.hpp"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#include <esp_timer.h>
#endif

namespace mosicrokey {
namespace {
uint64_t Micros() {
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
}
struct GameVm::State {
    lua_State* lua = nullptr;
    size_t allocated = 0;
    uint64_t deadline = 0;
    unsigned instructions = 0, draws = 0;
    float dt=0;
    uint32_t input=0;
    uint32_t random = 0x12345678;
    std::vector<uint16_t> frame = std::vector<uint16_t>(kWidth * kHeight);
    std::map<std::string, Asset> assets;
    ~State() { if (lua) lua_close(lua); }
    static void* Allocate(void* context, void* ptr, size_t old_size, size_t new_size) {
        auto& self = *static_cast<State*>(context);
        if (!ptr) old_size = 0;
        if (new_size == 0) { std::free(ptr); self.allocated -= old_size; return nullptr; }
        if (new_size > kLuaHeap || self.allocated - old_size > kLuaHeap - new_size) return nullptr;
#ifdef ESP_PLATFORM
        void* next = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        void* next = std::realloc(ptr, new_size);
#endif
        if (next) self.allocated = self.allocated - old_size + new_size;
        return next;
    }
    static State& Self(lua_State* lua) { return **static_cast<State**>(lua_getextraspace(lua)); }
    static void Hook(lua_State* lua, lua_Debug*) {
        auto& self = Self(lua); self.instructions += 1000;
        if (Micros() > self.deadline || self.instructions > 100000) luaL_error(lua, "Game execution budget exceeded");
    }
    static void DrawGuard(lua_State* lua) {
        auto& self = Self(lua);
        if (++self.draws > 1024) luaL_error(lua, "Drawing operation budget exceeded");
        if (Micros() > self.deadline) luaL_error(lua, "Execution time budget exceeded during drawing");
    }
    static int Coordinate(lua_State* lua, int index) {
        auto value = luaL_checkinteger(lua, index);
        if (value < -1024 || value > 1024) luaL_error(lua, "Coordinate outside API range");
        return static_cast<int>(value);
    }
    static uint16_t Color(lua_State* lua, int index) {
        auto value = luaL_checkinteger(lua, index);
        if (value < 0 || value > 65535) luaL_error(lua, "RGB565 color outside range");
        return static_cast<uint16_t>(value);
    }
    static int Clear(lua_State* lua) { DrawGuard(lua); std::fill(Self(lua).frame.begin(), Self(lua).frame.end(), Color(lua, 1)); return 0; }
    static int Rect(lua_State* lua) {
        DrawGuard(lua); int x = Coordinate(lua, 1), y = Coordinate(lua, 2), w = Coordinate(lua, 3), h = Coordinate(lua, 4);
        RasterRect(Self(lua).frame, x, y, w, h, Color(lua, 5)); return 0;
    }
    static int Line(lua_State* lua) { DrawGuard(lua); RasterLine(Self(lua).frame, Coordinate(lua, 1), Coordinate(lua, 2), Coordinate(lua, 3), Coordinate(lua, 4), Color(lua, 5)); return 0; }
    static int Text(lua_State* lua) {
        DrawGuard(lua); size_t size = 0; const char* text = luaL_checklstring(lua, 3, &size);
        if (size > 96 || std::strlen(text) != size ||
            std::any_of(text,text+size,[](unsigned char c){return c>127;})) return luaL_error(lua, "Text must be ASCII, at most 96 bytes");
        RasterText(Self(lua).frame, Coordinate(lua, 1), Coordinate(lua, 2), text, Color(lua, 4)); return 0;
    }
    static int Image(lua_State* lua) {
        DrawGuard(lua); auto name = luaL_checkstring(lua, 1);
        auto& self = Self(lua); auto found = self.assets.find(name);
        if (found == self.assets.end()) return luaL_error(lua, "Image not declared in manifest");
        int x = Coordinate(lua, 2), y = Coordinate(lua, 3); const auto& image = found->second;
        for (int row = 0; row < image.height; ++row) for (int column = 0; column < image.width; ++column)
            RasterPixel(self.frame, x + column, y + row, image.pixels[row * image.width + column]);
        return 0;
    }
    static int Random(lua_State* lua) {
        auto upper = luaL_checkinteger(lua, 1);
        if (upper < 1 || upper > 1000000) return luaL_error(lua, "Random upper bound outside range");
        auto& value = Self(lua).random; value ^= value << 13; value ^= value >> 17; value ^= value << 5;
        lua_pushinteger(lua, 1 + value % upper); return 1;
    }
    void Budget(uint64_t duration) { deadline = Micros() + duration; instructions = 0; draws = 0; lua_sethook(lua, Hook, LUA_MASKCOUNT, 1000); }
    Result Call(int arguments, uint64_t duration) {
        Budget(duration);
        int result = lua_pcall(lua, arguments, 0, 0);
        lua_sethook(lua, nullptr, 0, 0);
        if (result != LUA_OK) { const char* error = lua_tostring(lua, -1); std::string message = error ? error : "Lua error"; lua_pop(lua, 1); return Result::Fail(Error::Runtime, message); }
        if (Micros() > deadline) return Result::Fail(Error::Timeout, "Game execution time limit");
        return {};
    }
    static int Setup(lua_State* lua) {
        luaL_requiref(lua,"_G",luaopen_base,1); lua_pop(lua,1);
        luaL_requiref(lua,"table",luaopen_table,1); lua_pop(lua,1);
        luaL_requiref(lua,"string",luaopen_string,1); lua_pop(lua,1);
        luaL_requiref(lua,"math",luaopen_math,1); lua_pop(lua,1);
        luaL_requiref(lua,"utf8",luaopen_utf8,1); lua_pop(lua,1);
        for(const char* name:{"dofile","loadfile","load","pcall","xpcall","collectgarbage","print","warn","getmetatable","setmetatable"}) {lua_pushnil(lua); lua_setglobal(lua,name);}
        // Lua hooks cannot interrupt C pattern matching. Exclude it from the bounded API.
        lua_getglobal(lua,"string");
        for(const char* name:{"find","match","gmatch","gsub","dump","rep"}) {lua_pushnil(lua); lua_setfield(lua,-2,name);}
        lua_pop(lua,1);
        lua_getglobal(lua,"table");
        for(const char* name:{"sort","move"}) {lua_pushnil(lua); lua_setfield(lua,-2,name);}
        lua_pop(lua,1);
        static const luaL_Reg api[]={{"clear",Clear},{"rect",Rect},{"line",Line},{"text",Text},{"image",Image},{"random",Random},{nullptr,nullptr}};
        luaL_newlib(lua,api); lua_setglobal(lua,"game"); return 0;
    }
    static int RunFrame(lua_State* lua) {
        auto& self=Self(lua);
        lua_getglobal(lua,"update"); lua_pushnumber(lua,self.dt); lua_newtable(lua);
        static constexpr const char* keys[]={"up","down","left","right","confirm","back"};
        for(unsigned i=0;i<6;++i) {lua_pushboolean(lua,(self.input&(1u<<i))!=0); lua_setfield(lua,-2,keys[i]);}
        lua_call(lua,2,0); lua_getglobal(lua,"draw"); lua_call(lua,0,0); return 0;
    }
};
GameVm::GameVm() : state_(std::make_unique<State>()) {}
GameVm::~GameVm() = default;
void GameVm::CopyFrame(std::vector<uint16_t>& frame) const {frame=state_->frame;}
Result GameVm::Load(const std::string& source, const std::map<std::string, Asset>& assets) {
    if (source.empty() || source.size() > kMaxSource || static_cast<unsigned char>(source[0]) == 0x1b)
        return Result::Fail(Error::Invalid, "Only bounded Lua text is accepted");
    auto& self = *state_; self.assets = assets;
    self.lua = lua_newstate(State::Allocate, &self, 0x12345678u);
    if (!self.lua) return Result::Fail(Error::Runtime, "Lua heap unavailable");
    auto* lua = self.lua; *static_cast<State**>(lua_getextraspace(lua)) = &self;
    lua_pushcfunction(lua,State::Setup); auto setup=self.Call(0,100000); if(!setup) return setup;
    if (luaL_loadbufferx(lua, source.data(), source.size(), "main.lua", "t") != LUA_OK) {
        const char* error=lua_tostring(lua,-1); std::string message=error ? error : "Lua compiler allocation failure";
        lua_pop(lua, 1); return Result::Fail(Error::Invalid, message);
    }
    auto result = self.Call(0, 100000); if (!result) return result;
    for (const char* name : {"init", "update", "draw"}) {
        lua_getglobal(lua, name); bool function = lua_isfunction(lua, -1); lua_pop(lua, 1);
        if (!function) return Result::Fail(Error::Invalid, std::string("Missing function: ") + name);
    }
    lua_getglobal(lua, "init"); return self.Call(0, 100000);
}
Result GameVm::Tick(float dt, uint32_t input, std::vector<uint16_t>& frame) {
    auto& self = *state_; auto* lua = self.lua;
    if (!lua) return Result::Fail(Error::NotReady, "VM not loaded");
    if(!lua_checkstack(lua,16)) return Result::Fail(Error::Runtime,"Lua stack allocation failure");
    self.dt=std::clamp(dt,0.0f,0.1f); self.input=input;
    lua_pushcfunction(lua,State::RunFrame); auto result=self.Call(0,10000);
    if (result) frame = self.frame;
    return result;
}
} // namespace mosicrokey
