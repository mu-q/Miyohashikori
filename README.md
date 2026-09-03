# Miyohashikori

一个基于 Qt Widgets 的 Windows AI 桌宠。角色以《甜糖热恋》中的圣代桥冰织为原型：在桌面常驻显示立绘，支持接入 OpenAI 兼容的 LLM 进行陪伴式对话，并根据回复的情绪切换表情、播放本地日语语音。

> 本项目为非官方同人作品；角色及原作相关权利归原权利人所有。

## 当前功能

- 透明、无边框、置顶的桌宠窗口
- 立绘显示、右键模式切换，以及按 `emotion` 自动切换表情
- 文本输入与异步 LLM 对话，网络失败会自动重试
- 基于角色设定、few-shot 示例、最近 10 轮对话和短期记忆的回复上下文
- 解析 AI 回复末尾的 `[emotion:xxx]` 标签
- 按回复语义、关键词和情绪，从本地语音库中挑选日语语音播放
- 自动创建、读取并保存本地配置；窗口位置会在下次启动时恢复

目前支持的情绪标签为：`happy`、`shy`、`neutral`、`concerned`、`excited`。非 neutral 表情会在约 20 秒后恢复为 neutral。

## 环境要求

- Windows
- Qt 6（开发时使用 Qt 6.5.3，需包含 Widgets、Network、Multimedia 模块）
- 支持 C++17 的编译器，例如 MinGW 64-bit
- 一个 OpenAI Chat Completions 兼容接口的 API Key

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

构建产物通常位于构建目录下。运行程序时应保留项目的 `assets/` 与 `resources/voice/` 目录；开发运行时程序会自动向上查找这些资源。

## 配置

首次启动会生成一个默认配置。至少需要设置 `llmApiKey`：

```json
{
  "llmEndpoint": "https://api.deepseek.com/chat/completions",
  "llmApiKey": "请填写你的 API Key",
  "llmModel": "deepseek-chat",
  "ttsEndpoint": "",
  "voiceEnabled": true,
  "volume": 0.8,
  "windowPos": { "x": 1200, "y": 700 }
}
```

- `llmEndpoint`：OpenAI 兼容的 Chat Completions 地址。
- `llmApiKey`：服务商 API Key。不要将其提交到 Git 仓库或分享给他人。
- `llmModel`：模型名称。
- `voiceEnabled`：是否播放本地角色语音。
- `volume`：音量，范围 `0.0` 到 `1.0`。
- `windowPos`：窗口左上角坐标，由程序自动维护。

默认配置使用 DeepSeek；也可改为其他兼容服务及对应模型。配置损坏或缺失时，程序会回退到默认值并重新生成文件。

## 使用方式

- 在窗口底部输入文字，按 Enter 发送。
- 右键点击立绘可切换立绘模式或退出程序。
- 按住 Alt 后左键拖动立绘可移动窗口。
- 按住 Alt 后滚动鼠标滚轮可缩放立绘。

AI 的回复要求在结尾带有 `[emotion:xxx]`；该标签不会显示在回复气泡中，而是用于驱动表情和语音选择。

## 项目结构

```text
.
├─ mainwindow.*              # 桌宠窗口、输入、拖动、菜单与 UI 协调
├─ core/
│  ├─ ai/                    # 对话会话、上下文、emotion 解析
│  ├─ config/                # 本地 JSON 配置
│  ├─ spritecatalog.*        # 立绘模式与 emotion 映射
│  └─ voiceplayer.*          # 本地语音匹配与播放
├─ ui/                       # 立绘视图与回复气泡
├─ assets/modes/default/     # 按 emotion 命名的默认立绘
├─ resources/txt/            # few-shot 与语音索引资源
└─ resources/voice/          # 本地日语语音资源
```

立绘模式目录中使用 `neutral.png`、`happy.png`、`shy.png`、`concerned.png`、`excited.png` 等以情绪命名的图片。缺少某个情绪的图片时，会保持当前立绘不变。

## 已知边界与后续方向

当前版本仍是桌宠 MVP，暂未提供图形化设置页、独立输入/聊天记录窗口、系统托盘、单实例保护、日志系统和在线 TTS。API 配置目前需要手动编辑 JSON 文件。

更完整的需求与设计记录见 [requirements.md](.kiro/specs/hyori-desktop-pet/requirements.md) 和 [implementation.md](.kiro/specs/hyori-desktop-pet/implementation.md)。其中部分早期规划已经完成，README 以当前代码行为为准。
