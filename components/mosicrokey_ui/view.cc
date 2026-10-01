#include "view.hpp"
#include <algorithm>
LV_FONT_DECLARE(mosicrokey_font14);
namespace mosicrokey::ui {
namespace {
lv_obj_t* Label(lv_obj_t* parent,int x,int y,int width,int height,uint32_t color) {
    auto* label=lv_label_create(parent); lv_obj_set_pos(label,x,y); lv_obj_set_size(label,width,height);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    lv_obj_set_style_text_font(label,&mosicrokey_font14,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    return label;
}
std::string MenuTitle(std::string title) {
    for(char& c:title) if(c=='\n' || c=='\r' || c=='\t') c=' ';
    auto width=[](const std::string& text) {
        lv_point_t size{};
        lv_text_get_size(&size,text.c_str(),&mosicrokey_font14,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
        return size.x;
    };
    if(width(title)<=130) return title;
    do {
        size_t end=title.size()-1;
        while(end && (static_cast<unsigned char>(title[end])&0xc0)==0x80) --end;
        title.resize(end);
    } while(!title.empty() && width(title+"...")>130);
    return title+"...";
}
std::string Lines(const Snapshot& state,const std::vector<std::string>& entries) {
    std::string text; int first=std::max(0,state.selection-2);
    for(int i=first;i<std::min(first+3,static_cast<int>(entries.size()));++i)
        text+=(i==state.selection ? "> " : "  ")+MenuTitle(entries[i])+"\n";
    return text;
}
}
View::View(lv_obj_t* screen):root_(screen) {
    lv_obj_set_style_bg_color(root_,lv_color_hex(0x101820),0);
    lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0); lv_obj_set_style_pad_all(root_,0,0);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
    status_=Label(root_,5,1,150,18,0x8ea6ba);
    title_=Label(root_,5,22,150,20,0xf1f5fa);
    auto* viewport=lv_obj_create(root_); lv_obj_remove_style_all(viewport);
    lv_obj_set_pos(viewport,5,45); lv_obj_set_size(viewport,150,54);
    lv_obj_remove_flag(viewport,LV_OBJ_FLAG_SCROLLABLE);
    content_=Label(viewport,0,0,150,59,0xf1f5fa);
    footer_=Label(root_,5,108,150,18,0x8ea6ba);
    pixels_.resize(kWidth*kHeight);
}
void View::Update(const Snapshot& state) {
    if(state.mode==Mode::Game) {
        if(!canvas_) {
            canvas_=lv_canvas_create(root_); lv_obj_set_pos(canvas_,0,0);
            lv_canvas_set_buffer(canvas_,pixels_.data(),kWidth,kHeight,LV_COLOR_FORMAT_RGB565);
        }
        mode_=state.mode; return;
    }
    if(canvas_) {lv_obj_delete(canvas_); canvas_=nullptr;}
    mode_=state.mode;
    std::string status=std::string(state.wifi ? "W+" : "W-")+" "+(state.storage ? "TF+" : "TF-")+" "+
        (state.mode==Mode::Xiaozhi ? "X+" : "X-")+(state.agent_busy ? " A*" : " A.");
    lv_label_set_text(status_,status.c_str());
    const char* name="MosiCrokey"; uint32_t accent=0xf1f5fa;
    std::string content,footer="确认进入  返回首页";
    switch(state.mode) {
        case Mode::Home:
            content=Lines(state,{"小智语音","ESPClaw 创作","TF 游戏库","设备设置","诊断信息"}); break;
        case Mode::Xiaozhi:
            name="小智语音"; accent=0x50d5ca;
            content=state.voice_status+"\n"+(state.voice_text.empty() ? "确认开始或中断对话" : state.voice_text);
            footer="上下滚动  确认对话"; break;
        case Mode::Agent:
            name="ESPClaw 创作"; accent=0xffbd66;
            content=state.agent_text.empty() ? "gameask 创作需求\n确认取消当前请求" : state.agent_text;
            footer=state.agent_busy ? "后台执行  确认取消" : "上下滚动  返回首页"; break;
        case Mode::Library: {
            name="TF 游戏库"; accent=0x8ad996;
            if(!state.notice.empty()) content=state.notice;
            else if(state.games.empty()) content=state.storage ? "暂无游戏\n串口输入 demo 安装样例" : "TF 不可用\n设置中重新挂载";
            else {std::vector<std::string> games; for(const auto& game:state.games) games.push_back(game.title); content=Lines(state,games);}
            footer=state.notice.empty() ? "上下选择  确认游玩" : "确认或返回关闭提示"; break;
        }
        case Mode::Settings:
            name="设备设置";
            content=Lines(state,{"小智 WiFi 配网","重新挂载 TF"})+"模型配置请使用串口"; break;
        case Mode::Diagnostics:
            name="诊断信息";
            content="ESP32-S31\n160x128 / Lua 5.5\n版本与状态: status"; break;
        case Mode::Game: break;
    }
    if(state.mode==Mode::Settings) footer="确认执行  返回首页";
    if(state.mode==Mode::Diagnostics) footer="返回首页";
    lv_label_set_text(title_,name); lv_obj_set_style_text_color(title_,lv_color_hex(accent),0);
    // Move the shared label inside a clipped viewport for deterministic long-text scrolling.
    int scroll=(state.mode==Mode::Xiaozhi || state.mode==Mode::Agent) ? state.selection*18 : 0;
    lv_label_set_long_mode(content_,LV_LABEL_LONG_WRAP);
    lv_obj_set_height(content_,LV_SIZE_CONTENT);
    lv_label_set_text(content_,content.c_str());
    lv_obj_update_layout(content_);
    scroll=std::min(scroll,std::max(0,static_cast<int>(lv_obj_get_height(content_))-54));
    lv_obj_set_y(content_,-scroll);
    lv_label_set_text(footer_,state.notice.empty() || state.mode==Mode::Library ? footer.c_str() : state.notice.c_str());
}
void View::Frame(const std::vector<uint16_t>& frame) {
    if(frame.size()!=pixels_.size()) return;
    std::copy(frame.begin(),frame.end(),pixels_.begin());
    if(canvas_) lv_obj_invalidate(canvas_);
}
} // namespace mosicrokey::ui
