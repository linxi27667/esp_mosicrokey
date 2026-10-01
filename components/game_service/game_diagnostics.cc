#include "game_internal.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace mosicrokey {
Result GameService::SelfTest(const std::string& source,std::string& report) {
    std::lock_guard lock(impl_->vm_lock);
    if(impl_->vm) return Result::Fail(Error::Busy,"Exit game before runtime diagnostics");
    using Clock=std::chrono::steady_clock;
    auto micros=[](auto start) {return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();};
    std::vector<uint16_t> frame;
    int64_t maximum=0,total=0,initialization=0;
    {
        GameVm vm; auto start=Clock::now(); auto result=vm.Load(source,{}); initialization=micros(start);
        if(!result) return result;
        for(unsigned i=0;i<60;++i) {
            start=Clock::now(); result=vm.Tick(1.f/30.f,0,frame); auto duration=micros(start);
            if(!result) return result;
            if(frame.size()!=kWidth*kHeight) return Result::Fail(Error::Runtime,"Unexpected canvas size");
            total+=duration; maximum=std::max<int64_t>(maximum,duration);
#ifdef ESP_PLATFORM
            vTaskDelay(pdMS_TO_TICKS(1));
#endif
        }
    }
    for(unsigned i=0;i<100;++i) {
        GameVm vm; auto result=vm.Load(source,{}); if(!result) return result;
#ifdef ESP_PLATFORM
        vTaskDelay(pdMS_TO_TICKS(1));
#endif
    }
    auto reject=[](const char* source,const char* expected) {
        GameVm vm; auto result=vm.Load(source,{});
        return !result && result.message.find(expected)!=std::string::npos;
    };
    if(!reject("function init() while true do end end function update() end function draw() end","budget"))
        return Result::Fail(Error::Runtime,"Infinite-loop guard failed");
    if(!reject("function init() local s='a'; for i=1,25 do s=s..s end end function update() end function draw() end","memory"))
        return Result::Fail(Error::Runtime,"Lua heap guard failed");
    {
        GameVm vm;
        auto result=vm.Load("function init() assert(io==nil and os==nil and package==nil and load==nil and getmetatable==nil and setmetatable==nil) end function update() end function draw() end",{});
        if(!result) return result;
    }
    char summary[256];
    std::snprintf(summary,sizeof(summary),"frames=60 loads=100 init_us=%lld frame_mean_us=%lld frame_max_us=%lld loop_guard=PASS heap_guard=PASS restricted_libs=PASS storage=NOT_TESTED display=NOT_TESTED",
        static_cast<long long>(initialization),static_cast<long long>(total/60),static_cast<long long>(maximum));
    report=summary; return {};
}
}
