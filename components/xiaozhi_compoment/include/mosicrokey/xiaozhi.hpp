#pragma once
#include "mosicrokey/contracts.hpp"
#include <memory>
namespace mosicrokey {
std::unique_ptr<IXiaozhiService> CreateXiaozhi(IEvents& events);
void InitializeXiaozhi();
void RunXiaozhi();
}
