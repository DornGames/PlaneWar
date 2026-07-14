# ✈️ Plane War

> A classic vertical-scrolling shoot 'em up built with C++ and EasyX — dodge enemies, level up your firepower, and defeat the boss!

🎮 **Download:** Grab the latest `.exe` from [Releases](https://github.com/DornGames/PlaneWar/releases)  
📦 **Repository:** [https://github.com/DornGames/PlaneWar](https://github.com/DornGames/PlaneWar)  
📬 **Contact:** Go to Issues.

---

## 🎯 About the Game

**Plane War** is a classic arcade-style vertical shooter. Take control of your fighter jet, shoot down waves of enemy aircraft, collect power-ups, and defeat the boss to advance to the next level. With smooth 60 FPS gameplay, dynamic difficulty scaling, and rich visual effects — all drawn with primitive graphics — this game delivers a nostalgic yet polished shoot 'em up experience.

---

## 🕹️ Gameplay Features

| Feature | Description |
| :--- | :--- |
| 🎮 **4 Game States** | Menu → Playing → Pause → Game Over, with smooth transitions |
| 🌟 **Parallax Starfield** | Multi-layer scrolling stars + gradient background for immersive depth |
| ✈️ **Player Aircraft** | Free movement, engine flame animation, invincibility flicker after hit |
| 🔫 **Firepower System** | 4 power levels with distinct bullet patterns — pick up blue arrows to upgrade |
| 👾 **3 Enemy Types** | Small / Medium / Large — large enemies fire back at you |
| 👹 **Boss Battles** | One boss per level with health bar, horizontal movement, and fan-shaped bullet hell |
| 💊 **Item System** | Health (heart), Power-up (blue arrow), Bomb (black bomb) |
| 💣 **Bomb Mechanism** | Screen clear + damage to all enemies on screen |
| 💥 **Explosion Effects** | Shockwave rings + core glow + flying particles |
| 📊 **HUD Panel** | Score, lives (hearts), bombs, level, high score |
| 📈 **Dynamic Difficulty** | Enemy speed and spawn rate increase with each level |

---

## 🎮 Controls

| Key | Action |
| :--- | :--- |
| `↑` `↓` `←` `→` / `W` `A` `S` `D` | Move aircraft |
| (Auto-fire) | No key needed — fires continuously |
| `X` | Release bomb (screen clear) |
| `P` | Pause / Resume |
| `Space` | Start game / Restart |
| `ESC` | Return to menu / Quit game |

---

## 🛠️ Tech Stack

- **Language:** C++
- **Graphics Library:** EasyX (https://easyx.cn)
- **Platform:** Windows (GDI-based)
- **Build System:** Visual Studio
- **Rendering:** Double-buffered batch drawing, 60 FPS target

---

## 📥 How to Download & Play

> **Note:** The executable is statically linked — no additional dependencies required. Just download and run!

1. Go to the [Releases](https://github.com/DornGames/PlaneWar/releases) page of this repository
2. Download the latest `PlaneWar.exe` file
3. Double-click the `.exe` to launch the game

No installation, no runtime libraries, no extra files needed — just one `.exe` and you're ready to fly!

---

## 🔧 Build from Source

If you prefer to build the game yourself:

1. **Install Visual Studio** (2019 or later) with C++ desktop development workload
2. **Install EasyX** from https://easyx.cn
3. Clone this repository
4. Open the `.cpp` file in Visual Studio
5. Set configuration to **Release** and platform to **x64** (or **x86**)
6. Build the project (`Ctrl + Shift + B`)
7. The `.exe` will be generated in the `Release` folder

> **Note:** The code uses `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")` to hide the console window, so the game runs as a pure GUI application.

---

## 📜 Guidelines

- ✅ Fan content only — no commercial use
- ✅ Feel free to fork, learn, and modify
- ✅ Keep code clean and well-commented if contributing
- ❌ No NSFW or offensive content

---

## 🏢 Team

**Sub-Team:** DornGames  
**Copyright © 2023-2026 DornGames. All Rights Reserved.**

**Parent Team:** Dorn Hub  
**Copyright © 2021-2026 Dorn Hub. All Rights Reserved.**

---

## ©️ License

This project is licensed under the **MIT License** — a permissive open-source license that allows anyone to use, copy, modify, merge, publish, distribute, sublicense, and sell copies of the software, with minimal restrictions.

**Copyright © 2026 DornGames / Dorn Hub**

---

*Last updated: July 14, 2026*

---

A toast with Prickly Pear juice!
Dorn Games & Dorn Hub
```

---

# ✈️ 飞机大战

> 一款基于 C++ 和 EasyX 的经典竖版射击游戏 — 躲避敌机、升级火力、击败 BOSS！

🎮 **下载：** 从 [Releases](https://github.com/DornGames/PlaneWar/releases) 获取最新 `.exe`  
📦 **仓库地址：** [https://github.com/DornGames/PlaneWar](https://github.com/DornGames/PlaneWar)  
📬 **联系方式：** 前往Issues。

---

## 🎯 关于游戏

**《飞机大战》** 是一款经典的街机风格竖版射击游戏。操控你的战机，击落一波波敌机，拾取道具升级火力，击败 BOSS 进入下一关。游戏以流畅的 60 FPS 运行，难度动态提升，特效全部由图形基元绘制，带来复古而精致的射击游戏体验。

---

## 🕹️ 游戏特色

| 特色 | 描述 |
| :--- | :--- |
| 🎮 **4 种游戏状态** | 菜单 → 游戏 → 暂停 → 结束，切换流畅 |
| 🌟 **视差星空背景** | 多层滚动星星 + 渐变底图，营造沉浸感 |
| ✈️ **玩家战机** | 自由移动、引擎尾焰动画、受击无敌闪烁 |
| 🔫 **火力系统** | 4 个等级，弹幕形态逐级变化 — 拾取蓝色箭头升级 |
| 👾 **3 种敌机** | 小型 / 中型 / 大型 — 大型敌机会主动射击 |
| 👹 **BOSS 战** | 每关一个 BOSS，带血条、水平移动、扇形弹幕 |
| 💊 **道具系统** | 回血（红心）、火力升级（蓝箭）、炸弹（黑色炸弹） |
| 💣 **炸弹机制** | 清屏 + 对全场敌人造成伤害 |
| 💥 **爆炸特效** | 冲击波圆环 + 内核发光 + 飞散粒子 |
| 📊 **HUD 面板** | 分数、生命（红心）、炸弹数、关卡、最高分 |
| 📈 **动态难度** | 敌机速度和生成频率随关卡提升 |

---

## 🎮 操作说明

| 按键 | 功能 |
| :--- | :--- |
| `↑` `↓` `←` `→` / `W` `A` `S` `D` | 移动战机 |
| （自动开火） | 无需按键，持续射击 |
| `X` | 释放炸弹（清屏） |
| `P` | 暂停 / 继续 |
| `空格` | 开始游戏 / 重新开始 |
| `ESC` | 返回菜单 / 退出游戏 |

---

## 🛠️ 技术栈

- **语言：** C++
- **图形库：** EasyX（https://easyx.cn）
- **平台：** Windows（基于 GDI）
- **构建工具：** Visual Studio
- **渲染方式：** 双缓冲批量绘制，60 FPS 目标

---

## 📥 如何下载与游玩

> **提示：** 可执行文件采用静态链接编译，无需额外依赖。下载即可运行！

1. 前往本仓库的 [Releases](https://github.com/DornGames/PlaneWar/releases) 页面
2. 下载最新的 `PlaneWar.exe` 文件
3. 双击 `.exe` 即可启动游戏

无需安装、无需运行库、无需额外文件 — 一个 `.exe` 就够了！

---

## 🔧 从源码构建

如果你想自行编译游戏：

1. **安装 Visual Studio**（2019 或更新版本），勾选“使用 C++ 的桌面开发”工作负载
2. **安装 EasyX**，从 https://easyx.cn 下载安装
3. 克隆本仓库
4. 在 Visual Studio 中打开 `.cpp` 文件
5. 将配置切换为 **Release**，平台选择 **x64**（或 **x86**）
6. 生成项目（`Ctrl + Shift + B`）
7. `.exe` 将生成在 `Release` 文件夹中

> **注意：** 代码中使用了 `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")` 来隐藏控制台黑框，游戏将以纯 GUI 窗口方式运行。

---

## 📜 基本准则

- ✅ 仅限粉丝内容 — 不得用于商业用途
- ✅ 欢迎 Fork、学习和修改
- ✅ 如需贡献，请保持代码整洁并添加注释
- ❌ 禁止 NSFW 或冒犯性内容

---

## 🏢 团队

**子团队：** DornGames  
**版权所有 © 2023-2026 DornGames。保留所有权利。**

**母团队：** Dorn Hub  
**版权所有 © 2021-2026 Dorn Hub。保留所有权利。**

---

## ©️ 许可证

本项目采用 **MIT 许可证** — 一个宽松的开源协议，允许任何人自由使用、复制、修改、合并、发布、分发、再授权和销售本软件，仅需保留版权声明。

**版权所有 © 2026 DornGames / Dorn Hub**

---

*最后更新：2026年7月14日*

---

敬上一杯刺梨汁！
Dorn Games & Dorn Hub
```
