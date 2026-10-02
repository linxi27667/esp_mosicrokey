#pragma once
#include "mosicrokey/contracts.hpp"
#include <memory>
namespace mosicrokey
{
class App final : public IEvents
{
  public:
    App();
    ~App();
    Result System_Start(IXiaozhiService& voice, IAgentService& agent, IGameLibrary& library,
                        IGameRuntime& runtime);
    void Key_Input(Key key, bool pressed);
    // 正值为顺时针格数；异步提交，操作队列满时返回 Busy。
    Result Encoder_Input(int detents);
    // 串口连接程序每秒更新一次；超过 3 秒停发电脑命令，不重放离线操作。
    Result Pc_Bridge_Update(uint32_t session);
    Result Pc_Action_Acknowledge(uint32_t session, uint32_t sequence, PcAckStatus status);
    Result Page_Navigate(Mode mode);
    Result Agent_Ask(const std::string& text, AgentIntent intent = AgentIntent::Conversation);
    Result Agent_Configure(const AgentConfig& config);
    Result Agent_Cancel();
    Result Storage_Remount();
    Result Game_Install_Example(const std::string& lua);
    Result Game_Preview_Example(const std::string& lua);
    void Wifi_Configure();
    Result Wifi_Configure(const WifiConfig& config);
    void Audio_Diagnose();
    Result Game_Run_Self_Test(const std::string& source, std::string& report);
    Result Game_Capture_Frame(std::vector<uint16_t>& frame) const;
    Result Game_Read_Source(const std::string& id, unsigned offset, std::string& output) const;
    Snapshot State_Read() const;
    void Voice(const std::string& status, const std::string& text) override;
    void Agent(uint32_t request, bool busy, const std::string& text) override;
    void Notice(const std::string& text) override;
    void GameEnded(const std::string& reason) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mosicrokey
