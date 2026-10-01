# MosiCrokey UI 字体

`mosicrokey_font14.c` 是 Noto Sans SC 的 14 px、1 bpp LVGL 字体，共 21503 个字形；范围为 U+0020–007E、U+2000–206F、U+3000–303F、U+4E00–9FFF、U+FF00–FFEF。基本汉字及常用标点可直接显示；不覆盖所有扩展汉字或 emoji。

来源是本机 LVGL 测试资源 `lv_port_pc_vscode/lvgl/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf`，原文件 10560380 B，SHA256：

`ae82f4e2a55e1316a55bcc1d05e9555ce08d8bda07e893b486896b626fd852ff`

生成的 C 文件 3326149 B，SHA256：

`b46c543f210f943311a36d44ccab9709fb27d91edb69fc5b0b3b2accddef45f6`

脚本：`scripts/generate_font.py`，使用 Pillow 栅格化、阈值 100、LVGL 9.5 fmt_txt 格式。生成文件已独立纳入工程，正常构建不访问原字体目录。可选重生成命令：

```text
python scripts/generate_font.py 你提供的NotoSansSC-Regular.ttf assets/fonts/mosicrokey_font14.c
```

字体许可为 SIL OFL 1.1，原件在 `OFL.txt`，发布副本位于 `release/licenses/fonts/OFL.txt`。上游小智图标/基本字体另由锁定的 xiaozhi-fonts 组件提供。
