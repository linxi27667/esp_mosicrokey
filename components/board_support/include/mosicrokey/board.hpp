#pragma once
#include "mosicrokey/contracts.hpp"
#include <functional>
namespace mosicrokey::board {
Result InitializeDisplay();
void Flush(int x1, int y1, int x2, int y2, void* pixels);
Result MountCard();
bool CardMounted();
void SetStorageFailed();
Result StartKeys(std::function<void(Key, bool)> callback);
}
