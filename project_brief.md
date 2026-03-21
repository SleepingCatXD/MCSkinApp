# Project Brief: MC Skin Dataset Manager (V1)

## 1. 项目概述
这是一个使用 C++17 和 Qt6 开发的桌面端应用，专为 Minecraft 皮肤和 2D 角色概念图提供本地化的数据整理、分类归档与 AI 训练（如 LoRA）打标服务。

## 2. 技术栈与构建系统
* **语言**: C++17
* **GUI 框架**: Qt6 (Widgets)
* **构建系统**: CMake (极其重要：不使用传统的 Visual Studio .sln 项目，必须通过 `CMakeLists.txt` 管理依赖和编译)
* **IDE 环境**: MSVC (Windows 64-bit)

## 3. 架构设计 (MVC 变体)
项目严格遵守“UI 与数据逻辑分离”的原则，分为两个平级的 CMake 子模块：
* **`DatasetCore` (静态库)**: 绝对禁止包含任何 Qt UI 头文件。专门负责底层的文件移动、ID 生成、目录解析以及 `metadata.jsonl` 的序列化/反序列化操作。核心入口类为 `DatasetManager`。
* **`MCSKinApp` (主程序)**: 依赖 `DatasetCore`。负责所有的 GUI 展示（`MCSkinApp.ui`, `AnnotationWindow.ui`, `GalleryWindow.ui`），将用户的点击行为转化为对 `DatasetManager` API 的调用。

## 4. 核心数据结构 (Schema)
所有标注数据统一存储在工作区下的 `Minecraft_Skin_Dataset/metadata.jsonl` 中。
**核心原则：隐式 Null 原则 (Implicit Null)**。如果某个旧数据没有对应的节点或属性，读取时自动视为空/False，禁止写向后兼容的热修复脚本。

典型的单行 JSON 结构如下：
```json
{
  "id": "000001",
  "meta": {
    "is_ai_generated": true,
    "is_commercial": false,
    "permission_ai": false,
    "author": "PixelArtist",
    "source_url": "https://..."
  },
  "ai_training": {
    "prompt": "1girl, white hair, blue eyes...",
    "negative_prompt": "bad anatomy..."
  },
  "attributes": {
    "hair_color": "white",
    "has_pompoms": true
  },
  "files": {
    "input_char": ["input_char.png"],
    "gt_skin": ["gt_skin.png"]
  }
}