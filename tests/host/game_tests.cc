#include "game_internal.hpp"
#include "mosicrokey/focus.hpp"
#include <cJSON.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace mosicrokey;
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool valid,const char* name) {++checks; if(!valid) {std::cerr<<"FAIL: "<<name<<"\n"; std::exit(1);}}
struct Events:IEvents {
    void Voice(const std::string&,const std::string&) override {}
    void Agent(uint32_t,bool,const std::string&) override {}
    void Notice(const std::string&) override {}
    void GameEnded(const std::string&) override {}
};
std::string Read(const std::string& path) {std::ifstream file(path,std::ios::binary); return {std::istreambuf_iterator<char>(file),{}};}
std::string JsonWrite(const std::string& job,const std::string& source,const char* path="main.lua") {
    auto* json=cJSON_CreateObject(); cJSON_AddStringToObject(json,"job",job.c_str()); cJSON_AddStringToObject(json,"path",path); cJSON_AddStringToObject(json,"content",source.c_str());
    char* output=cJSON_PrintUnformatted(json); std::string result=output; cJSON_free(output); cJSON_Delete(json); return result;
}
bool Tool_Passed(GameService& service,const std::string& name,const std::string& arguments,std::string& output) {
    auto result=service.Tool(name,arguments,output);
    if(!result) std::cerr<<name<<" cause: "<<result.message<<"\n";
    return bool(result);
}
std::string Begin(GameService& library,const char* id="test") {
    std::string output; Check(Tool_Passed(library,"game_begin",std::string("{\"id\":\"")+id+"\",\"title\":\"Test\"}",output),"begin");
    auto* json=cJSON_Parse(output.c_str()); std::string job=cJSON_GetObjectItem(json,"job")->valuestring; cJSON_Delete(json); return job;
}
std::string JsonPreview(const std::string& source) {
    auto* json=cJSON_CreateObject(); cJSON_AddStringToObject(json,"id","snake");
    cJSON_AddStringToObject(json,"title","Snake USB"); cJSON_AddStringToObject(json,"source",source.c_str());
    char* encoded=cJSON_PrintUnformatted(json); std::string result=encoded; cJSON_free(encoded); cJSON_Delete(json); return result;
}
const std::string good="function init() end function update(dt,i) end function draw() game.clear(1234) end";
int main(int argc,char** argv) {
    Check(argc==3,"arguments"); fs::path root=fs::absolute(argv[1]);
    // Delete only the fixed test-card child of this executable's build directory.
    Check(root.filename()=="test-card","test workspace boundary"); fs::remove_all(root); fs::create_directories(root);
    Events events; GameService library(root.generic_string(),events); std::string output;
    for(int from=0;from<7;++from) for(int to=0;to<7;++to) {
        auto action=ChangeFocus(static_cast<Mode>(from),static_cast<Mode>(to));
        Check(action.release_audio==(from==1 && to!=1),"audio release");
        Check(action.acquire_audio==(from!=1 && to==1),"audio acquisition");
        Check(action.stop_game==(from==4 && to!=4),"game release");
    }
    Check(BackDestination(Mode::Game)==Mode::Library,"game back destination");
    Check(!library.Tool("game_begin","{\"id\":\"../escape\",\"title\":\"Bad\"}",output),"id traversal");
    auto job=Begin(library); auto request="{\"job\":\""+job+"\"}";
    Check(!library.Tool("game_write_file",JsonWrite(job,good,"../main.lua"),output),"path traversal");
    Check(!library.Tool("game_write_file",JsonWrite(job,good,"assets/../../x.rgb565"),output),"nested traversal");
    Check(!library.Tool("game_commit",request,output),"commit without validation");
    Check(!library.Tool("game_write_file",JsonWrite(job,std::string(65537,'x')),output),"source limit");
    Check(Tool_Passed(library,"game_write_file",JsonWrite(job,good),output),"source write");
    Check(Tool_Passed(library,"game_validate",request,output),"probe valid");
    Check(Tool_Passed(library,"game_commit",request,output),"commit valid");
    std::vector<GameInfo> games; Check(bool(library.List(games)) && games.size()==1,"installed enumeration");
    auto old=games[0]; Check(bool(library.Start(old)),"runtime start"); std::vector<uint16_t> frame;
    Check(Tool_Passed(library,"game_list","{\"id\":\"test\",\"offset\":0}",output) && output.find("game.clear(1234)")!=std::string::npos,"agent reads installed source");
    Check(!library.Tool("game_list","{\"id\":\"test\",\"offset\":-1}",output),"negative source offset");
    Check(!library.Tool("game_list","{\"id\":\"test\",\"offset\":99999}",output),"source offset bounds");
    Check(bool(library.Tick(1.f/30,frame)) && frame.size()==160*128 && frame[0]==1234,"render pixels");
    auto replacement=Begin(library); auto replace="{\"job\":\""+replacement+"\"}";
    Check(Tool_Passed(library,"game_write_file",JsonWrite(replacement,"function init() end function update(dt,i) end function draw() game.clear(5678) end"),output),"replacement write");
    auto replacement_valid=library.Tool("game_validate",replace,output);
    auto replacement_commit=replacement_valid ? library.Tool("game_commit",replace,output) : replacement_valid;
    if(!replacement_commit) std::cerr<<"replacement cause: "<<replacement_commit.message<<"\n";
    Check(bool(replacement_commit),"replacement commit");
    Check(bool(library.Tick(1.f/30,frame)) && frame[0]==1234,"running revision immutable");
    library.List(games); Check(games[0].revision!=old.revision,"latest revision selected");
    library.Stop(); Check(bool(library.Start(games[0])) && bool(library.Tick(1.f/30,frame)) && frame[0]==5678,"new launch new revision"); library.Stop();
    {GameService reboot(root.generic_string(),events); std::vector<GameInfo> recovered; Check(bool(reboot.List(recovered)) && recovered[0].revision==games[0].revision,"reboot recovery");}
    WriteFile(games[0].path+"/main.lua","corrupt"); library.List(games); Check(games.size()==1 && games[0].revision==old.revision,"corrupt revision fallback");
    fs::remove(old.path+"/READY"); library.List(games); Check(games.empty(),"missing commit ignored");
    Check(!library.Start(old),"incomplete launch denied");
    auto escape=old; escape.revision="../.staging"; escape.path=root.generic_string()+"/games/test/../.staging";
    Check(!library.Start(escape),"launch traversal denied");
    for(const auto& source:std::vector<std::string>{
        "function init() while true do end end function update() end function draw() end",
        "function init() local a=string.pack('c300000','x') end function update() end function draw() end",
        "function init() end function update() while true do end end function draw() end",
        "function init() end function update() end function draw() for i=1,1025 do game.clear(0) end end",
        "function init() end function update() end function draw() error('fault') end"}) {
        GameVm vm; auto start=std::chrono::steady_clock::now(); auto status=vm.Load(source,{});
        if(status) status=vm.Tick(1.f/30,0,frame);
        Check(!status,"sandbox rejects runaway or memory failure");
        Check(std::chrono::steady_clock::now()-start<std::chrono::seconds(1),"sandbox bounded wall time");
    }
    GameVm locked;
    Check(bool(locked.Load("function init() assert(io==nil and os==nil and package==nil and debug==nil and load==nil and pcall==nil and warn==nil and getmetatable==nil and setmetatable==nil and string.match==nil and string.rep==nil and table.move==nil and table.sort==nil) end function update() end function draw() end",{})),"restricted libraries, immutable native metatables and native-loop budget bypasses");
    GameVm binary; Check(!binary.Load(std::string(1,'\x1b')+"Lua",{}),"bytecode rejected");
    auto image_job=Begin(library,"art"); auto image_request="{\"job\":\""+image_job+"\"}";
    auto sprite_source="function init() end function update() end function draw() game.clear(0) game.image('assets/dot.rgb565',1,2) end";
    Check(Tool_Passed(library,"game_write_file",JsonWrite(image_job,sprite_source),output),"image source write");
    Check(!library.Tool("game_write_file","{\"job\":\""+image_job+"\",\"path\":\"assets/dot.rgb565\",\"encoding\":\"base64\",\"content\":\"@@@@\"}",output),"base64 rejection");
    Check(Tool_Passed(library,"game_write_file","{\"job\":\""+image_job+"\",\"path\":\"assets/dot.rgb565\",\"encoding\":\"base64\",\"content\":\"AQABAP//\"}",output),"RGB565 asset write");
    auto image_valid=library.Tool("game_validate",image_request,output);
    auto image_commit=image_valid ? library.Tool("game_commit",image_request,output) : image_valid;
    if(!image_commit) std::cerr<<"image package cause: "<<image_commit.message<<"\n";
    Check(bool(image_commit),"image package commit");
    library.List(games); auto art=*std::find_if(games.begin(),games.end(),[](const auto& g){return g.id=="art";});
    Check(bool(library.Start(art)) && bool(library.Tick(1.f/30,frame)) && frame[2*160+1]==65535,"RGB565 image drawing"); library.Stop();
    auto art_next=Begin(library,"art");
    Check(Tool_Passed(library,"game_write_file",JsonWrite(art_next,sprite_source),output),"art revision source");
    Check(Tool_Passed(library,"game_validate","{\"job\":\""+art_next+"\"}",output),"artwork carried into revision");
    library.DiscardStaging();
    auto large_job=Begin(library,"paged"); std::string large="--"+std::string(5000,'x')+"\n"+good;
    auto large_request="{\"job\":\""+large_job+"\"}";
    auto long_written=library.Tool("game_write_file",JsonWrite(large_job,large),output);
    auto long_valid=long_written ? library.Tool("game_validate",large_request,output) : long_written;
    auto long_commit=long_valid ? library.Tool("game_commit",large_request,output) : long_valid;
    if(!long_commit) std::cerr<<"long source cause: "<<long_commit.message<<"\n";
    Check(bool(long_commit),"long source install");
    Check(Tool_Passed(library,"game_list","{\"id\":\"paged\",\"offset\":4096}",output) && output.find("game.clear(1234)")!=std::string::npos,"source pagination");
    GameVm late_heap;
    Check(bool(late_heap.Load("function init() end function update() local t={} for i=1,5000 do t[i]='"+std::string(256,'x')+"'..i end end function draw() end",{})),"heap probe load");
    auto oom=late_heap.Tick(1.f/30,0,frame);
    Check(!oom && oom.message.find("memory")!=std::string::npos,"frame OOM is protected");
    auto snake=Read(argv[2]); Check(bool(library.ImportExample(snake)),"snake install"); library.List(games);
    auto snake_info=*std::find_if(games.begin(),games.end(),[](const auto& game){return game.id=="snake";});
    for(unsigned i=0;i<100;++i) {
        Check(bool(library.Start(snake_info)),"repeat load"); library.Input(Key::Down,true);
        Check(bool(library.Tick(1.f/30,frame)),"repeat frame"); library.Stop(); Check(!library.Running(),"repeat release");
    }
    GameVm replay1,replay2; Check(bool(replay1.Load(snake,{})) && bool(replay2.Load(snake,{})),"replay setup");
    for(unsigned i=0;i<100;++i) {std::vector<uint16_t> a,b; auto input=1u<<(i/20%4); Check(bool(replay1.Tick(1.f/30,input,a)) && bool(replay2.Tick(1.f/30,input,b)) && a==b,"deterministic replay");}
    GameService absent((root/"missing-card").generic_string(),events); Check(!absent.ImportExample(snake),"missing card");
    Check(Tool_Passed(absent,"game_preview",JsonPreview(good),output) && output.find("\"persistent\":false")!=std::string::npos,"USB preview reports temporary storage");
    std::vector<GameInfo> previews;
    Check(bool(absent.List(previews)) && previews.size()==1 && previews[0].temporary,"preview discoverable without TF");
    auto preview=previews[0];
    Check(bool(absent.Start(preview)) && bool(absent.Tick(1.f/30,frame)) && frame[0]==1234,"preview runs actual sandbox");
    std::vector<uint16_t> frozen;
    Check(bool(absent.CaptureFrame(frozen)) && frozen==frame,"device frame capture freezes actual canvas");
    Check(Tool_Passed(absent,"game_list","{\"id\":\"preview-snake\"}",output) && output.find("game.clear(1234)")!=std::string::npos,"Agent reads preview for modification");
    Check(!absent.Tool("game_preview",JsonPreview("function init() while true do end end function update() end function draw() end"),output),"preview probe rejects runaway");
    Check(!absent.Tool("game_preview",JsonPreview(std::string(65537,'x')),output),"preview source limit");
    Check(!absent.Tool("game_preview",JsonPreview("function init() end function update() end function draw() error('fault') end"),output),"preview frame probe rejects fault");
    absent.List(previews); Check(previews[0].revision==preview.revision,"invalid preview leaves published revision intact");
    Check(Tool_Passed(absent,"game_preview",JsonPreview("function init() end function update() end function draw() game.clear(5678) end"),output),"Agent modifies preview");
    Check(bool(absent.Tick(1.f/30,frame)) && frame[0]==1234,"running preview remains immutable during replacement");
    absent.Stop(); Check(!absent.Start(preview),"stale preview rejected on launch");
    absent.List(previews); Check(bool(absent.Start(previews[0])) && bool(absent.Tick(1.f/30,frame)) && frame[0]==5678,"preview relaunch selects new version");
    auto forged=previews[0]; forged.temporary=false;
    Check(!absent.Start(forged),"RAM path cannot bypass TF package validation");
    Check(!absent.Tool("game_begin","{\"id\":\"snake\",\"title\":\"Snake\"}",output),"preview does not fabricate TF installation");
    absent.Stop(); Check(!absent.CaptureFrame(frozen),"capture after exit rejected");
    {GameService power_cycle((root/"missing-card").generic_string(),events); Check(!power_cycle.List(previews) && previews.empty(),"USB preview explicitly lost on reboot");}
    auto interrupted=Begin(library,"pending"); library.Tool("game_write_file",JsonWrite(interrupted,good),output);
    GameService reboot(root.generic_string(),events); reboot.List(games);
    Check(std::none_of(games.begin(),games.end(),[](const auto& game){return game.id=="pending";}),"unfinished staging ignored");
    library.DiscardStaging();
    Check(bool(library.Start(snake_info)),"concurrent game start");
    std::atomic<bool> finished{false}; std::atomic<unsigned> rendered{0};
    std::thread renderer([&]{std::vector<uint16_t> image; while(!finished) {if(library.Tick(1.f/30,image)) ++rendered; std::this_thread::yield();}});
    for(int i=0;i<5;++i) {
        auto installed=library.ImportExample(snake);
        if(!installed) std::cerr<<"background install cause: "<<installed.message<<"\n";
        Check(bool(installed),"background revision installation");
    }
    finished=true; renderer.join(); Check(rendered>0,"game continues during installation"); library.Stop();
#ifdef _WIN32
    auto locked_job=Begin(library,"windows-lock");
    auto locked_request="{\"job\":\""+locked_job+"\"}";
    Check(Tool_Passed(library,"game_write_file",JsonWrite(locked_job,good),output) &&
          Tool_Passed(library,"game_validate",locked_request,output),"prepare real Windows lock probe");
    auto locked_path=(root/"games"/".staging"/locked_job).generic_string();
    HANDLE held=CreateFileA(locked_path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
                            OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    Check(held!=INVALID_HANDLE_VALUE,"hold staging directory without delete sharing");
    std::thread release([&]{std::this_thread::sleep_for(std::chrono::milliseconds(25)); CloseHandle(held);});
    auto committed_locked=library.Tool("game_commit",locked_request,output);
    release.join();
    if(!committed_locked) std::cerr<<"Windows lock cause: "<<committed_locked.message<<"\n";
    Check(bool(committed_locked),"bounded host retry commits after real Windows handle releases");
    auto conflict_job=Begin(library,"windows-conflict");
    auto conflict_request="{\"job\":\""+conflict_job+"\"}";
    Check(Tool_Passed(library,"game_write_file",JsonWrite(conflict_job,good),output) &&
          Tool_Passed(library,"game_validate",conflict_request,output),"prepare immutable destination probe");
    auto conflict_path=root/"games"/"windows-conflict"/"r00000001";
    fs::create_directory(conflict_path); WriteFile((conflict_path/"sentinel").generic_string(),"keep");
    Check(!library.Tool("game_commit",conflict_request,output) && Read((conflict_path/"sentinel").generic_string())=="keep",
          "host retry never replaces an existing game revision");
#endif
    std::string probe_report;
    Check(bool(library.SelfTest(snake,probe_report)) && probe_report.find("loop_guard=PASS")!=std::string::npos,"runtime diagnostic uses actual VM and limits");
    std::cout<<"PASS "<<checks<<" checks: focus, validation, recovery, immutable revisions, sandbox, input replay, 100 load/exit cycles\n";
}
