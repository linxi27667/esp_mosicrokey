#include "view.hpp"
#include "game_internal.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <cstring>
using namespace mosicrokey;
std::array<uint16_t,kWidth*kHeight> pixels{};
void Flush(lv_display_t* display,const lv_area_t* area,uint8_t* bytes) {
    auto* data=reinterpret_cast<uint16_t*>(bytes);
    for(int y=area->y1;y<=area->y2;++y) for(int x=area->x1;x<=area->x2;++x) pixels[y*kWidth+x]=*data++;
    lv_display_flush_ready(display);
}
void Save(lv_display_t* display,const std::filesystem::path& path) {
    lv_refr_now(display); std::ofstream stream(path,std::ios::binary); stream<<"P6\n160 128\n255\n";
    for(auto color:pixels) {uint8_t rgb[]={static_cast<uint8_t>(((color>>11)&31)*255/31),static_cast<uint8_t>(((color>>5)&63)*255/63),static_cast<uint8_t>((color&31)*255/31)}; stream.write(reinterpret_cast<char*>(rgb),3);}
}
int main(int argc,char** argv) {
    if(argc!=3) return 1; std::filesystem::path root(argv[1]); std::filesystem::create_directories(root);
    lv_init(); auto* display=lv_display_create(kWidth,kHeight); lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    static uint16_t buffer[kWidth*20]; lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,Flush); ui::View view(lv_screen_active());
    Snapshot state; state.wifi=true; state.storage=true;
    state.voice_status="正在聆听"; state.voice_text="你好，我是小智。按确认开始语音对话，方向键可以滚动长文本。中文字体覆盖基本汉字和常用标点。";
    state.agent_busy=true; state.agent_text="game_validate: 完成\n正在安装新版贪吃蛇\n返回首页后继续后台创作";
    state.games={{"snake","贪吃蛇","r00000001",""},{"maze","迷宫探险","r00000001",""},{"pong","弹球挑战","r00000002",""},{"blocks","方块堆叠","r00000001",""}};
    constexpr const char* names[]={"home","voice","agent","library","game","settings","diagnostics"};
    for(unsigned i=0;i<7;++i) {
        if(i==4) continue;
        state.mode=static_cast<Mode>(i); state.selection=i==3 ? 2 : 0; view.Update(state); Save(display,root/(std::string(names[i])+".ppm"));
    }
    state.mode=Mode::Library; state.selection=0;
    state.games[0].title="这是一个标题很长的中文游戏名称";
    view.Update(state); Save(display,root/"library-long-title.ppm");
    state.games.clear(); view.Update(state); Save(display,root/"library-empty.ppm");
    state.storage=false; state.notice="TF 不可用，请检查接线"; view.Update(state); Save(display,root/"storage-error.ppm");
    state.mode=Mode::Xiaozhi; state.selection=2; state.notice.clear(); view.Update(state); Save(display,root/"voice-scroll.ppm");
    std::ifstream source(argv[2]); std::string lua{std::istreambuf_iterator<char>(source),{}};
    GameVm game; auto result=game.Load(lua,{}); if(!result) {std::cerr<<result.message; return 1;}
    std::vector<uint16_t> frame;
    for(int i=0;i<12;++i) {result=game.Tick(1.f/30,1u<<static_cast<unsigned>(Key::Down),frame); if(!result) return 1;}
    state.mode=Mode::Agent; state.selection=2047;
    state.agent_text=std::string(4080,'a')+"\nEND OF LONG REPLY";
    view.Update(state); Save(display,root/"agent-scroll-end.ppm");
    state.selection=0; state.agent_busy=false; state.agent_text.clear();
    view.Update(state); Save(display,root/"agent-empty.ppm");
    state.mode=Mode::Game; view.Update(state); view.Frame(frame); Save(display,root/"snake.ppm");
    std::cout<<"Captured 13 shared LVGL 9.5 UI/game states, 160x128\n";
}
