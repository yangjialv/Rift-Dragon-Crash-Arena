# Rift Dragon Crash Arena 声音资产需求

> 用途：交给音频生成 AI 制作第一版 Boss 战声音资产。
> 当前阶段：战斗 Demo。先保证攻击可读性和命中反馈，再补环境细节。
> 引擎：Unreal Engine 5.3。

---

## 1. 总体声音方向

游戏包含两个视觉与声音阶段：

### Phase 1：赛博裂隙

- 实体机械世界正在崩坏；
- 声音以低沉机械、金属冲击、蓝紫电流和空间裂隙为主；
- 保留一定实体重量，不能只有轻薄的电子提示音。

### Phase 2：源码空间

- 世界外壳消失，只剩绿色底层数据；
- 声音以数字故障、二进制脉冲、绿色电磁扫描和空间解构为主；
- 比 Phase 1 更不稳定、更快速，但不能变成持续刺耳的高频噪声。

### 角色声音语言

- Boss：巨型龙的生物重量，加上数字裂隙和能量武器质感；
- Player：柔软、湿润、有弹性的小型液态史莱姆；
- Anchor：结晶能量柱、玻璃碎裂与数字解构的混合；
- 有效弱点撞击必须是整场战斗最强、最明确的正反馈音。

---

## 2. 统一生成要求

将下面这段作为所有音效提示词的共同要求：

```text
Original sound effect for a stylized sci-fi boss battle video game.
Clean isolated sound, no dialogue, no vocals, no background music,
no watermark, no clipping, no excessive reverb, strong readable transient,
professional game-audio quality, suitable for layering and editing.
```

额外要求：

- 单次音效开头不要留静音；
- 单次音效结尾保留短而自然的尾音；
- 循环音必须首尾无缝；
- 不要在音效中加入人声台词；
- 不要把所有声音都做成巨大爆炸；不同操作必须容易区分；
- 生成时尽量保留动态范围，不要把波形压满；
- 如果生成工具支持 Seed，同一组音效使用相同 Seed 或同一提示词骨架；
- 建议导出无损 `WAV`，48 kHz、24 bit；工具不支持时可先导出最高质量 WAV。

空间格式建议：

| 类型 | 建议格式 |
|---|---|
| Boss、Player、Anchor、攻击音效 | Mono，导入 UE 后由空间化系统定位 |
| BGM、环境氛围、胜负提示 | Stereo |
| 循环声音 | 单独提供无缝循环版本 |

---

## 3. 第一批必须生成的资产

这一批用于完成一局可听懂的 Boss 战。优先按照表格顺序生成。

### 3.1 Player 与撞击反馈

#### `SFX_Player_Damaged`

- 时机：玩家被弹幕、冲击波或龙焰扣血；
- 类型：单次，建议 Mono；
- 长度：0.2～0.5 秒；
- 要求：柔软史莱姆受冲击的湿润弹性声，加一层短促数字损伤；不能像人类惨叫。

```text
A short readable damage hit for a small liquid slime character,
soft wet elastic impact combined with a brief digital glitch spark,
responsive and punchy, not disgusting, not a human voice, 0.35 seconds.
```

#### `SFX_Player_Dash`

- 时机：鼠标右键按下，玩家立即冲刺；
- 类型：单次，建议 Mono；
- 长度：0.2～0.45 秒；
- 要求：瞬间释放、快速掠过，带液态拉伸感；不能有蓄力前奏。

```text
An immediate fast dash launch for a liquid slime hero,
sharp elastic release, compact wet whoosh and clean sci-fi speed trail,
instant response with no charge-up, energetic, 0.3 seconds.
```

#### `SFX_Player_JumpRelease`

- 时机：拖拽瞄准后释放左键，史莱姆弹射；
- 类型：单次，建议 Mono；
- 长度：0.3～0.6 秒；
- 要求：被压缩的水滴恢复形状并弹出的声音，比 Dash 更圆、更有弹性。

```text
A compressed liquid slime springing into a long jump,
rounded elastic release, soft watery tension snap and airy upward whoosh,
playful but powerful, 0.45 seconds.
```

#### `SFX_BossBody_Rebound`

- 时机：玩家撞到 Boss 身体但没有造成弱点伤害，并被直线弹回；
- 类型：单次，建议 Mono；
- 长度：0.25～0.55 秒；
- 要求：坚硬鳞片与弹性史莱姆相撞，明确表达“撞上了，但攻击无效”。

```text
A small elastic slime slamming into the armored scales of a gigantic dragon
and being forcefully repelled, hard low metallic scale impact plus rubbery bounce,
clearly blocked rather than successful, 0.4 seconds.
```

#### `SFX_WeakPoint_Hit`

- 时机：从 Anchor 重撞暴露弱点并真正扣除 Boss HP；
- 类型：单次，可使用 Mono 主体加 Stereo 强化层；
- 长度：0.45～0.9 秒；
- 要求：全场最强的正反馈；包含核心破裂、能量爆发和短促低频冲击，不能只是一声金属碰撞。

```text
A decisive critical weak-point hit on a colossal cyber dragon,
powerful crystal-core fracture, concentrated energy burst, deep cinematic impact,
bright digital crack and satisfying success accent,
the strongest positive combat feedback in the battle, 0.7 seconds.
```

---

### 3.2 冲击波

#### `SFX_Shockwave_Warning`

- 时机：Boss 咆哮、地面圆环预警出现；
- 类型：单次；
- 长度：约 1.0～1.4 秒；
- 要求：低频能量向中心聚集，末尾自然导向释放，不提前出现爆炸峰值。

```text
A giant cyber dragon charging a circular ground shockwave,
deep sub-bass pressure building, restrained mechanical resonance,
digital energy gathering toward one point, clear danger anticipation,
no explosion until the end, 1.2 seconds.
```

#### `SFX_Shockwave_Release`

- 时机：火焰圆环开始向外扩张；
- 类型：单次；
- 长度：0.7～1.3 秒；
- 要求：沉重但清晰的环形能量爆发，重点是“向外扩散”，不是普通炸弹。

```text
A massive circular ground shockwave released by a dragon,
heavy low-frequency energy pulse spreading rapidly outward across an arena,
fiery edge, digital distortion and strong initial impact,
not a conventional bomb explosion, 1 second.
```

---

### 3.3 龙焰激光

龙焰必须拆成预警、点火、持续和结束，不能只使用一个声音。

#### `SFX_Laser_Warning`

- 时机：地面警戒区域出现，Boss 进入瞄准；
- 类型：单次或可循环版本；
- 长度：1.0～1.5 秒；
- 要求：浅淡但明确，逐渐升高的能量音，不要比正式喷射更吵。

```text
A restrained warning charge for a cyber dragon breath laser,
thin electrical resonance, rising energy pressure and subtle targeting pulse,
clearly dangerous but quieter than the actual attack, 1.3 seconds.
```

#### `SFX_Laser_Ignite`

- 时机：停顿结束，龙焰正式从嘴部喷出；
- 类型：单次；
- 长度：0.25～0.6 秒；
- 要求：必须有极清楚的点火瞬态，用来标记伤害正式开始。

```text
A cyber dragon breath weapon igniting instantly,
violent compact fire burst, plasma crack and sharp readable attack onset,
powerful but very short, 0.4 seconds.
```

#### `SFX_Laser_Loop`

- 时机：龙焰持续喷射和横扫期间；
- 类型：无缝循环；
- 长度：1.5～3 秒；
- 要求：持续高能火焰与等离子流动，不能包含新的起爆声，以免每次循环都产生节奏跳变。

```text
A seamless looping cyber dragon breath beam,
continuous roaring flame, plasma stream and controlled digital turbulence,
stable energy with subtle internal movement,
no ignition transient, no ending impact, perfect seamless loop, 2 seconds.
```

#### `SFX_Laser_End`

- 时机：龙焰关闭，Boss 进入恢复；
- 类型：单次；
- 长度：0.3～0.7 秒；
- 要求：能量快速衰减并带少量余火，明确告诉玩家危险已经结束。

```text
A dragon plasma breath shutting down,
fast energy decay, brief residual flame and electrical tail,
clear end-of-danger cue, compact, 0.5 seconds.
```

---

### 3.4 弹幕

第一版八种弹幕共用同一套基础声音。不要立即为八种模式分别制作整套资产。

#### `SFX_Barrage_Charge`

- 时机：Boss 播放 Attack 前摇；
- 类型：单次；
- 长度：0.6～1.0 秒；
- 要求：一连串能量弹即将形成，与激光蓄力和冲击波低频预警明显不同。

```text
A giant cyber dragon preparing a volley of energy projectiles,
several compact plasma motes rapidly forming around one firing point,
rhythmic digital charge, readable anticipation, no projectile release,
0.8 seconds.
```

#### `SFX_Barrage_Shot_01`、`02`、`03`

- 时机：每颗或每组弹丸发射；
- 类型：三个相近的单次变体；
- 长度：0.1～0.3 秒；
- 要求：短促、轻量，密集播放时不能刺耳，也不能盖住其他攻击提示。

```text
Three subtle variations of a compact cyber dragon energy projectile launch,
short plasma pop, light digital spark and quick air displacement,
clean and non-fatiguing during rapid repeated fire,
each variation 0.2 seconds, no long reverb tail.
```

#### `SFX_Barrage_PlayerHit`

- 时机：弹丸命中玩家；
- 类型：单次；
- 长度：0.2～0.45 秒；
- 要求：能量弹破裂加史莱姆受击，比通用 Player Damage 更偏能量质感。

```text
A small plasma projectile bursting against a liquid slime character,
compact energy crack, soft wet impact and brief digital damage spark,
clear but not oversized, 0.35 seconds.
```

---

### 3.5 开场、转阶段与结算

#### `SFX_Boss_Takeoff`

- 时机：龙从 Ground Idle 切换到起飞动画；
- 类型：单次；
- 长度：1.5～3 秒；
- 要求：巨大翅膀、沉重离地和空气压力，不要包含地面碎裂，方便分别同步。

```text
A colossal cyber dragon launching into flight from a platform,
huge wing beat, heavy body lift and powerful downward air pressure,
majestic and threatening, no ground-breaking sound, 2.5 seconds.
```

#### `SFX_CyberCenter_Fracture`

- 时机：龙起飞时 Cyber 中心盘碎裂；
- 类型：单次；
- 长度：1～2 秒；
- 要求：大型机械石材断裂，并带数据故障层；不能像普通玻璃杯破碎。

```text
A large cyber arena platform violently fracturing beneath a dragon,
heavy stone-metal breakup, multiple massive chunks, deep structural crack,
layered with digital corruption glitches, 1.6 seconds.
```

#### `SFX_PhaseTransition_Start`

- 时机：Phase 2 扩散球从世界中心出现；
- 类型：单次；
- 长度：0.5～1 秒；
- 要求：空间被打开，不使用普通爆炸。

```text
A spherical source-code world transition activating at the center of an arena,
bright green electromagnetic pulse, space opening and digital reality inversion,
precise sci-fi onset, not an explosion, 0.7 seconds.
```

#### `SFX_PhaseTransition_Loop`

- 时机：球体持续向外扩张；
- 类型：无缝循环；
- 长度：2～4 秒；
- 要求：持续扫描、字符流与空间解构，频率不能过于尖锐。

```text
A seamless looping spherical reality-conversion field expanding through an arena,
green electromagnetic scanning, flowing binary data and spatial deconstruction,
wide evolving texture, controlled high frequencies, perfect seamless loop,
3 seconds.
```

#### `SFX_PhaseTransition_End`

- 时机：Code 世界完全覆盖场地；
- 类型：单次；
- 长度：0.5～1.2 秒；
- 要求：扫描完成并稳定下来，作为 Phase 2 开始标志。

```text
A digital world conversion completing and locking into place,
descending electromagnetic resolve, deep system confirmation pulse
and clean green data shimmer, decisive but not explosive, 0.8 seconds.
```

#### `SFX_Boss_Death`

- 时机：Boss HP 归零并播放 Death Montage；
- 类型：单次；
- 长度：2～5 秒；
- 要求：巨龙失去力量、数字核心崩溃；不要在文件中加入音乐，便于和 BGM 混合。

```text
A colossal cyber dragon dying as its digital core collapses,
deep creature power loss, failing mechanical energy, cascading code corruption
and a final heavy collapse accent, no music, no dialogue, 4 seconds.
```

#### `SFX_Victory`

- 时机：Victory 结果出现；
- 类型：Stereo 单次；
- 长度：1～3 秒；
- 要求：短促、有成就感，但不做成长篇胜利音乐。

```text
A short victorious stinger for defeating a cyber dragon boss,
bright digital resolution, confident energy rise and satisfying final accent,
instrumental, no vocals, 2 seconds.
```

#### `SFX_Defeat`

- 时机：Player HP 归零；
- 类型：Stereo 单次；
- 长度：1～3 秒；
- 要求：能量下沉、系统失效，但不要做恐怖或悲惨的人声效果。

```text
A short defeat stinger for a stylized sci-fi action game,
energy collapsing downward, muted digital system failure and restrained final hit,
instrumental, no vocals, not horror, 2 seconds.
```

---

## 4. 第一批 BGM

### `BGM_Boss_Phase1`

- 类型：Stereo 无缝循环；
- 长度：60～90 秒；
- 建议速度：128～140 BPM；
- 目的：有紧迫感，但需要给攻击预警和音效留下空间；
- 禁止：人声、歌词、持续全频率轰炸、过多电影式环境铺垫。

```text
Original instrumental seamless-loop boss battle music for a cyber rift arena,
132 BPM, dark industrial percussion, heavy mechanical pulse,
cyan and magenta electronic energy, colossal dragon presence,
clear rhythmic gaps for gameplay sound effects,
tense but not constantly maximal, no vocals, 75 seconds.
```

### `BGM_Boss_Phase2`

- 类型：Stereo 无缝循环；
- 长度：60～90 秒；
- 建议速度：140～155 BPM；
- 目的：延续 Phase 1 的节奏身份，但加入绿色源码空间、故障和失控感；
- 最好与 Phase 1 使用相同生成工程、相同 Seed 或同一主题描述。

```text
Original instrumental seamless-loop second-phase boss battle music,
an intensified evolution of a cyber rift boss theme,
148 BPM, aggressive digital glitches, flowing binary-code arpeggios,
deep mechanical percussion and unstable green electromagnetic energy,
high pressure with clear space for combat cues, no vocals, 75 seconds.
```

如果 AI 无法生成真正无缝循环，至少要求：

1. 开头不要使用只出现一次的长渐入；
2. 结尾不要使用终止和弦；
3. 保留节拍稳定的中段，后续可以人工裁切循环。

---

## 5. 第二批：Anchor 声音

第一批战斗音效通过后再生成。

| 文件名 | 时机 | 核心听感 | 长度 |
|---|---|---|---:|
| `SFX_Anchor_Emerge` | Anchor 从地面冒出 | 结晶升起、数据组装 | 0.8～1.5 秒 |
| `SFX_Anchor_Attach` | 玩家成功吸附 | 柔软吸附、能量连接 | 0.2～0.5 秒 |
| `SFX_Anchor_OverloadLoop` | 过载逐渐升高 | 可循环脉冲，紧迫但不刺耳 | 1～2 秒循环 |
| `SFX_Anchor_Launch` | 玩家从 Anchor 弹出 | 强弹性释放与能量断开 | 0.3～0.6 秒 |
| `SFX_Anchor_Shatter` | Anchor 自动碎裂 | 能量晶体和数字碎裂 | 0.8～1.5 秒 |

统一提示词骨架：

```text
Sound for a floating crystalline digital anchor in a sci-fi arena,
combining solid energy crystal, clean glass-like detail and source-code glitches,
readable game sound, no music, no voice, no excessive reverb.
```

`SFX_Anchor_OverloadLoop` 必须满足：

- 可以无缝循环；
- 本身不要包含最终爆炸；
- 后续由 UE 根据 `OverloadAlpha` 提高音量与音调；
- 碎裂时停止循环，改播 `SFX_Anchor_Shatter`。

---

## 6. 第三批：环境与细节

这些内容不阻塞战斗 Demo：

| 文件名 | 用途 |
|---|---|
| `AMB_CyberRift` | Phase 1 低音量机械裂隙环境循环 |
| `AMB_SourceCodeVoid` | Phase 2 数据雨与虚空环境循环 |
| `SFX_Player_Move_01/02/03` | 史莱姆普通移动的轻微湿润蠕动 |
| `SFX_Player_AttachedMove_01/02` | 沿 Anchor 表面移动 |
| `SFX_Barrage_NearMiss` | 弹丸高速掠过玩家附近 |
| `SFX_UI_Hover` | 按钮悬停 |
| `SFX_UI_Confirm` | Restart 等按钮确认 |

环境氛围必须低调，不能掩盖激光、冲击波和弹幕预警。

---

## 7. 交付与筛选规则

每个重要音效建议让 AI 生成 3～5 个候选，但项目中只保留筛选后的版本。

筛选标准：

1. 不看画面时，也能区分冲击波、弹幕和激光；
2. `Laser_Ignite` 和 `Shockwave_Release` 的攻击起点足够清楚；
3. 高频不刺耳，密集弹幕连续播放不会疲劳；
4. `BossBody_Rebound` 明确表达无效撞击；
5. `WeakPoint_Hit` 明显强于普通受击；
6. 循环资产没有接缝、起爆音或明显节奏跳变；
7. 不含可识别的现有影视、游戏旋律或受版权保护采样；
8. 确认所用 AI 服务允许将生成结果用于当前项目和计划中的发布方式。

建议保留原始生成信息：

```text
文件名
生成工具
生成日期
Prompt
Seed（如果有）
原始文件
最终裁切文件
授权或许可记录
```

---

## 8. UE 接入顺序

声音生成完毕后，按照以下顺序接入和测试：

1. Player Damage、Boss Body Rebound、Weak Point Hit；
2. Shockwave Warning / Release；
3. Laser Warning / Ignite / Loop / End；
4. Barrage Charge / Shot / Player Hit；
5. Boss Takeoff、中心盘碎裂和阶段转换；
6. Boss Death、Victory、Defeat；
7. Phase 1 / Phase 2 BGM；
8. Anchor 与环境细节。

接入时需要特别处理：

- 密集弹幕发射声的并发数量限制；
- 循环声音在攻击结束、Boss 死亡和 Restart 时可靠停止；
- 3D 衰减距离，保证竞技场边缘仍能听清 Boss 的攻击提示；
- BGM、环境、Boss 攻击和 Player 反馈使用不同 Sound Class；
- 动画 Notify 最终负责精确触发 Projectile、Shockwave 和 Laser 的关键音效。
