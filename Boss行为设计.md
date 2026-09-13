# Rift Dragon Crash Arena：Boss 行为设计

> 版本：v0.2
> 更新日期：2026-09-12
> 目标：确定 Demo Boss 的两阶段生命值、攻击编排、弹幕池和弱点窗口。
> 实现原则：使用 C++ 分层状态机，不引入 Behavior Tree。

---

## 1. 设计目标

Boss 的职责不是随机播放技能，而是逐步教会玩家理解空间规则，再在第二阶段
组合这些规则。一次完整交互形成：

```text
读取 Boss 前摇
→ 判断当前空间是否安全
→ 使用走位、冲刺、跳跃或更换 Anchor 应对
→ 完成一组攻击
→ 利用弱点窗口从 Anchor 重撞
```

行为系统必须满足：

1. Phase 1 使用基础且可预测的攻击教学；
2. Phase 2 使用组合与复杂轨迹提高压力；
3. 每次攻击都必须存在可以理解和执行的解法；
4. 弹幕的随机性只发生在预先规定的槽位中；
5. 相同弹幕模式不能无意义地连续重复；
6. 视觉、伤害判定和动画释放时机保持一致；
7. Boss 或 Player 死亡后立即停止全部攻击；
8. 所有选择和状态转换可记录、可复现、可测试。

---

## 2. 为什么当前不使用 Behavior Tree

Boss 固定在圆形竞技场中心，不需要寻路、巡逻或搜索目标。当前难点是攻击、
恢复、场景切换和弱点暴露之间的严格时序，而不是移动决策。

因此采用：

```text
C++ 通用状态机
＋
两阶段攻击时间表
＋
槽位内受约束随机
```

以后出现 Boss 自主移动、召唤物协同、多敌人共享行为或大量可视化分支时，
再评估 Behavior Tree。

---

## 3. 生命值与阶段划分

### 3.1 默认生命值

| 对象 | 默认生命值 | 单次伤害 | 说明 |
|---|---:|---:|---|
| Player | 5 | 1 | 允许四次失误，第五次失败 |
| Boss | 5 | 1 | 每次有效弱点重撞扣 1 HP |

玩家受击无敌时间保持 1 秒，防止同一组重叠攻击在一帧内连续扣血。

### 3.2 Boss 阶段

| 阶段 | Boss HP | 玩家需要完成的有效重撞 | 目的 |
|---|---:|---:|---|
| Phase 1 | 5～4 | 2 次 | 教学单项攻击与基础躲避 |
| Phase 2 | 3～1 | 3 次 | 使用双攻击和复杂弹幕组合 |
| Dead | 0 | — | 停止攻击，Player 回到安全地面后进入 Victory |

`Phase 2 Start Hit Points` 默认是 3。Boss 在弱点命中后 HP 变为 3 的同一时刻
进入 Phase 2，使竞技场空间扩散能够立即响应。

如果以后修改 Boss 总生命值，必须同步检查阶段阈值，保证 Phase 1 和 Phase 2
都至少包含一次有效弱点循环。

---

## 4. 分层行为模型

```text
Encounter Result
└── Combat Phase
    └── Phase Round
        └── Action State
            └── Current Attack / Barrage Pattern
```

### 4.1 Encounter Result

```text
Playing
Victory
Defeat
```

任一方死亡后停止 Boss 状态机，并清理冲击波、激光和仍在飞行的弹幕。

### 4.2 Action State

```text
Idle
SelectingAttack
Preparing
Attacking
Recovery
WeakPointExposed
Dead
```

状态只描述当前行为阶段。攻击内容由 `Current Attack` 和
`Barrage Pattern` 决定。

### 4.3 Current Attack

```text
None
Shockwave
AimedVolley
FanBarrage
SweepLaser
```

`AimedVolley` 和 `FanBarrage` 共用一个弹丸 Actor。两者的区别是调度方式和
弹幕宽度，而不是使用不同的子弹类。

---

## 5. 玩家空间状态

攻击选择时记录一次玩家状态：

| 状态 | 判定 |
|---|---|
| Grounded | 玩家在地面且未附着 |
| Airborne | 玩家正在跳跃或冲撞飞行 |
| Attached | 玩家附着在 Anchor 表面 |

当前正式编排以阶段轮次为主，空间状态主要用于日志、瞄准和以后扩展。
Boss 不能因为玩家在攻击前摇中改变状态，就瞬间更换已经选定的攻击。

---

## 6. 八种弹幕的阶段归属

### 6.1 Phase 1 基础弹幕

| 弹幕 | 用途 | 基础解法 |
|---|---|---|
| Legacy Aimed Volley | 教学精确连射 | 横向移动或右键冲刺 |
| Predictive Triple Volley | 教学预判射击 | 改变原移动方向 |
| Gap Barrage Wall | 教学寻找安全缺口 | 提前移动到缺口 |

Phase 1 不使用曲线、追踪、旋转缺口、双螺旋和密集扇形，避免玩家尚未理解
基础控制时同时处理过多轨迹规则。

### 6.2 Phase 2 高级弹幕

| 弹幕 | 用途 | 基础解法 |
|---|---|---|
| Curved Spin Volley | 改变直线弹道预期 | 观察弯曲方向后反向移动 |
| Limited Homing Volley | 迫使玩家持续换位 | 诱导后冲刺脱离追踪 |
| Rotating Gap Barrage | 让安全缺口随波次移动 | 跟随缺口转移 |
| Double Spiral Barrage | 最终视觉与空间压力 | 读取旋臂之间的通路 |
| Legacy Dense Fan | 高密度正面封锁 | 提前离开扇区或穿过边缘 |

这些模式仍共用 `Barrage Projectile Class`。第一版先验证运动和判定，之后再用
颜色、拖尾和声音区分轨迹类型。

---

## 7. Phase 1 正式编排

Phase 1 包含两个弱点循环，每个循环由两次独立攻击组成。

### 7.1 第一轮：基础瞄准与跳跃

```text
Legacy Aimed Volley
→ Recovery
→ Single Shockwave
→ WeakPointExposed
→ 有效重撞：Boss 5 HP → 4 HP
```

目的：先教学横向闪避，再教学跳跃或使用 Anchor 躲避地面攻击。

### 7.2 第二轮：缺口/预判与激光

```text
Predictive Triple Volley 或 Gap Barrage Wall
→ Recovery
→ Sweep Laser
→ WeakPointExposed
→ 有效重撞：Boss 4 HP → 3 HP
→ Arena Phase Transition
```

`Predictive Triple Volley` 与 `Gap Barrage Wall` 二选一。若玩家错过弱点窗口并
重新进入这一轮，优先换成另一个模式，避免原样重复。

如果必要的 Projectile 或 Laser 类没有配置，状态机回退到仍可执行的基础攻击，
不能卡死在 `SelectingAttack`。

---

## 8. Phase 2 正式编排

Phase 2 改为随机轮次制。每轮抽取三种不同的具体攻击，第三次攻击完成后立即暴露
弱点。弹幕的不同 Pattern 分别视为不同攻击，因此一轮可以出现两种弹幕，但同一个
Pattern 不会重复。

当前 Phase 2 攻击池：

- Enhanced Double Shockwave；
- Sweep Laser；
- Curved Spin Volley；
- Limited Homing Volley；
- Rotating Gap Barrage；
- Double Spiral Barrage；
- Legacy Dense Fan。

每次选择先按大类权重抽取：`Barrage = 0.5`、`Enhanced Shockwave = 0.3`、
`Sweep Laser = 0.2`。抽中 Barrage 后，再在五种 Phase 2 弹幕 Pattern 中等概率
选择一种。这样五种弹幕共同占 50%，不会分别与 Shockwave、Laser 竞争而稀释它们。

每轮从可用池中抽取三个不重复项目。Shockwave 和 Laser 在一轮内各最多出现一次；
Barrage 可以出现多次，但每次必须使用尚未出现的 Pattern。某个类别没有可用项目时，
其余类别权重自动重新归一化。轮次示例：

```text
Limited Homing Volley
→ Phase 2 Inter Attack Delay
→ Enhanced Double Shockwave
→ Phase 2 Inter Attack Delay
→ Sweep Laser
→ 立即 WeakPointExposed
```

### 8.1 Enhanced Double Shockwave

二重 Shockwave 在随机池中只算一种、一次攻击行为：只播放一次 Roar、一次前摇和
一次 Warning。Pulse 1 开始后默认 `0.28` 秒释放 Pulse 2，此时第一道波仍在扩散；
两道波结束以后才算该攻击完成。

两道波使用相同的初始半径、最终半径、扩散速度、视觉宽度、判定宽度和判定高度，
分别拥有命中资格，但仍服从 Player 的全局受击无敌时间。

### 8.2 攻击结束与眩晕衔接

`Phase 2 Attacks Before Stun = 3`。前两次攻击之间保留
`Phase 2 Inter Attack Delay`；第三次攻击的 Active 阶段一结束，状态机直接从
`Attacking` 进入 `WeakPointExposed`，不经过 `Recovery`，也不等待 Inter Attack
Delay。激光最后收招时，Boss 高度恢复与眩晕并行，不能推迟弱点窗口。

### 8.3 错过弱点窗口

玩家错过弱点窗口时：

1. Boss HP 不变；
2. 战斗不会锁死；
3. 清空本轮使用记录并重新随机抽取三种攻击；
4. 新一轮内部仍保证攻击方式不重复；
5. 不提前降低 Boss HP。

---

## 9. 攻击职责

### 9.1 Shockwave

- 固定使用 `Shockwave World Anchor` 作为世界空间圆心；
- 只伤害判定高度内的玩家；
- 不追踪玩家，不影响 Anchor；
- Phase 1 单次释放；Phase 2 抽到该攻击时连续释放两道波；
- 火焰 Torus 的半径、宽度和高度与伤害判定对应。

### 9.2 Barrage

- 从 `ProjectileOrigin` 生成；
- 从龙嘴高度下降到玩家巡航高度后继续贴地飞行；
- 只有真实碰撞才伤害 Player 或增加 Anchor 过载；
- 弹幕生成后不能因为玩家实时位置而无限改写解法；
- 只有 Limited Homing 使用受限时间和角度的追踪。

### 9.3 Sweep Laser

- Boss 在预警阶段跟随 Player 调整朝向；
- 预警结束后进入停顿；
- 停顿结束时决定直射或固定方向横扫；
- 正式喷射后不再持续追踪 Player；
- `LaserEffect` 和 `DamageVolume` 使用同一 Beam Root 尺寸；
- Boss 在预警时下降，恢复时回到正常飞行高度。

---

## 10. 弱点规则

### Phase 1

- 两次攻击完成后暴露；
- 基础暴露时间 `3.0` 秒；
- 乘以当前 `Weak Point Stun Duration Multiplier = 2.0` 后，实际为 6 秒。

### Phase 2

- 每轮三次不重复攻击完成后暴露；
- 第三次攻击 Active 结束后立即暴露，不经过 Recovery；
- 基础暴露时间 `2.25` 秒；
- 乘以当前倍率后，实际为 4.5 秒。

有效伤害必须同时满足：

1. Player 正处于 Crash；
2. Crash 从 Anchor 发起；
3. 命中 Weak Point；
4. Weak Point 正在暴露；
5. 本次 Crash 尚未造成过弱点伤害。

有效命中固定扣 1 HP。撞 Boss 身体或受保护弱点只触发反弹，不扣 HP。
一次有效命中会立即消费并关闭当前弱点窗口，随后先播放 Boss 受击表现，再进入
下一轮；同一个暴露窗口不能通过连续使用多个 Anchor 扣除多点 HP。

---

## 11. 核心参数默认值

```text
Player Maximum Health = 5
Boss Maximum Hit Points = 5
Damage Per Qualified Crash = 1
Phase 2 Start Hit Points = 3

Phase 1 Attacks Before Exposure = 2
Phase 1 Base Weak Point Exposure = 3.0 s

Phase 2 Attacks Before Stun = 3
Phase 2 Inter Attack Delay = 0.65 s
Phase 2 Shockwave Pulse Delay = 0.28 s
Phase 2 Base Weak Point Exposure = 2.25 s
Phase 2 Barrage Weight = 0.5
Phase 2 Shockwave Weight = 0.3
Phase 2 Laser Weight = 0.2

Player Movement Speed Multiplier = 1.5
Barrage Speed Multiplier = 1.5
Homing Count Multiplier = 2
Homing Size Multiplier = 4.0
Weak Point Impact Volume Multiplier = 4.0

Weak Point Stun Duration Multiplier = 2.0
Player Invulnerability Duration = 1.0 s
Attack Selection Random Seed = -1
```

玩家左键跳跃/弹射的计时冷却在落到 `Arena Floor Collision` 顶面时立即清空；
右键地面冲刺继续使用自己的冷却，不适用落地刷新。

阶段视觉规则：Anchor 在 Cyber 区域使用蓝紫材质，在扩散球经过后替换为 Code
绿色材质；Phase 1 生成的弹幕使用蓝色材质，Phase 2 生成的弹幕使用红色材质。
两者都直接切换两套材质，不要求材质提供额外颜色参数。

若现有 Blueprint 保存过旧默认值，需要在组件 Details 中确认 Player HP、Boss HP
和阶段阈值实际显示为 `5 / 5 / 3`。C++ 默认值改变不应被误认为一定覆盖所有已保存
Blueprint 实例配置。

### 11.1 战斗音频节奏

- 有效弱点命中必须强于 Anchor 碎裂：先出现清楚的重击与厚玻璃破裂，随后由 Boss
  位置播放受伤痛吼；Boss HP 归零时不播普通痛吼，改为完整死亡吼叫；
- 转阶段的 Start、持续 Loop 和 End 是一个完整句子。Start 建立巨大空间压力，End
  落在 Phase 2 的重拍，不能只是一个轻微电子提示音；
- Phase 2 强化 Shockwave 只播放一次 Warning；两道 Pulse 各在真实扩散开始时播放
  一次 Release，第二声可略高音高，使玩家听出“第二道波”而不是攻击重开；
- 全程只使用一首 `BGM_Boss_Main`。开场低音量淡入，起飞后进入正常音量，Phase 2
  和低血量时平滑增强；不能切歌、重播或改变速度，结算时在约 2.5 秒内淡出。

---

## 12. Debug 规则

### Barrage Debug

```text
Debug Force Barrage = true
Debug Barrage Pattern = 要测试的模式
```

启用后持续重复指定弹幕，不推进正式阶段轮次。测试完成必须关闭。

### Laser Debug

`Debug Force Sweep Laser` 只用于单独调整激光。测试完成必须关闭。

### Arena Phase Debug

正式流程中 `Debug Auto Start Transition On Begin Play` 必须关闭。场景扩散只在
Boss HP 从 4 降到 3、正式进入 Phase 2 时触发。

---

## 13. 日志要求

Phase 1 选择日志至少记录 Boss HP、Round、Step、Player Spatial State、
Current Attack、Barrage Pattern、Previous Attack 和 Random Seed。

Phase 2 选择日志至少记录当前轮次 Step、具体攻击 Key、Current Attack、
Barrage Pattern、本轮已用数量、Locked Target 和 Random Seed。

---

## 14. 实机验收

### 生命与阶段

- [ ] Player 开局是 5 HP；
- [ ] Boss 开局是 5 HP；
- [ ] 前两次有效重撞发生在 Phase 1；
- [ ] Boss 从 4 HP 降到 3 HP 时开始场景扩散；
- [ ] Phase 2 需要三次有效重撞才会死亡；
- [ ] Boss 0 HP 后立即停止攻击，Player 回到环形地面后再显示 Victory。

### Phase 1

- [ ] 第一轮顺序是 Legacy Aimed Volley → Single Shockwave；
- [ ] 第二轮第一段只会选择 Predictive 或 Gap Wall；
- [ ] 第二轮第二段是 Sweep Laser；
- [ ] Phase 1 不会出现曲线、追踪、旋转缺口、双螺旋或密集扇形；
- [ ] 每两次攻击后暴露一次弱点。

### Phase 2

- [ ] 每轮正好完成三次攻击后进入 WeakPointExposed；
- [ ] 第一层类别概率约为 Barrage 50% / Shockwave 30% / Laser 20%；
- [ ] 抽中 Barrage 后才进行第二层 Pattern 随机；
- [ ] 同一轮不会重复 Shockwave、Laser 或相同弹幕 Pattern；
- [ ] Enhanced Shockwave 在同一次攻击状态中连续扩散两道波；
- [ ] 两道波只共用一次预警，但分别拥有独立伤害资格；
- [ ] 第二道波按 `Phase 2 Shockwave Pulse Delay` 提前释放，与第一道波同时存在；
- [ ] 第三次攻击结束后直接眩晕，不插入 Recovery；
- [ ] 追踪弹数量为原配置 2 倍，显示和碰撞尺寸为原配置 4 倍；
- [ ] 所有弹幕实际移动速度为原配置 1.5 倍；
- [ ] 错过弱点不会让战斗卡死。

### 安全与收尾

- [ ] Debug 开关关闭后不会破坏正式编排；
- [ ] Player 或 Boss 死亡时清理全部攻击 Actor；
- [ ] Restart 后生命值、阶段、场景和攻击轮次完全重置；
- [ ] 一局目标时长保持在约 60～120 秒；
- [ ] 所有攻击都有至少一种稳定、可重复的躲避方法。
