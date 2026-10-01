#pragma once
#include "mosicrokey/contracts.hpp"
#include <memory>
namespace mosicrokey {
class App final : public IEvents {
public:
    App();
    ~App();
    Result Start(IXiaozhiService& voice, IAgentService& agent, IGameLibrary& library, IGameRuntime& runtime);
    void Input(Key key, bool pressed);
    Result Navigate(Mode mode);
    Result Ask(const std::string& text, AgentIntent intent = AgentIntent::Conversation);
    Result ConfigureAgent(const AgentConfig& config);
    Result Cancel();
    Result Remount();
    Result InstallExample(const std::string& lua);
    void ConfigureWifi();
    Result ConfigureWifi(const WifiConfig& config);
    void DiagnoseAudio();
    Result TestGameRuntime(const std::string& source, std::string& report);
    Result CaptureGameFrame(std::vector<uint16_t>& frame) const;
    Result ReadGameSource(const std::string& id,unsigned offset,std::string& output) const;
    Snapshot Read() const;
    void Voice(const std::string& status, const std::string& text) override;
    void Agent(uint32_t request, bool busy, const std::string& text) override;
    void Notice(const std::string& text) override;
    void GameEnded(const std::string& reason) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
Result StartConsole(App& app);
}
