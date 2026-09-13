# 音频生成配置与使用

本工具按 [声音资产需求](audio_asset_requirements.md) 生成试听候选，不会自动导入 UE、修改蓝图或替换正式音频。

## 1. 配置内容

- 服务：ElevenLabs，密钥只发送到 `https://api.elevenlabs.io`，拒绝跳转。
- 脚本：`tools/generate_audio.py`，使用 Python 标准库。
- 清单：`tools/audio_manifest.json`，共 42 项；配置名称、时长、声道、批次和循环要求。
- 第一批及音乐的英文提示词读取需求文档；后续细节音效的补充提示词保存在清单。
- 请求前检查音效提示词不超过 450 字符、音乐提示词不超过 4100 字符，超限直接本地报错。
- 密钥不写入文档、脚本或清单。推荐运行时隐藏输入；也支持已有的 `ELEVENLABS_API_KEY` 环境变量。
- 已在聊天中发送过的密钥，建议本轮结束后在服务商后台撤销并换新。以后不需要再发到聊天。

音效使用 `eleven_text_to_sound_v2`，音乐使用 `music_v1`，默认提示词遵循度为 `0.4`。账户需有相应接口权限和可用额度。脚本不会购买套餐。

## 2. 先检查清单（不花额度）

在项目根目录打开 PowerShell。此电脑可使用 UE 自带 Python，不需要关闭编辑器：

```powershell
$audioPython = 'F:\epic\UNREAL ENGINE\UE_5.3\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
& $audioPython tools/generate_audio.py --batch samples
```

没有 `--generate` 就不会请求接口，也不会读取密钥。

## 3. 生成四个 v0.4 拟真基准样本

```powershell
& $audioPython tools/generate_audio.py --batch samples --generate --prompt-key
```

出现 `ElevenLabs API key (hidden):` 后粘贴密钥并回车，输入不显示。不要将密钥直接写进命令。

v0.4 默认样本分别是 Boss 起飞、Boss 咆哮、Anchor 碎裂和弱点命中，用于验证生物拟真、物理层次和关键打击感。每种只请求一次；接口请求时长分别为 5.0、3.5、2.2、1.6 秒，共 12.3 秒。实际扣费由服务商决定。

输出位于 `audio/generated/每次运行的时间戳/`：

- `*.original.mp3`：接口返回的原始音频，保留不覆盖。
- `*.attempt.json`：发起请求前记录的提示词与配置。
- `*.result.json`：成功下载后的文件校验和与接口返回信息。
- `*.wav` / `*.wav-info.json`：提供 FFmpeg 时生成的转换候选及格式信息。

这些输出以及 `.local/` 工具依赖已加入 Git ignore。记录不包含密钥。

## 4. 转为 UE 使用的 WAV

接口默认下载 44.1 kHz、128 kbps MP3，兼顾账户兼容性。转换后的 WAV 是 **48 kHz、16-bit PCM**，不是需求文档期望的原生 24-bit 无损母带；转换不能恢复 MP3 丢失的细节。

如果已有 FFmpeg，在生成命令后增加 `--ffmpeg '完整的ffmpeg.exe路径'` 即可同时转换。

本机已下载一份项目专用 FFmpeg，可直接这样运行：

```powershell
& $audioPython tools/generate_audio.py --batch samples --generate --prompt-key --ffmpeg '.local/audio-deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
```

该依赖位于已忽略的 `.local/`，不会提交到 Git。

也可以只转换已下载音频，不重复调用付费接口：

```powershell
& $audioPython tools/generate_audio.py --batch samples --convert-only 'audio/generated/实际运行目录名' --ffmpeg '完整的ffmpeg.exe路径'
```

转换过程只把接口返回音频改为 `48 kHz / 16-bit WAV`，不再自动去除开头、裁切时长或增加淡入淡出。这样可以完整保留翅膀准备、真实撞击主体、碎片散落和空间尾音。最终剪辑必须在试听后另行完成。循环接缝仍需人工检查；音乐的循环要求只通过提示词表达，不保证天然无缝。

## 5. 后续批次

先试听四个风格基准样本，再决定是否生成其余内容：

| `--batch` | 内容 | 请求数 |
| --- | --- | ---: |
| `samples` | 四个 v0.4 拟真基准样本 | 4 |
| `first` | 第一批战斗音效 | 26 |
| `music` | 已使用 `audio/BGM` 中的现有曲目，不调用生成接口 | 0 |
| `anchors` | 五种锚点音效 | 5 |
| `details` | 环境、移动、擦弹及 UI | 10 |
| `all` | 全部需要生成的音效，包含已试听项目，不含现有 BGM | 41 |

基准样本通过后生成第一批剩余音效：

```powershell
& $audioPython tools/generate_audio.py --batch first --exclude-samples --generate --prompt-key --ffmpeg '.local/audio-deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
```

`--exclude-samples` 会跳过当前清单中的四个基准样本，避免重复消耗额度。

每次生成都会新建目录；再次运行同一批次会重新付费生成，不会自动跳过旧样本。没有自动重试：如果超时或中途退出，先检查本地结果及服务商使用记录，避免重复扣费。音乐生成可能需要不同的套餐或权限。

只补生成某一个音效，使用 `--asset` 覆盖批次选择，例如：

```powershell
& $audioPython tools/generate_audio.py --asset SFX_WeakPoint_Hit --generate --prompt-key --ffmpeg '.local/audio-deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
```

正式使用前还需要人工试听、统一响度、检查循环和账户对应的商用许可，然后再选择合格候选导入 UE。

## 6. v0.3 已生成的 24 个战斗音效功能

这些文件目前是**生成完成的候选资产**，还没有自动接入 UE。下面的“触发条件”是后续接入时必须遵守的规则。

### 6.1 Player 操作和伤害反馈

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Player_Dash` | 鼠标右键按下，并且贴地冲刺真正开始的瞬间 | 史莱姆立即压缩并向前释放 | 每次成功冲刺一次；冷却中无效输入不播放 |
| `SFX_Player_JumpRelease` | 左键按住蓄力后松开并真正起跳时 | 史莱姆由蓄力压缩切换为弹射 | 不在开始瞄准时播放；无法形成有效方向时不播放 |
| `SFX_Player_Damaged` | Player HP 实际减少 1 点时 | 告诉玩家“这次攻击确实扣血了” | 一次扣血只播放一次；无敌时间内的重复碰撞不播放 |

`Player_Damaged` 是通用生命值反馈。弹幕可以同时低音量叠加 `Barrage_PlayerHit` 作为攻击来源层；Shockwave 和 Laser 不需要再制作另一份 Player 受击声。

### 6.2 冲撞 Boss 的结果

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_BossBody_Rebound` | Player 撞到 Boss 身体，得到 Rebound，但 Boss HP 没有减少 | 撞到了坚硬身体，攻击无效并被弹回 | 身体反弹时单独播放；弱点命中允许极低音量复用其低频冲击层 |
| `SFX_WeakPoint_Hit` | Player 命中有效弱点，并且 Boss HP 实际减少 1 点 | 本轮攻击成功，是全场最强正反馈 | 只有真正扣除 Boss HP 才播放；当前原型叠加弱点主体、Anchor 玻璃层和低频身体层 |
| `SFX_Boss_PainRoar` | 有效弱点扣血后且 Boss HP 仍大于 0 | 龙受到真实伤害后的痛吼 | 不与 Shockwave 主动咆哮混用；最终一击改播 Boss Death |

两种 Gameplay 结果互斥。当前弱点临时混音会低音量复用身体音效中的低频冲击，
但不会把它当成一次身体反弹事件；专用弱点资产通过后可移除这一临时层。

### 6.3 Shockwave

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Shockwave_Warning` | Boss 进入 Roar/冲击波前摇，地面预警环出现时 | 三次低频脉冲构成攻击倒计时 | 每次 Shockwave 在前摇开始时播放一次，此时没有伤害 |
| `SFX_Shockwave_Release` | 预警结束，火焰圆环开始扩散且伤害判定激活时 | 攻击正式释放 | Phase 2 强化版只预警一次，但 Pulse 1 与 Pulse 2 开始时各播放一次 Release |

### 6.4 Laser

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Laser_Warning` | 地面警戒区域出现，Boss 开始瞄准和下降时 | 激光正在锁定，但尚未造成伤害 | 每轮 Laser 只播放一次；停顿阶段可以让尾音结束 |
| `SFX_Laser_Ignite` | 预警与停顿结束，Laser Damage Volume 正式激活时 | 龙焰点火，伤害从这一拍开始 | 与 Niagara 龙焰显示、碰撞启用同一时刻播放 |
| `SFX_Laser_Loop` | 龙焰持续喷射和横扫期间 | 持续危险区域仍然存在 | Ignite 后开始循环；喷射结束、Boss 死亡、眩晕或 Restart 时必须停止 |
| `SFX_Laser_End` | Laser Damage Volume 关闭、龙焰停止时 | 危险正式结束 | 先停止 Loop，再播放 End；不能在恢复动作结束后延迟播放 |

完整顺序固定为：

```text
Warning → 短暂停顿 → Ignite + Loop → 停止 Loop + End
```

### 6.5 Barrage 弹幕

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Barrage_Charge` | Boss 进入 Attack Montage、第一颗弹丸尚未生成时 | 一轮弹幕即将开始 | 一整轮弹幕只播放一次，不跟随每颗弹丸重复 |
| `SFX_Barrage_Shot_01` | 实际生成一颗或一组弹丸时 | 中等、圆润的基础发射音 | 与 02、03 随机选择，只播放其中一个 |
| `SFX_Barrage_Shot_02` | 同上 | 更低、更空的发射变体 | 用来减少连续重复感，不代表另一种攻击模式 |
| `SFX_Barrage_Shot_03` | 同上 | 更轻、低频更少的发射变体 | 用来减少连续重复感，不代表另一种攻击模式 |
| `SFX_Barrage_PlayerHit` | 弹丸真正命中 Player 并造成扣血时 | 表明伤害来源是弹幕 | 在命中位置播放较低音量，同时播放一次 `Player_Damaged`；未扣血不播放 |

八种弹幕模式暂时共用这套声音。发射节奏来自真实 Projectile Spawn 时间，而不是音频文件内部。Shot 组需要独立并发限制，密集弹幕中同时最多保留约 4 个发射声。

### 6.6 开场

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Boss_Takeoff` | Ground Idle 三秒结束，Boss 开始起飞 Montage 时 | 龙的翅膀、身体抬升与空气压力 | 跟随起飞动画播放，不包含地面碎裂 |
| `SFX_CyberCenter_Fracture` | Cyber Center 的 Chaos 碎裂真正触发时 | Boss 脚下中心盘断裂 | 与 Chaos 破碎帧同步，不在 BeginPlay 提前播放 |

这两个声音允许叠加，因为一个属于 Boss，一个属于地面；需要分别用动画时机和碎裂事件触发。

### 6.7 Phase 2 场景转换

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_PhaseTransition_Start` | Boss HP 达到阶段阈值，扩散球在世界中心出现时 | Code 世界转换开始 | 播放一次，同时开始场景转换流程 |
| `SFX_PhaseTransition_Loop` | 扩散球半径持续扩大时 | Cyber 世界正在被 Code 世界逐步替换 | Start 后循环；球体尚未覆盖全场时持续播放 |
| `SFX_PhaseTransition_End` | 扩散结束，Code 地面、柱子、天空和 Boss 外观全部切换完成时 | Phase 2 正式锁定 | 先停止 Loop，再播放 End；随后才恢复正常攻击节奏 |

完整顺序固定为：

```text
Start → 循环 Loop 并扩张球体 → 停止 Loop + End → Phase 2 战斗
```

### 6.8 死亡与结算

| 文件 | 什么时候播放 | 它代表什么 | 播放规则 |
| --- | --- | --- | --- |
| `SFX_Boss_Death` | Boss HP 变成 0、Death Montage 开始时 | 最终痛吼、巨龙失去力量和数字核心崩溃 | 立即停止所有 Boss 攻击循环后播放；当前原型会叠加低音高 Boss Roar，正式资产需自带完整最终吼叫 |
| `SFX_Victory` | Boss 死亡流程稳定、Victory 结果界面出现时 | 玩家正式获胜 | Stereo、非空间化；不要取代 Boss Death，可在其后半段进入 |
| `SFX_Defeat` | Player HP 变成 0、Defeat 结果界面出现时 | 本局失败 | Stereo、非空间化；立即停止攻击循环和 Player 操作声 |

Victory 与 Defeat 互斥，一局只能播放其中一个。

### 6.9 建议试听顺序

不要按文件名从头到尾随便听，按游戏流程检查：

1. `Player_Dash → Player_JumpRelease`：确认 Player 是同一种柔软水滴乐器；
2. `BossBody_Rebound → WeakPoint_Hit`：确认失败撞击和成功命中差距足够大；
3. `Shockwave_Warning → Shockwave_Release`：确认倒计时自然落到释放重拍；
4. `Laser_Warning → Laser_Ignite → Laser_Loop → Laser_End`：确认是一句完整攻击；
5. `Barrage_Charge → Shot 01/02/03 连续交替 → Barrage_PlayerHit`：确认密集播放不刺耳；
6. `Boss_Takeoff + CyberCenter_Fracture`：确认两层叠加后仍能分辨；
7. `PhaseTransition_Start → Loop → End`：确认转阶段有开始、过程、完成；
8. `Boss_Death → Victory`，以及单独的 `Defeat`：确认结算关系正确。

## 7. 本机连接说明与自检

本机已有代理监听 `127.0.0.1:7897`。UE 自带旧版 Python/pip 与系统的 HTTPS 代理配置存在兼容问题。本轮通过在当前 PowerShell 进程中设置以下变量完成工具下载，不修改系统设置、不关闭 TLS 证书验证：

```powershell
$env:HTTPS_PROXY = 'http://127.0.0.1:7897'
```

仅在该本地代理正在运行时使用这行；其他电脑按自己的网络配置运行。

离线自检（不使用密钥或额度）：

```powershell
& $audioPython -m unittest discover -s tools -p test_generate_audio.py
```

覆盖清单和提示词、最短时长、音乐/循环请求、默认不联网、接口报错不泄露密钥且不自动重试。

## 8. 接口依据

- [ElevenLabs 音效生成接口](https://elevenlabs.io/docs/api-reference/text-to-sound-effects/convert)：音效时长 0.5～30 秒、循环参数及输出格式。
- [ElevenLabs 音乐生成接口](https://elevenlabs.io/docs/api-reference/music/compose)：音乐长度、纯器乐参数及输出格式。
- [ElevenLabs 音效说明](https://elevenlabs.io/docs/help-center/product/core-capabilities/sound-effects/what-is-sound-effects)：提示词最长 450 字符。

## 9. 本轮生成记录（2026-09-13）

已成功生成三个试听样本，均保留 MP3 原音和 48 kHz / 16-bit / 单声道 WAV：

| 样本 | WAV 目标时长 | 本地输出 |
| --- | ---: | --- |
| 玩家受击 | 0.35 秒 | [SFX_Player_Damaged.wav](../audio/generated/20260913T082627_301274Z/SFX_Player_Damaged.wav) |
| 玩家冲刺 | 0.30 秒 | [SFX_Player_Dash.wav](../audio/generated/20260913T082627_301274Z/SFX_Player_Dash.wav) |
| 弱点命中 | 0.70 秒 | [SFX_WeakPoint_Hit.wav](../audio/generated/20260913T082839_394767Z/SFX_WeakPoint_Hit.wav) |

弱点命中的第一次请求被接口以 `HTTP 400 / invalid_text_length` 拒绝，未返回音频。精简公共提示词、增加 450 字符本地检查后，只补生成该项并成功。前两个样本未重复生成。共三次成功请求、一次被拒绝请求；实际额度以账户使用记录为准。

六项离线测试通过。音频已完成格式及非空检查，尚未进行人工听感验收，也未导入 UE。此处试听文件被 Git 忽略，其他电脑仅拉取仓库不会获得文件。

试听反馈：第一版高频瞬态偏尖锐。第二版将三个提示词调整为圆润的中低频主体、柔和瞬态和清晰节奏脉冲；旧文件保留用于对比。

第二版试听文件：

| 样本 | 第二版 WAV |
| --- | --- |
| 玩家受击 | [SFX_Player_Damaged.wav](../audio/generated/20260913T090016_119783Z/SFX_Player_Damaged.wav) |
| 玩家冲刺 | [SFX_Player_Dash.wav](../audio/generated/20260913T090016_119783Z/SFX_Player_Dash.wav) |
| 弱点命中 | [SFX_WeakPoint_Hit.wav](../audio/generated/20260913T090016_119783Z/SFX_WeakPoint_Hit.wav) |

第二版三项均生成成功并通过格式、时长与非空检查，仍需人工试听确认节奏和听感。

随后音频方向升级为 v0.3：统一采用“柔软流体节拍 + 巨龙低频重拍 + 受控数字切分”，并重写全部 41 项 Prompt。默认提示词遵循度调整为 `0.4`。全部 Prompt 以需求文档为唯一来源，清单不再保留第二份内嵌文案；七项离线测试已通过。

v0.3 四个风格基准样本已于 2026-09-13 生成：

| 验证方向 | 样本 |
| --- | --- |
| 操作手感 | [SFX_Player_Dash.wav](../audio/generated/20260913T094229_648902Z/SFX_Player_Dash.wav) |
| 密集弹幕节奏 | [SFX_Barrage_Shot_01.wav](../audio/generated/20260913T094229_648902Z/SFX_Barrage_Shot_01.wav) |
| 攻击预警 | [SFX_Shockwave_Warning.wav](../audio/generated/20260913T094229_648902Z/SFX_Shockwave_Warning.wav) |
| 核心正反馈 | [SFX_WeakPoint_Hit.wav](../audio/generated/20260913T094229_648902Z/SFX_WeakPoint_Hit.wav) |

四项均保留原始 MP3，并转换为 48 kHz / 16-bit / Mono WAV。格式和非空检查通过，尚未进行人工听感验收。单颗弹丸候选的峰值明显低于另外三项，试听时需要重点判断其音色是否合适；暂不直接增益或重新生成，以免在密集播放前过早决定混音响度。

第一批剩余 20 项随后生成完成，运行目录为 [20260913T100502_803238Z](../audio/generated/20260913T100502_803238Z/)。20 个原始 MP3、20 个 WAV 和对应结果记录齐全，未发生接口失败。WAV 格式均通过检查；Victory 与 Defeat 为 Stereo，其余为 Mono。

四个基准样本和剩余 20 项已汇总到 [v0.3_first_batch_review](../audio/generated/v0.3_first_batch_review/)；这里共 24 个 WAV，方便连续试听。汇总目录只是副本，原始生成结果仍保留。所有生成目录均被 Git 忽略，尚未导入 UE，也没有做最终响度统一。

### v0.4 拟真改版

试听确认 v0.3 存在两个方向性问题：标志性声音被过度简化；旧转换流程按目标时长裁切，丢失了真实动作和衰减。v0.4 将重复微音效与标志性演出分开设计，转换流程改为完整保留接口返回音频，并新增独立的 Boss Roar。

已生成 [v0.4_realistic_showcase](../audio/generated/v0.4_realistic_showcase/)：

| 文件 | 时长 | 功能 |
| --- | ---: | --- |
| `SFX_Boss_Takeoff.wav` | 5.00 秒 | 完整巨龙起飞与多次振翅 |
| `SFX_Boss_Roar.wav` | 约 3.48 秒 | Shockwave 前摇中的真实巨龙咆哮 |
| `SFX_Anchor_Shatter.wav` | 2.20 秒 | Anchor 能量晶体和厚玻璃碎裂 |
| `SFX_WeakPoint_Hit.wav` | 1.60 秒 | 有效弱点的多层重击与玻璃爆碎 |

四个 WAV 均为 48 kHz / 16-bit / Mono，未裁切，格式和非空检查通过。峰值介于约 `-1.4 dBFS` 至 `-0.4 dBFS`，没有发现数字削波，但正式接入时需要统一降低增益并在整场战斗中混音。v0.4 清单共 42 项，八项离线测试通过。

在四个基准通过试听后，继续生成了第一批剩余 22 个战斗音效和剩余 4 个 Anchor 音效：

- 战斗运行目录：[20260913T113702_355554Z](../audio/generated/20260913T113702_355554Z/)，22 项；
- Anchor 运行目录：[20260913T113850_088513Z](../audio/generated/20260913T113850_088513Z/)，4 项；
- 加上四个拟真基准后，汇总目录为 [v0.4_combat_anchor_review](../audio/generated/v0.4_combat_anchor_review/)，共 30 项。

首次生成的 `SFX_Barrage_Shot_01` 峰值约 `-45.3 dBFS`，近似静音，未作为有效候选。它在 [20260913T114123_981795Z](../audio/generated/20260913T114123_981795Z/) 单独补生成，峰值约 `-1.6 dBFS`，并替换了汇总目录中的试听副本；失败原始文件仍保留，其他音效没有重复生成。

30 个汇总 WAV 均为 48 kHz / 16-bit，28 个 Mono，Victory 与 Defeat 为 Stereo；完整时长约 57.8 秒。部分文件出现非常接近 0 dBFS 的峰值，尚未进行最终响度和限幅处理，接入 UE 前需要统一混音。
