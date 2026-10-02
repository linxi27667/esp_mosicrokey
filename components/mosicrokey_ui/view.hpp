#pragma once
#include "mosicrokey/contracts.hpp"
#include <lvgl.h>
#include <array>
namespace mosicrokey::ui
{
class View
{
  public:
    explicit View(lv_obj_t* screen);
    void Update(const Snapshot& state);
    void Frame(const std::vector<uint16_t>& frame);

  private:
    lv_obj_t *root_, *status_, *title_, *content_, *footer_, *canvas_ = nullptr;
    lv_obj_t *viewport_, *focus_, *test_ = nullptr;
    std::array<lv_obj_t*, 6> tiles_{};
    std::array<lv_obj_t*, 6> tile_labels_{};
    std::array<lv_obj_t*, 9> key_indicators_{};
    void Menu_Draw(const Snapshot& state, const std::vector<std::string>& entries);
    void Tiles_Draw(const Snapshot& state);
    void Display_Test_Draw();
    std::vector<uint16_t> pixels_;
    Mode mode_ = Mode::Home;
};
} // namespace mosicrokey::ui
