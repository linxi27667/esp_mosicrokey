#include "game_internal.hpp"
#include <fstream>
#include <iostream>
using namespace mosicrokey;
unsigned checks=0,failures=0;
void Check(bool value,const char* name) {
    ++checks; if(!value) {++failures; std::cout<<"FAIL "<<name<<"\n";}
}
bool Pixel(const std::vector<uint16_t>& frame,int x,int y,uint16_t color) {
    return frame.size()==kWidth*kHeight && frame[y*kWidth+x]==color;
}
bool Text(const std::vector<uint16_t>& frame,int x,int y,const char* text) {
    if(frame.size()!=kWidth*kHeight) return false;
    std::vector<uint16_t> reference(kWidth*kHeight); RasterText(reference,x,y,text,0xffff);
    for(size_t i=0;i<reference.size();++i) if(reference[i] && frame[i]!=reference[i]) return false;
    return true;
}
int main(int argc,char** argv) {
    if(argc!=2 && !(argc==3 && std::string(argv[2])=="--extended")) return 2;
    std::ifstream file(argv[1],std::ios::binary);
    std::string source{std::istreambuf_iterator<char>(file),{}};
    GameVm vm; std::vector<uint16_t> frame;
    Check(bool(vm.Load(source,{})),"source loads in actual sandbox");
    Check(bool(vm.Tick(0,0,frame)),"initial draw");
    Check(Pixel(frame,65,69,0xffff),"initial head at (8,6), separate from food");
    Check(Pixel(frame,73,69,0xfca0),"initial food at (9,6)");
    Check(Text(frame,4,12,"SCORE 0"),"initial score zero");
    auto step=[&](uint32_t input,unsigned count=1) {
        bool result=true;
        for(unsigned i=0;i<count;++i) for(float dt:{0.1f,0.1f,0.05f})
            if(!vm.Tick(dt,input,frame)) result=false;
        return result;
    };
    Check(step(0),"first movement");
    Check(Pixel(frame,73,69,0xffff),"first food eaten; new head not covered by old food");
    Check(Text(frame,4,12,"SCORE 1"),"first meal increases score");
    Check(Pixel(frame,137,29,0xfca0),"next food uses deterministic game.random and avoids snake");
    Check(step(1u<<static_cast<unsigned>(Key::Up),5) && Pixel(frame,73,29,0xffff),"up input moves five cells");
    Check(step(1u<<static_cast<unsigned>(Key::Right),8) && Pixel(frame,137,29,0xffff),"right input reaches second food");
    Check(Text(frame,4,12,"SCORE 2"),"second meal increases score");
    Check(step(1u<<static_cast<unsigned>(Key::Down)) && step(1u<<static_cast<unsigned>(Key::Left)) && step(1u<<static_cast<unsigned>(Key::Up)),"self-collision replay executes");
    Check(Text(frame,52,53,"GAME OVER"),"self-collision produces failure feedback");
    Check(bool(vm.Tick(.01f,1u<<static_cast<unsigned>(Key::Confirm),frame)) && Pixel(frame,65,69,0xffff),"confirm restarts after failure");
    Check(Text(frame,4,12,"SCORE 0"),"restart resets score");
    GameVm wrap; Check(bool(wrap.Load(source,{})),"wrap scenario loads");
    bool wrapped=true;
    for(unsigned i=0;i<25;++i) for(float dt:{.1f,.1f,.05f}) if(!wrap.Tick(dt,0,frame)) wrapped=false;
    Check(wrapped && Pixel(frame,105,69,0xffff),"wrap allows 25 steps without wall failure");
    GameVm reverse; Check(bool(reverse.Load(source,{})),"reverse scenario loads");
    for(float dt:{.1f,.1f,.05f}) reverse.Tick(dt,0,frame);
    for(float dt:{.1f,.1f,.05f}) reverse.Tick(dt,1u<<static_cast<unsigned>(Key::Left),frame);
    Check(Pixel(frame,81,69,0xffff),"180 degree reverse is ignored");
    if(argc==3) {
        // Test fixtures appended to this candidate's lexical scope. The exported
        // production source is unchanged; no privileged Lua API is introduced.
        const std::string win_fixture=R"(
local probe_init, probe_first = init, true
function init()
    probe_init()
    if not probe_first then return end
    probe_first = false
    snake={{x=8,y=6}}
    for y=0,12 do for x=0,19 do
        if not (x==8 and y==6) and not (x==9 and y==6) then
            table.insert(snake,{x=x,y=y})
        end
    end end
    dx,dy,food,elapsed,score,dead,won,confirm_before=1,0,{x=9,y=6},0,256,false,false,false
end
)";
        GameVm winner;
        Check(bool(winner.Load(source+win_fixture,{})),"259-cell win fixture initializes within sandbox limits");
        bool completed=true;
        for(float dt:{.1f,.1f,.05f}) if(!winner.Tick(dt,0,frame)) completed=false;
        Check(completed,"full grid finishes without looping or exhausting its budget");
        Check(Text(frame,52,53,"YOU WIN"),"full grid shows victory feedback");
        Check(bool(winner.Tick(.1f,0,frame)) && Text(frame,52,53,"YOU WIN"),"victory remains stable without input");
        Check(bool(winner.Tick(.01f,1u<<static_cast<unsigned>(Key::Confirm),frame)) &&
              Text(frame,4,12,"SCORE 0") && Pixel(frame,65,69,0xffff),"victory confirm restarts normal game");
        Check(Pixel(frame,57,69,0x07e0),"V3 body is green after restart");
        GameVm rapid;
        Check(bool(rapid.Load(source,{})),"rapid-turn scenario loads");
        completed=bool(rapid.Tick(.01f,1u<<static_cast<unsigned>(Key::Up),frame)) &&
                  bool(rapid.Tick(.01f,1u<<static_cast<unsigned>(Key::Left),frame));
        for(float dt:{.1f,.1f,.03f}) if(!rapid.Tick(dt,0,frame)) completed=false;
        Check(completed && Pixel(frame,65,61,0xffff),"two turns before one movement cannot reverse into the neck");
        const std::string crowd_fixture=R"(
local probe_init = init
function init()
    probe_init()
    snake={{x=8,y=6}}
    for y=0,12 do for x=0,19 do
        if not (x==8 and y==6) and not (x==9 and y==6) and not (x==8 and y==0) then
            table.insert(snake,{x=x,y=y})
        end
    end end
    dx,dy,food,elapsed,score,dead,won,confirm_before=1,0,{x=9,y=6},0,255,false,false,false
end
)";
        GameVm crowded;
        Check(bool(crowded.Load(source+crowd_fixture,{})),"258-cell food-search fixture initializes");
        completed=true;
        for(float dt:{.1f,.1f,.05f}) if(!crowded.Tick(dt,0,frame)) completed=false;
        Check(completed,"crowded food search respects instruction and time limits");
        Check(Pixel(frame,65,21,0xfca0),"crowded search places food in the sole free cell");
        Check(Text(frame,4,12,"SCORE 256"),"crowded search preserves scoring");
    }
    std::cout<<(failures ? "FAIL " : "PASS ")<<checks<<" Snake behavior checks; failures="<<failures<<"\n";
    return failures ? 1 : 0;
}
