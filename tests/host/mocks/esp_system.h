#pragma once
inline void esp_restart() {}
inline unsigned esp_get_free_heap_size() { return 1024; }
inline unsigned esp_get_minimum_free_heap_size() { return 512; }
inline int esp_reset_reason() { return 1; }
