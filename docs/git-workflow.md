# Git 版本管理

仓库：[linxi27667/esp_mosicrokey](https://github.com/linxi27667/esp_mosicrokey)，私有，GitHub 账号 linxi27667。首个基线提交为 96431996322a2dd72048bfb2e54c53f5debb45ca；推送后须核对远端 main 与本地 HEAD 相同。

当前工程使用独立 Git 仓库，默认分支 main。首次提交保存 v0.1.9 已实现源码、固定上游快照、依赖锁、补丁和当前脱敏验证证据；当时两小时测试仍在运行，不能把该提交解释为整机验收完成。

源码修改和验证结果分别以明确的提交记录。每轮修复先定位实际问题，再进行必要测试，通过后提交；不要提交没有证据的通过结论。发布标签对应具体提交，已推送的标签不移动。整机验证未完成时发布应标为预发布，说明 USB 范围和待实测项目。

third_party 保存固定上游源码，不作为子模块；克隆后构建不依赖原工作目录。`.gitattributes` 保留原字节，避免换行转换破坏来源及证据 SHA256。

build、managed_components、原始 logs、sdkconfig 和生成的固件/ELF 不进入源码历史；依赖由锁文件恢复，固件通过构建脚本生成。release/README.md 保留交付说明。已脱敏并校验的精选证据放在 docs/validation。Wi-Fi 密码、API 密钥及设备私有配置不提交，推送前检查暂存文件。

常用操作：

```powershell
git status --short
git diff
git add <本次变更的具体文件>
git diff --cached
git commit -m "fix: 描述实际问题及修复"
git push origin main
```

不要用强制推送覆盖共享历史，也不要为当前调试改动重写已发布提交。
