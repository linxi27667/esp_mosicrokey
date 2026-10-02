#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace mosicrokey {
constexpr int kWidth = 160, kHeight = 128;
enum class Mode { Home, Xiaozhi, Agent, Library, Game, Settings, Diagnostics, Computer, InputTest, DisplayTest };
enum class Key { Up, Down, Left, Right, Confirm, Back, Function, SwitchMode, EncoderPress };
enum class PcProfile { Media, Shortcuts };
enum class PcAckStatus { Failed, Executed, Simulated };
enum class Error { Ok, Busy, Invalid, Storage, NotReady, Timeout, Runtime };
struct Result {
    Error error = Error::Ok;
    std::string message;
    explicit operator bool() const { return error == Error::Ok; }
    static Result Fail(Error error, std::string message) { return {error, std::move(message)}; }
};
struct GameInfo { std::string id, title, revision, path; bool temporary = false; };
struct AgentConfig { std::string url, model, key; };
struct WifiConfig { std::string ssid, password; };
enum class AgentIntent { Conversation, PublishGame };
struct Snapshot {
    Mode mode = Mode::Home;
    int selection = 0;
    bool wifi = false, storage = false, agent_busy = false;
    std::string voice_status = "启动中", voice_text, agent_text, notice;
    std::vector<GameInfo> games;
    uint32_t request_id = 0;
    uint16_t key_mask = 0;
    uint32_t input_events = 0;
    int32_t encoder_total = 0;
    std::string last_input = "等待输入";
    bool pc_connected = false;
    PcProfile pc_profile = PcProfile::Media;
    uint32_t pc_sequence = 0;
    std::string pc_action;
    std::string pc_result = "请启动电脑连接程序";
};
class IXiaozhiService {
public:
    virtual ~IXiaozhiService() = default;
    virtual void SetForeground(bool enabled) = 0;
    virtual void ToggleChat() = 0;
    virtual void ConfigureWifi() = 0;
    virtual Result ConfigureWifi(const WifiConfig& config) = 0;
    virtual void DiagnoseAudio() = 0;
};
class IAgentService {
public:
    virtual ~IAgentService() = default;
    virtual Result Submit(const std::string& text, uint32_t& request, AgentIntent intent = AgentIntent::Conversation) = 0;
    virtual Result Cancel(uint32_t request) = 0;
    virtual Result Configure(const AgentConfig& config) = 0;
};
class IGameLibrary {
public:
    virtual ~IGameLibrary() = default;
    virtual Result List(std::vector<GameInfo>& games) = 0;
    virtual Result Tool(const std::string& name, const std::string& json, std::string& output) = 0;
    virtual Result ImportExample(const std::string& lua) = 0;
    virtual void DiscardStaging() = 0;
};
class IGameRuntime {
public:
    virtual ~IGameRuntime() = default;
    virtual Result Start(const GameInfo& game) = 0;
    virtual void Stop() = 0;
    virtual void Input(Key key, bool pressed) = 0;
    virtual Result Tick(float dt, std::vector<uint16_t>& frame) = 0;
    virtual bool Running() const = 0;
    virtual Result SelfTest(const std::string& source, std::string& report) = 0;
    virtual Result CaptureFrame(std::vector<uint16_t>& frame) const = 0;
};
class IEvents {
public:
    virtual ~IEvents() = default;
    virtual void Voice(const std::string& status, const std::string& text) = 0;
    virtual void Agent(uint32_t request, bool busy, const std::string& text) = 0;
    virtual void Notice(const std::string& text) = 0;
    virtual void GameEnded(const std::string& reason) = 0;
};
} // namespace mosicrokey
