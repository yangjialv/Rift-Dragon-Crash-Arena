# Rift Dragon Crash Arena 声音资产需求

> 用途：交给音频生成 AI 制作第一版 Boss 战声音资产。
> 当前阶段：战斗 Demo。先保证攻击可读性和命中反馈，再补环境细节。
> 引擎：Unreal Engine 5.3。
> 声音方向版本：v0.4（2026-09-13），真实物理层次、节奏编排与标志性重击。

本地批量生成工具与密钥配置见 [音频生成配置与使用](audio_generation_setup.md)。

---

## 1. 声音总纲：柔软、厚重、可读的电子节拍

本作不是纯写实巨龙，也不是高频堆叠的传统赛博朋克。声音应当像画面一样具有鲜明轮廓：**软质流体的弹性节拍、巨龙与竞技场的真实物理重量、数字空间的受控脉冲，以及关键事件具有电影级分层细节**。

“有节奏感”不等于给每个音效加音乐。节奏应主要来自玩家操作、Boss 前摇、攻击释放和命中的触发顺序：

```text
前摇建立拍点 → 出手落在重拍 → 危险持续保持脉冲 → 命中或闪避给出回拍
```

重复声音要短、圆、清楚；标志性声音必须保留准备、主体和衰减。多个声音连续播放后，应当自然组成节奏，而不是把所有内容压缩成相同长度。

### 1.1 核心听感

- 轮廓清晰且具有真实材质：重要事件必须听到空气、身体、结构、碎片与空间尾音；
- 主体集中在中低频：Player 温软，Boss 厚重；Anchor 与弱点允许真实玻璃/晶体高频，但必须由低频撞击托住；
- 高频只负责少量轮廓，避免持续电流、尖啸、玻璃脆裂和锐利数字火花；
- 一种动作对应一种节奏手势，玩家只听声音也能判断即将发生什么；
- 强弱有层级：普通移动 < 弹丸发射 < 玩家受击 < Boss 攻击释放 < 弱点有效命中；
- 留出空隙。短音效结束后要让画面、动作和 BGM 有呼吸空间。

### 1.2 两阶段声音身份

| 阶段 | 节奏 | 材质 | 听感 |
|---|---|---|---|
| Phase 1：赛博裂隙 | 稳定、厚重、可预测 | 软化机械、低沉金属、蓝紫能量 | 像实体机械世界按照清楚的拍点运转 |
| Phase 2：源码空间 | 更密、更切分，但仍可读 | 带限数据脉冲、滤波故障、绿色扫描 | 像同一节奏被底层代码重新编排 |

Phase 2 必须继承 Phase 1 的节奏身份，不能突然变成另一套音乐或持续故障噪声。变化来自更快的切分、更活跃的左右运动和更短的呼吸间隔。

### 1.3 对象声音语言

| 对象 | 主体材质 | 节奏身份 | 禁止方向 |
|---|---|---|---|
| Player | 水滴、软膜、橡胶弹性 | 轻巧的“压缩—释放” | 恶心黏液、人声、尖锐气流 |
| Boss | 生物胸腔、厚重翼压、低沉机甲 | 低频重拍和长呼吸 | 每次动作都是巨大爆炸 |
| Anchor | 能量晶体、厚玻璃、实体碎块、数据连接 | 稳定脉冲逐渐加速 | 只有轻薄玻璃声、持续高频鸣叫 |
| Cyber 世界 | 中低频机械循环 | 稳定四拍与轻微切分 | 空洞电影轰鸣铺满全场 |
| Code 世界 | 带限数字颗粒、扫描脉冲 | 更快的八分/十六分切分 | 刺耳 bitcrush、白噪声墙 |

有效弱点命中是整场最强正反馈，使用“高速撞击 + 核心断裂 + 厚玻璃爆碎 + 低频能量爆发 + 碎片尾音”的完整层次。玻璃高频用于表现爽感，但不能缺少身体重量。

---

## 2. 节奏与生成规范

### 2.1 节奏基准

- Phase 1 BGM：`132 BPM`，四分音符约 `455 ms`，八分音符约 `227 ms`；
- Phase 2 BGM：`148 BPM`，四分音符约 `405 ms`，八分音符约 `203 ms`；
- 短操作音效控制在一个八分音符附近；关键命中可以形成“两段式”手势，但不要在单个文件里生成多次攻击；
- 弹幕节奏必须由 UE 的实际发射事件形成。每颗弹丸只播放一次短音，禁止在一颗弹丸音效中烘焙一串连射；
- Warning、Release、Hit 是三个不同节奏位置，必须拆成不同资产；
- 当前攻击逻辑尚未强制量化到 BPM。音效应提供节奏手势，最终由动画 Notify 和攻击调度对齐画面。

长度按照功能分级，不再把所有声音限制在一秒以内：

| 层级 | 参考长度 | 适用对象 |
|---|---:|---|
| 高频重复微音效 | 0.4～0.7 秒 | 弹丸、移动、UI；主体短但保留自然尾音 |
| 普通动作与碰撞 | 0.7～1.5 秒 | Dash、Jump、受击、反弹 |
| 关键攻击与碎裂 | 1.5～3 秒 | WeakPoint、Shockwave、Anchor、中心盘 |
| 标志性演出 | 3～6 秒 | Boss 起飞、咆哮、死亡、转阶段 |
| 持续状态 | 2～4 秒循环 | Laser、Anchor 过载、场景转换 |

这里是生成长度，不代表必须等待声音播完才能推进逻辑。攻击判定仍按游戏事件触发，尾音可以自然延续；循环声则在状态结束时停止。

### 2.2 统一 Prompt 前缀

脚本会自动把下面的声音方向加到非音乐音效前：

```text
Layered game SFX with believable physics, full body, controlled highs and
natural decay. No music, dialogue, human voice or clipping.
```

各资产 Prompt 只描述该声音自身，不重复堆叠通用形容词。避免使用容易生成尖锐结果的词：`sharp`、`piercing`、`crisp glass`、`bright crack`、`violent electrical spark`。

### 2.3 文件与动态规范

- 单次音效保留必要的准备动作和完整自然尾音，生成后不自动裁短；
- 循环资产不能包含起爆、结束或不可重复的长渐变；
- 保留动态范围，不把所有波形压满；
- Mono：Boss、Player、Anchor、攻击与命中，由 UE 负责空间化；
- Stereo：BGM、环境氛围、胜负提示；
- 目标母带为 `WAV / 48 kHz / 24 bit`；当前 API 候选为 MP3 转 `48 kHz / 16 bit WAV`，只用于筛选与 Demo；转换只改变格式，不裁切原始表演；
- 同组声音保持相似材质，使用 2～3 个轻微变体避免机械重复；
- 密集音效必须限制并发，不能靠提升音量抢占注意力。

---

## 3. 第一批必须生成的资产

这一批用于完成一局可听懂的 Boss 战。优先按照表格顺序生成。

### 3.1 Player 与撞击反馈

#### `SFX_Player_Damaged`

- 时机：玩家被弹幕、冲击波或龙焰扣血；
- 类型：单次，建议 Mono；
- 长度：0.6～1.0 秒；
- 要求：柔软史莱姆受冲击的湿润弹性声，加一层低调的数字损伤；主体偏中低频，不能尖锐，也不能像人类惨叫。

```text
A small liquid slime taking damage: wet membrane compression, internal fluid slosh,
a firm body impact, brief digital distortion and an elastic recovery tail.
Physical and readable without gore, 0.8 seconds.
```

#### `SFX_Player_Dash`

- 时机：鼠标右键按下，玩家立即冲刺；
- 类型：单次，建议 Mono；
- 长度：0.6～0.9 秒；
- 要求：瞬间释放、快速掠过，带液态拉伸感；声音圆润、有一次清晰节奏落点，不能尖锐，也不能有蓄力前奏。

```text
An immediate liquid-slime dash: body squashes, wet membrane snaps forward, fluid
mass rushes through the air, then reforms with a small watery tail. Fast, tactile
and elastic with no charge-up, 0.7 seconds.
```

#### `SFX_Player_JumpRelease`

- 时机：拖拽瞄准后释放左键，史莱姆弹射；
- 类型：单次，建议 Mono；
- 长度：0.7～1.1 秒；
- 要求：被压缩的水滴恢复形状并弹出的声音，比 Dash 更圆、更有弹性。

```text
A liquid slime releasing a charged jump: deep body compression, stretched wet
membrane release, elastic launch, smooth air movement and a subtle fluid tail.
Buoyant, physical and powerful, 0.9 seconds.
```

#### `SFX_BossBody_Rebound`

- 时机：玩家撞到 Boss 身体但没有造成弱点伤害，并被直线弹回；
- 类型：单次，建议 Mono；
- 长度：0.7～1.1 秒；
- 要求：坚硬鳞片与弹性史莱姆相撞，明确表达“撞上了，但攻击无效”。

```text
A liquid slime slamming into giant dragon armor: wet body impact against thick
scales, deep armored resonance, strong elastic compression, forceful rebound and
a short backward air trail. Clearly blocked and unsuccessful, 0.9 seconds.
```

#### `SFX_WeakPoint_Hit`

- 时机：从 Anchor 重撞暴露弱点并真正扣除 Boss HP；
- 类型：单次，可使用 Mono 主体加 Stereo 强化层；
- 长度：1.3～2.0 秒；
- 要求：全场最强的正反馈；必须包含撞击、核心断裂、厚玻璃爆碎、低频爆发和碎片落下，形成完整而爽快的五层结构。

```text
A devastating critical hit on a cyber dragon: fast body impact, deep core fracture,
thick energy glass exploding into many shards, powerful low-frequency energy burst,
then smaller pieces scattering and ringing out. Extremely satisfying and weighty,
1.6 seconds.
```

---

### 3.2 冲击波

#### `SFX_Shockwave_Warning`

- 时机：Boss 咆哮、地面圆环预警出现；
- 类型：单次；
- 长度：约 2.0～2.8 秒；
- 要求：三次均匀、逐渐增强的低频脉冲，让玩家听懂倒计时；末尾留出极短呼吸，不提前出现释放峰值。

```text
A giant dragon charging a ground shockwave: deep chest and arena rumble, three
spaced energy pulses growing heavier, stone vibrating under pressure and a brief
tense gap before release. A physical, unmistakable countdown, 2.4 seconds.
```

#### `SFX_Shockwave_Release`

- 时机：火焰圆环开始向外扩张；
- 类型：单次；
- 长度：1.4～2.2 秒；
- 要求：沉重但清晰的环形能量爆发，重点是“向外扩散”，不是普通炸弹。

```text
A dragon releasing a massive circular ground wave: deep central impact, ground and
air pressure bursting outward, rolling fire-energy edge, arena structure resonating
and a broad distant decay. Clearly expanding in every direction, 1.8 seconds.
```

---

### 3.3 龙焰激光

龙焰必须拆成预警、点火、持续和结束，不能只使用一个声音。

#### `SFX_Laser_Warning`

- 时机：地面警戒区域出现，Boss 进入瞄准；
- 类型：单次或可循环版本；
- 长度：1.8～2.6 秒；
- 要求：浅淡但明确，以稳定瞄准脉冲建立节奏并缓慢升压；不能尖啸，也不要比正式喷射更吵。

```text
A dragon breath weapon charging: throat pressure, contained furnace rumble, plasma
gathering inside the mouth and three targeting pulses rising in intensity.
Believable biological machinery, dangerous but quieter than firing, 2.2 seconds.
```

#### `SFX_Laser_Ignite`

- 时机：停顿结束，龙焰正式从嘴部喷出；
- 类型：单次；
- 长度：0.7～1.1 秒；
- 要求：必须有极清楚但不刺耳的点火重拍，用来标记伤害正式开始。

```text
A cyber dragon breath weapon igniting: violent internal combustion, pressurized
flame bursting from the mouth, dense plasma punch and a short turbulent fire tail.
An unmistakable damage-start impact with physical force, 0.9 seconds.
```

#### `SFX_Laser_Loop`

- 时机：龙焰持续喷射和横扫期间；
- 类型：无缝循环；
- 长度：2.5～4 秒；
- 要求：持续高能火焰与等离子流动，不能包含新的起爆声，以免每次循环都产生节奏跳变。

```text
A seamless dragon breath loop: sustained pressurized flame jet, roaring combustion,
large turbulent air movement, dense plasma current and a steady cyber energy pulse.
Continuous physical power with no new ignition or shutdown, 3 seconds.
```

#### `SFX_Laser_End`

- 时机：龙焰关闭，Boss 进入恢复；
- 类型：单次；
- 长度：0.9～1.5 秒；
- 要求：能量快速衰减并带少量余火，明确告诉玩家危险已经结束。

```text
A dragon breath weapon shutting down: fuel pressure cuts off, flame collapses,
remaining plasma sputters through the mouth, hot air exhales and the arena echo
decays. A clear physical end to danger, 1.2 seconds.
```

---

### 3.4 弹幕

第一版八种弹幕共用同一套基础声音。不要立即为八种模式分别制作整套资产。

#### `SFX_Barrage_Charge`

- 时机：Boss 播放 Attack 前摇；
- 类型：单次；
- 长度：1.2～1.8 秒；
- 要求：一连串能量弹即将形成，与激光蓄力和冲击波低频预警明显不同。

```text
A cyber dragon preparing an energy-projectile volley: multiple plasma cores form
inside the mouth, four rhythmic energy pulses rise in pitch, electrical pressure
thickens and ends on a tense gap before the first shot, 1.5 seconds.
```

#### `SFX_Barrage_Shot_01`、`02`、`03`

- 时机：每颗或每组弹丸发射；
- 类型：三个相近的单次变体；
- 长度：0.4～0.6 秒；
- 要求：短促、轻量，密集播放时不能刺耳，也不能盖住其他攻击提示。

```text
One cyber-dragon projectile launch: compact plasma ignition, pressurized energy pop,
short physical air push and a quick flying tail. One shot only, punchy but controlled
for dense rhythmic repetition, 0.5 seconds.
```

#### `SFX_Barrage_PlayerHit`

- 时机：弹丸命中玩家；
- 类型：单次；
- 长度：0.6～1.0 秒；
- 要求：能量弹破裂加史莱姆受击，比通用 Player Damage 更偏能量质感。

```text
A plasma projectile striking a liquid slime: energy shell bursts, wet body compresses,
fluid splashes inward, digital damage distorts briefly and the body recoils.
Detailed but much smaller than a weak-point hit, 0.8 seconds.
```

---

### 3.5 开场、转阶段与结算

#### `SFX_Boss_Takeoff`

- 时机：龙从 Ground Idle 切换到起飞动画；
- 类型：单次；
- 长度：4～6 秒；
- 要求：必须听到身体蓄力、翼膜展开、多次巨大振翅、空气被推动和逐渐升空，不能只是一声短促风声。不要包含地面碎裂，方便分别同步。

```text
A colossal dragon taking flight: heavy crouch, leathery wings unfolding, three
enormous distinct wing flaps with realistic membrane movement, deep air displacement,
turbulent gusts and gradual body lift. Majestic biological scale, no ground break,
5 seconds.
```

#### `SFX_Boss_Roar`

- 时机：Boss 播放 Roar Montage，Shockwave 进入前摇；
- 类型：Mono，由 Boss 位置进行 3D 空间化；
- 长度：3～4.5 秒；
- 要求：真实巨龙的吸气、胸腔、喉部和口腔共鸣；不是人类吼叫，也不是单纯合成器。可与 Shockwave Warning 能量层叠加。

```text
A colossal dragon roar: deep chest inhale, wet throat motion and enormous animal
vocal cords building into a sustained roar, powerful mouth resonance, turbulent
breath and a fading growl. Believable giant anatomy with subtle cyber energy,
3.5 seconds.
```

#### `SFX_CyberCenter_Fracture`

- 时机：龙起飞时 Cyber 中心盘碎裂；
- 类型：单次；
- 长度：2.2～3.2 秒；
- 要求：大型机械石材断裂，并带数据故障层；不能像普通玻璃杯破碎。

```text
A large cyber arena platform collapsing beneath a dragon: deep structural groan,
major stone-metal fracture, several heavy slabs breaking loose and falling,
smaller debris scattering, distant low impacts and a corrupted digital energy tail.
Large physical scale with a complete decay, 2.8 seconds.
```

#### `SFX_PhaseTransition_Start`

- 时机：Phase 2 扩散球从世界中心出现；
- 类型：单次；
- 长度：1.2～1.8 秒；
- 要求：空间被打开，不使用普通爆炸。

```text
A source-code conversion sphere activating at the arena center: deep system pulse,
space folding open, green electromagnetic field expanding and surrounding arena
matter beginning to digitize. Precise physical scale and spacious tail, 1.5 seconds.
```

#### `SFX_PhaseTransition_Loop`

- 时机：球体持续向外扩张；
- 类型：无缝循环；
- 长度：2～4 秒；
- 要求：持续扫描、字符流与空间解构，频率不能过于尖锐。

```text
A seamless expanding source-code field: broad electromagnetic scanning waves,
thousands of flowing binary particles, physical matter dissolving into data and a
steady syncopated system pulse moving through space. No start or finish, 4 seconds.
```

#### `SFX_PhaseTransition_End`

- 时机：Code 世界完全覆盖场地；
- 类型：单次；
- 长度：1.2～1.8 秒；
- 要求：扫描完成并稳定下来，作为 Phase 2 开始标志。

```text
A digital world conversion completing: the expanding field reaches the horizon,
remaining cyber matter resolves into code, a deep system lock lands, then a wide
green data afterglow settles through the arena. Decisive completion, 1.5 seconds.
```

#### `SFX_Boss_Death`

- 时机：Boss HP 归零并播放 Death Montage；
- 类型：单次；
- 长度：4.5～6.5 秒；
- 要求：巨龙失去力量、数字核心崩溃；不要在文件中加入音乐，便于和 BGM 混合。

```text
A colossal cyber dragon dying: wounded roar and failing breath, heavy wings losing
tension, three slowing core pulses, mechanical systems collapsing, code corruption
spreading through the body and one enormous final impact with a long decay,
5.5 seconds.
```

#### `SFX_Victory`

- 时机：Victory 结果出现；
- 类型：Stereo 单次；
- 长度：1～3 秒；
- 要求：短促、有成就感，但不做成长篇胜利音乐。

```text
A short instrumental victory stinger: the arena motif rises through three confident
digital notes, warm mechanical percussion answers, then a broad triumphant chord
and energy shimmer ring out. Strong achievement without becoming a song, 3 seconds.
```

#### `SFX_Defeat`

- 时机：Player HP 归零；
- 类型：Stereo 单次；
- 长度：1～3 秒；
- 要求：能量下沉、系统失效，但不要做恐怖或悲惨的人声效果。

```text
A short instrumental defeat stinger: the arena pulse misses a beat, player energy
drains through a descending digital tone, soft mechanical ambience collapses and an
unresolved low chord fades into space. Restrained rather than tragic, 3 seconds.
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
Original instrumental seamless-loop boss battle music for a stylized cyber-rift
arena, 132 BPM. Rounded industrial drums, elastic electronic bass, muted mechanical
syncopation and a simple memorable three-note dragon motif. Heavy but playful,
clear gaps on major beats for warnings and impacts, warm low-mid mix, restrained
cymbals and highs, no vocals, no long intro or final cadence, 75 seconds.
```

### `BGM_Boss_Phase2`

- 类型：Stereo 无缝循环；
- 长度：60～90 秒；
- 建议速度：140～155 BPM；
- 目的：延续 Phase 1 的节奏身份，但加入绿色源码空间、故障和失控感；
- 最好与 Phase 1 使用相同生成工程、相同 Seed 或同一主题描述。

```text
Original instrumental seamless-loop phase-two evolution of the same cyber-rift boss
theme, 148 BPM. Preserve the three-note dragon motif and elastic bass identity,
add denser syncopated drums, band-limited code pulses and playful binary arpeggios.
Urgent and unstable but still warm, with deliberate gaps for attack cues,
band-limited glitch texture, no vocals and no final cadence, 75 seconds.
```

如果 AI 无法生成真正无缝循环，至少要求：

1. 开头不要使用只出现一次的长渐入；
2. 结尾不要使用终止和弦；
3. 保留节拍稳定的中段，后续可以人工裁切循环。

---

## 5. 第二批：Anchor 声音

Anchor 是玩家建立移动节奏的关键，不应听成脆弱玻璃。主体是有重量的陶瓷能量体，配合柔和的数据脉冲。生成、吸附、蓄能、弹出、碎裂组成一条完整节奏链。

### `SFX_Anchor_Emerge`

- 时机：从 Floor Disc 的随机位置冒出；长度约 1.8 秒。

```text
A crystalline energy anchor emerging from the arena floor: ground pressure opens,
large crystal segments rise and lock together, energy flows through the structure,
small fragments settle and a resonant data connection activates, 1.8 seconds.
```

### `SFX_Anchor_Attach`

- 时机：Player 成功吸附；长度约 0.7 秒。

```text
A liquid slime attaching to a solid crystal anchor: wet body contact, surface suction,
fluid spreading against the crystal and a warm energy connection locking into place.
Tactile and satisfying, 0.7 seconds.
```

### `SFX_Anchor_OverloadLoop`

- 时机：Anchor 过载进度持续增加；2.5 秒无缝循环；
- 本身不加速、不爆炸，由 UE 根据 `OverloadAlpha` 调整音高和音量；
- 碎裂时立刻停止，改播 `SFX_Anchor_Shatter`。

```text
A seamless anchor overload loop: energy circulating inside a stressed crystal body,
four steady internal pulses, subtle structural vibration and contained electrical
pressure. Stable neutral tempo with no fracture, climax, start or end, 2.5 seconds.
```

### `SFX_Anchor_Launch`

- 时机：Player 从 Anchor 跳跃或冲刺离开；长度约 0.9 秒。

```text
A liquid slime launching from an energy anchor: body compresses against crystal,
energy connection stretches and breaks, wet membrane releases with force, fluid mass
accelerates through the air and leaves a short elastic tail, 0.9 seconds.
```

### `SFX_Anchor_Shatter`

- 时机：Anchor 自动碎裂；长度约 2.2 秒；碎块本身不阻挡 Player。

```text
A large crystalline energy anchor overloading and shattering: stressed crystal
creaks, heavy internal fracture, thick glass bursting into solid shards, pieces
tumbling across the arena and a fading digital discharge. Detailed physical weight,
2.2 seconds.
```

---

## 6. 第三批：环境与细节

环境负责维持空间感，不负责提供主节拍。Player 移动声由实际移动距离触发，形成轻微、不规则但有弹性的脚步替代物。

### `AMB_CyberRift`

```text
A quiet seamless cyber-rift arena ambience: distant rounded machinery breathing,
deep spatial pressure and slow muted energy pulses, wide but unobtrusive,
no melody or foreground impacts, with a smoothly rolled-off top end, 20 seconds.
```

### `AMB_SourceCodeVoid`

```text
A quiet seamless source-code void ambience: soft flowing data rain, deep empty
space and faint band-limited binary pulses moving across stereo,
weightless and unobtrusive, no melody or harsh bit-crushing, 20 seconds.
```

### `SFX_Player_Move_01`

```text
One tiny liquid-slime movement beat: soft wet membrane press and rounded release,
subtle, clean and playful, no footsteps or sticky gore, 0.25 seconds.
```

### `SFX_Player_Move_02`

```text
One tiny liquid-slime movement variation: slightly lower soft body squish followed
by a small elastic release, subtle and playful, no footsteps, 0.25 seconds.
```

### `SFX_Player_Move_03`

```text
One tiny liquid-slime movement variation: gentle watery roll landing on a muted
membrane pop, subtle and round, no bubbly cartoon squeak, 0.25 seconds.
```

### `SFX_Player_AttachedMove_01`

```text
A small slime moving one step over an energy-anchor surface: soft suction release,
short liquid stretch and a faint warm ceramic response, quiet, 0.3 seconds.
```

### `SFX_Player_AttachedMove_02`

```text
A small slime moving one step over an energy-anchor surface: slightly lower tacky
release, rounded wet slide and muted energy response, quiet, 0.3 seconds.
```

### `SFX_Barrage_NearMiss`

```text
One plasma projectile passing close to the player: compact rounded Doppler whoof
with a soft filtered energy tail, quick and readable with most energy in the low-mid
range, no impact, 0.3 seconds.
```

### `SFX_UI_Hover`

```text
One soft sci-fi menu hover: tiny liquid membrane tick fused with a muted digital
pulse, immediate and understated, no melody, 0.12 seconds.
```

### `SFX_UI_Confirm`

```text
One sci-fi menu confirmation: rounded liquid click followed by a warm two-note
digital resolve, positive and compact with restrained highs, 0.25 seconds.
```

环境氛围必须低调，不能掩盖激光、冲击波和弹幕预警。

---

## 7. 交付与筛选规则

先为每个声音家族生成少量样本，确认材质后再批量生成。每个关键音效保留 2～4 个候选，项目只接入筛选后的版本。

筛选标准：

1. 连续播放 Player 的移动、跳跃、冲刺时，听起来像同一种柔软乐器；
2. 不看画面也能区分冲击波的低频倒计时、激光的持续蓄压和弹幕的短促切分；
3. `Laser_Ignite` 与 `Shockwave_Release` 有明确重拍，但边缘不刺耳；
4. 同时播放 6～10 个弹丸发射声仍然柔和，不形成高频噪声墙；
5. `BossBody_Rebound` 是“沉闷阻挡—弹回”，`WeakPoint_Hit` 是“深层命中—能量展开”，两者不能混淆；
6. Phase 2 比 Phase 1 更密、更数字化，但必须保留相同的节奏身份；
7. 关闭 BGM 时攻击仍然可读；打开 BGM 后所有 Warning 仍能被听见；
8. 循环资产没有接缝、起爆音、结束音或不可重复的节奏变化；
9. 不含可识别的现有影视、游戏旋律或受版权保护采样；
10. 确认生成服务允许用于当前项目和计划中的发布方式。

试听不能只在文件浏览器里完成。最终验收必须放入真实战斗，至少测试：普通移动、密集弹幕、激光完整流程、连续两次 Shockwave、弱点命中和转阶段。

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

- 动画 Notify 或实际伤害事件触发声音，不用与判定脱节的固定 Delay；
- Warning、Release、Hit 分别触发，不能把整次攻击烘焙成一个音频；
- 密集弹幕发射声设置独立并发组，建议同时最多 4 个，优先保留最新声音；
- 三个弹丸变体随机播放，音高只做约 `±2%` 的轻微变化，避免破坏阶段音高身份；
- Player 移动音按实际移动距离触发，不按每帧播放；移动速度改变时优先改变触发间隔，而不是大幅拉伸音高；
- Anchor Overload 循环由 `OverloadAlpha` 连续控制音量和小幅音高，碎裂时立即停止；
- 循环声音在攻击结束、Boss 死亡和 Restart 时可靠停止；
- 3D 衰减距离，保证竞技场边缘仍能听清 Boss 的攻击提示；
- BGM、环境、Boss 攻击、Player 反馈和 UI 使用不同 Sound Class；
- Warning 播放时让 BGM 短暂降低约 `2～3 dB`，不要把 Warning 本身做得更尖；
- Boss 关键攻击使用竞技场范围内稳定可读的 3D 衰减，普通弹丸仍按距离衰减；
- Phase 2 切换时先交叉淡化环境和 BGM，再启用更密的 Phase 2 攻击声音。

### 8.1 推荐混音优先级

```text
失败/胜利与弱点命中
> Player 受击与 Boss 攻击释放
> 攻击 Warning
> 弹丸发射与 Anchor 操作
> Player 移动
> BGM
> 环境氛围
```

这里的优先级表示发生遮盖冲突时谁应保持清楚，不表示上层声音永远更响。最终通过 Sound Class、Concurrency 和短暂 Ducking 实现。

### 8.2 当前 v0.4 接入状态

2026-09-13 已将首批 30 个确认音效导入 `/Game/Audio/v0_4`，并完成第一轮 C++ 事件接线：

- Player：成功起跳、成功冲刺、HP 实际减少；
- Boss：身体反弹、有效弱点、死亡与胜负结算；
- 攻击：Shockwave、Laser、八种 Barrage 共用的三套发射变化；
- 开场：Boss 起飞与 Cyber Center 实际碎裂；
- Anchor：冒出、吸附、过载循环、离开和实际碎裂；
- Phase 2：扩散开始、持续循环和完成。

密集弹幕发射声采用三种变化轮换、约 `±2%` 音高变化，并限制为同时最多 4 个发射声。当前音量属于首轮安全值，下一步应在完整 PIE 战斗中做混音验收；BGM、环境氛围、移动细节、Sound Class 和 Warning Ducking 仍属于后续批次。
