#include "view.hpp"
#include <algorithm>
#include <cstdio>
LV_FONT_DECLARE(mosicrokey_font14);

namespace mosicrokey::ui
{
namespace
{
constexpr uint32_t kBackground = 0x0c121a, kText = 0xecf2fa, kMuted = 0x92a7bd;
constexpr uint32_t kAccent = 0x53d4c5;

lv_obj_t* Label_Create(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color)
{
    auto* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, &mosicrokey_font14, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

lv_obj_t* Panel_Create(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color)
{
    auto* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    return panel;
}

void Visible_Set(lv_obj_t* object, bool visible)
{
    if (visible)
    {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

std::string Menu_Title_Fit(std::string title)
{
    for (char& c : title)
    {
        if (c == '\n' || c == '\r' || c == '\t')
        {
            c = ' ';
        }
    }
    auto width = [](const std::string& text)
    {
        lv_point_t size{};
        lv_text_get_size(&size, text.c_str(), &mosicrokey_font14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
        return size.x;
    };
    if (width(title) <= 136)
    {
        return title;
    }
    do
    {
        size_t end = title.size() - 1;
        while (end && (static_cast<unsigned char>(title[end]) & 0xc0) == 0x80)
        {
            --end;
        }
        title.resize(end);
    } while (!title.empty() && width(title + "...") > 136);
    return title + "...";
}
} // namespace

View::View(lv_obj_t* screen) : root_(screen)
{
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    Panel_Create(root_, 0, 0, 160, 18, 0x182432);
    status_ = Label_Create(root_, 5, 2, 150, 14, kMuted);
    lv_obj_set_style_text_font(status_, &lv_font_montserrat_12, 0);
    title_ = Label_Create(root_, 5, 21, 150, 19, kText);
    viewport_ = Panel_Create(root_, 5, 43, 150, 61, kBackground);
    focus_ = Panel_Create(viewport_, 0, 0, 150, 20, 0x213c42);
    Visible_Set(focus_, false);
    content_ = Label_Create(viewport_, 4, 0, 142, 61, kText);
    footer_ = Label_Create(root_, 5, 109, 150, 18, kMuted);
    for (unsigned i = 0; i < tiles_.size(); ++i)
    {
        tiles_[i] = Panel_Create(root_, 5 + (i % 2) * 77, 41 + (i / 2) * 22, 73, 20, 0x182432);
        lv_obj_set_style_radius(tiles_[i], 3, 0);
        tile_labels_[i] = Label_Create(tiles_[i], 4, 1, 65, 18, kText);
        Visible_Set(tiles_[i], false);
    }
    for (unsigned i = 0; i < key_indicators_.size(); ++i)
    {
        key_indicators_[i] = Label_Create(viewport_, 3 + i * 16, 0, 15, 19, kMuted);
        lv_obj_set_style_text_font(key_indicators_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(key_indicators_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_opa(key_indicators_[i], LV_OPA_COVER, 0);
        Visible_Set(key_indicators_[i], false);
    }
    pixels_.resize(kWidth * kHeight);
}

void View::Menu_Draw(const Snapshot& state, const std::vector<std::string>& entries)
{
    int first = std::max(0, state.selection - 2);
    std::string text;
    for (int i = first; i < std::min(first + 3, static_cast<int>(entries.size())); ++i)
    {
        text += Menu_Title_Fit(entries[i]) + "\n";
    }
    lv_obj_set_y(focus_, (state.selection - first) * 20);
    Visible_Set(focus_, !entries.empty());
    lv_obj_set_style_text_line_space(content_, 2, 0);
    lv_label_set_text(content_, text.c_str());
}

void View::Tiles_Draw(const Snapshot& state)
{
    constexpr const char* home[] = {"小智语音", "AI 创作", "游戏库", "电脑控制", "设备设置", "诊断信息"};
    constexpr const char* media[] = {"1 音量+", "2 音量-", "3 上一首", "4 下一首", "5 播放", "6 静音"};
    constexpr const char* shortcuts[] = {"1 复制", "2 粘贴", "3 撤销", "4 重做", "5 保存", "6 Tab"};
    const char* const* names = state.mode == Mode::Home               ? home
                               : state.pc_profile == PcProfile::Media ? media
                                                                      : shortcuts;
    for (unsigned i = 0; i < tiles_.size(); ++i)
    {
        bool selected =
            state.mode == Mode::Home ? state.selection == static_cast<int>(i) : (state.key_mask & (1u << i));
        lv_obj_set_style_bg_color(tiles_[i], lv_color_hex(selected ? 0x25474c : 0x182432), 0);
        lv_obj_set_style_border_width(tiles_[i], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(tiles_[i], lv_color_hex(kAccent), 0);
        lv_label_set_text(tile_labels_[i], names[i]);
        Visible_Set(tiles_[i], true);
    }
}

void View::Display_Test_Draw()
{
    if (test_)
    {
        return;
    }
    test_ = Panel_Create(root_, 0, 0, 160, 128, 0x000000);
    lv_obj_set_style_border_width(test_, 1, 0);
    lv_obj_set_style_border_color(test_, lv_color_hex(0xffffff), 0);
    auto* header = Label_Create(test_, 5, 5, 150, 18, 0xffffff);
    lv_label_set_text(header, "TL   160x128    TR");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_12, 0);
    constexpr uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
    constexpr const char* names[] = {"R", "G", "B", "W", "K"};
    for (unsigned i = 0; i < 5; ++i)
    {
        auto* bar = Panel_Create(test_, 5 + i * 30, 29, 30, 57, colors[i]);
        auto* name = Label_Create(bar, 8, 20, 14, 18, i == 2 || i == 4 ? 0xffffff : 0x000000);
        lv_label_set_text(name, names[i]);
    }
    auto* text = Label_Create(test_, 5, 91, 150, 18, 0xffffff);
    lv_label_set_text(text, "红 绿 蓝 白 黑");
    auto* footer = Label_Create(test_, 5, 112, 150, 14, 0xffffff);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_12, 0);
    lv_label_set_text(footer, "BL  BACK=EXIT   BR");
}

void View::Update(const Snapshot& state)
{
    if (state.mode == Mode::Game)
    {
        if (!canvas_)
        {
            canvas_ = lv_canvas_create(root_);
            lv_obj_set_pos(canvas_, 0, 0);
            lv_canvas_set_buffer(canvas_, pixels_.data(), kWidth, kHeight, LV_COLOR_FORMAT_RGB565);
        }
        lv_obj_move_foreground(canvas_);
        mode_ = state.mode;
        return;
    }
    if (canvas_)
    {
        lv_obj_delete(canvas_);
        canvas_ = nullptr;
    }
    if (state.mode == Mode::DisplayTest)
    {
        Display_Test_Draw();
        lv_obj_move_foreground(test_);
        mode_ = state.mode;
        return;
    }
    if (test_)
    {
        lv_obj_delete(test_);
        test_ = nullptr;
    }
    mode_ = state.mode;
    for (auto* tile : tiles_)
    {
        Visible_Set(tile, false);
    }
    for (auto* key : key_indicators_)
    {
        Visible_Set(key, false);
    }
    bool tiles = state.mode == Mode::Home || state.mode == Mode::Computer;
    Visible_Set(viewport_, !tiles);
    Visible_Set(focus_, false);
    std::string status = std::string(state.wifi ? "W+" : "W-") + (state.storage ? " TF+" : " TF-") +
                         (state.agent_busy ? " AI*" : " AI.") + (state.pc_connected ? " PC+" : " PC-");
    lv_label_set_text(status_, status.c_str());
    const char* title = "MosiCrokey";
    uint32_t accent = kText;
    std::string text, footer = "旋钮选择  确认进入";
    bool menu = false;
    lv_obj_set_style_text_line_space(content_, 0, 0);
    lv_obj_set_pos(content_, 4, 0);
    lv_obj_set_height(content_, LV_SIZE_CONTENT);
    lv_label_set_long_mode(content_, LV_LABEL_LONG_WRAP);
    switch (state.mode)
    {
    case Mode::Home:
        Tiles_Draw(state);
        break;
    case Mode::Xiaozhi:
        title = "小智语音";
        accent = kAccent;
        text =
            state.voice_status + "\n" + (state.voice_text.empty() ? "确认开始或中断对话" : state.voice_text);
        footer = "旋钮滚动  确认对话";
        break;
    case Mode::Agent:
        title = "ESPClaw 创作";
        accent = 0xffbd66;
        text = state.agent_text.empty() ? "串口 gameask 创作游戏\n请求可在后台执行" : state.agent_text;
        footer = state.agent_busy ? "确认取消  返回首页" : "旋钮滚动  返回首页";
        break;
    case Mode::Library:
        title = "游戏库";
        accent = 0x8ad996;
        if (!state.notice.empty())
        {
            text = state.notice;
        }
        else if (state.games.empty())
        {
            text = "暂无游戏\n设置内试玩贪吃蛇";
        }
        else
        {
            std::vector<std::string> names;
            for (const auto& game : state.games)
            {
                names.push_back(game.title);
            }
            Menu_Draw(state, names);
            menu = true;
        }
        footer = state.notice.empty() ? "确认游玩  返回首页" : "确认关闭错误提示";
        break;
    case Mode::Settings:
        title = "设备设置";
        Menu_Draw(state, {"小智 WiFi 配网", "重新挂载 TF", "按键与旋钮检测", "屏幕颜色与边界", "试玩贪吃蛇"});
        menu = true;
        footer = "旋钮选择  确认执行";
        break;
    case Mode::Diagnostics:
        title = "诊断信息";
        text = "ESP32-S31\nSPI TFT 160x128\n版本与日志: status";
        footer = "返回首页";
        break;
    case Mode::Computer:
        title = state.pc_profile == PcProfile::Media ? "电脑 / 媒体" : "电脑 / 快捷键";
        accent = 0x8fabff;
        Tiles_Draw(state);
        footer = state.pc_result == "等待电脑确认" ? state.pc_result : "K7换组  K8回设备";
        if (!state.pc_connected)
        {
            footer = "等待电脑连接程序";
        }
        if (state.pc_result == "电脑执行失败" || state.pc_result == "电脑确认超时" ||
            state.pc_result == "电脑模拟确认" || state.pc_result == "电脑已执行" ||
            state.pc_result == "电脑操作已过期" || state.pc_result == "模式切换后已丢弃旧输入")
        {
            footer = state.pc_result;
        }
        break;
    case Mode::InputTest:
    {
        title = "按键与旋钮检测";
        accent = kAccent;
        for (unsigned i = 0; i < key_indicators_.size(); ++i)
        {
            bool pressed = state.key_mask & (1u << i);
            lv_label_set_text(key_indicators_[i], i == 8 ? "E" : std::to_string(i + 1).c_str());
            lv_obj_set_style_bg_color(key_indicators_[i], lv_color_hex(pressed ? kAccent : 0x182432), 0);
            lv_obj_set_style_text_color(key_indicators_[i], lv_color_hex(pressed ? kBackground : kText), 0);
            Visible_Set(key_indicators_[i], true);
        }
        lv_obj_set_y(content_, 23);
        text = "旋钮:" + std::to_string(state.encoder_total) + "  次:" + std::to_string(state.input_events) +
               "\n" + state.last_input;
        footer = "返回退出  旋钮计数";
        break;
    }
    case Mode::Game:
    case Mode::DisplayTest:
        break;
    }
    lv_label_set_text(title_, title);
    lv_obj_set_style_text_color(title_, lv_color_hex(accent), 0);
    if (!menu && !tiles)
    {
        lv_label_set_text(content_, text.c_str());
    }
    if (state.mode == Mode::Xiaozhi || state.mode == Mode::Agent)
    {
        lv_obj_update_layout(content_);
        int scroll =
            std::min(state.selection * 18, std::max(0, static_cast<int>(lv_obj_get_height(content_)) - 61));
        lv_obj_set_y(content_, -scroll);
    }
    if (!state.notice.empty() && state.mode != Mode::Library)
    {
        footer = state.notice;
    }
    lv_label_set_text(footer_, footer.c_str());
}

void View::Frame(const std::vector<uint16_t>& frame)
{
    if (frame.size() != pixels_.size())
    {
        return;
    }
    std::copy(frame.begin(), frame.end(), pixels_.begin());
    if (canvas_)
    {
        lv_obj_invalidate(canvas_);
    }
}
} // namespace mosicrokey::ui
