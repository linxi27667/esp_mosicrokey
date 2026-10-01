#pragma once
constexpr int MALLOC_CAP_8BIT=1;
constexpr int MALLOC_CAP_INTERNAL=2, MALLOC_CAP_SPIRAM=4;
inline unsigned heap_caps_get_free_size(int) {return 0;}
inline unsigned heap_caps_get_minimum_free_size(int) {return 0;}
