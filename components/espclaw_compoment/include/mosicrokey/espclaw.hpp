#pragma once
#include "mosicrokey/contracts.hpp"
#include <memory>
namespace mosicrokey {
std::unique_ptr<IAgentService> CreateAgent(IGameLibrary& library, IEvents& events);
}
