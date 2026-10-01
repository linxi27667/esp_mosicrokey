#pragma once
#include "mosicrokey/contracts.hpp"
namespace mosicrokey::ui {
Result Start();
void Update(const Snapshot& snapshot);
void Frame(const std::vector<uint16_t>& pixels);
}
