#pragma once
#include <string>
namespace mosicrokey {
inline std::string RedactSecret(std::string text,const std::string& secret) {
    if(secret.empty()) return text;
    size_t offset=0;
    while((offset=text.find(secret,offset))!=std::string::npos) {
        text.replace(offset,secret.size(),"[redacted]"); offset+=10;
    }
    return text;
}
}
