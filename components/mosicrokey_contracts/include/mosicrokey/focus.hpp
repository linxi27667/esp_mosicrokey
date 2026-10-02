#pragma once
#include "contracts.hpp"
namespace mosicrokey {
struct FocusChange { bool release_audio, acquire_audio, stop_game; };
constexpr FocusChange ChangeFocus(Mode from, Mode to) {
    return {from==Mode::Xiaozhi && to!=Mode::Xiaozhi,
            from!=Mode::Xiaozhi && to==Mode::Xiaozhi,
            from==Mode::Game && to!=Mode::Game};
}
constexpr Mode BackDestination(Mode mode) {
    if(mode==Mode::Game) return Mode::Library;
    if(mode==Mode::InputTest || mode==Mode::DisplayTest) return Mode::Settings;
    return Mode::Home;
}
constexpr Mode HomeDestination(unsigned selection) {
    constexpr Mode choices[]={Mode::Xiaozhi,Mode::Agent,Mode::Library,Mode::Computer,Mode::Settings,Mode::Diagnostics};
    return selection<6 ? choices[selection] : Mode::Home;
}
}
