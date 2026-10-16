# Boss 标准行为树与 AIController 设计

日期：2026-10-16。分支：`refactor/gas-3c`。

**状态：已实现，待用户原关卡完整实机验收。** Editor/Win64 Development 与 Win64 Shipping 编译通过；42 项自动测试通过，0 警告、0 失败。
保存后的可视化资产和原关卡启动已验证。本轮没有重写用户的 Boss 蓝图或地图。

重构开始前的详细讲解见 [敌人 AI 系统详解 PDF](RDCA_敌人AI系统详解.pdf)，其 Markdown 源稿是
[enemy_ai_project_guide.md](enemy_ai_project_guide.md)。该 PDF 保留实施前快照；本页记录实施结果与实际用法。

## 现在如何查看与验收

1. 打开 UE 的 Content Browser（内容浏览器），进入 `Content/AI`。
2. 双击 `BT_RiftDragonCombat`：现在有真正可编辑的行为树图，不再只有运行时构建的树。
3. 双击 `BB_RiftDragonCombat` 查看黑板；九个项目字段外，UE 自动加入一个 `SelfActor`，共十个键。
4. 开始 Play 后保持游戏运行，再打开树。在树编辑器顶部的运行实例下拉框中选择 Boss 的 AIController，观察亮起的分支和右侧黑板。
5. 检查开场不出招、第一阶段两招眩晕、第二阶段三招去重、双波只算一招、弱点命中关闭窗口、死亡和 Restart。

原关卡已配置原生 `RDCABossAIController`，Auto Possess AI 为 `Placed in World or Spawned`，
它会加载保存的 `BT_RiftDragonCombat`；不必再次更换父类或添加组件。
也可以在 Boss 的 Class Defaults 中把 AI Controller Class 设为 `BP_RiftDragonAIController`，
这个蓝图已经指定同一树资产，但不是本轮运行的必要步骤。

权重、招间时间和技能尺寸仍在原 Encounter/攻击蓝图调整，不要在树中再加一套攻击持续时间。

## 1. “标准”在这个项目中具体指什么

UE 没有要求所有敌人使用同一棵树。这里采用常见的分工：

1. AIController 负责控制角色、确定目标、运行树和清理生命周期。
2. Blackboard（黑板）保存树要使用的决策信息。
3. Behavior Tree（行为树）使用可视化资产组织优先级和执行顺序。
4. Decorator（条件节点）读取黑板，判断分支能否执行，并监听变化来中断分支。
5. Service（服务节点）低频维护目标等信息，不负责攻击持续时间。
6. Task（任务节点）发起一个动作，并在完成/取消时返回结果。
7. GAS 管技能、生命、伤害与技能状态；动画和 Cue 根据真实结果表现。

“树通过黑板做决策”不意味着黑板成为第二份真实血量或技能状态，也不意味着执行伤害前取消安全检查。
最终的技能激活、扣血与取消仍必须在游戏逻辑中验证。

## 2. 最终资产与类

| 对象 | 职责 |
|---|---|
| `BP_BossPlaceholder` / `ARDCABossPawn` | 龙的身体、已有骨骼体/碰撞/Socket/组件 |
| `ARDCABossAIController` | 初始化、目标管理、事件订阅、启动/停止 AI |
| `BP_RiftDragonAIController` | 仅指定树资产等配置，不在蓝图 Tick 中重新选招 |
| `BB_RiftDragonCombat` | 可编辑、可在运行时查看的黑板资产 |
| `BT_RiftDragonCombat` | 可打开、拖拽和运行时观察的主行为树资产 |
| `UBossAttackPolicyComponent` | 一阶段编排、二阶段类别权重、轮内去重和随机种子 |
| `BossEncounterComponent` | 战斗阶段、轮次事实、窗口与恢复时间、原配置及兼容事件 |
| Boss GAS Abilities | 实际执行弹幕、激光、冲击波，区分完成和取消 |

三个 AI 资产已经生成并保存到 `rdca/Content/AI/`，已验证重新加载与真实执行。
选择算法不需要硬拆成八棵树；阶段策略可以由原生任务调用专门的选招组件。

现有配置先由旧 Encounter 字段提供给新组件读取，不能复制一套新的默认值导致已调好的蓝图失效。
是否改成攻击配置 Data Asset（可复用的数据资源），留到后续维护优化，不作为此次接入的前置工作。

## 3. 黑板：每个字段都说明谁写、谁读

| 字段 | 类型 | 写入者 | 使用者 |
|---|---|---|---|
| `TargetActor` | Object，基类 Pawn | Controller 的目标管理 | 无目标条件、选招任务、技能请求目标 |
| `IsBattleOver` | Bool | 战斗结束事件 → Controller | 最高优先级结束分支 |
| `IsEncounterHeld` | Bool | 开场/转阶段 Hold 事件 → Controller | 暂停出招分支 |
| `IsStunned` | Bool | 弱点窗口开/关事件 → Controller | 等待眩晕结束分支 |
| `CombatPhase` | Enum | 阶段变化事件 → Controller | 一/二阶段策略条件 |
| `NextActionTime` | Float | 战斗规则发布截止时刻 → Controller | 等待出招时机任务 |
| `SelectedAttack` | Enum：None/Barrage/Shockwave/Laser | 选招任务 | 攻击类型条件、执行任务 |
| `SelectedBarragePattern` | Enum，现有弹幕模式 | 选招任务 | 弹幕技能请求 |
| `HasAttackPlan` | Bool | 选招任务创建，执行/取消时清除 | 已有计划校验、避免重复选择 |

所有字段关闭 Instance Synced（跨多个 Boss 同步），防止不同 Boss 共用目标和计划。
任务用 `FBlackboardKeySelector` 指定要读取的字段，在资产中可见，不在各个 cpp 里散写字符串。

黑板不保存第二份实时 Health、完成招数或轮内去重集合。这些数据由 GAS/战斗规则保存，
选招组件查询它们；黑板中的阶段、眩晕等是用于决策的镜像。

### TargetActor 必须真正参与攻击

选招/执行任务从黑板读取目标，形成技能请求并传给 GAS。
弹幕预判、追踪和激光预警使用该目标，不再各自调用 `GetPlayerPawn(0)` 找一次玩家。
Controller 获取玩家对象时可以使用玩家控制器；目标管理不等于禁止使用游戏框架接口。

激光的停顿锁向、停顿结束判定左右仍由技能按原规则完成。
“读取目标”不等于全程追着玩家，更不能用每帧更新的黑板位置覆盖已经锁定的方向。

## 4. 可视化主树

Selector：从左到右按优先级选分支。Sequence：依次完成子节点。

```text
Root
└── Selector：BossCombat
    │   Service：ValidateTarget（低频兜底，约 0.2 秒）
    │
    ├── [IsBattleOver = true]
    │   └── WaitForShutdown：不再发起动作，等待 Controller 停止树
    │
    ├── [IsEncounterHeld = true]
    │   └── WaitForResume：等待 Hold 解除
    │
    ├── [TargetActor 未设置]
    │   └── WaitForTarget：等待目标管理写入有效目标
    │
    ├── [IsStunned = true]
    │   └── WaitForStunEnd：等待有效弱点命中或窗口超时
    │
    └── Sequence：ChooseAndAttack
        ├── WaitUntilNextAction：等待 NextActionTime
        ├── Selector：ChoosePhaseStrategy
        │   ├── [CombatPhase = Phase2] → SelectPhase2Attack
        │   └── [CombatPhase = Phase1] → SelectPhase1Attack
        └── Selector：ExecuteSelectedAttack
            ├── [SelectedAttack = Barrage]   → ActivateAndWaitAbility（弹幕）
            ├── [SelectedAttack = Shockwave] → ActivateAndWaitAbility（波）
            └── [SelectedAttack = Laser]     → ActivateAndWaitAbility（激光）
```

三个攻击执行节点复用同一个 C++ 任务类，资产里填写不同的技能标签。
弹幕具体模式由选招策略确定并发布到黑板，实际技能消费 Encounter 中的同一份计划，
不会再次随机；不需要为每个模式编写不同的执行任务。

这里不另设“正在攻击就跳去等待”的高优先级分支：
释放任务本身会保持 InProgress（还没完成），直到本次 GAS 技能结束。
否则容易因为技能刚激活就进入忙碌状态，把自己的释放任务中断。

## 5. AIController 的具体工作

### 启动

1. `OnPossess` 获得 Boss，确认必要组件与 ASC。
2. 若 Boss 还未完成 BeginPlay，订阅明确的初始化完成通知；不重置生命，不提前出招。
3. 初始化指定的黑板资产，写入初始状态和有效目标。
4. 订阅战斗、阶段、Hold、弱点窗口、目标死亡/销毁等事件。
5. 获得唯一决策控制权，然后运行指定的 `BT_RiftDragonCombat`。

目标优先由玩家 Controller 的 Pawn 变化事件维护，本机 UE 5.3 提供
`OnPossessedPawnChanged` / `GetOnNewPawnNotifier()`；目标死亡/销毁时立即清除黑板引用。
根节点服务只作低频校验和补偿，不每帧反复搜索玩家。

### 运行

- Controller 不随机选技能、不生成弹丸、不修改每轮计数。
- Controller 不每帧复制全部黑板值，条件节点也不在判断时反过来刷新黑板。
- 状态事件更新对应键；需要多个键一起发布时先暂停观察通知，完整写入后再恢复，避免半更新状态。
- 不使用 `SetFocus` 或 Controller 旋转强行覆盖龙的朝向；保留原朝向/激光锁向逻辑。
- 固定竞技场 Boss 不必添加 AI Perception（感知组件）、巡逻、NavMesh 或 MoveTo。

### 结束与失去控制

- 死亡/胜负：先禁止新请求，取消技能并清理攻击，然后安全停止树。
- 不在 GAS 结束回调深处强行递归执行整棵树的停止与启动；必要的树停止排到安全时机处理。
- `OnUnPossess` / `EndPlay`：取消自己拥有的动作，移除任务与控制器的订阅，停止树。
- 再次控制 Boss 时重新同步事实，不能把上一轮缓存目标/攻击计划带回来，也不能重置真实血量。

## 6. 条件与中断规则

使用 UE 自带的 Blackboard 条件节点，不用自定义 Tick 条件来模拟黑板观察。
通常选择 `Notify Observer = On Result Change`：条件真假变化时才要求重新判断。

Observer Aborts 的含义：

- None：本分支进入时检查，不主动打断别的任务。
- Self：条件失效时中断本分支。
- Lower Priority：条件成立时中断更低优先级分支。
- Both：同时支持上述两种中断。

| 条件 | 设置 | 原因 |
|---|---|---|
| 战斗结束 | Both | 立即禁止继续攻击，等待安全关停 |
| 开场/转阶段 Hold | Both | 进入时中断当前出招，解除后离开等待分支 |
| 没有有效目标 | Both | 目标失效时停止请求；目标恢复后可以继续 |
| 每轮结束的眩晕 | None | 这是本次技能正常完成后的结果，不应将最后一招误记为被取消 |
| 一/二阶段策略 | None | 一次选招中确定策略；转阶段由 Hold 分支负责打断 |
| 选定的攻击类别 | None | 计划确定后执行，不因外部重新抽选而切换技能 |

眩晕等待任务监听窗口关闭事件，结束后自行返回成功，因此不需要 Self 来解除等待。
本轮眩晕只来自完整攻击循环；如果以后增加可在攻击中被玩家强制打晕的机制，
应先在 GAS 中取消攻击，再发布强制眩晕，单独设计其抢占规则。

## 7. 任务怎样执行，怎样结束

### 选招任务

- 查询一次阶段策略及当前轮次，读取黑板 TargetActor。
- 生成一次待消费的攻击计划：攻击类别、弹幕模式、目标和必要参数；执行时才分配独立执行编号。
- 将树需要的选择结果写入黑板，设 `HasAttackPlan = true`。
- 不播放动画、不放技能、不增加完成招数。
- 同一计划未消费前不能再次随机，资源缺失时按原规则选择可用方案。

### GAS 释放任务

1. 校验黑板目标、计划和配置的技能标签。
2. **激活前**登记本次技能完成通知，处理技能可能在激活调用内同步结束的情况。
3. 把目标与选招参数传给 GAS，保存具体技能实例/执行编号；不是只监听“任意技能结束”。
4. 激活成功后保持 InProgress；正常结束返回成功，激活失败/取消返回失败。
5. 收到 Abort（树要求中断）时，只取消自己拥有的那次技能并清除订阅。
6. 正常完成、失败、取消、节点销毁、关卡退出都释放订阅；重复结束只处理一次。

使用 ASC 的技能完成事件或项目的带编号完成通知，不在任务 Tick 里反复问“技能结束了吗”。
任务状态保存在节点实例或 NodeMemory（每个 AI 独立的节点内存），不能存到共享模板上。

技能内部允许 Tick：追踪、扫射、波扩散本来就需要持续更新。
“行为树事件驱动”不等于让物理、轨迹和材质停止逐帧更新。

### 等待任务

- Hold、目标和眩晕：监听黑板键或对应事件，不另开每帧倒计时。
- 招间恢复：`NextActionTime` 是规则层发布的唯一截止时刻；任务只按剩余时间等待。
- 如果截止时刻变化，重排等待；任务退出时清掉自己的定时器。
- 使用随游戏暂停的时间，不用电脑的真实时钟。P 暂停后不能在后台偷偷结束冷却。
- 旧自主模式和新树模式不能同时维护状态时间；新模式下原查询接口从同一时间来源读取。

## 8. 保留原战斗规则

一阶段仍是教学编排，二阶段仍先抽弹幕整体/波/激光，再抽弹幕子类型。
默认类别权重为 0.5 / 0.3 / 0.2，轮内去重后在剩余类别中重新归一化。
默认一阶段每两招、二阶段每三招开放弱点；双波是同一次技能，只计一次。

**计数只能由确认过的技能正常完成结果更新，不能再增加一个“行为树执行完就 +1”的任务。**

最后一招正常结束时，规则层在同一条完成处理流程中立即开放弱点，并发布 `IsStunned`；
行为树随后转入等待分支。不会额外等一轮恢复，也不等待低频服务发现眩晕。

有效弱点命中关闭窗口并重置轮次，超时也重置轮次。阶段切换更新 CombatPhase 并开启 Hold。
原弱点碰撞、反弹、激光停顿锁向、固定波生成高度、两阶段材质/特效、音频与 3C 不重做。

## 9. 本轮已完成的迁移

| 当前实现 | 目标实现 |
|---|---|
| C++ 运行时创建默认树，无树图资源 | 正式 BT/BB 资产，可运行时观察分支 |
| Controller Tick 和条件检查中刷新黑板 | 生命周期/战斗事件更新，低频服务校验目标 |
| 自定义条件 Tick 并混合直接查询 | UE 黑板条件和观察者中断 |
| 目标写入黑板，但攻击仍各自找玩家 | 任务读黑板目标并传给技能，技能按原规则瞄准/锁定 |
| 任务逐帧比较攻击结束编号 | 任务订阅带身份的技能完成/取消事件 |
| Encounter 同时包含选招算法和底层执行 | 独立选招策略组件，Encounter 保留规则事实与兼容配置 |
| 未指定资产时默认原生树/旧调度回退 | 正式 AI 模式要求正确资产，配置错误明确报错并停止；旧模式仅显式启用 |

条件节点不直接依赖 Encounter；任务通过明确的技能/选招接口调用业务逻辑。
释放与伤害入口仍可以直接检查真实状态，避免黑板更新间隙让非法动作通过。

## 10. 实施记录与验收边界

1. **接口准备**：目标传递、带编号完成事件、唯一状态时间；保持当前树能运行。
2. **原生节点**：事件等待任务、选招任务、GAS 执行任务和目标校验服务；补生命周期测试。
3. **可视化资产**：建立 BB、BT 和控制器配置蓝图，按第 4 节连接，用原生节点执行。
4. **正式切换**：指定资产、移除正式路径的默认树构建和混合查询，旧模式只留明确迁移开关。
5. **整体验收**：回归现有 38 组测试并新增以下检查，再做实机关卡与打包检查。

- 黑板条件改变能触发正确中断，不依赖条件 Tick 或再次调用刷新函数。
- 黑板指定的目标确实进入弹幕/激光技能，不被技能内的全局玩家查询覆盖。
- 最后一招正常结束立即眩晕，只增加一次计数；取消不计数。
- 技能同步结束、激活失败、重复回调、旧任务结束不会卡树或取消新技能。
- 两个 Boss 共享同一树资源时，目标、计划、节点内存和回调不相互影响。
- Hold、玩家死亡、Boss 死亡、目标销毁、失去控制、Restart 均无残留订阅/技能。
- 缺少正式树/黑板时明确报错，不偷偷用旧大脑攻击而让人误以为迁移成功。
- 可视化树显示真实运行分支，BB 里能看到目标、阶段、计划等变化。
- 原蓝图参数、动画、两阶段场景、声音和 3C 保持；正式树/BB/控制器资源随打包正确 Cook。

上述五步的代码与资产工作已完成。自动验证与剩余人工项目见 [测试报告](test_report.md)。

实际新增/修改入口：

- `rdca/Source/rdca/Private/Boss/AI/RDCABossAIController.cpp`：目标、初始化完成事件、黑板事件同步、停止/取消。
- `RDCABossBTNodes.cpp`：UE 原生黑板条件观察者、事件式等待/执行任务、0.2 秒目标校验服务。
- `BossAttackPolicyComponent.cpp`：原选择算法迁移，参数和轮次事实仍读取 Encounter。
- `RDCABossBehaviorTreeFactory.cpp`：仅供创建资产/测试；正式运行不偷偷创建默认树。
- `rdca/Source/rdcaEditor/`：仅编辑器模块，创建可视化图并保存资产；不进入 Shipping。

资产工具 `-run=RDCABossAIAssets` 只允许首次创建，不覆盖已存在的树；
已有资产使用 `-VerifyOnly` 只读检查。可选 `-ConfigureBoss` 会先在 Saved/Backups 备份 Boss 蓝图，
再设置其三个 AI 默认项。本轮只使用只读验证，没有执行该写入模式。
不要在 UE 编辑器打开时运行写入模式。

保存后的树图有 15 个主节点（含编辑器 Root，条件/服务作为附属节点），黑板有 10 个键。
原关卡无渲染冒烟已确认：保存资产启动 → Hold 解除 → 两招 → 眩晕 → 超时进入下一轮。
该检查使用 `-NullRHI -nosound`，不能替代画面、音频、输入、二阶段转场和 Restart 的人工验收。
尚未重新 Cook/Stage 或生成安装包，尚未提交 Git。

## 参考依据

采用 Epic 的 Behavior Tree / Blackboard / Decorator / Service / Task 分工，具体 API 按本机 UE 5.3 校验。

- [Epic：行为树与黑板](https://dev.epicgames.com/documentation/unreal-engine/behavior-trees-in-unreal-engine)
- [Epic：快速入门及黑板条件的 Observer Aborts](https://dev.epicgames.com/documentation/unreal-engine/behavior-tree-in-unreal-engine---quick-start-guide)
- [Epic：服务节点](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-behavior-tree-node-reference-services)

本机头文件已确认：`UBTDecorator_Blackboard` 支持黑板变化通知、Controller 提供 Pawn 变化通知、ASC 提供 `OnAbilityEnded`。
实现依据本机 UE 5.3 源码，构建、资产重载与测试结果分别记录，不把接口存在或编译成功等同于实机验收完成。
