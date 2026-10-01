#pragma once
#include "mosicrokey/contracts.hpp"
#include <memory>
namespace mosicrokey {
class GameService final : public IGameLibrary, public IGameRuntime {
public:
    explicit GameService(std::string root, IEvents& events);
    ~GameService();
    Result List(std::vector<GameInfo>& games) override;
    Result Tool(const std::string& name, const std::string& json, std::string& output) override;
    Result Start(const GameInfo& game) override;
    void Stop() override;
    void Input(Key key, bool pressed) override;
    Result Tick(float seconds, std::vector<uint16_t>& frame) override;
    bool Running() const override;
    Result SelfTest(const std::string& source, std::string& report) override;
    Result CaptureFrame(std::vector<uint16_t>& frame) const override;
    Result ImportExample(const std::string& lua) override;
    void DiscardStaging() override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
