# Windows Demo 打包与验收

## 当前 GAS / 3C 重构版本

工作分支：`refactor/gas-3c`。GAS 阶段 1～4 已经用户确认正常；
阶段 5/6 的 3C、Cue 与控制层迁移已获用户确认；标准可视化 Boss AI 已完成，待完整实机验收。

- [x] Editor Development 与 Win64 Shipping 编译链接；
- [x] 25 组 GAS/3C 自动测试，0 警告、0 失败；
- [x] Cue 与控制层迁移后，Editor/Shipping 再次编译通过，30 组测试通过；
- [x] 用户确认 Cue、鼠标投影与预输入迁移后的当前版本正常；
- [x] 用户确认新 3C 当前版本正常；
- [x] Boss Pawn 迁移基础 Editor/Shipping 编译、32 组自动测试通过；
- [x] 用户确认 Boss 改为新 Pawn 父类后正常；
- [x] 新 AIController/原生行为树 Editor/Shipping 编译，38 组自动测试通过；
- [x] 标准可视化树版本 Editor/Shipping 编译、42 项测试通过；保存资产重载与原关卡 AI 启动检查通过；
- [x] `/Game/AI` 已加入 Always Cook，包含正式 BT、BB 和可选控制器蓝图；生成工具位于 Editor-only 模块，不进入 Shipping；
- [ ] 原关卡完整两阶段、表现、输入和 Restart 实机验收；
- [ ] 重新 Cook/Stage/Archive；
- [ ] 新包的独立启动、Restart 与低配机验收。

以下旧版本打包记录不代表本重构分支已重新生成安装包。原蓝图将自动继承三个原生组件，
不需要为了打包手工添加；保留原动态资源 Cook 和硬件光标 Stage 配置。

## 当前首包配置

- 平台：Windows 64-bit
- 配置：Development
- 默认地图：`/Game/Maps/Arena_Level`
- 默认 GameMode：`BP_RDCAGameMode`
- Cook：只显式加入 Arena 地图，并额外保留动态加载的 HUD、战斗音效和 BGM
- 容器：Pak + Io Store
- 运行库：包含 prerequisite installer

Development 首包用于发现 Cook、资源引用和运行时问题。通过验收后再把
`BuildConfiguration` 改为 `PPBC_Shipping` 并开启 `ForDistribution`。

## 2026-09-14 首包结果

- Windows Development 的 Build、Cook、Stage、Pak、Io Store 和 Archive 均成功；
- 输出目录：`rdca/Saved/PackagedBuilds/Development/Windows`；
- 完整目录约 `1.887 GiB`，共 34 个文件；
- 已进行无渲染、无声音的独立启动冒烟测试；
- 日志确认加载了 `Arena_Level` 和 `BP_RDCAGameMode_C`，并进入了实际战斗与失败结算；
- 未发现 Fatal、Missing Package、LoadClass failed 等启动错误。

冒烟测试只证明独立包能够启动并运行主要逻辑，不能验证画面、声音、输入手感和
完整胜利流程，以下项目仍需正常启动 `rdca.exe` 后人工验收。

## 独立运行验收

不要只在 PIE 中验收。直接启动打包目录中的 `rdca.exe`，依次检查：

1. 启动后直接进入 `Arena_Level`，不会进入 OpenWorld 模板或黑屏；
2. Player、Boss、环形地面、碰撞代理和摄像机均正确生成；
3. Boss 地面待机、起飞、中心地面碎裂和 Anchor 冒出正常；
4. 左键立即松开、短蓄力和满蓄力的落点与弧高正确；
5. 右键冲刺、Anchor 附着/换面和直线 Boss 冲撞正常；
6. 弹幕、Shockwave、Laser 的预警、视觉与伤害一致；
7. WeakPoint 的 Protected/Exposed 材质、伤害和反弹正确；
8. Phase 2 环境、天空、Boss 材质、Anchor 与弹幕颜色完成切换；
9. 战斗音效与唯一一首 BGM 均存在，阶段音量和结算淡出正常；
10. Victory、Defeat 和 Restart 能完成完整闭环；
11. 1920x1080 窗口与全屏下准星、落点、HUD 没有 DPI 偏移；
12. 退出游戏后查看 `Saved/Logs`，没有 Missing Package、LoadClass 或 Fatal 错误。

## 发布前检查

- 确认 BGM、Skybox、模型、动画、Niagara 和字体允许用于作品集 Demo；
- 删除或关闭仍可见的 Debug Draw、屏幕日志和强制攻击模式；
- 在另一台没有安装 Unreal Engine 的 Windows 电脑上启动一次；
- 压缩完整打包目录，而不是只复制 `.exe`；
- README 或下载页注明操作方式、UE 版本、Demo 状态和第三方资产来源。
