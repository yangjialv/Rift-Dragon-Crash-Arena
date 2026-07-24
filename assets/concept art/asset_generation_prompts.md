# Asset Generation Prompts

项目名称：**Rift Dragon Crash Arena**
用途：用于生成 UE5 伪 3D Boss 战 Demo 的主要角色、Boss、空间节点、弱点、特效、场景组件等资产。
生成流程建议：

```text
原画 / 概念图
↓
建模参考图 / 三视图
↓
图生 3D / 文生 3D
↓
Blender 清理
↓
UE5 导入
```

---

# 1. 通用 Prompt 部分

## 1.1 全局美术风格 Prompt

适用于所有原画、模型、道具和特效生成。

```text
Dark fantasy pseudo-3D boss arena game, stylized 3D game asset, clear silhouette, readable shapes, glowing rift energy, broken ancient ruins, abyssal purple and blue lighting, crystal core elements, floating stone fragments, non-realistic but game-ready, medium-low poly friendly design, suitable for Unreal Engine 5.
```

中文设计说明：

```text
暗黑奇幻、伪 3D Boss 战、风格化 3D 游戏资产、轮廓清晰、形体易读、裂隙能量发光、破碎遗迹、深渊紫蓝光、晶体核心、浮空碎石、适合 UE5 的中低模风格。
```

---

## 1.2 通用负面 Prompt

适用于大多数资产生成，避免生成不可用或过度复杂的内容。

```text
no human character, no full humanoid body, no realistic gore, no excessive smoke, no blurry silhouette, no complex background, no tiny unreadable details, no overly thin parts, no messy geometry, no copyright character, no existing game character, no text, no logo, no watermark
```

---

## 1.3 Boss 类资产负面 Prompt

适用于 Boss 龙头、Boss 弱点、Boss 攻击相关资产。

```text
no full dragon body, no wings, no legs, no rider, no humanoid body, no complex background, no smoke covering the shape, no extreme perspective, no tiny unreadable details, no text, no logo, no watermark, no overly realistic gore
```

---

## 1.4 玩家类资产负面 Prompt

适用于玩家小型空间生命体。

```text
no human body, no arms, no legs, no realistic animal, no detailed human face, no weapon, no armor, no complex costume, no text, no logo, no watermark
```

---

## 1.5 建模参考图通用要求

当目标是用于 Meshy / Tripo / Rodin 等图生 3D 工具时，Prompt 中应尽量加入以下描述：

```text
orthographic reference sheet, front view, side view, 3/4 view, clean neutral background, centered object, clear silhouette, simple readable shapes, medium-low poly friendly, no dramatic lighting, no smoke, no complex background, suitable for generating a 3D model
```

---

# 2. Boss：裂隙龙首 Boss

## 2.1 资产定位

名称：**Rift Dragon Head / 裂隙龙首**

用途：

```text
主 Boss 模型，用于 UE5 伪 3D Boss 战场景。
Boss 不需要完整身体，只需要巨大龙头、部分脖颈、额头弱点、嘴部攻击源和背后空间裂隙。
```

设计关键词：

```text
巨大龙头
空间裂隙
暗黑奇幻
额头晶核弱点
嘴部能量核心
破碎龙角
石质鳞片
黑色晶体
Boss 压迫感
```

---

## 2.2 Boss 氛围原画 Prompt

用途：生成好看的 Boss 概念图，用于确定风格和项目展示。

```text
A giant rift dragon head emerging from a dimensional portal, dark fantasy boss monster, enormous scale contrast against a tiny glowing player creature, only the dragon head and partial neck visible, no full body. The dragon has large broken horns, a glowing crystal weak point on its forehead, an open mouth with an energy core inside, cracked stone-like scales mixed with dark crystal, floating ruin fragments around it, abyssal purple and blue rift light, dramatic boss arena composition, stylized 3D game concept art, clear silhouette, readable weak point, suitable for Unreal Engine 5 boss battle.
```

负面 Prompt：

```text
no full dragon body, no wings, no legs, no humanoid body, no rider, no text, no logo, no watermark, no excessive smoke covering the shape, no overly realistic gore
```

---

## 2.3 Boss 建模参考图 Prompt

用途：生成适合图生 3D 的 Boss 参考图。

```text
Orthographic reference sheet of a stylized giant rift dragon head for a 3D game asset. Include front view, side view, and 3/4 view. Clean white or neutral background, centered object, clear silhouette, symmetrical design, large broken horns, strong jaw shape, open mouth with a visible energy core, glowing forehead crystal weak point, dark fantasy stone-and-crystal surface, simple readable shapes, medium-low poly friendly, no full body, no wings, no legs, no smoke, no dramatic lighting, suitable for generating a 3D model.
```

负面 Prompt：

```text
no full dragon body, no complex background, no smoke, no extreme perspective, no tiny details, no text labels, no watermark, no humanoid face, no wings, no legs
```

---

## 2.4 Boss 图生 3D Prompt

用途：将 Boss 原画或参考图输入 Meshy / Tripo / Rodin 时使用。

```text
Generate a stylized game-ready 3D model of a giant rift dragon head only, based on the reference image. No full body, no wings, no legs. The model should have a strong readable silhouette, large broken horns, a clear forehead crystal weak point area, an open mouth with an energy core cavity, cracked stone-like scales mixed with dark crystal, and a dark fantasy rift creature feeling. Keep the geometry clean, medium-low poly, centered, suitable for Unreal Engine 5. The forehead weak point and mouth core area should be easy to identify and later attach collision components to.
```

---

# 3. 玩家：小型空间生命体

## 3.1 资产定位

名称：**Rift Core / Space Seed / Anchor Wisp / 空间锚核**

用途：

```text
玩家操控角色。不是人形角色，而是一个小型非人形空间生命体。
主要通过 WASD 移动和鼠标蓄力弹射冲撞进行操作。
```

设计关键词：

```text
小型
非人形
发光核心
柔软晶体
漂浮
弹性
可冲撞
可拉伸
神秘但可爱
轮廓清楚
```

---

## 3.2 玩家原画 Prompt

用途：生成玩家概念图。

```text
A small non-humanoid rift core creature for a pseudo-3D boss arena game, glowing bioluminescent body, soft round crystal-like form, floating shell fragments orbiting around it, cute but mysterious, no arms, no legs, no human face. The creature should feel agile and elastic, designed for bouncing, dashing, and crashing into boss weak points. Clear silhouette, simple readable shape, glowing inner core, subtle particle trail, stylized 3D game asset concept, clean background, suitable for Unreal Engine 5.
```

负面 Prompt：

```text
no human body, no arms, no legs, no realistic animal, no face with detailed eyes, no weapon, no armor, no complex costume, no text, no watermark
```

---

## 3.3 玩家建模参考图 Prompt

用途：生成玩家三视图或建模参考。

```text
Orthographic reference sheet of a small non-humanoid rift core creature for a 3D game asset. Include front view, side view, and top view. The creature is a simple floating glowing core with a soft crystal-like body and a few orbiting shell fragments. No arms, no legs, no human face. Clean neutral background, centered object, simple readable silhouette, medium-low poly friendly, designed for a pseudo-3D boss arena game where it moves, bounces, stretches, and crashes into weak points.
```

---

## 3.4 玩家图生 3D Prompt

用途：用于 Meshy / Tripo 等模型生成工具。

```text
Generate a small non-humanoid glowing rift core creature for a pseudo-3D boss arena game. The model should be simple, round, crystal-like, floating, with a clear silhouette and no arms or legs. Add a few small floating shell fragments around the core if possible. Keep it medium-low poly, clean, centered, and suitable for Unreal Engine 5. The asset should be easy to scale, animate with simple squash-and-stretch, and attach Niagara trails to.
```

---

# 4. Boss 弱点：额头晶核

## 4.1 资产定位

名称：**Boss Weak Point Crystal / 弱点晶核**

用途：

```text
挂载在 Boss 额头或口腔位置。
玩家只有在 Crashing 状态下撞击暴露弱点，才会对 Boss 造成伤害。
```

设计关键词：

```text
发光晶核
攻击目标
碰撞点
清晰可读
护盾破碎
紫蓝裂隙能量
```

---

## 4.2 弱点晶核原画 Prompt

```text
A glowing boss weak point crystal for a dark fantasy rift dragon boss, placed on a dragon forehead. The crystal is sharp, readable, and clearly attackable, with bright rift energy inside, cracked surface, purple-blue glow, circular energy outline, designed as a collision target for a player crash attack. Stylized 3D game asset concept, clean background, simple silhouette, suitable for Unreal Engine 5.
```

---

## 4.3 弱点晶核图生 3D Prompt

```text
Generate a stylized glowing crystal weak point game prop. It should be a clear attack target, medium-low poly, sharp but readable, with a bright rift energy core inside and a simple outline. The model should be suitable for attaching to a boss forehead in Unreal Engine 5. Keep the shape clean, centered, and easy to use as a collision target.
```

---

# 5. 空间节点：Space Node / Rift Anchor

## 5.1 资产定位

名称：**Space Node / Rift Anchor / 空间节点**

用途：

```text
场地中的可交互目标。
玩家冲撞空间节点后，节点激活，Boss 护盾下降。
当 Boss 护盾归零后，Boss 弱点暴露。
```

设计关键词：

```text
漂浮晶体
空间锚点
圆形能量环
碎石环绕
可冲撞目标
削弱护盾
```

---

## 5.2 空间节点原画 Prompt

```text
A floating rift anchor crystal used as an interactive space node in a dark fantasy boss arena. It has a glowing central crystal, a circular energy ring, small broken stone fragments orbiting around it, and visible rift energy lines. It should look like something the player can crash into or activate. Stylized 3D game prop concept, clear silhouette, readable gameplay object, clean background, suitable for Unreal Engine 5.
```

---

## 5.3 空间节点建模参考图 Prompt

```text
Orthographic reference sheet of a floating rift anchor crystal game prop. Include front view, side view, and top view. The prop has a glowing central crystal, a simple circular energy ring, and a few small floating stone fragments. Clean neutral background, centered object, clear readable silhouette, medium-low poly friendly, designed as an interactive collision target in a pseudo-3D boss arena.
```

---

## 5.4 空间节点图生 3D Prompt

```text
Generate a floating rift anchor crystal game prop with a glowing center, simple circular energy ring, and small broken stone fragments around it. It should be medium-low poly, clean, centered, and suitable for Unreal Engine 5. The object must have a clear central collision target and be readable from a top-down or isometric camera.
```

---

# 6. Boss 攻击组件：裂隙吐息 / 直线攻击

## 6.1 资产定位

名称：**Rift Breath / 裂隙吐息**

用途：

```text
Boss 的主要直线攻击。
攻击前地面出现预警线，短暂延迟后爆发，玩家若站在范围内则受伤。
攻击结束后可以进入 Boss 弱点暴露窗口。
```

设计关键词：

```text
地面预警
直线伤害
空间撕裂
紫蓝能量
红色危险边缘
清晰可读
```

---

## 6.2 裂隙预警线 Prompt

用途：生成地面 Decal / 贴图 / 特效参考。

```text
A top-down game attack warning decal, long rectangular rift warning mark on the ground, glowing red-purple energy, cracked space texture, clear danger zone, readable from an isometric camera, transparent background if possible, stylized dark fantasy VFX texture, suitable for Unreal Engine 5 material or decal.
```

---

## 6.3 裂隙爆发特效 Prompt

用途：生成 Boss 直线攻击特效参考。

```text
A dark fantasy rift breath attack effect for a giant dragon head boss, a long straight beam of distorted space energy tearing across the ground, purple-blue core with red warning edges, cracked ground, energy particles, readable danger area, stylized Unreal Engine 5 VFX concept, suitable for a pseudo-3D boss arena.
```

---

# 7. Boss 弹幕组件：Rift Bullet / Energy Orb

## 7.1 资产定位

名称：**Rift Bullet / 裂隙弹幕**

用途：

```text
Boss 的弹幕攻击。
可以用于环形弹幕、扇形弹幕或追踪弹。
```

设计关键词：

```text
能量球
紫蓝发光
小型弹幕
拖尾
清晰轮廓
碰撞伤害
```

---

## 7.2 弹幕球原画 Prompt

```text
A small rift energy projectile for a boss bullet pattern, glowing purple-blue orb with a bright core, dark fantasy magical energy, simple readable shape, circular silhouette, subtle particle trail, designed for a pseudo-3D boss arena, stylized game VFX concept, suitable for Unreal Engine 5 Niagara projectile.
```

---

## 7.3 弹幕拖尾 Prompt

```text
A stylized rift energy trail for a small projectile, purple-blue glowing particles, short comet-like trail, clean readable shape, dark fantasy magic effect, transparent background if possible, suitable for Unreal Engine 5 Niagara VFX.
```

---

# 8. 玩家冲撞特效：Phase Crash VFX

## 8.1 资产定位

名称：**Phase Crash Trail / 相位冲撞拖尾**

用途：

```text
玩家鼠标蓄力弹射后，在 Crashing 状态下快速移动时使用。
用于强调速度、弹跳、冲撞和命中反馈。
```

设计关键词：

```text
高速拖尾
相位能量
紫蓝光
晶体粒子
拉伸
冲击
动感强
```

---

## 8.2 冲撞拖尾 Prompt

```text
A fast phase crash trail effect for a small glowing rift creature, stretched energy streak, purple-blue light, sharp motion blur, small crystal particles, impact-ready visual, stylized game VFX concept, readable from an isometric camera, suitable for Unreal Engine 5 Niagara.
```

---

## 8.3 命中弱点冲击 Prompt

```text
A powerful crystal impact effect when a small glowing creature crashes into a boss weak point, bright flash, radial shockwave, broken crystal particles, purple-blue rift energy burst, clear hit feedback, stylized Unreal Engine 5 game VFX concept.
```

---

# 9. 场景组件：破碎竞技场

## 9.1 资产定位

名称：**Broken Rift Arena / 破碎裂隙竞技场**

用途：

```text
Boss 战主场景。
玩家在竞技场平面上移动，Boss 位于场地边缘或上方，空间裂隙和漂浮碎片提供视觉氛围。
```

设计关键词：

```text
圆形竞技场
破碎石板
深渊背景
空间裂隙
漂浮遗迹
清晰战斗平面
斜俯视可读
```

---

## 9.2 竞技场氛围图 Prompt

```text
A dark fantasy pseudo-3D boss arena, circular broken stone platform floating in an abyss, dimensional rift in the background, broken ancient pillars, floating ruin fragments, glowing cracks on the ground, purple-blue rift light, designed for a giant dragon head boss battle, clear gameplay space, readable top-down/isometric layout, stylized Unreal Engine 5 environment concept art.
```

---

## 9.3 模块化地块 Prompt

用途：生成场景模块参考。

```text
Modular broken stone arena tiles for a dark fantasy boss arena, medium-low poly game assets, cracked floor pieces, clean shapes, readable from top-down camera, suitable for Unreal Engine 5. Include square, hexagonal, and broken edge platform pieces, neutral background, no characters.
```

---

# 10. 场景装饰：漂浮遗迹碎片

## 10.1 资产定位

名称：**Floating Ruin Fragments / 漂浮遗迹碎片**

用途：

```text
用于 Boss 背景、场地边缘和空间裂隙周围的装饰。
不参与核心碰撞，主要增强空间撕裂氛围。
```

设计关键词：

```text
破碎石块
遗迹残片
漂浮
裂缝发光
低模
背景装饰
```

---

## 10.2 漂浮遗迹碎片原画 Prompt

```text
Floating broken ruin fragments for a dimensional rift boss arena, cracked stone blocks, small ancient pillar pieces, glowing rift cracks, dark fantasy style, medium-low poly friendly, clean silhouettes, game-ready prop concept, suitable for Unreal Engine 5 background decoration.
```

---

## 10.3 漂浮遗迹碎片图生 3D Prompt

```text
Generate a set of medium-low poly floating broken stone fragments for a dark fantasy rift arena. The pieces should include cracked blocks, small pillar fragments, and broken platform edges. Keep the geometry clean, centered, and suitable for Unreal Engine 5. These props are for background decoration and should not be too detailed.
```

---

# 11. UI 图标组件

## 11.1 资产定位

用途：

```text
用于玩家技能、Boss 护盾、弱点暴露等 UI 状态显示。
图标要求简洁、清楚、小尺寸可读。
```

设计关键词：

```text
高对比
无文字
方形图标
紫蓝能量
暗黑奇幻
可读性强
```

---

## 11.2 Phase Crash 技能图标 Prompt

```text
A simple skill icon for Phase Crash, showing a glowing rift core launching forward with a sharp energy trail, dark fantasy UI style, purple-blue glow, high contrast, readable at small size, square icon, no text, no logo.
```

---

## 11.3 Boss Shield 图标 Prompt

```text
A simple UI icon for boss shield, glowing cracked crystal barrier, purple-blue energy, dark fantasy style, high contrast, readable at small size, square icon, no text, no logo.
```

---

## 11.4 WeakPoint Exposed 图标 Prompt

```text
A simple UI icon for exposed weak point, glowing crystal target with broken shield fragments, bright purple-blue core, dark fantasy style, high contrast, readable at small size, square icon, no text, no logo.
```

---

# 12. 音效生成 Prompt，可选

## 12.1 玩家冲撞音效 Prompt

```text
A short powerful magical impact sound for a small rift creature crashing into a crystal weak point, sharp transient, crystal crack, deep energy pulse, dark fantasy game sound effect, 1 second duration.
```

---

## 12.2 Boss 龙吼音效 Prompt

```text
A giant dark fantasy dragon roar mixed with dimensional rift distortion, deep and threatening, suitable for a boss attack warning, 2 seconds duration.
```

---

## 12.3 破盾音效 Prompt

```text
A magical shield breaking sound, crystal shatter, energy burst, dark fantasy rift magic, clear impact, suitable for a boss shield break moment, 1.5 seconds duration.
```

---

## 12.4 空间裂缝音效 Prompt

```text
A tearing dimensional rift sound, magical distortion, stone cracking, low rumble, purple energy ambience, suitable for a ground rift attack in a boss arena, 2 seconds duration.
```

---

# 13. 推荐生成顺序

两天半开发时间下，建议按以下顺序生成资产：

## 13.1 P0：必须优先生成

```text
1. Boss 裂隙龙首氛围图
2. Boss 裂隙龙首建模参考图
3. Boss 裂隙龙首 3D 模型
4. 玩家小型空间生命体原画
5. 空间节点原画或模型
```

## 13.2 P1：推荐生成

```text
6. Boss 弱点晶核
7. 玩家冲撞拖尾参考
8. 裂隙攻击预警贴图
9. 弹幕球参考
```

## 13.3 P2：有时间再生成

```text
10. 竞技场概念图
11. 漂浮遗迹碎片
12. UI 图标
13. 音效
```

---

# 14. 最小可用资产方案

如果时间非常紧，可以采用以下最小资产组合：

```text
Boss：
AI 生成龙头模型，弱点用 UE5 发光球体单独挂载。

玩家：
UE5 球体 / 晶体 + 发光材质 + Niagara 拖尾。

空间节点：
UE5 晶体 / 球体 + 圆环 Mesh + 发光材质。

弹幕：
UE5 小球 + 发光材质 + Niagara 拖尾。

裂缝攻击：
透明红色长条 Mesh / Decal + Box Collision。

竞技场：
UE5 基础地面 + Fab 免费石块 / 遗迹碎片。

UI：
UMG Progress Bar + Text。
```

---

# 15. 生成时注意事项

1. 原画图追求风格和气氛；
2. 建模参考图追求干净、对称、轮廓清楚；
3. 图生 3D 不要期待一次成功；
4. Boss 弱点最好在 UE5 中单独挂载，不要依赖 Boss 模型自带结构；
5. 所有可交互对象都要保证从斜俯视相机下清楚可读；
6. 不要直接使用已有游戏的角色名、风格名或 IP 名称；
7. 不要生成完整龙身，只生成龙头和部分脖颈；
8. 不要生成复杂烟雾背景，否则图生 3D 质量会下降；
9. 模型进入 UE5 前最好用 Blender 检查比例、朝向、法线和多余碎面；
10. 美术资产优先服务玩法可读性，而不是追求复杂精细度。

---

# 16. 一次性生成任务建议

建议先生成以下 4 张关键图：

```text
1. Rift Dragon Head Boss - atmosphere concept
2. Rift Dragon Head Boss - orthographic reference sheet
3. Small Rift Core Player - reference sheet
4. Floating Rift Anchor Node - reference sheet
```

然后将第 2、3、4 张用于图生 3D。

第一轮不要追求最终美术质量，优先确认：

```text
Boss 是否像巨大龙头
额头弱点是否明显
玩家是否非人形且小巧
空间节点是否像可交互目标
所有对象轮廓是否清楚
```
