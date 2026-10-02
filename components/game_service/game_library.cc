#include "game_internal.hpp"
#include <cJSON.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

namespace mosicrokey {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
Json Parse(const std::string& text) { return Json(cJSON_ParseWithLength(text.data(), text.size()), cJSON_Delete); }
std::string String(cJSON* object, const char* key) {
    auto* value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) ? value->valuestring : "";
}
bool Identifier(const std::string& value) {
    return !value.empty() && value.size() <= 32 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
bool SafeFile(const std::string& path) {
    if (path == "main.lua") return true;
    if (!path.starts_with("assets/") || !path.ends_with(".rgb565")) return false;
    auto name = path.substr(7, path.size() - 14);
    return Identifier(name);
}
bool IsDirectory(const std::string& path) { struct stat st{}; return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
bool Mkdir(const std::string& path) {
#ifdef _WIN32
    int result = _mkdir(path.c_str());
#else
    int result = mkdir(path.c_str(), 0755);
#endif
    return result == 0 || (errno == EEXIST && IsDirectory(path));
}
Result Game_Commit_Directory(const std::string& source,const std::string& destination) {
#ifdef _WIN32
    // Windows 主机文件句柄可能短暂禁止目录移动；最多等 100 ms，不覆盖已有版本。
    int file_error=0; DWORD native_error=0;
    for(unsigned attempt=0;attempt<=10;++attempt) {
        if(std::rename(source.c_str(),destination.c_str())==0) return {};
        file_error=errno; native_error=GetLastError();
        bool locked=native_error==ERROR_ACCESS_DENIED || native_error==ERROR_SHARING_VIOLATION || native_error==ERROR_LOCK_VIOLATION;
        if(!locked || GetFileAttributesA(destination.c_str())!=INVALID_FILE_ATTRIBUTES || attempt==10) break;
        Sleep(10);
    }
    return Result::Fail(Error::Storage,"Commit rename failed: errno="+std::to_string(file_error)+" win32="+std::to_string(native_error));
#else
    if(std::rename(source.c_str(),destination.c_str())==0) return {};
    return Result::Fail(Error::Storage,std::string("Commit rename failed: ")+std::strerror(errno));
#endif
}
std::vector<std::string> Directories(const std::string& path) {
    std::vector<std::string> result;
    auto* directory = opendir(path.c_str());
    if (!directory) return result;
    while (auto* entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name != "." && name != ".." && IsDirectory(path + "/" + name)) result.push_back(name);
    }
    closedir(directory);
    std::sort(result.begin(), result.end());
    return result;
}
std::vector<std::string> PackageFiles(const std::string& path) {
    std::vector<std::string> files;
    struct stat st{};
    if (stat((path + "/main.lua").c_str(), &st) == 0) files.emplace_back("main.lua");
    auto* directory = opendir((path + "/assets").c_str());
    if (!directory) return files;
    while (auto* entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") files.push_back("assets/" + name);
    }
    closedir(directory);
    return files;
}
Result DecodeAsset(const std::string& bytes, Asset& asset) {
    if (bytes.size() < 4) return Result::Fail(Error::Invalid, "RGB565 header missing");
    auto u8 = [&](size_t i) { return static_cast<unsigned char>(bytes[i]); };
    asset.width = u8(0) | (u8(1) << 8); asset.height = u8(2) | (u8(3) << 8);
    if (asset.width < 1 || asset.width > kWidth || asset.height < 1 || asset.height > kHeight ||
        bytes.size() != 4 + static_cast<size_t>(asset.width * asset.height * 2))
        return Result::Fail(Error::Invalid, "Invalid RGB565 dimensions or length");
    asset.pixels.resize(asset.width * asset.height);
    for (size_t i = 0; i < asset.pixels.size(); ++i) asset.pixels[i] = u8(4 + i * 2) | (u8(5 + i * 2) << 8);
    return {};
}
Result Base64(const std::string& input, std::string& output) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (input.size() % 4 || input.size() > kMaxPackage * 4 / 3 + 4) return Result::Fail(Error::Invalid, "Invalid base64 length");
    output.clear();
    for (size_t i = 0; i < input.size(); i += 4) {
        uint32_t bits = 0; int padding = 0;
        for (int j = 0; j < 4; ++j) {
            char c = input[i + j]; const char* found = c == '\0' ? nullptr : std::strchr(alphabet, c);
            if (c == '=') { ++padding; bits <<= 6; }
            else if (!found || padding) return Result::Fail(Error::Invalid, "Invalid base64 character");
            else bits = (bits << 6) | static_cast<unsigned>(found - alphabet);
        }
        if (padding > 2 || (padding && i + 4 != input.size())) return Result::Fail(Error::Invalid, "Invalid base64 padding");
        output.push_back(static_cast<char>(bits >> 16));
        if (padding < 2) output.push_back(static_cast<char>(bits >> 8));
        if (padding < 1) output.push_back(static_cast<char>(bits));
    }
    return {};
}
std::string Dump(cJSON* object) {
    char* text = cJSON_PrintUnformatted(object);
    std::string result = text ? text : ""; cJSON_free(text); return result;
}
Result ValidateStage(Stage& stage) {
    auto manifest = Json(cJSON_CreateObject(), cJSON_Delete);
    cJSON_AddNumberToObject(manifest.get(), "format_version", 1);
    cJSON_AddNumberToObject(manifest.get(), "api_version", 1);
    cJSON_AddStringToObject(manifest.get(), "id", stage.id.c_str());
    cJSON_AddStringToObject(manifest.get(), "title", stage.title.c_str());
    cJSON_AddStringToObject(manifest.get(), "revision", stage.revision.c_str());
    cJSON_AddStringToObject(manifest.get(), "entry", "main.lua");
    auto* files = cJSON_AddArrayToObject(manifest.get(), "files");
    size_t total = 0;
    for (const auto& file : PackageFiles(stage.path)) {
        if (!SafeFile(file)) return Result::Fail(Error::Invalid, "Invalid asset path");
        std::string bytes; auto result = ReadFile(stage.path + "/" + file, bytes, file == "main.lua" ? kMaxSource : kMaxPackage);
        if (!result) return result;
        total += bytes.size();
        if (total > kMaxPackage) return Result::Fail(Error::Invalid, "Package exceeds 512 KiB");
        auto* item = cJSON_CreateObject(); cJSON_AddItemToArray(files, item);
        cJSON_AddStringToObject(item, "path", file.c_str());
        cJSON_AddNumberToObject(item, "size", bytes.size());
        cJSON_AddStringToObject(item, "checksum", HexChecksum(bytes).c_str());
    }
    auto result = WriteFile(stage.path + "/manifest.json", Dump(manifest.get()));
    if (!result) return result;
    GameInfo info; std::string source; std::map<std::string, Asset> assets;
    result = InspectPackage(stage.path, info, &source, &assets);
    if (!result) return result;
    GameVm probe;
    result = probe.Load(source, assets);
    std::vector<uint16_t> frame;
    for (int i = 0; result && i < 3; ++i) result = probe.Tick(1.0f / 30.0f, 0, frame);
    stage.validated = static_cast<bool>(result);
    return result;
}
} // namespace

uint32_t Checksum(const std::string& bytes) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : bytes) hash = (hash ^ c) * 16777619u;
    return hash;
}
std::string HexChecksum(const std::string& bytes) { char text[9]; std::snprintf(text, sizeof(text), "%08x", static_cast<unsigned>(Checksum(bytes))); return text; }
Result ReadFile(const std::string& path, std::string& bytes, size_t limit) {
    struct stat st{};
    if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || static_cast<uint64_t>(st.st_size) > limit)
        return Result::Fail(Error::Storage, "File missing, unreadable or too large: " + path);
    auto* file = std::fopen(path.c_str(), "rb");
    if (!file) return Result::Fail(Error::Storage, "Cannot open: " + path);
    bytes.resize(st.st_size);
    auto count = std::fread(bytes.data(), 1, bytes.size(), file);
    bool failed = std::ferror(file); std::fclose(file);
    return count == bytes.size() && !failed ? Result{} : Result::Fail(Error::Storage, "Read failed: " + path);
}
Result WriteFile(const std::string& path, const std::string& bytes) {
    auto* file = std::fopen(path.c_str(), "wb");
    if (!file) return Result::Fail(Error::Storage, "Cannot write: " + path);
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fflush(file)) ok = false;
#ifndef _WIN32
    if (fsync(fileno(file))) ok = false;
#endif
    if (std::fclose(file)) ok = false;
    return ok ? Result{} : Result::Fail(Error::Storage, "Write failed; check card and free space");
}
Result InspectPackage(const std::string& path, GameInfo& info, std::string* source, std::map<std::string, Asset>* assets) {
    std::string text; auto result = ReadFile(path + "/manifest.json", text, 16384);
    if (!result) return result;
    auto json = Parse(text);
    auto* format = cJSON_GetObjectItem(json.get(), "format_version");
    auto* api = cJSON_GetObjectItem(json.get(), "api_version");
    if (!cJSON_IsNumber(format) || format->valuedouble != 1 || !cJSON_IsNumber(api) || api->valuedouble != 1 || String(json.get(), "entry") != "main.lua")
        return Result::Fail(Error::Invalid, "Unsupported game format or API");
    info = {String(json.get(), "id"), String(json.get(), "title"), String(json.get(), "revision"), path};
    if (!Identifier(info.id) || info.title.empty() || info.title.size() > 96 || info.revision.size() != 9 || info.revision[0] != 'r' ||
        !std::all_of(info.revision.begin() + 1, info.revision.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return Result::Fail(Error::Invalid, "Invalid game identity");
    auto* files = cJSON_GetObjectItem(json.get(), "files");
    if (!cJSON_IsArray(files) || cJSON_GetArraySize(files) > 32) return Result::Fail(Error::Invalid, "Invalid file list");
    bool has_entry = false; size_t total = text.size(); std::vector<std::string> seen;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, files) {
        auto file = String(item, "path");
        if (!SafeFile(file) || std::find(seen.begin(), seen.end(), file) != seen.end()) return Result::Fail(Error::Invalid, "Unsafe or duplicate file path");
        seen.push_back(file);
        std::string bytes; result = ReadFile(path + "/" + file, bytes, file == "main.lua" ? kMaxSource : kMaxPackage);
        if (!result) return result;
        auto* size = cJSON_GetObjectItem(item, "size");
        total += bytes.size();
        if (!cJSON_IsNumber(size) || size->valuedouble != bytes.size() || String(item, "checksum") != HexChecksum(bytes) || total > kMaxPackage)
            return Result::Fail(Error::Invalid, "Package checksum, length or size limit failed");
        if (file == "main.lua") { has_entry = true; if (source) *source = bytes; }
        else {
            Asset asset; result = DecodeAsset(bytes, asset); if (!result) return result;
            if (assets) assets->emplace(file, std::move(asset));
        }
    }
    return has_entry ? Result{} : Result::Fail(Error::Invalid, "main.lua missing");
}
GameService::GameService(std::string root, IEvents& events) : impl_(std::make_unique<Impl>(std::move(root), events)) {}
GameService::~GameService() = default;
Result GameService::List(std::vector<GameInfo>& games) {
    std::lock_guard lock(impl_->library_lock); games.clear();
    if (!impl_->preview_source.empty()) games.push_back(impl_->preview);
    if (!IsDirectory(impl_->root)) return games.empty() ? Result::Fail(Error::Storage, "TF card unavailable") : Result{};
    auto root = impl_->root + "/games";
    if (!Mkdir(root)) return Result::Fail(Error::Storage, "Game directory unavailable");
    for (const auto& id : Directories(root)) {
        if (!Identifier(id)) continue;
        auto revisions = Directories(root + "/" + id);
        for (auto revision = revisions.rbegin(); revision != revisions.rend(); ++revision) {
            auto path = root + "/" + id + "/" + *revision;
            std::string marker, manifest;
            if (!ReadFile(path + "/READY", marker, 8) || !ReadFile(path + "/manifest.json", manifest, 16384) || marker != HexChecksum(manifest)) continue;
            GameInfo info;
            if (InspectPackage(path, info) && info.id == id && info.revision == *revision) { games.push_back(std::move(info)); break; }
        }
    }
    return {};
}
Result GameService::Tool(const std::string& name, const std::string& input, std::string& output) {
    if (name == "game_list") {
        std::vector<GameInfo> games; auto result = List(games); if (!result) return result;
        auto request=Parse(input);
        if(!cJSON_IsObject(request.get())) return Result::Fail(Error::Invalid,"Expected JSON object");
        auto id=String(request.get(),"id");
        if(!id.empty()) {
            auto found=std::find_if(games.begin(),games.end(),[&](const auto& game){return game.id==id;});
            if(found==games.end()) return Result::Fail(Error::Invalid,"Game not found");
            std::lock_guard lock(impl_->library_lock);
            std::string source,manifest;
            if(found->temporary) {
                if(found->revision!=impl_->preview.revision) return Result::Fail(Error::Busy,"Preview changed; refresh list");
                source=impl_->preview_source;
                auto info=Json(cJSON_CreateObject(),cJSON_Delete);
                cJSON_AddStringToObject(info.get(),"id",found->id.c_str());
                cJSON_AddStringToObject(info.get(),"title",found->title.c_str());
                cJSON_AddStringToObject(info.get(),"revision",found->revision.c_str());
                cJSON_AddBoolToObject(info.get(),"persistent",false);
                cJSON_AddNumberToObject(info.get(),"api_version",1);
                manifest=Dump(info.get());
            } else {
                result=ReadFile(found->path+"/main.lua",source,kMaxSource); if(!result) return result;
                result=ReadFile(found->path+"/manifest.json",manifest,16384); if(!result) return result;
            }
            auto* offset=cJSON_GetObjectItem(request.get(),"offset");
            double position=offset ? offset->valuedouble : 0;
            if((offset && !cJSON_IsNumber(offset)) || position<0 || position>source.size() || position!=static_cast<size_t>(position))
                return Result::Fail(Error::Invalid,"Invalid source offset");
            size_t start=static_cast<size_t>(position),end=std::min(start+4096,source.size());
            if(start<source.size() && (static_cast<unsigned char>(source[start])&0xc0)==0x80) return Result::Fail(Error::Invalid,"Offset splits UTF-8 character");
            while(end<source.size() && end>start && (static_cast<unsigned char>(source[end])&0xc0)==0x80) --end;
            auto object=Parse(manifest);
            cJSON_AddStringToObject(object.get(),"source",source.substr(start,end-start).c_str());
            cJSON_AddNumberToObject(object.get(),"next_offset",end);
            cJSON_AddBoolToObject(object.get(),"eof",end==source.size());
            output=Dump(object.get()); return {};
        }
        auto array = Json(cJSON_CreateArray(), cJSON_Delete);
        for (const auto& game : games) { auto* object = cJSON_CreateObject(); cJSON_AddItemToArray(array.get(), object);
            cJSON_AddStringToObject(object, "id", game.id.c_str()); cJSON_AddStringToObject(object, "title", game.title.c_str());
            cJSON_AddStringToObject(object, "revision", game.revision.c_str());
            cJSON_AddBoolToObject(object,"persistent",!game.temporary); }
        output = Dump(array.get()); return {};
    }
    std::lock_guard lock(impl_->library_lock);
    auto json = Parse(input); if (!cJSON_IsObject(json.get())) return Result::Fail(Error::Invalid, "Expected JSON object");
    if(name=="game_preview") {
        auto id=String(json.get(),"id"),title=String(json.get(),"title"),source=String(json.get(),"source");
        if(!Identifier(id) || id.size()>24) return Result::Fail(Error::Invalid,"Invalid preview id: use 1..24 lowercase ASCII letters/digits/-/_");
        if(title.empty() || title.size()>89) return Result::Fail(Error::Invalid,"Invalid preview title: string required, 1..89 UTF-8 bytes");
        if(source.empty()) return Result::Fail(Error::Invalid,"Missing source: supply complete Lua in the source string property");
        if(source.size()>kMaxSource) return Result::Fail(Error::Invalid,"Preview source exceeds 64 KiB");
        GameVm probe; auto result=probe.Load(source,{}); std::vector<uint16_t> frame;
        for(int i=0;result && i<3;++i) result=probe.Tick(1.f/30,0,frame);
        if(!result) return result;
        impl_->preview={"preview-"+id,"临时 "+title,"p"+std::to_string(++impl_->preview_sequence),"ram://preview/"+id,true};
        impl_->preview_source=std::move(source);
        auto response=Json(cJSON_CreateObject(),cJSON_Delete);
        cJSON_AddBoolToObject(response.get(),"preview",true);
        cJSON_AddBoolToObject(response.get(),"persistent",false);
        cJSON_AddStringToObject(response.get(),"id",impl_->preview.id.c_str());
        cJSON_AddStringToObject(response.get(),"revision",impl_->preview.revision.c_str());
        output=Dump(response.get()); return {};
    }
    auto games_root = impl_->root + "/games";
    if (!IsDirectory(impl_->root) || !Mkdir(games_root) || !Mkdir(games_root + "/.staging")) return Result::Fail(Error::Storage, "TF card unavailable");
    if (name == "game_begin") {
        auto id = String(json.get(), "id"), title = String(json.get(), "title");
        if (!Identifier(id) || title.empty() || title.size() > 96) return Result::Fail(Error::Invalid, "Invalid id or title");
        if (impl_->stages.size() >= 4) return Result::Fail(Error::Busy, "Too many staged games");
        if (!Mkdir(games_root + "/" + id)) return Result::Fail(Error::Storage, "Cannot create game directory");
        unsigned revision = 1;
        for (const auto& entry : Directories(games_root + "/" + id)) if (entry.size() == 9 && entry[0] == 'r') revision = std::max(revision, static_cast<unsigned>(std::strtoul(entry.c_str() + 1, nullptr, 10) + 1));
        for (const auto& [job, stage] : impl_->stages) if (stage.id == id) revision = std::max(revision, static_cast<unsigned>(std::strtoul(stage.revision.c_str() + 1, nullptr, 10) + 1));
        if (revision > 99999999) return Result::Fail(Error::Invalid, "Revision limit reached");
        std::string job, path;
        do { job = "job" + std::to_string(++impl_->job_sequence); path = games_root + "/.staging/" + job; } while (IsDirectory(path));
        if (!Mkdir(path) || !Mkdir(path + "/assets")) return Result::Fail(Error::Storage, "Cannot create staging directory");
        // Carry forward optional artwork when revising a game; main.lua is supplied by the Agent.
        auto versions=Directories(games_root+"/"+id);
        for(auto version=versions.rbegin();version!=versions.rend();++version) {
            auto previous=games_root+"/"+id+"/"+*version;
            GameInfo info; std::string marker,manifest;
            if(!ReadFile(previous+"/READY",marker,8) || !ReadFile(previous+"/manifest.json",manifest,16384) ||
               marker!=HexChecksum(manifest) || !InspectPackage(previous,info) || info.id!=id || info.revision!=*version) continue;
            for(const auto& file:PackageFiles(previous)) if(file!="main.lua" && SafeFile(file)) {
                std::string bytes; auto result=ReadFile(previous+"/"+file,bytes,kMaxPackage);
                if(result) result=WriteFile(path+"/"+file,bytes);
                if(!result) return result;
            }
            break;
        }
        char revision_text[10]; std::snprintf(revision_text, sizeof(revision_text), "r%08u", revision);
        impl_->stages.emplace(job, Stage{job, id, title, revision_text, path, false});
        output = "{\"job\":\"" + job + "\",\"revision\":\"" + revision_text + "\"}"; return {};
    }
    auto found = impl_->stages.find(String(json.get(), "job"));
    if (found == impl_->stages.end()) return Result::Fail(Error::Invalid, "Unknown staging job");
    auto& stage = found->second;
    if (name == "game_write_file") {
        auto path = String(json.get(), "path");
        if (!SafeFile(path)) return Result::Fail(Error::Invalid, "Only main.lua and assets/*.rgb565 are allowed");
        std::string bytes = String(json.get(), "content"), encoding = String(json.get(), "encoding");
        if (encoding == "base64") { std::string decoded; auto result = Base64(bytes, decoded); if (!result) return result; bytes = std::move(decoded); }
        else if (encoding != "utf8" && !encoding.empty()) return Result::Fail(Error::Invalid, "Unknown encoding");
        if (bytes.size() > (path == "main.lua" ? kMaxSource : kMaxPackage)) return Result::Fail(Error::Invalid, "File size limit");
        size_t total = bytes.size();
        for (const auto& file : PackageFiles(stage.path)) { struct stat st{}; if (file != path && stat((stage.path + "/" + file).c_str(), &st) == 0) total += st.st_size; }
        if (total > kMaxPackage) return Result::Fail(Error::Invalid, "Package size limit");
        stage.validated = false; auto result = WriteFile(stage.path + "/" + path, bytes);
        output = "{\"written\":true}"; return result;
    }
    if (name == "game_validate") { stage.validated = false; auto result = ValidateStage(stage); output = "{\"valid\":true}"; return result; }
    if (name == "game_commit") {
        if (!stage.validated) return Result::Fail(Error::Invalid, "Validate before commit");
        auto result = ValidateStage(stage); if (!result) return result;
        std::string manifest; result = ReadFile(stage.path + "/manifest.json", manifest, 16384); if (!result) return result;
        result = WriteFile(stage.path + "/READY", HexChecksum(manifest)); if (!result) return result;
        auto destination = games_root + "/" + stage.id + "/" + stage.revision;
        result=Game_Commit_Directory(stage.path,destination); if(!result) return result;
        output = "{\"installed\":true,\"id\":\"" + stage.id + "\"}";
        impl_->stages.erase(found); return {};
    }
    return Result::Fail(Error::Invalid, "Unknown game tool");
}
Result GameService::Start(const GameInfo& game) {
    GameInfo inspected; std::string source; std::map<std::string, Asset> assets;
    { std::lock_guard lock(impl_->library_lock);
      if(game.temporary) {
          if(impl_->preview_source.empty() || game.id!=impl_->preview.id || game.revision!=impl_->preview.revision || game.path!=impl_->preview.path)
              return Result::Fail(Error::Invalid,"Preview expired; refresh game library");
          source=impl_->preview_source;
      } else {
      auto expected = impl_->root + "/games/" + game.id + "/" + game.revision;
      if (!Identifier(game.id) || game.revision.size()!=9 || game.revision[0]!='r' ||
          !std::all_of(game.revision.begin()+1,game.revision.end(),[](char c){return c>='0'&&c<='9';}) ||
          game.path != expected) return Result::Fail(Error::Invalid, "Game path outside library");
      std::string marker, manifest;
      auto result=ReadFile(game.path+"/READY",marker,32); if(!result) return result;
      result=ReadFile(game.path+"/manifest.json",manifest,16384); if(!result) return result;
      if(marker!=HexChecksum(manifest)) return Result::Fail(Error::Invalid,"Incomplete game revision");
      result = InspectPackage(game.path, inspected, &source, &assets); if (!result) return result;
      if(inspected.id!=game.id || inspected.revision!=game.revision) return Result::Fail(Error::Invalid,"Game identity mismatch"); } }
    auto vm = std::make_unique<GameVm>(); auto result = vm->Load(source, assets); if (!result) return result;
    std::lock_guard lock(impl_->vm_lock); impl_->keys = 0; impl_->vm = std::move(vm); return {};
}
void GameService::Stop() { std::lock_guard lock(impl_->vm_lock); impl_->vm.reset(); impl_->keys = 0; }
void GameService::Input(Key key, bool pressed) { uint32_t bit = 1u << static_cast<unsigned>(key); if (pressed) impl_->keys.fetch_or(bit); else impl_->keys.fetch_and(~bit); }
bool GameService::Running() const { std::lock_guard lock(impl_->vm_lock); return impl_->vm != nullptr; }
Result GameService::CaptureFrame(std::vector<uint16_t>& frame) const {
    std::lock_guard lock(impl_->vm_lock);
    if(!impl_->vm) return Result::Fail(Error::NotReady,"No game running");
    impl_->vm->CopyFrame(frame); return {};
}
Result GameService::Tick(float dt, std::vector<uint16_t>& frame) {
    std::lock_guard lock(impl_->vm_lock);
    if (!impl_->vm) return Result::Fail(Error::NotReady, "No game running");
    auto result = impl_->vm->Tick(dt, impl_->keys.load(), frame);
    if (!result) impl_->vm.reset();
    return result;
}
Result GameService::ImportExample(const std::string& lua) {
    std::string output; auto result = Tool("game_begin", "{\"id\":\"snake\",\"title\":\"贪吃蛇\"}", output); if (!result) return result;
    auto begin = Parse(output); auto job = String(begin.get(), "job");
    auto write = Json(cJSON_CreateObject(), cJSON_Delete);
    cJSON_AddStringToObject(write.get(), "job", job.c_str()); cJSON_AddStringToObject(write.get(), "path", "main.lua"); cJSON_AddStringToObject(write.get(), "content", lua.c_str());
    result = Tool("game_write_file", Dump(write.get()), output); if (!result) return result;
    auto request = "{\"job\":\"" + job + "\"}";
    result = Tool("game_validate", request, output); return result ? Tool("game_commit", request, output) : result;
}
void GameService::DiscardStaging() {
    std::lock_guard lock(impl_->library_lock);
    // Only remove files in jobs created by this instance, never recurse through arbitrary paths.
    for (const auto& [job,stage] : impl_->stages) {
        for(const auto& file:PackageFiles(stage.path)) if(SafeFile(file)) std::remove((stage.path+"/"+file).c_str());
        std::remove((stage.path+"/manifest.json").c_str()); std::remove((stage.path+"/READY").c_str());
        rmdir((stage.path+"/assets").c_str()); rmdir(stage.path.c_str());
    }
    impl_->stages.clear();
}
} // namespace mosicrokey
