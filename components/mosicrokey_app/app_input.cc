#include "app_internal.hpp"
#include "mosicrokey/focus.hpp"
#include <esp_timer.h>
#include <algorithm>
#include <climits>
#include <cstdio>

extern const char snake_text[] asm("_binary_main_lua_start");

namespace mosicrokey
{
namespace
{
constexpr int64_t kPcLeaseUs = 3000000;
constexpr const char* kMediaActions[] = {"volume_up", "volume_down", "previous",
                                         "next",      "play_pause",  "mute"};
constexpr const char* kShortcutActions[] = {"copy", "paste", "undo", "redo", "save", "tab"};
} // namespace

//=====页面相关的按键业务；由 APP 操作任务串行执行=====
void App::Impl::Key_Press_Process(Key key)
{
    Snapshot snapshot = owner.State_Read();
    if (snapshot.mode == Mode::InputTest)
    {
        // 检测页让 K7/K8 保持原始输入可见；K6 是唯一退出键。
        if (key == Key::Back)
        {
            Page_Change_Process(Mode::Settings);
        }
        return;
    }
    if (key == Key::SwitchMode)
    {
        Page_Change_Process(snapshot.mode == Mode::Computer ? device_return : Mode::Computer);
        return;
    }
    if (snapshot.mode == Mode::Computer)
    {
        if (key == Key::Function)
        {
            std::lock_guard lock(mutex);
            state.pc_profile = state.pc_profile == PcProfile::Media ? PcProfile::Shortcuts : PcProfile::Media;
            state.pc_result = "按键功能已切换";
        }
        else if (key == Key::EncoderPress)
        {
            Pc_Action_Submit("mute");
        }
        else if (key <= Key::Back)
        {
            const auto* actions = snapshot.pc_profile == PcProfile::Media ? kMediaActions : kShortcutActions;
            Pc_Action_Submit(actions[static_cast<unsigned>(key)]);
        }
        return;
    }
    if (key == Key::Function)
    {
        Page_Change_Process(Mode::Home);
        return;
    }
    if (key == Key::EncoderPress)
    {
        key = Key::Confirm;
    }
    if (snapshot.mode == Mode::Library && !snapshot.notice.empty() &&
        (key == Key::Back || key == Key::Confirm))
    {
        std::lock_guard lock(mutex);
        state.notice.clear();
        return;
    }
    if (key == Key::Back)
    {
        Page_Change_Process(BackDestination(snapshot.mode));
        return;
    }
    if (snapshot.mode == Mode::DisplayTest || snapshot.mode == Mode::Diagnostics)
    {
        return;
    }
    if (snapshot.mode == Mode::Game)
    {
        return;
    }
    if (key == Key::Up || key == Key::Down || key == Key::Left || key == Key::Right)
    {
        bool backward = key == Key::Up || key == Key::Left;
        int steps = snapshot.mode == Mode::Home && (key == Key::Up || key == Key::Down) ? 2 : 1;
        Encoder_Rotate_Process(backward ? -steps : steps);
        return;
    }
    if (key != Key::Confirm)
    {
        return;
    }
    switch (snapshot.mode)
    {
    case Mode::Home:
        Page_Change_Process(HomeDestination(snapshot.selection));
        break;
    case Mode::Xiaozhi:
        voice->ToggleChat();
        break;
    case Mode::Agent:
        if (snapshot.agent_busy)
        {
            owner.Agent_Cancel();
        }
        break;
    case Mode::Settings:
        switch (snapshot.selection)
        {
        case 0:
            owner.Wifi_Configure();
            break;
        case 1:
            owner.Storage_Remount();
            break;
        case 2:
            Page_Change_Process(Mode::InputTest);
            break;
        case 3:
            Page_Change_Process(Mode::DisplayTest);
            break;
        case 4:
        {
            auto result = owner.Game_Preview_Example(snake_text);
            if (result)
            {
                Page_Change_Process(Mode::Library);
            }
            else
            {
                owner.Notice(result.message);
            }
            break;
        }
        }
        break;
    case Mode::Library:
        if (snapshot.selection >= 0 && snapshot.selection < static_cast<int>(snapshot.games.size()))
        {
            auto result = runtime->Start(snapshot.games[snapshot.selection]);
            Storage_Check_Result(result);
            if (result)
            {
                ++game_generation;
                Page_Change_Process(Mode::Game);
            }
            else
            {
                owner.Notice(result.message);
            }
        }
        break;
    default:
        break;
    }
}

void App::Impl::Encoder_Rotate_Process(int detents)
{
    auto snapshot = owner.State_Read();
    if (snapshot.mode == Mode::Computer)
    {
        Pc_Action_Submit(detents > 0 ? "volume_up" : "volume_down", std::min(20, std::abs(detents)));
        return;
    }
    int limit = 0;
    switch (snapshot.mode)
    {
    case Mode::Home:
        limit = 6;
        break;
    case Mode::Settings:
        limit = 5;
        break;
    case Mode::Library:
        limit = snapshot.games.size();
        break;
    case Mode::Xiaozhi:
    case Mode::Agent:
        limit = 2048;
        break;
    default:
        return;
    }
    std::lock_guard lock(mutex);
    state.selection = std::clamp(state.selection + detents, 0, std::max(0, limit - 1));
}

Result App::Encoder_Input(int detents)
{
    if (!detents || detents < -1024 || detents > 1024)
    {
        return Result::Fail(Error::Invalid, "Encoder detents must be -1024..1024 excluding zero");
    }
    {
        std::lock_guard lock(impl_->mutex);
        auto& state = impl_->state;
        state.encoder_total =
            std::clamp<int64_t>(int64_t(state.encoder_total) + detents, INT32_MIN, INT32_MAX);
        ++state.input_events;
        state.last_input = detents > 0 ? "旋钮 顺时针" : "旋钮 逆时针";
    }
    return impl_->Command_Submit(Impl::CommandType::Encoder, detents);
}

//=====电脑连接有租期；离线操作不缓存、超时不重发=====
Result App::Pc_Bridge_Update(uint32_t session)
{
    if (!session || session > INT32_MAX)
    {
        return Result::Fail(Error::Invalid, "Invalid PC session");
    }
    std::lock_guard lock(impl_->mutex);
    if (impl_->pc_session != session || !impl_->state.pc_connected)
    {
        impl_->state.pc_result = "电脑已连接";
        impl_->state.pc_action.clear();
        if (impl_->pc_session != session)
        {
            impl_->state.pc_sequence = 0;
        }
    }
    impl_->pc_session = session;
    impl_->pc_heartbeat_us = esp_timer_get_time();
    impl_->state.pc_connected = true;
    return {};
}

void App::Impl::Pc_Link_Check()
{
    std::lock_guard lock(mutex);
    if (state.pc_connected && esp_timer_get_time() - pc_heartbeat_us >= kPcLeaseUs)
    {
        state.pc_connected = false;
        state.pc_result = "电脑连接已断开";
    }
    else if (state.pc_result == "等待电脑确认" && esp_timer_get_time() - pc_sent_us >= 1500000)
    {
        state.pc_result = "电脑确认超时";
    }
}

void App::Impl::Pc_Action_Submit(const char* action, int amount)
{
    uint32_t session, sequence;
    {
        std::lock_guard lock(mutex);
        if (!state.pc_connected || esp_timer_get_time() - pc_heartbeat_us >= kPcLeaseUs)
        {
            state.pc_connected = false;
            state.pc_result = "电脑未连接";
            return;
        }
        session = pc_session;
        sequence = ++state.pc_sequence;
        pc_sent_us = esp_timer_get_time();
        state.pc_action = action;
        state.pc_result = "等待电脑确认";
    }
    // 单次 printf 防止和背景日志在一条协议行中交叉；字段均来自固定动作表。
    std::printf("MOSI_PC {\"v\":1,\"session\":%lu,\"seq\":%lu,\"action\":\"%s\",\"amount\":%d}\n",
                static_cast<unsigned long>(session), static_cast<unsigned long>(sequence), action, amount);
}

Result App::Pc_Action_Acknowledge(uint32_t session, uint32_t sequence, PcAckStatus status)
{
    std::lock_guard lock(impl_->mutex);
    if (!impl_->state.pc_connected || session != impl_->pc_session || !sequence ||
        sequence != impl_->state.pc_sequence || impl_->state.pc_action.empty() ||
        esp_timer_get_time() - impl_->pc_heartbeat_us >= kPcLeaseUs ||
        static_cast<unsigned>(status) > static_cast<unsigned>(PcAckStatus::Simulated))
    {
        return Result::Fail(Error::Invalid, "Stale PC acknowledgement");
    }
    switch (status)
    {
    case PcAckStatus::Failed:
        impl_->state.pc_result = "电脑执行失败";
        break;
    case PcAckStatus::Executed:
        impl_->state.pc_result = "电脑已执行";
        break;
    case PcAckStatus::Simulated:
        impl_->state.pc_result = "电脑模拟确认";
        break;
    }
    return {};
}
} // namespace mosicrokey
