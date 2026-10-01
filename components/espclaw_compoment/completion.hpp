#pragma once
#include "mosicrokey/contracts.hpp"

namespace mosicrokey {
struct AgentCompletion { bool success; std::string text; };
inline AgentCompletion FinishAgent(bool model_success,AgentIntent intent,
                                   const std::string& publication,const std::string& reply) {
    if(!publication.empty())
        return {model_success,publication+(model_success ? "\n" : "；后续请求未完成：")+reply};
    if(model_success && intent==AgentIntent::PublishGame)
        return {false,"未发布游戏：模型未提交新版本，请重试或调整需求。"};
    return {model_success,reply};
}
} // namespace mosicrokey
