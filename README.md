# Plane War

---

## About the Game

**Plane War** is a complete vertical-scrolling shoot 'em up game built with C++ and the EasyX graphics library. Take control of your fighter jet, shoot down waves of enemy aircraft, collect power-ups, defeat bosses, and climb the leaderboard. All graphics are rendered using primitive drawing functions, delivering a nostalgic arcade experience with smooth 60 FPS gameplay.

---

### Features

| Category | Features |
| :--- | :--- |
| 🎮 **Game States** | Main Menu → Playing → Paused (with Info Panel) → Game Over / Victory |
| ✈️ **Player System** | WASD/Arrow key movement, lives (upgradeable cap to 5), invincibility after hit, engine flame animation |
| 🔫 **Firepower System** | 3 fire levels (single/double/triple spread), permanent damage upgrade (max level 4) |
| 👾 **Enemy System** | 7 enemy types: Normal / Fast / Shooter / Zigzag / Tank / Diver / Strafer, each with unique movement and attack patterns |
| 👹 **Boss Battles** | One boss per level with health bar, 6 attack patterns (Ring / Aim / Spiral / Petal / Laser / Summon) |
| 💊 **Item System** | 4 temporary items (Fire Up / Heal / Shield / Bomb) + 2 permanent rare items (Max Life / Power Up) |
| 💥 **Visual Effects** | Explosion particles, hit flash, screen shake, laser ultimate, shield aura, parallax scrolling starfield |
| 📊 **HUD** | Score, lives (heart icons), level, firepower level, bomb count, energy bar (ultimate) |
| 🏆 **Leaderboard** | Local file storage, top 10 scores with player names |
| 📖 **Enemy Codex** | Press I while paused to view player stats and enemy data for current level |

---

### Controls

| Key | Action |
| :--- | :--- |
| `W` `A` `S` `D` / `↑` `←` `↓` `→` | Move aircraft (8-directional) |
| `Space` | Auto-fire (hold down) |
| `X` | Release laser ultimate (requires full energy) |
| `R` | Dash (quick directional dash + brief invincibility) |
| `B` | Drop bomb (screen clear + damage to boss) |
| `P` | Pause / Resume |
| `I` | Toggle Info Panel (while paused) |
| `Enter` / `Space` | Start game / Return to menu (on result screens) |
| `ESC` | Return to main menu / Quit game |

---

### Tech Stack

- **Language:** C++ (C++11 and above)
- **Graphics Library:** EasyX (https://easyx.cn)
- **Platform:** Windows
- **IDE:** Visual Studio
- **Rendering:** Double-buffered batch drawing, 60 FPS target
- **Audio:** Optional (controlled by `ENABLE_SOUND` macro), uses `winmm.lib` for `.wav` playback

---

### Download & Play

> The executable is statically linked — no additional dependencies required. Just download and run!

1. Go to the [Releases](https://github.com/DornGames/PlaneWar/releases) page
2. Download the latest `PlaneWar.exe`
3. Double-click the `.exe` to launch the game

No installation, no runtime libraries, no extra files — just one `.exe` and you're ready to fly!

---

### Build from Source

**Prerequisites:**
- Visual Studio (2019 or later) with "Desktop development with C++" workload
- EasyX graphics library (download from https://easyx.cn)

**Steps:**
1. Clone this repository
2. Open the `.cpp` file in Visual Studio
3. Set character set to **Use Multi-Byte Character Set** or enable `/utf-8` option
4. Switch configuration to **Release**, platform to **x64** (or **x86**)
5. Build the project (`Ctrl + Shift + B`)
6. The `.exe` will be generated in the `Release` folder

> **Note:** The code uses `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")` to hide the console window, so the game runs as a pure GUI application.

---

### Team & Copyright

**Sub-Team:** DornGames  
**Copyright © 2023-2026 DornGames. All Rights Reserved.**

**Parent Team:** Dorn Hub  
**Copyright © 2021-2026 Dorn Hub. All Rights Reserved.**

---

### License

This project is licensed under the **MIT License** — a permissive open-source license that allows anyone to use, copy, modify, merge, publish, distribute, sublicense, and sell copies of the software, with minimal restrictions.

**Copyright © 2026 DornGames / Dorn Hub**

---

# 飞机大战

---

## 关于游戏

**《飞机大战》** 是一款基于 C++ 和 EasyX 图形库开发的完整纵版飞行射击游戏。操控你的战机，击落一波波敌机，收集道具强化自身，击败每关的 Boss，冲击排行榜。所有图形均使用绘图基元渲染，带来流畅 60 FPS 的复古街机体验。

---

### 功能特性

| 类别 | 功能 |
| :--- | :--- |
| 🎮 **游戏状态** | 主菜单 → 游戏进行 → 暂停（含属性面板）→ 失败 / 通关 |
| ✈️ **玩家系统** | WASD/方向键移动、生命值（可提升上限至 5）、受击无敌闪烁、引擎尾焰动画 |
| 🔫 **火力系统** | 3 级火力（单发/双发/三发散射），永久伤害提升（上限 4 级） |
| 👾 **敌机系统** | 7 种敌机：普通/快速/射击/蛇形/装甲/俯冲/横掠，每种拥有独特的移动与攻击模式 |
| 👹 **Boss 战** | 每关一个 Boss，带独立血条，6 种弹幕招式（环形/瞄准/螺旋/花瓣/激光/召唤） |
| 💊 **道具系统** | 4 种临时道具（火力增强/回血/护盾/炸弹）+ 2 种永久稀有道具（生命上限/攻击力提升） |
| 💥 **视觉效果** | 爆炸粒子、受击白闪、屏幕震动、激光大招、护盾光环、视差滚动星空背景 |
| 📊 **HUD 界面** | 分数、生命（爱心图标）、关卡、火力等级、炸弹数量、能量条（大招） |
| 🏆 **排行榜** | 本地文件存储，记录历史前 10 名玩家成绩 |
| 📖 **敌机图鉴** | 暂停时按 I 键打开，查看我方属性与当前关卡敌机详细数据 |

---

### 操作说明

| 按键 | 功能 |
| :--- | :--- |
| `W` `A` `S` `D` / `↑` `←` `↓` `→` | 移动战机（支持八方向） |
| `空格` | 自动连射（按住即可） |
| `X` | 释放激光大招（需能量集满） |
| `R` | 闪避冲刺（朝当前方向快速位移 + 短暂无敌） |
| `B` | 投放炸弹（清屏 + 对 Boss 造成伤害） |
| `P` | 暂停 / 继续游戏 |
| `I` | 暂停时打开/关闭属性图鉴面板 |
| `回车` / `空格` | 开始游戏 / 结算界面返回菜单 |
| `ESC` | 返回主菜单 / 退出游戏 |

---

### 技术栈

- **编程语言：** C++（C++11 及以上）
- **图形库：** EasyX（https://easyx.cn）
- **平台：** Windows
- **开发环境：** Visual Studio
- **渲染方式：** 双缓冲批量绘制，60 FPS 目标
- **音频支持：** 可选（通过 `ENABLE_SOUND` 宏控制），使用 `winmm.lib` 播放 `.wav`

---

### 下载与运行

> 可执行文件采用静态链接编译，无需安装额外运行库，下载即可运行。

1. 前往本仓库的 [Releases](https://github.com/DornGames/PlaneWar/releases) 页面
2. 下载最新的 `PlaneWar.exe` 文件
3. 双击 `.exe` 文件即可启动游戏

无需安装、无需配置、无需额外文件 —— 一个 `.exe` 就够了！

---

### 从源码构建

**前置条件：**
- Visual Studio（2019 或更新版本），安装“使用 C++ 的桌面开发”工作负载
- EasyX 图形库（从 https://easyx.cn 下载安装）

**构建步骤：**
1. 克隆本仓库到本地
2. 在 Visual Studio 中打开 `.cpp` 源文件
3. 设置字符集为 **使用多字节字符集** 或开启 `/utf-8` 编译选项
4. 将配置切换为 **Release**，平台选择 **x64**（或 **x86**）
5. 生成项目（`Ctrl + Shift + B`）
6. 生成的 `.exe` 位于 `Release` 文件夹中

> **注意：** 代码通过 `#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")` 隐藏控制台窗口，游戏将以纯 GUI 方式运行。

---

### 团队与版权

**子团队：** DornGames  
**版权所有 © 2023-2026 DornGames。保留所有权利。**

**母团队：** Dorn Hub  
**版权所有 © 2021-2026 Dorn Hub。保留所有权利。**

---

### 许可证

本项目采用 **MIT 许可证** —— 一个宽松的开源协议，允许任何人自由使用、复制、修改、合并、发布、分发、再授权和销售本软件，仅需保留版权声明。

**版权所有 © 2026 DornGames / Dorn Hub**

---

*最后更新：2026年7月14日*

---

敬上一杯刺梨汁！🍹  
Dorn Games & Dorn Hub
