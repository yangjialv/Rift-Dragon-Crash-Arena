# Rift Dragon Crash Arena：Boss 行为设计

> 版本：v0.1  
> 目标：确定 Demo Boss 的决策结构、战斗阶段、攻击高度与弱点窗口。  
> 实现原则：当前使用 C++ 分层状态机，不引入 Behavior Tree。

---

## 1. 设计目标

Boss 的职责不是随机播放技能，而是持续改变玩家对地面、空中和 Anchor
的选择。一次完整交互应形成：

```text
读取 Boss 前摇
→ 判断当前空间是否安全
→ 使用走位、冲刺、跳跃或换 Anchor 应对
→ 攻击序列结束
→ 利用弱点窗口进行重撞
```

行为系统必须满足：

1. 不连续重复同一种攻击；
2. 根据玩家所在高度和是否附着选择攻击；
3. 每种视觉攻击与真实碰撞高度一致；
4. 前摇开始后锁定攻击，不临时追踪并改变答案；
5. 弱点窗口由攻击节奏产生，而不是每个技能后机械出现；
6. Phase 2 提高组合压力，但必须保留可读解法；
7. 所有选择和状态转换可记录、可复现、可测试。

---

## 2. 为什么暂不使用 UE Behavior Tree

当前 Boss 固定在竞技场中心，不需要寻路、巡逻、搜索目标或复杂移动。
攻击数量有限，并且攻击、恢复和弱点暴露需要严格时序。

因此当前采用：

```text
C++ 通用状态机
＋
攻击类型
＋
玩家空间状态
＋
带约束的权重选择
```

以后出现以下需求时，再考虑 Behavior Tree：

- Boss 需要移动和追踪；
- 多种敌人需要共享 Task；
- 需要召唤物协同行为；
- 设计师需要大量编辑器可视化分支。

---

## 3. 分层行为模型

Boss 行为拆成四层：

```text
Encounter Result
└── Combat Phase
    └── Action State
        └── Current Attack
```

### 3.1 Encounter Result

```text
Playing
Victory
Defeat
```

任一方死亡后，停止状态机并清理所有攻击 Actor。

### 3.2 Combat Phase

| 阶段 | Boss HP | 目的 |
|---|---:|---|
| Phase 1 | 3–2 | 教会玩家分别识别三类基础攻击 |
| Phase 2 | 1 | 使用有序组合攻击，压缩安全时间 |
| Dead | 0 | 停止攻击并进入胜利流程 |

阶段只在一次攻击或弱点窗口结束后切换，不能在技能中途改变当前攻击。

### 3.3 通用 Action State

重构后的状态只描述行为阶段：

```text
Idle
SelectingAttack
Preparing
Attacking
Recovery
WeakPointExposed
Dead
```

不再为每种攻击分别建立 `PreparingFanAttack`、`LaserAttacking`
等枚举状态。

### 3.4 Current Attack

```text
None
Shockwave
AimedVolley
FanBarrage（Phase 2）
SweepLaser
```

`ActionState` 决定 Boss 正处于前摇、攻击还是恢复；
`CurrentAttack` 决定具体执行哪种技能。

---

## 4. 玩家空间状态

决策器在选择攻击时读取一次玩家状态：

| 玩家状态 | 判定 |
|---|---|
| Grounded | 玩家处于地面高度且未附着 |
| Airborne | 玩家正在抛物线移动并高于地面 |
| Attached | 玩家附着在 Anchor 表面 |

选择完成后锁定攻击。玩家随后改变状态是成功应对，而不是要求 Boss
在前摇过程中重新选择克制技能。

---

## 5. 攻击目录与高度职责

### 5.1 Shockwave

- 目标：地面玩家；
- 高度：仅覆盖 `GroundDamageMaximumHeight` 以下；
- 解法：高弧跳跃、附着较高 Anchor；
- 不影响 Anchor；
- 不追踪玩家。

### 5.2 AimedVolley

- 目标：验证精确锁定、走位和右键冲刺；
- 前摇开始时锁定目标，连射期间不重新追踪；
- 默认向中心、左右第一层和左右第二层五个锁定点依次发射；
- 弹丸保留现有玩家伤害与 Anchor 过载逻辑；
- 只有真实碰撞时才影响玩家或 Anchor。

### 5.3 FanBarrage（Phase 2）

- 目标：使用高密度扇形封锁一片空间；
- 只在 Phase 2 出现，不承担精确锁定测试；
- 解法：提前识别安全扇区、换面或换 Anchor；
- 命中 Anchor 时仍使用现有固定过载规则。

FanBarrage 与 AimedVolley 复用同一个 Projectile Actor，但使用不同的
发射调度、数量、角度和视觉颜色。

首轮实现参数调整为 21 发双波交错扇形、100 度、每发间隔 0.035 秒、
速度 1250。第二波落在第一波角度间隙中，提高空间压迫而不使用持续追踪。
组合在开始时只锁定一次玩家位置，第二段不会重新瞬时瞄准。

### 5.4 SweepLaser

- 目标：切割一条空间路线；
- 视觉柱、Niagara、`DamageVolume` 必须共享同一个 `BeamRoot`；
- 只有 `DamageVolume` 真实覆盖 Anchor 时才持续增加过载；
- 第一版使用一种明确高度，不覆盖整个竞技场垂直空间；
- Phase 2 可以通过不同高度或连续两次扫描形成组合。
- 预警前段朝玩家持续瞄准，末尾 0.25 秒锁定；
- 锁定时读取玩家在预警期间的横向走位方向；
- 激光朝该侧快速扫过固定角度，激活后不再读取玩家位置；
- 当前旋转 Boss Actor，将来正式骨骼模型可把同一逻辑映射到头部/嘴部 Socket。

---

## 6. Phase 1 攻击选择

Phase 1 每完成两次独立攻击，进入一次弱点暴露窗口。

基础权重：

| 玩家状态 | Shockwave | AimedVolley | SweepLaser |
|---|---:|---:|---:|
| Grounded | 40 | 35 | 20 |
| Airborne | 10 | 30 | 35 |
| Attached | 5 | 10 | 40 |

选择约束：

1. 上一次使用的攻击本次权重归零；
2. 如果某种攻击 Actor 或必要组件未配置，其权重归零；
3. 所有有效权重之和为零时回退到 `AimedVolley`；
4. 随机种子可配置，便于复现测试；
5. 同一轮弱点暴露前，优先选择两种不同的空间压力。

---

## 7. Phase 2 组合攻击

Phase 2 不简单提高所有数值，而是从以下组合中选择一个：

### 7.1 Ground Pressure

```text
Shockwave 前摇
→ Shockwave 扩张
→ 短间隔
→ AimedVolley
→ Recovery
→ WeakPointExposed
```

玩家先用高弧离地，再决定落点和弹幕缝隙。

### 7.2 Anchor Pressure

```text
FanBarrage 前摇
→ 高密度扇形封锁当前空间
→ 短间隔
→ SweepLaser
→ Recovery
→ WeakPointExposed
```

玩家需要提前离开危险 Anchor，并从新位置组织反击。

玩家从 Anchor 成功跳跃或冲刺后，该 Anchor 会自动碎裂并异地补位，
因此 Anchor Pressure 同时要求玩家规划下一处落点，不能反复使用同一安全点。

组合规则：

1. 两段攻击之间必须至少保留一次明确输入机会；
2. 第二段不重新瞬时锁定玩家；
3. 组合结束后必定暴露弱点；
4. 不允许 Shockwave 与覆盖整个 Anchor 高度的激光同时生效；
5. Phase 2 的压力来自选择叠加，不依赖不可读的速度提升。

首轮实现使用 0.65 秒段间间隔；Grounded 优先选择 Ground Pressure，
Attached 优先选择 Anchor Pressure，Airborne 使用可复现随机选择。第一轮之后
两种组合交替出现，保证完整战斗能够覆盖两套 Phase 2 机制。

---

## 8. 弱点暴露规则

### Phase 1

- 完成两次攻击后暴露；
- 默认持续 3 秒；
- 有效重撞后立即关闭；
- 错过后正常进入下一轮，不锁死战斗。

### Phase 2

- 完成一次组合攻击后暴露；
- 默认持续 2.25 秒；
- 有效重撞后立即关闭；
- 暴露期间 Boss 不生成新的攻击。

只有以下条件同时成立才扣除 Boss HP：

1. 玩家处于 Crash；
2. 撞击来自 Anchor；
3. 命中 WeakPoint；
4. WeakPoint 正处于 Exposed；
5. 本次 Crash 尚未造成过弱点伤害。

---

## 9. 状态转换

### Phase 1

```text
Idle
→ SelectingAttack
→ Preparing
→ Attacking
→ Recovery
→ 攻击计数未达到 2：SelectingAttack
→ 攻击计数达到 2：WeakPointExposed
→ SelectingAttack
```

### Phase 2

```text
SelectingCombo
→ Preparing Attack A
→ Attacking A
→ ComboInterval
→ Preparing Attack B
→ Attacking B
→ Recovery
→ WeakPointExposed
→ SelectingCombo
```

实现时 `ComboInterval` 可以是 Recovery 的一种上下文，不必增加公开枚举。

---

## 10. 参数初值

```text
Phase 1
Attacks Before Exposure = 2
Weak Point Exposure = 3.0 s

Phase 2
Weak Point Exposure = 2.25 s
Combo Interval = 0.45 s

Selection
Prevent Immediate Repeat = true
Random Seed = -1
```

具体攻击速度、数量和高度继续由各攻击配置控制。

---

## 11. 日志与测试要求

每次选择记录：

```text
Phase
Player Spatial State
Candidate Weights
Selected Attack / Combo
Previous Attack
Random Seed
```

必须测试：

1. 同种攻击不会连续出现；
2. 地面状态明显提高 Shockwave 和 AimedVolley 选择率；
3. Attached 状态明显提高 Laser 选择率；
4. 玩家在前摇后换位不会导致攻击瞬间改向；
5. Phase 1 每两次攻击暴露一次弱点；
6. Phase 2 每个完整组合后暴露一次弱点；
7. 视觉高度与伤害碰撞高度一致；
8. Boss 或玩家死亡后所有组合立即停止。

---

## 12. 实现顺序

1. 将攻击类型从状态枚举中拆出；
2. 实现通用 `Preparing / Attacking / Recovery`；
3. 实现玩家空间状态检测；
4. 实现 Phase 1 带权重且防重复的攻击选择；
5. 接入两次攻击后暴露弱点；
6. 将高密度扇形保留为 Phase 2 的 `FanBarrage`；
7. 实现 Phase 2 组合序列；
8. 增加确定性日志和回归测试；
9. 最后调整视觉、速度、数量和高度。
