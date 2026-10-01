# 游戏创作与版本库

用户从串口 gameask 提交创作需求，只有本轮实际发布记录才算完成；普通 ask 用于对话/分析。真实 ESPClaw 通过 game_begin、game_write_file、game_validate、game_commit、game_list 创建或修改 TF 包，固件生成 manifest.json 并完成限制探测。提交成功后刷新游戏库。

修改时 game_list 读取原清单及源码分页，原 ID 创建新修订，资源继承。版本目录不可变；启动时选择版本并将源码/资源读入 VM，后台安装不会替换正在运行的内容。

TF 根目录 /sdcard，版本 /sdcard/games/ID/r00000001，暂存 /sdcard/games/.staging/jobN；READY 最后写入并与清单校验匹配。重启忽略无提交标记及损坏版本，优先返回最新有效版本；断电留下的孤立暂存目录不会自动安装，也不自动格式化卡。

首个样例为[贪吃蛇](../../examples/snake/main.lua)。API、受限库、路径/校验和/资源格式见[游戏 API](../game-api.md)。首版无通用存档或游戏音频，真实 AI 创作及 TF 重启持久化验收待设备与服务配置。
