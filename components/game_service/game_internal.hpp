#pragma once
#include "mosicrokey/game.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <cstddef>

namespace mosicrokey {
constexpr size_t kMaxSource = 64 * 1024, kMaxPackage = 512 * 1024, kLuaHeap = 256 * 1024;
struct Asset { int width = 0, height = 0; std::vector<uint16_t> pixels; };
class GameVm {
public:
    GameVm();
    ~GameVm();
    Result Load(const std::string& source, const std::map<std::string, Asset>& assets);
    Result Tick(float dt, uint32_t input, std::vector<uint16_t>& frame);
    void CopyFrame(std::vector<uint16_t>& frame) const;
private:
    struct State;
    std::unique_ptr<State> state_;
};
struct Stage {
    std::string job, id, title, revision, path;
    bool validated = false;
};
struct GameService::Impl {
    std::string root;
    IEvents& events;
    std::mutex library_lock, vm_lock;
    std::map<std::string, Stage> stages;
    std::unique_ptr<GameVm> vm;
    std::atomic<uint32_t> keys{0};
    unsigned job_sequence = 0;
    unsigned preview_sequence = 0;
    GameInfo preview;
    std::string preview_source;
    Impl(std::string root, IEvents& events) : root(std::move(root)), events(events) {}
};
uint32_t Checksum(const std::string& bytes);
std::string HexChecksum(const std::string& bytes);
Result ReadFile(const std::string& path, std::string& bytes, size_t limit);
Result WriteFile(const std::string& path, const std::string& bytes);
Result InspectPackage(const std::string& path, GameInfo& info, std::string* source = nullptr,
                      std::map<std::string, Asset>* assets = nullptr);
void RasterPixel(std::vector<uint16_t>& pixels, int x, int y, uint16_t color);
void RasterRect(std::vector<uint16_t>& pixels, int x, int y, int w, int h, uint16_t color);
void RasterLine(std::vector<uint16_t>& pixels, int x0, int y0, int x1, int y1, uint16_t color);
void RasterText(std::vector<uint16_t>& pixels, int x, int y, const char* text, uint16_t color);
}
