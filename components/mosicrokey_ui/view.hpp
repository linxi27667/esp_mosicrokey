#pragma once
#include "mosicrokey/contracts.hpp"
#include <lvgl.h>
namespace mosicrokey::ui {
class View {
public:
    explicit View(lv_obj_t* screen);
    void Update(const Snapshot& state);
    void Frame(const std::vector<uint16_t>& frame);
private:
    lv_obj_t *root_, *status_, *title_, *content_, *footer_, *canvas_=nullptr;
    std::vector<uint16_t> pixels_;
    Mode mode_=Mode::Home;
};
}
