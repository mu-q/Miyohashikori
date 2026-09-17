# Miyohashikori

一个基于 Qt Widgets 的 Windows AI 桌宠。角色以《甜糖热恋》中的圣代桥冰织为原型：在桌面常驻显示立绘，支持接入 OpenAI 兼容的 LLM 进行陪伴式对话，并根据回复的情绪切换表情、通过 GPT-SoVITS 合成语音。

> 本项目为非官方同人作品；角色及原作相关权利归原权利人所有。

## 免安装版快速开始

发布者提供的完整 ZIP 已包含程序、Qt 运行库、SQLite 驱动、立绘和本地语音库。使用者无需安装 Qt：

1. 将 ZIP **完整解压**到普通目录，不要直接在压缩软件中运行。
2. 双击 `Miyohashikori.exe`。
3. 首次运行会在 `%USERPROFILE%\.hyori` 创建配置和数据库。退出程序后打开其中的 `config.json`，填写自己的 `llmApiKey`，再重新启动。
4. Windows SmartScreen 若提示未知发布者，请核对发布页给出的 SHA-256；未购买代码签名证书的个人构建通常会出现该提示。

用户的配置、日记、笔记、课表和缓存位于 `%USERPROFILE%\.hyori`，不会写进程序目录。因此升级时可以解压到新目录直接运行，原有数据会继续使用。不要把自己的 `.hyori` 目录、API Key 或数据库打进发布包。

## 当前功能

- 透明、无边框、置顶的桌宠窗口，并保留 Windows 任务栏入口
- 立绘显示、右键模式切换，以及按 `emotion` 自动切换表情
- 文本输入与异步 LLM 对话，网络失败会自动重试
- 基于角色设定、few-shot 示例、最近 10 轮对话和短期记忆的回复上下文
- 解析 AI 回复末尾的 `[emotion:xxx]` 标签
- 可调用本机 GPT-SoVITS API 合成回复语音，并缓存已生成的音频
- TTS 未配置或合成失败时，自动退回本地日语语音库
- 自动创建、读取并保存本地配置；窗口位置会在下次启动时恢复
- 专注模式、番茄钟、背景视频与本次运行的对话记录窗口
- 启动时同时显示透明桌宠和“陪伴与专注”主窗口；主窗口包含角色场景、圆形番茄钟、课程侧栏与手账入口
- 专注页课程表侧边栏：多学期、手动课程、Excel/CSV 导入、仅显示本周课程，以及独立周课表窗口和课前 30/20 分钟语音提醒
- 半透明手账窗口：按日期保存日记、管理带标签的笔记，并从左侧栏浏览历史记录
- 基于 SQLite 的工作待办数据底座（业务界面待后续接入）

目前支持的情绪标签为：`happy`、`shy`、`neutral`、`concerned`、`excited`。非 neutral 表情会在约 20 秒后恢复为 neutral。

## 环境要求

- Windows
- Qt 6（开发时使用 Qt 6.5.3，需包含 Widgets、Network、Multimedia、SQL 模块及 QSQLITE 驱动）
- 支持 C++17 的编译器，例如 MinGW 64-bit
- 一个 OpenAI Chat Completions 兼容接口的 API Key
- 可选：已安装并训练好模型的 GPT-SoVITS v2Pro 整合包

项目使用 qmake，工程文件为 [Miyohashikori.pro](Miyohashikori.pro)。

## 构建与运行

1. 使用 Qt Creator 打开 `Miyohashikori.pro`。
2. 选择已安装的 Qt 6 Kit（例如 Desktop Qt 6.5.3 MinGW 64-bit）。
3. 构建并运行项目。
4. 首次运行后，程序会在用户目录下生成配置文件：`~/.hyori/config.json`。
5. 退出程序，编辑该配置文件填入 API Key，再重新启动程序。

也可以在已配置 Qt 命令行环境的终端中执行：

```powershell
qmake Miyohashikori.pro
mingw32-make
```

## 制作 Windows 免安装版

先构建 Release：

```powershell
qmake CONFIG+=release Miyohashikori.pro
mingw32-make -j4
```

再运行发布脚本。下面的“完整包”会收集 Qt/MinGW 运行库、QSQLITE、立绘、图标和整个本地语音库，并生成 ZIP：

```powershell
.\scripts\deploy_windows.ps1 `
  -Executable .\release\Miyohashikori.exe `
  -Destination .\dist\Miyohashikori-portable-2026.09.16 `
  -QtBin D:\path\to\Qt\6.5.3\mingw_64\bin `
  -CompilerBin D:\path\to\Qt\Tools\mingw1120_64\bin `
  -Version 2026.09.16 `
  -IncludeVoiceLibrary `
  -CreateZip
```

若只想制作体积较小的测试包，可去掉 `-IncludeVoiceLibrary`；但该包不包含本地语音播放和 TTS 失败后的语音回退，不建议作为“完整功能版”发布。也可去掉 `-CreateZip`，只生成待检查的目录。

脚本要求目标目录不存在或为空，以免旧 DLL 混入；结束前会检查关键 Qt DLL、`platforms/qwindows.dll`、`sqldrivers/qsqlite.dll`、立绘及可选语音资源，并生成 `release-manifest.txt`（含版本、文件数、大小和 EXE SHA-256）。发布前建议：

1. 在未安装 Qt 的 Windows 10/11 64 位电脑或 Windows Sandbox 中完整解压并启动。
2. 验证桌宠和主窗口同时出现，并检查对话、语音、课程导入、课前提醒、日记/笔记以及重启后的数据持久化。
3. 确认发布目录中没有 `config.json`、`hyori.db`、API Key、缓存或个人课程文件。
4. 在发布页同时提供版本号、ZIP 的 SHA-256、系统要求和已知问题。

本地语音库、立绘、角色素材以及 GPT-SoVITS 模型可能受各自许可或原权利人条款约束；公开分发前请确认你拥有相应发布权限。

## 配置

首次启动会生成一个默认配置。至少需要设置 `llmApiKey`：

```json
{
  "llmEndpoint": "https://api.deepseek.com/chat/completions",
  "llmApiKey": "请填写你的 API Key",
  "llmModel": "deepseek-chat",
  "ttsEnabled": true,
  "ttsEndpoint": "http://127.0.0.1:9880/tts",
  "ttsReferenceAudioPath": "C:/path/to/Miyohashikori/resources/voice/ko/ko0007.ogg",
  "ttsReferenceText": "そうですね。帰って温かいミルクティでも",
  "ttsReferenceLanguage": "ja",
  "ttsTextLanguage": "ja",
  "ttsSpeedFactor": 1.0,
  "voiceEnabled": true,
  "volume": 0.8,
  "windowPos": { "x": 1200, "y": 700 }
}
```

- `llmEndpoint`：OpenAI 兼容的 Chat Completions 地址。
- `llmApiKey`：服务商 API Key。不要将其提交到 Git 仓库或分享给他人。
- `llmModel`：模型名称。
- `ttsEnabled`：是否优先使用 GPT-SoVITS 合成语音。
- `ttsEndpoint`：GPT-SoVITS v2 API 的 `/tts` 地址。
- `ttsReferenceAudioPath`：参考音频的本机绝对路径。完整免安装版首次运行会自动填写随包参考音频的实际路径，通常无需手工修改。
- `ttsReferenceText`：参考音频的准确台词，必须与音频内容一致。
- `ttsReferenceLanguage`、`ttsTextLanguage`：参考音频与待合成文本的语言代码；当前桌宠显示中文，但会把同一回复的隐藏日语译文交给 TTS。
- `ttsSpeedFactor`：合成语速，推荐从 `1.0` 开始调整。
- `voiceEnabled`：语音总开关；关闭后 TTS 和本地语音均不播放。
- `volume`：音量，范围 `0.0` 到 `1.0`。
- `windowPos`：窗口左上角坐标，由程序自动维护。

默认配置使用 DeepSeek；也可改为其他兼容服务及对应模型。配置缺失时程序会生成默认文件；配置损坏时会先将原文件备份为 `config.json.invalid-时间戳.bak`，再生成默认配置。

## 启动 GPT-SoVITS TTS

GPT-SoVITS、推理环境和模型体积较大，且许可各异，**不会被免安装发布脚本打进 ZIP**；不安装它也能使用随包本地语音。需要实时合成时，请另行准备兼容的 GPT-SoVITS v2Pro 整合包和以下训练结果：

- SoVITS：`hyori_v2pro_e8_s1008.pth`
- GPT：`hyori_v2pro-e10.ckpt`

先关闭训练 WebUI 和推理 WebUI，再在项目根目录运行（免安装版使用 `tools\start_hyori_tts.ps1`）：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\start_hyori_tts.ps1
```

这个窗口需要保持运行。看到服务监听 `127.0.0.1:9880` 后，再启动桌宠。桌宠收到完整回复后会异步请求 `/tts`，生成的 WAV 会缓存在 `~/.hyori/cache/tts/`；接口不可用、参考音频无效或合成失败时，会自动播放原有本地语音。

脚本默认使用 `D:\Program\project\GPT-SoVITS-v2pro-20250604`。若整合包移动过，可显式传入路径：

```powershell
.\scripts\start_hyori_tts.ps1 -GptSovitsRoot "D:\path\to\GPT-SoVITS" -Port 9880
```

## 使用方式

- 在窗口底部输入文字，按 Enter 发送。
- 点击任务栏中的“冰织”可在桌宠与其他应用之间切换；右键立绘选择“隐藏冰织”会最小化到任务栏，再次点击任务栏按钮即可恢复。
- 点击输入框右侧的圆形书页按钮打开“冰织手账”；悬浮按钮可查看用途。左侧可切换日记/笔记、选择历史记录或新建内容，右侧负责编辑、保存和删除。
- 日记每天限一篇；笔记支持使用中文或英文逗号分隔标签。切换记录或关闭窗口时，未保存的内容会提示保存。
- 右键点击立绘可切换立绘模式或退出程序。
- 按住 Alt 后左键拖动立绘可移动窗口。
- 按住 Alt 后滚动鼠标滚轮可缩放立绘。
- 在专注模式右侧的“学期”菜单中新建学期，然后用“＋课程”手动填写；双击课程可编辑。
- 侧边栏只列出当前教学周实际发生的课程；点击“查看周课表”可在独立窗口按时间轴查看一周安排，并切换上一周或下一周。
- 先创建并选择学期，再点击侧边栏底部“导入 Excel / CSV”。程序会自动识别常见中英文表头和列顺序；课程名称必须存在，星期、时间和教学周可使用独立列，也可使用 `时间段`、`周次`，或由首次/最后上课日期推导。教师和教室可选。导入会追加到当前学期，不覆盖已有课程。
- `.xlsx`、`.csv` 和 `.tsv` 可直接读取，无需安装 Microsoft Excel；旧版 `.xls` 在 Windows 下通过本机 Excel 读取，或可先另存为 `.xlsx`/UTF-8 CSV。
- 桌宠运行期间，当前学期的课程会在课前 30 分钟和 20 分钟各提醒一次；语音总开关关闭时只显示文字。

AI 的回复要求在结尾带有 `[emotion:xxx]`；该标签不会显示在回复气泡中，而是用于驱动表情和语音选择。

## 项目结构

```text
.
├─ mainwindow.*              # 桌宠窗口、输入、拖动、菜单与 UI 协调
├─ core/
│  ├─ ai/                    # 对话会话、上下文、emotion 解析
│  ├─ config/                # 本地 JSON 配置
│  ├─ data/                  # SQLite 连接、迁移、数据模型与仓储
│  ├─ schedule/              # Excel/CSV 导入与课程提醒调度
│  ├─ spritecatalog.*        # 立绘模式与 emotion 映射
│  ├─ ttsclient.*            # GPT-SoVITS 请求与合成音频缓存
│  └─ voiceplayer.*          # 合成音频及本地语音播放
├─ ui/                       # 立绘、回复气泡、专注页与课程表侧边栏
├─ assets/modes/default/     # 按 emotion 命名的默认立绘
├─ resources/txt/            # few-shot 与语音索引资源
└─ resources/voice/          # 本地日语语音资源
```

立绘模式目录中使用 `neutral.png`、`happy.png`、`shy.png`、`concerned.png`、`excited.png` 等以情绪命名的图片。缺少某个情绪的图片时，会保持当前立绘不变。

## 已知边界与后续方向

当前版本仍是桌宠 MVP，暂未提供图形化设置页、待办业务界面、系统托盘、单实例保护和日志系统。LLM 与本地 TTS 配置目前需要手动编辑 JSON 文件，GPT-SoVITS 服务也需要单独启动。业务数据保存在 `~/.hyori/hyori.db`，配置仍保存在 `~/.hyori/config.json`。

更完整的需求与设计记录见 [requirements.md](.kiro/specs/hyori-desktop-pet/requirements.md) 和 [implementation.md](.kiro/specs/hyori-desktop-pet/implementation.md)。其中部分早期规划已经完成，README 以当前代码行为为准。
