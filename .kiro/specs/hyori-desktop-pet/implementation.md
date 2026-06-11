# 实现文档：Miyohashikori AI 桌宠

## 1. 文档目的

本文档用于描述当前项目的实现设计，重点包括：

- 主要业务逻辑
- 模块分层
- 类职责
- 关键成员变量
- 核心数据流

本文档以当前 MVP 阶段为主，不追求一次覆盖所有未来功能。

## 2. 总体架构

当前实现建议划分为 4 层：

1. 应用与路径层
2. 配置层
3. AI 会话层
4. UI 展示层

### 2.1 分层说明

#### 应用与路径层

负责应用公共路径、用户目录、配置目录和日志目录的统一管理。

#### 配置层

负责配置数据结构、JSON 序列化与反序列化、配置文件读写。

#### AI 会话层

负责：

- 组织 system prompt
- 维护上下文
- 调用 OpenAI 兼容接口
- 解析回复
- 解析 emotion
- 向 UI 发射结构化结果

#### UI 展示层

负责：

- 立绘显示
- 主窗口显示
- 文本输入
- 回复展示
- 窗口拖拽
- 菜单

## 3. 业务主流程

## 3.1 启动流程

1. `main.cpp` 创建 `QApplication`
2. 构造 `MainWindow`
3. `MainWindow` 创建 `ConfigManager`
4. `ConfigManager::load()` 读取 `~/.hyori/config.json`
5. `MainWindow` 创建 `OpenAiChatSession`
6. `MainWindow` 初始化 UI
7. 主窗口显示，若配置中有位置则恢复窗口位置

## 3.2 对话流程

1. 用户在输入框输入文本
2. 按回车提交
3. `MainWindow` 调用 `IAiSession::submit(text)`
4. `OpenAiChatSession` 检查配置是否完整
5. 将用户消息加入 `ChatHistory`
6. 构造 OpenAI 兼容请求 JSON
7. 使用 `QNetworkAccessManager` 发送异步请求
8. 收到返回后解析 `choices[0].message.content`
9. 使用 `EmotionParser` 提取 emotion 和清洗正文
10. 将助手消息加入 `ChatHistory`
11. 通过信号将文本和 emotion 发给 UI
12. `MainWindow` 更新显示文本
13. 后续阶段再根据 emotion 切换立绘

## 3.3 配置生成与恢复流程

1. 启动时检查 `~/.hyori` 目录是否存在
2. 检查 `config.json` 是否存在
3. 若不存在，则使用 `AppConfig::defaults()` 生成默认配置并写入
4. 若存在，则读取 JSON
5. 若 JSON 格式损坏，则回退为默认配置并覆盖原文件

## 4. 模块设计

## 4.1 `main.cpp`

### 职责

- 启动 Qt 应用
- 设置应用名称
- 创建并显示主窗口

### 主要对象

- `QApplication a`
- `MainWindow w`

## 4.2 `AppPaths`

文件：

- [core/apppaths.h](/D:/个人/GitHub/Miyohashikori/core/apppaths.h:1)
- [core/apppaths.cpp](/D:/个人/GitHub/Miyohashikori/core/apppaths.cpp:1)

### 职责

- 提供应用公共路径

### 主要函数

- `QString appDataRoot()`
- `QString configFilePath()`
- `QString logsRoot()`
- `QString assetsRoot()`
- `QString modesRoot()`

### 设计意义

- 避免各个模块自己拼接路径
- 后续日志、缓存、设置、资源路径都可从这里统一扩展

## 4.3 `AppConfig`

文件：

- [core/config/appconfig.h](/D:/个人/GitHub/Miyohashikori/core/config/appconfig.h:1)
- [core/config/appconfig.cpp](/D:/个人/GitHub/Miyohashikori/core/config/appconfig.cpp:1)

### 职责

- 定义配置字段
- 提供默认配置
- 提供 JSON 转换

### 主要字段

- `QString llmEndpoint`
- `QString llmApiKey`
- `QString llmModel`
- `QString ttsEndpoint`
- `QPoint windowPos`
- `bool voiceEnabled`
- `double volume`

### 主要函数

- `static AppConfig defaults()`
- `static AppConfig fromJson(const QJsonObject &obj)`
- `QJsonObject toJson() const`

### 设计意义

- 把“配置内容”与“配置文件读写”解耦
- 便于设置窗口和 AI 模块直接复用

## 4.4 `ConfigManager`

文件：

- [core/config/configmanager.h](/D:/个人/GitHub/Miyohashikori/core/config/configmanager.h:1)
- [core/config/configmanager.cpp](/D:/个人/GitHub/Miyohashikori/core/config/configmanager.cpp:1)

### 职责

- 加载配置
- 保存配置
- 保持当前配置快照

### 主要成员变量

- `AppConfig config_`

### 主要函数

- `bool load()`
- `bool save() const`
- `const AppConfig &config() const`
- `void setConfig(const AppConfig &config)`
- `bool ensureDataDirectory() const`

### 业务逻辑

- `load()` 负责自动创建目录与配置文件
- `save()` 使用 `QSaveFile` 安全写入
- 若文件损坏则回退默认配置

### 设计意义

- 所有配置访问统一从这里进入
- 避免主窗口或 AI 模块直接读写文件

## 4.5 `IAiSession`

文件：

- [core/ai/iaisession.h](/D:/个人/GitHub/Miyohashikori/core/ai/iaisession.h:1)

### 职责

- 定义 AI 会话的统一接口

### 主要函数

- `virtual void submit(const QString &userText) = 0`

### 主要信号

- `assistantMessage(const QString &text)`
- `assistantEmotion(const QString &token)`
- `sessionError(const QString &message)`

### 设计意义

- UI 不依赖具体模型实现
- 后续可切换不同 provider

## 4.6 `ChatHistory`

文件：

- [core/ai/chathistory.h](/D:/个人/GitHub/Miyohashikori/core/ai/chathistory.h:1)
- [core/ai/chathistory.cpp](/D:/个人/GitHub/Miyohashikori/core/ai/chathistory.cpp:1)

### 职责

- 管理最近上下文消息
- 输出 OpenAI 接口需要的 `messages` 数组

### 内部结构

```cpp
struct Message {
    QString role;
    QString content;
};
```

### 主要成员变量

- `QVector<Message> messages_`

### 主要函数

- `void clear()`
- `void addUserMessage(const QString &text)`
- `void addAssistantMessage(const QString &text)`
- `QJsonArray toOpenAiMessages(const QString &systemPrompt) const`
- `void trimToRecentTurns()`

### 当前约束

- 最多保留 20 条消息
- 对应最近 10 轮 user/assistant 上下文

### 设计意义

- 将上下文裁剪逻辑从 `OpenAiChatSession` 中独立出来

## 4.7 `EmotionParser`

文件：

- [core/ai/emotionparser.h](/D:/个人/GitHub/Miyohashikori/core/ai/emotionparser.h:1)
- [core/ai/emotionparser.cpp](/D:/个人/GitHub/Miyohashikori/core/ai/emotionparser.cpp:1)

### 职责

- 从模型原始回复中提取 emotion 标签
- 清洗最终展示文本

### 结果结构

```cpp
struct Result {
    QString text;
    QString emotion;
};
```

### 主要函数

- `static Result parse(const QString &rawText)`

### 当前支持的 emotion

- `happy`
- `shy`
- `neutral`
- `concerned`
- `excited`

### 解析规则

- 使用正则匹配 `[emotion:xxx]`
- 默认 emotion 为 `neutral`
- 取最后一个匹配结果
- 从正文中移除标签并 `trimmed()`

### 设计意义

- UI 永远处理结构化结果，而不是自行解析字符串

## 4.8 `OpenAiChatSession`

文件：

- [core/ai/openaichatsession.h](/D:/个人/GitHub/Miyohashikori/core/ai/openaichatsession.h:1)
- [core/ai/openaichatsession.cpp](/D:/个人/GitHub/Miyohashikori/core/ai/openaichatsession.cpp:1)

### 职责

- 调用 OpenAI 兼容聊天接口
- 组织请求 JSON
- 维护上下文
- 解析响应
- 处理重试

### 主要成员变量

- `QNetworkAccessManager *network_`
- `ConfigManager *configManager_`
- `ChatHistory history_`

### 内部结构

```cpp
struct PendingRequest {
    QString userText;
    int attempt = 0;
};
```

### 主要函数

- `void submit(const QString &userText) override`
- `QString buildSystemPrompt() const`
- `void sendRequest(const PendingRequest &request)`
- `void handleReply(QNetworkReply *reply, PendingRequest request)`
- `void retryRequest(const PendingRequest &request, const QString &reason)`

### 当前业务规则

#### 配置校验

发送前必须检查：

- `llmEndpoint`
- `llmApiKey`
- `llmModel`

若缺失则通过 `sessionError` 返回错误。

#### system prompt

当前 prompt 负责约束：

- 角色身份为圣代桥冰织
- 风格温柔、热恋、体贴、略带害羞
- 回复自然简洁
- 必须输出 `[emotion:xxx]`

#### 请求格式

使用 OpenAI 兼容接口：

- `model`
- `messages`
- `temperature`

#### 重试策略

- 最大总尝试次数：4 次
- 失败后的重试间隔：1 秒、2 秒、4 秒

### 设计意义

- 这是 AI 链路的核心实现
- 后续替换 provider 时，只需改这里或新增其他 `IAiSession` 实现

## 4.9 `SpriteCatalog`

文件：

- [core/spritecatalog.h](/D:/个人/GitHub/Miyohashikori/core/spritecatalog.h:1)
- [core/spritecatalog.cpp](/D:/个人/GitHub/Miyohashikori/core/spritecatalog.cpp:1)

### 当前职责

- 扫描 `assets/modes/`
- 管理当前模式和图片

### 当前主要成员变量

- `QString modesBase_`
- `QStringList modeNames_`
- `QString currentMode_`
- `QStringList frameFiles_`
- `QVector<QImage> frames_`
- `int currentIndex_`

### 当前问题

这个类当前更像“资源浏览器”，而不是“emotion 表情管理器”。

### 后续建议

应逐步改造成：

- `currentEmotion_`
- `availableEmotions_`
- `emotion -> image list` 映射

## 4.10 `CharacterSpriteView`

文件：

- [ui/characterspriteview.h](/D:/个人/GitHub/Miyohashikori/ui/characterspriteview.h:1)
- [ui/characterspriteview.cpp](/D:/个人/GitHub/Miyohashikori/ui/characterspriteview.cpp:1)

### 职责

- 负责立绘区域绘制
- 响应缩放与右键点击

### 后续建议

- 接 emotion 切换
- 接淡入淡出动画
- 逐步去掉“手动切帧”的原型意味

## 4.11 `MainWindow`

文件：

- [mainwindow.h](/D:/个人/GitHub/Miyohashikori/mainwindow.h:1)
- [mainwindow.cpp](/D:/个人/GitHub/Miyohashikori/mainwindow.cpp:1)

### 当前职责

- 构建桌宠主窗口
- 持有 `ConfigManager`
- 持有 `IAiSession`
- 接收 AI 回复并更新 UI
- 处理拖拽、菜单、位置恢复

### 主要成员变量

- `SpriteCatalog *catalog_`
- `CharacterSpriteView *sprite_`
- `QLabel *replyLabel_`
- `QLineEdit *inputLine_`
- `ConfigManager *configManager_`
- `IAiSession *ai_`
- `bool dragging_`
- `bool draggingStarted_`
- `QPoint dragOffset_`

### 主要函数

- `applyWindowChrome()`
- `persistWindowPosition() const`
- `wireAiSession()`
- `syncChromeToSprite()`
- `showPetMenu(const QPoint &globalPos)`
- `startDrag(const QPoint &globalPress)`
- `updateDrag(const QPoint &globalPos)`
- `endDrag()`

### 当前实现状态

- 已切换到 `OpenAiChatSession`
- 已支持配置读取
- 已支持窗口位置保存恢复
- 仍保留原型输入框与回复标签

### 当前局限

- `MainWindow` 仍承担了太多 UI 和交互职责
- 后续应逐步拆出 `InputDialog` 和 `SpeechBubble`

## 5. 当前配置字段说明

配置文件建议结构如下：

```json
{
  "llmEndpoint": "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions",
  "llmApiKey": "your-api-key",
  "llmModel": "qwen-plus",
  "ttsEndpoint": "",
  "windowPos": {
    "x": 1200,
    "y": 700
  },
  "voiceEnabled": true,
  "volume": 0.8
}
```

## 6. 当前推荐 AI 提供方

当前建议使用：

- 服务：阿里云百炼 DashScope
- 协议：OpenAI 兼容接口
- 模型：`qwen-plus`

原因：

- 接入简单
- 与当前 Qt/C++ HTTP 实现兼容度高
- 适合桌宠陪聊型场景

## 7. 后续扩展设计

## 7.1 `InputDialog`

建议后续新增：

- 多行输入框
- 历史记录区
- 发送按钮
- loading 状态

主要变量建议：

- `QPlainTextEdit *editor_`
- `QListWidget *historyList_`
- `QPushButton *sendButton_`
- `QLabel *statusLabel_`

## 7.2 `SpeechBubble`

建议后续新增：

- 独立气泡窗口
- 自动换行
- 自动消失

主要变量建议：

- `QLabel *textLabel_`
- `QTimer *hideTimer_`
- `QWidget *anchor_`

## 7.3 emotion 驱动的立绘管理

建议后续将 `SpriteCatalog` 重构为：

- `QString currentEmotion_`
- `QHash<QString, QStringList> emotionFrames_`
- `QTimer *restoreNeutralTimer_`

## 7.4 设置窗口

建议后续新增：

- `QLineEdit *llmEndpointEdit_`
- `QLineEdit *apiKeyEdit_`
- `QLineEdit *modelEdit_`
- `QLineEdit *ttsEndpointEdit_`
- `QCheckBox *voiceEnabledCheck_`
- `QSlider *volumeSlider_`

## 8. 当前实现边界

当前阶段暂不在代码中实现以下内容：

- 独立输入对话框
- 气泡窗口
- TTS
- 托盘
- 单实例
- 日志滚动

这些能力已经在需求层明确，但实现层暂只保留扩展接口和设计方向。

## 9. 推荐迭代顺序

建议按以下顺序继续开发：

1. `InputDialog`
2. `SpeechBubble`
3. `SpriteCatalog` emotion 化
4. emotion 驱动立绘切换
5. `SettingsDialog`
6. `TtsService`
7. Tray / Single Instance / Logger
