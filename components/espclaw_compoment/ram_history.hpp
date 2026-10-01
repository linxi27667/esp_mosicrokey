#pragma once
#include <cJSON.h>
#include <string>
#include <utility>

namespace mosicrokey {
// claw_core supplies text-only USER/ASSISTANT_FINAL records; message_json is
// optional. Reporting persistence success must mean the active user is present,
// because the core then omits its own duplicate user message from the request.
inline bool AppendRamHistory(std::string& history, const char* role,
                             const char* message_json, const char* text) {
    cJSON* messages=cJSON_Parse(history.c_str());
    if(!cJSON_IsArray(messages)) {cJSON_Delete(messages); messages=cJSON_CreateArray();}
    cJSON* item=message_json ? cJSON_Parse(message_json) : nullptr;
    if(!item && text) {
        item=cJSON_CreateObject();
        if(item && (!cJSON_AddStringToObject(item,"role",role) || !cJSON_AddStringToObject(item,"content",text))) {
            cJSON_Delete(item); item=nullptr;
        }
    }
    if(!messages || !item || !cJSON_AddItemToArray(messages,item)) {
        cJSON_Delete(item); cJSON_Delete(messages); return false;
    }
    while(cJSON_GetArraySize(messages)>8) cJSON_DeleteItemFromArray(messages,0);
    char* json=cJSON_PrintUnformatted(messages);
    while(json && std::char_traits<char>::length(json)>16384 && cJSON_GetArraySize(messages)>1) {
        cJSON_free(json); cJSON_DeleteItemFromArray(messages,0); json=cJSON_PrintUnformatted(messages);
    }
    bool valid=json && std::char_traits<char>::length(json)<=16384;
    if(valid) history=json;
    cJSON_free(json); cJSON_Delete(messages); return valid;
}
// A publication transaction takes its previous game state from game_list.
// Preserve conversation history in storage, but don't feed earlier pseudo-tool
// replies or candidate-only instructions back as successful publication facts.
inline bool CurrentTurnHistory(const char* text,std::string& output) {
    std::string current="[]";
    if(!AppendRamHistory(current,"user",nullptr,text)) return false;
    output=std::move(current); return true;
}
}
