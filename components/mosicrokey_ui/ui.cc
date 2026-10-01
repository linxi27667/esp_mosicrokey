#include "mosicrokey/ui.hpp"
#include "mosicrokey/board.hpp"
#include "view.hpp"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <mutex>
#include <memory>
#include <algorithm>

namespace mosicrokey::ui {
namespace {
std::mutex mutex;
Snapshot latest;
std::vector<uint16_t> pending_frame;
bool changed=true,frame_changed=false;
std::unique_ptr<View> view;
bool enabled=false;
uint32_t Tick() {return static_cast<uint32_t>(esp_timer_get_time()/1000);}
void Flush(lv_display_t* display,const lv_area_t* area,uint8_t* pixels) {
    board::Flush(area->x1,area->y1,area->x2,area->y2,pixels); lv_display_flush_ready(display);
}
void UiTask(void*) {
    Snapshot state; std::vector<uint16_t> frame;
    for(;;) {
        bool update=false,draw=false;
        {std::lock_guard lock(mutex);
            if(changed) {state=latest; changed=false; update=true;}
            if(frame_changed) {frame=pending_frame; frame_changed=false; draw=true;}}
        if(update) view->Update(state);
        if(draw) view->Frame(frame);
        auto delay=lv_timer_handler(); vTaskDelay(pdMS_TO_TICKS(std::clamp<uint32_t>(delay,5,20)));
    }
}
}
Result Start() {
    auto result=board::InitializeDisplay(); if(!result) return result;
    lv_init(); lv_tick_set_cb(Tick);
    auto* display=lv_display_create(kWidth,kHeight);
    constexpr size_t size=kWidth*20*2;
    void* first=heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    void* second=heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    if(!display || !first || !second) {heap_caps_free(first); heap_caps_free(second); return Result::Fail(Error::Runtime,"Display buffer allocation failed");}
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,first,second,size,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,Flush);
    view=std::make_unique<View>(lv_screen_active());
    if(xTaskCreatePinnedToCore(UiTask,"mosi_ui",8192,nullptr,3,nullptr,0)!=pdPASS) return Result::Fail(Error::Runtime,"UI task allocation failed");
    enabled=true; return {};
}
void Update(const Snapshot& state) {if(!enabled) return; std::lock_guard lock(mutex); latest=state; changed=true;}
void Frame(const std::vector<uint16_t>& pixels) {if(!enabled) return; std::lock_guard lock(mutex); pending_frame=pixels; frame_changed=true;}
}
