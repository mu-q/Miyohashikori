# 需求文档：圣代桥冰织 AI 桌面宠物

## 简介

本文档定义了基于 Qt Widgets 的 AI 桌面宠物应用需求。角色为《甜糖热恋》中的圣代桥冰织，使用静态/序列立绘图片展示角色，集成 LLM 对话和 TTS 语音合成，为用户提供陪伴式交互体验。

## 术语

- **Desktop_Pet_Window**: 显示角色立绘的透明桌面窗口（QWidget）
- **Sprite_Image**: 角色立绘 PNG 图片（带透明通道）
- **Expression**: 角色当前表情状态，对应一张立绘图片
- **Speech_Bubble**: 显示角色对话文字的气泡组件
- **Input_Dialog**: 用户输入文字的对话框
- **AI_Dialog_System**: 基于 LLM HTTP API 的对话生成模块
- **TTS_System**: 基于 GPT-SoVITS HTTP API 的语音合成模块
- **Config**: 存储在 JSON 文件中的用户配置

## 需求

### 需求 1：桌面窗口与立绘显示

**用户故事：** 作为用户，我希望在桌面上看到冰织的立绘图片，以便获得视觉陪伴。

#### 验收标准

1. WHEN 应用启动，THE Desktop_Pet_Window SHALL 显示一个无边框、背景透明的窗口，并在其中渲染角色立绘
2. THE Desktop_Pet_Window SHALL 使用 `Qt::FramelessWindowHint` 和 `Qt::WA_TranslucentBackground` 实现透明背景
3. THE Desktop_Pet_Window SHALL 始终置于其他窗口之上（`Qt::WindowStaysOnTopHint`）
4. THE Sprite_Image SHALL 为带 alpha 通道的 PNG 文件，支持不规则轮廓
5. WHEN 用户按住立绘拖动，THE Desktop_Pet_Window SHALL 跟随鼠标移动到新位置
6. WHEN 用户右键点击立绘，THE Desktop_Pet_Window SHALL 弹出上下文菜单（包含：打开对话、设置、退出）

### 需求 2：表情切换

**用户故事：** 作为用户，我希望冰织根据对话内容切换不同表情立绘，以便交互更生动。

#### 验收标准

1. THE Desktop_Pet_Window SHALL 支持至少 5 种表情立绘：neutral（默认）、happy（开心）、shy（害羞）、concerned（担心）、excited（兴奋）
2. WHEN AI 回复包含情感标签 `[emotion:xxx]`，THE AI_Dialog_System SHALL 解析该标签并通知 Desktop_Pet_Window 切换到对应表情
3. THE AI_Dialog_System SHALL 在系统 Prompt 中要求 LLM 根据用户输入的情绪和 AI 自身回复的内容综合判断情感，选择最合适的情感标签附加在回复末尾
4. WHEN 切换表情，THE Desktop_Pet_Window SHALL 使用淡入淡出动画（`QPropertyAnimation` 控制 `windowOpacity`，时长 200ms）
5. IF 指定表情的立绘文件不存在，THEN THE Desktop_Pet_Window SHALL 保持当前表情不变
6. WHEN 对话结束 10 秒后，THE Desktop_Pet_Window SHALL 自动恢复为 neutral 表情

### 需求 3：用户输入与对话

**用户故事：** 作为用户，我希望能输入文字与冰织对话，以便获得陪伴和情感交流。

#### 验收标准

1. WHEN 用户左键单击立绘或按下全局快捷键（默认 `Ctrl+Shift+H`），THE Input_Dialog SHALL 弹出
2. THE Input_Dialog SHALL 包含一个多行文本输入框，最大输入长度为 500 字符
3. WHEN 用户提交输入（点击发送或按 `Ctrl+Enter`），THE AI_Dialog_System SHALL 发送请求并显示加载动画
4. THE Speech_Bubble SHALL 显示在立绘旁边，展示 AI 的回复文字
5. THE Speech_Bubble SHALL 在显示 8 秒后自动消失
6. THE Input_Dialog SHALL 保留最近 20 条对话记录供用户查看
7. IF AI 服务不可用，THEN THE Input_Dialog SHALL 显示错误提示并提供预设回复

### 需求 4：AI 对话系统

**用户故事：** 作为用户，我希望冰织的回复符合其温柔热恋的角色性格，以便获得真实的角色体验。

#### 验收标准

1. THE AI_Dialog_System SHALL 通过 HTTP POST 请求调用 OpenAI 兼容格式的 LLM API
2. THE AI_Dialog_System SHALL 在每次请求中携带系统 Prompt，定义冰织的性格（温柔体贴、热恋女友、偶尔害羞）
3. THE AI_Dialog_System SHALL 维护最近 10 轮对话历史作为上下文
4. THE AI_Dialog_System SHALL 要求 LLM 在回复末尾附加情感标签，格式为 `[emotion:xxx]`（xxx 为 happy/shy/neutral/concerned/excited 之一）
5. THE AI_Dialog_System SHALL 在请求失败时最多重试 3 次（间隔 1s、2s、4s）
6. THE AI_Dialog_System SHALL 使用 `QNetworkAccessManager` 异步发送请求，不阻塞 UI 线程

### 需求 5：语音合成

**用户故事：** 作为用户，我希望听到冰织的语音回复，以便获得更沉浸的体验。

#### 验收标准

1. WHEN AI 生成文字回复后，THE TTS_System SHALL 调用 GPT-SoVITS HTTP API 合成语音
2. THE TTS_System SHALL 使用 `QMediaPlayer` 播放合成的音频
3. THE TTS_System SHALL 缓存已合成的音频（最多 50 条，LRU 淘汰）
4. THE Input_Dialog SHALL 提供语音开关，用户可随时启用/禁用语音
5. IF 语音合成失败，THEN THE Desktop_Pet_Window SHALL 仅显示文字回复，不影响其他功能
6. THE TTS_System SHALL 支持调节播放音量（0.0 ~ 1.0）

### 需求 6：配置管理

**用户故事：** 作为用户，我希望能配置 AI 和语音服务地址，以便连接自己部署的后端。

#### 验收标准

1. THE Desktop_Pet_Window 右键菜单 SHALL 提供"设置"入口，打开配置对话框
2. THE Config SHALL 以 JSON 格式保存在用户目录（`~/.hyori/config.json`）
3. THE Config SHALL 包含：LLM endpoint、LLM API key、TTS endpoint、窗口默认位置、语音开关和音量
4. WHEN 用户保存设置，THE Desktop_Pet_Window SHALL 立即应用新配置，无需重启
5. IF 配置文件不存在或损坏，THE Desktop_Pet_Window SHALL 使用默认配置并自动创建配置文件

### 需求 7：单实例运行

**用户故事：** 作为用户，我希望应用只运行一个实例，以便避免重复启动。

#### 验收标准

1. WHEN 应用启动，SHALL 检测是否已有实例在运行
2. IF 已有实例运行，THEN 新启动的进程 SHALL 激活现有窗口并退出自身
3. THE 单实例检测 SHALL 使用 `QSharedMemory` 实现

### 需求 8：系统托盘

**用户故事：** 作为用户，我希望通过系统托盘管理桌宠，以便在不需要时隐藏立绘。

#### 验收标准

1. THE Desktop_Pet_Window SHALL 在系统托盘显示图标
2. THE 托盘图标右键菜单 SHALL 包含：显示/隐藏立绘、打开对话、退出
3. WHEN 用户点击托盘图标，THE Desktop_Pet_Window SHALL 切换显示/隐藏状态
4. WHEN 用户选择退出，THE Desktop_Pet_Window SHALL 清理资源并关闭应用

### 需求 9：性能与稳定性

**用户故事：** 作为用户，我希望应用轻量稳定，不影响系统性能。

#### 验收标准

1. THE Desktop_Pet_Window SHALL 在 3 秒内完成启动
2. THE Desktop_Pet_Window SHALL 在隐藏状态下 CPU 占用接近 0%
3. THE AI_Dialog_System 和 THE TTS_System 的所有网络请求 SHALL 异步执行，不阻塞 UI
4. WHEN 发生未捕获异常，THE Desktop_Pet_Window SHALL 记录日志并显示错误提示，不崩溃退出
5. THE Desktop_Pet_Window SHALL 将运行日志写入 `~/.hyori/logs/hyori.log`（最大 10MB，滚动保留 3 个）
