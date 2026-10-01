# 游戏包与 Lua API v1

逻辑画布固定 160×128，RGB565，左上角原点。默认 30 FPS，固件提供方向/确认输入并处理返回键退出。首版游戏无声音、无通用存档。

## Lua 入口

```lua
function init()
  -- 初始化一次
end
function update(dt, input)
  -- dt：秒；input.up/down/left/right/confirm/back：布尔按键状态
end
function draw()
  game.clear(0)
  game.text(4, 4, "HELLO", 0xffff)
end
```

draw 与 update 共用一帧预算。输入为当前按下状态，单次触发需要脚本自行检测上升沿；Back 由业务退出，不能通过脚本阻止。完整示例见[贪吃蛇](../examples/snake/main.lua)。v0.1.6 示例采用板载 GLM 修复后逐字节导出的 V3（SHA256 54df1351ba60b757d340e16787566acfe7433aea7b4c83c687c612860b00b8d7），经过 32 项回放，加入每次移动只接受一次转向、有界找食物和满屏胜利。

| API | 参数与效果 |
|---|---|
| game.clear(color) | 整张画布清为 RGB565 颜色 |
| game.rect(x,y,w,h,color) | 填充矩形并裁剪至画布 |
| game.line(x1,y1,x2,y2,color) | 画线并裁剪 |
| game.text(x,y,text,color) | ASCII HUD 文本，最多 96 B，小号点阵字体 |
| game.image(path,x,y) | 绘制预加载 RGB565 图片，path 为清单中的 assets/name.rgb565 |
| game.random(n) | 返回 1..n 的整数，n 为 1..1000000；固定初始种子用于确定性回放 |

颜色范围 0..65535。坐标与矩形宽高为 -1024..1024；负宽高不绘制。中文标题/AI 文本由 LVGL 字体显示，游戏 HUD API 使用 ASCII。

## 格式与路径

```text
/sdcard/games/
  .staging/jobN/              # 未提交任务
  snake/r00000001/
    manifest.json
    main.lua
    READY
    assets/                  # 可选图片
/sdcard/agent/                # ESPClaw 会话、记忆、技能状态
/system/skills/game-creator/  # Flash 只读系统技能
```

游戏 ID：1..32 个小写 ASCII 字母、数字、-、_。标题 1..96 B。修订号为 r 加 8 位十进制数字。唯一入口 main.lua；资源路径只允许 assets/安全名称.rgb565，不接受绝对路径、..、反斜线或多级子目录。

清单由固件生成，示例结构：

```json
{
  "format_version": 1,
  "api_version": 1,
  "id": "snake",
  "title": "贪吃蛇",
  "revision": "r00000001",
  "entry": "main.lua",
  "files": [
    {"path": "main.lua", "size": 1234, "checksum": "实际8位十六进制值"}
  ]
}
```

checksum 为文件原始字节的 FNV-1a 32 位小写 8 位十六进制；示例 size/checksum 必须替换为实际值。READY 是 manifest.json 原始字节同算法的 8 B 校验值，不带换行。清单、文件长度、资源及标记均通过才能枚举；修改 TF 文件时应使用工具重新生成并提交。

资源格式：小端 uint16 宽、uint16 高，然后宽×高个小端 RGB565 像素。宽 1..160，高 1..128，字节长度必须为 4 + 2×宽×高；不含透明通道。

这些校验用于检测文件损坏，不提供签名或来源认证。

## 五个 Agent 工具

| 工具 | 输入 / 行为 |
|---|---|
| game_begin | {id,title}；创建暂存任务，返回 job 与 revision。相同 ID 继承最新有效版本的资源 |
| game_write_file | {job,path,content,encoding}；main.lua 使用 utf8，图片使用 base64；重写后须重新校验 |
| game_validate | {job}；生成清单，校验路径、大小、校验值、资源、Lua 语法，执行初始化和三帧受限探测 |
| game_commit | {job}；仅接受校验后的任务，再校验，写入 READY、flush/fsync 后重命名到不可变版本目录 |
| game_list | {} 返回最新有效游戏列表；{id,offset} 返回清单及不超过 4096 B 的源码分页，含 next_offset/eof |

修改游戏先 game_list 读取当前源码所有分页，保持 ID，写完整新 main.lua。首次校验失败后最多三轮修正，16 次工具迭代上限仍同时适用。任务结束/取消时清理本请求的暂存任务；已经提交的版本保留。

重启忽略未完成、标记错误或文件损坏的版本，选择该 ID 最新有效版本。旧版本保留；孤立断电暂存目录保留但不枚举，首版不提供自动历史清理。FAT32 断电后的实际恢复需实板故障测试，当前主机已覆盖残留与损坏恢复。

## USB 临时预览

仅连接 USB 时，Agent 可调用第六个工具 `game_preview({id,title,source})`。
ID 为 1..24 个小写 ASCII 字母、数字、-、_，标题最多 89 UTF-8 字节；完整 Lua 源码最多 64 KiB。预览不接受图片资源，使用同一受限 VM 校验初始化及三帧后发布。

游戏库显示“临时”标题，返回 ID 为 `preview-原ID`，修订号为 `pN`，`persistent=false`。内存中只保留最新预览；重启后丢失。当前游戏保持启动时的 VM，修改后须退出并重新启动。TF 安装工具仍须真实挂载的 TF 卡，不会因预览而报告安装成功。

修改前用 `game_list({id:"preview-snake",offset:0})` 读取源码，再以不带 `preview-` 的原 ID 提交完整新源码。串口 `frame` 导出运行中的实际 160×128 RGB565 画布；`scripts/capture-frame.py 日志 输出.png` 校验全部行及 FNV-1a 后生成 PNG。此画面不能代替实体屏幕的颜色、偏移和背光验收。

串口 `game_selftest` 使用真实设备 VM 执行样例、100 次加载释放以及超时、内存与库限制诊断；此命令不测试 TF 存储或实体显示。

只读串口 `game_source <id> [offset]` 可导出设备当前实际源码。`scripts/export-game-source.py preview-snake 输出.lua` 校验分页与修订号后存到电脑，不构成 TF 持久化。`build-host/snake_probe.exe 输出.lua` 在同一受限运行时回放指定初始布局、两次进食、方向、穿墙、自撞与确认重开；这是针对本次 AI Snake 验收场景的检查，通用安装校验不保证游戏玩法正确。

针对本轮 V3，追加 `--extended` 执行 32 项检查，增加满屏胜利/确认重开、仅一个空格时有界食物搜索及同一移动前两次转向。满屏和拥挤场景通过追加局部测试夹具构造，生产导出源码保持不变；这些是实际源码的主机回放，不能称为设备满屏实测。

## 执行限制

| 项目 | 上限 |
|---|---|
| Lua 源码 | 64 KiB |
| 包总量（清单及文件） | 512 KiB |
| 文件清单 | 32 项 |
| 活跃暂存任务 | 4 个 |
| Lua 自定义堆 | 256 KiB，ESP32 优先 PSRAM |
| Lua 栈槽 | 4096 |
| 顶层脚本 / init | 分别 100 ms |
| update + draw | 每帧 10 ms，共 100000 条指令预算 |
| 绘图操作 | 每帧 1024 次 |
| 默认帧率 | 30 FPS |

仅加载文本源码，拒绝 Lua 字节码。开放所需 base/table/string/math/utf8；不开放 io/os/package/debug/coroutine、文件、网络、GPIO、动态库。

移除 load/loadfile/dofile、pcall/xpcall、collectgarbage、print/warn、getmetatable/setmetatable；禁止访问及修改内建元表，避免原生回调递归绕过栈预算。移除不可在 Lua hook 中及时打断的 string.find/match/gmatch/gsub/dump/rep 和 table.sort/move。指令 hook 每 1000 条检查预算和时间；原生绘图调用自身有尺寸与次数限制，调用结束也检查时间。设备实际耗时仍须验证。

所有初始化和帧准备/运行放在受保护调用内，避免内存耗尽发生在未保护的 Lua API 调用。超时、内存耗尽、脚本错误释放 VM 并发布错误，业务返回游戏库；安装探测失败则给 Agent 返回结构化错误。
