/*==============================================================================
 *  飞机大战 (Plane War)  ——  基于 C++ 与 EasyX 图形库的射击小游戏
 *------------------------------------------------------------------------------
 *  【游戏特色】
 *    - 开始菜单 / 游戏中 / 暂停 / 结束 四种状态切换
 *    - 多层视差滚动星空背景（渐变底图 + 三层星星）
 *    - 玩家战机：自由移动、引擎尾焰动画、受击无敌闪烁
 *    - 子弹升级系统：拾取道具后火力等级 1~4，弹幕形态随之变化
 *    - 三种敌机（小型/中型/大型），大型敌机会主动射击
 *    - BOSS 战：每过一关出现，血条、左右移动、扇形弹幕
 *    - 道具系统：回血、火力升级、炸弹
 *    - 炸弹机制：清屏并对全场造成伤害
 *    - 爆炸特效：冲击波圆环 + 内核 + 粒子飞散
 *    - HUD 信息面板：分数、生命（红心）、炸弹数、关卡、最高分
 *    - 难度随关卡动态提升
 *
 *  【操作说明】
 *    方向键 / W A S D  ——  移动战机
 *    自动开火（无需按键）
 *    X                 ——  释放炸弹
 *    P                 ——  暂停 / 继续
 *    空格              ——  开始游戏 / 重新开始
 *    ESC               ——  返回菜单 / 退出游戏
 *
 *  【编译环境】
 *    Windows + Visual Studio + EasyX 图形库（https://easyx.cn）
 *    工程字符集建议使用 Unicode（本文件的文本输出按 Unicode 编写）。
 *============================================================================*/
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include <iostream>
#include <EasyX.h>   // EasyX 图形库核心头文件
#include <conio.h>      // 控制台输入（本例主要用键盘异步检测）
#include <tchar.h>      // TCHAR / _T / _stprintf_s 等 Unicode 兼容宏
#include <time.h>       // 随机数种子
#include <math.h>       // 三角函数、开方等
#include <vector>       // 动态数组容器
#include <algorithm>    // remove_if 等算法

using namespace std;

/*==============================================================================
 *  一、全局常量与枚举
 *============================================================================*/

const int   WIN_W = 480;          // 窗口宽度（竖屏）
const int   WIN_H = 700;          // 窗口高度
const int   FPS = 60;           // 目标帧率
const int   FRAME_MS = 1000 / FPS;// 每帧毫秒数
const double PI = 3.14159265358979;

// 游戏状态机：控制整体流程
enum GameState
{
    STATE_MENU,      // 主菜单
    STATE_PLAY,      // 游戏进行中
    STATE_PAUSE,     // 暂停
    STATE_OVER       // 游戏结束
};

/*==============================================================================
 *  二、通用工具函数
 *============================================================================*/

 // 返回 [a, b] 之间的随机整数
inline int randInt(int a, int b)
{
    if (b < a) { int t = a; a = b; b = t; }
    return a + rand() % (b - a + 1);
}

// 返回 [a, b] 之间的随机浮点数
inline float randFloat(float a, float b)
{
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

// 将 v 限制在 [lo, hi] 范围内
inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// 颜色线性插值：t 从 0 到 1，返回介于 a、b 之间的颜色
COLORREF lerpColor(COLORREF a, COLORREF b, float t)
{
    t = clampf(t, 0.0f, 1.0f);
    int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    return RGB((int)(ar + (br - ar) * t),
        (int)(ag + (bg - ag) * t),
        (int)(ab + (bb - ab) * t));
}

// 轴对齐包围盒（AABB）碰撞检测：两个矩形以“中心点 + 宽高”表示
inline bool hitAABB(float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh)
{
    return fabs(ax - bx) * 2 < (aw + bw)
        && fabs(ay - by) * 2 < (ah + bh);
}

/*==============================================================================
 *  三、数据结构定义
 *============================================================================*/

 // 背景星星（用于多层视差滚动）
struct Star
{
    float x, y;      // 位置
    float speed;     // 下落速度（越快代表离镜头越近）
    int   size;      // 半径
    int   bright;    // 亮度（灰度值）
};

// 子弹（玩家或敌人）
struct Bullet
{
    float x, y;      // 位置（中心）
    float vx, vy;    // 速度分量
    int   dmg;       // 伤害
    bool  alive;     // 是否存活
};

// 敌机
struct Enemy
{
    float x, y;        // 中心位置
    float vx, vy;      // 速度
    int   type;        // 0 小型 / 1 中型 / 2 大型
    int   hp, maxHp;   // 血量
    float w, h;        // 碰撞盒宽高
    float size;        // 绘制用半高
    int   score;       // 击杀得分
    int   fireTimer;   // 射击计时（仅大型有效）
    int   hitFlash;    // 受击白闪计时
    bool  alive;
};

// 道具
enum ItemType { ITEM_HEALTH, ITEM_POWER, ITEM_BOMB };
struct Item
{
    float x, y;
    float vy;
    int   type;      // 见 ItemType
    int   frame;     // 用于上下浮动动画
    bool  alive;
};

// 爆炸特效（含粒子）
struct Explosion
{
    static const int PN = 12;  // 粒子数量
    float x, y;                // 爆心
    int   frame;               // 当前帧
    int   maxFrame;            // 生命总帧数
    float radius;              // 冲击波最大半径
    float pang[PN];            // 各粒子飞散角度
    float pspd[PN];            // 各粒子飞散速度
    bool  alive;
};

// BOSS
struct Boss
{
    float x, y;        // 中心
    float vx;          // 水平速度
    int   hp, maxHp;   // 血量
    float w, h;        // 碰撞盒
    float size;        // 绘制用尺寸
    int   fireTimer;   // 射击计时
    int   phase;       // 弹幕形态阶段
    int   hitFlash;    // 受击白闪
    bool  entering;    // 是否处于入场下降阶段
    bool  active;      // 是否在场
};

// 玩家战机
struct Player
{
    float x, y;        // 中心位置
    float w, h;        // 碰撞盒
    int   hp, maxHp;   // 生命
    int   power;       // 火力等级（1~4）
    int   bombs;       // 炸弹数量
    int   fireTimer;   // 自动开火计时
    int   invincible;  // 无敌帧（受击后闪烁）
    bool  alive;
};

// 每帧输入状态
struct Input
{
    bool left, right, up, down; // 持续按住的方向
    bool bombEdge;              // 本帧刚按下 X（边沿触发）
    bool pauseEdge;            // 本帧刚按下 P
    bool startEdge;            // 本帧刚按下 空格
    bool quitEdge;             // 本帧刚按下 ESC
};

/*==============================================================================
 *  四、绘图辅助函数（全部用图形基元绘制，无需外部图片资源）
 *============================================================================*/

 // 绘制一颗爱心（HUD 生命与回血道具共用）
void drawHeart(int cx, int cy, int r, COLORREF color)
{
    setfillcolor(color);
    solidcircle(cx - r / 2, cy - r / 4, r / 2);          // 左半圆
    solidcircle(cx + r / 2, cy - r / 4, r / 2);          // 右半圆
    POINT p[3] = {
        { cx - r,        cy - r / 6 },
        { cx + r,        cy - r / 6 },
        { cx,            cy + r     }
    };
    solidpolygon(p, 3);                                  // 下方三角
}

// 绘制玩家战机（机头朝上，带闪烁尾焰）
void drawPlayerShip(int cx, int cy, int frame)
{
    const float s = 24.0f;                               // 半高（尺寸基准）
    COLORREF body = RGB(210, 220, 235);
    COLORREF wing = RGB(70, 130, 210);
    COLORREF cockpit = RGB(120, 235, 255);

    // 引擎尾焰：长度随帧数闪烁
    int flame = 10 + (frame % 6) * 2;
    POINT flameOut[3] = {
        { cx - 6, cy + (int)(0.55f * s) },
        { cx,     cy + (int)(0.55f * s) + flame },
        { cx + 6, cy + (int)(0.55f * s) }
    };
    setfillcolor(RGB(255, 170, 40));
    solidpolygon(flameOut, 3);
    POINT flameIn[3] = {
        { cx - 3, cy + (int)(0.55f * s) },
        { cx,     cy + (int)(0.55f * s) + flame - 4 },
        { cx + 3, cy + (int)(0.55f * s) }
    };
    setfillcolor(RGB(255, 240, 150));
    solidpolygon(flameIn, 3);

    // 机翼
    POINT wingPts[6] = {
        { cx - (int)(1.05f * s), cy + (int)(0.30f * s) },
        { cx - (int)(0.30f * s), cy + (int)(0.15f * s) },
        { cx + (int)(0.30f * s), cy + (int)(0.15f * s) },
        { cx + (int)(1.05f * s), cy + (int)(0.30f * s) },
        { cx + (int)(0.35f * s), cy - (int)(0.20f * s) },
        { cx - (int)(0.35f * s), cy - (int)(0.20f * s) }
    };
    setfillcolor(wing);
    solidpolygon(wingPts, 6);

    // 机身（机头朝上）
    POINT bodyPts[5] = {
        { cx,                    cy - (int)(0.95f * s) },
        { cx - (int)(0.30f * s), cy - (int)(0.10f * s) },
        { cx - (int)(0.32f * s), cy + (int)(0.75f * s) },
        { cx + (int)(0.32f * s), cy + (int)(0.75f * s) },
        { cx + (int)(0.30f * s), cy - (int)(0.10f * s) }
    };
    setfillcolor(body);
    solidpolygon(bodyPts, 5);

    // 尾翼
    POINT tailPts[3] = {
        { cx - (int)(0.55f * s), cy + (int)(0.95f * s) },
        { cx,                    cy + (int)(0.55f * s) },
        { cx + (int)(0.55f * s), cy + (int)(0.95f * s) }
    };
    setfillcolor(wing);
    solidpolygon(tailPts, 3);

    // 座舱
    setfillcolor(cockpit);
    solidellipse(cx - (int)(0.16f * s), cy - (int)(0.05f * s),
        cx + (int)(0.16f * s), cy + (int)(0.35f * s));
}

// 绘制敌机（机头朝下，通用可缩放，供三种敌机复用）
void drawEnemyShip(int cx, int cy, float s,
    COLORREF body, COLORREF wing, COLORREF cockpit, bool flash)
{
    if (flash) { body = RGB(255, 255, 255); wing = RGB(255, 255, 255); }

    // 尾翼（在最底层）
    POINT tailPts[3] = {
        { cx - (int)(0.55f * s), cy - (int)(0.95f * s) },
        { cx,                    cy - (int)(0.55f * s) },
        { cx + (int)(0.55f * s), cy - (int)(0.95f * s) }
    };
    setfillcolor(wing);
    solidpolygon(tailPts, 3);

    // 机翼
    POINT wingPts[6] = {
        { cx - (int)(1.05f * s), cy - (int)(0.30f * s) },
        { cx - (int)(0.30f * s), cy - (int)(0.15f * s) },
        { cx + (int)(0.30f * s), cy - (int)(0.15f * s) },
        { cx + (int)(1.05f * s), cy - (int)(0.30f * s) },
        { cx + (int)(0.35f * s), cy + (int)(0.20f * s) },
        { cx - (int)(0.35f * s), cy + (int)(0.20f * s) }
    };
    setfillcolor(wing);
    solidpolygon(wingPts, 6);

    // 机身（机头朝下）
    POINT bodyPts[5] = {
        { cx,                    cy + (int)(0.95f * s) },
        { cx - (int)(0.30f * s), cy + (int)(0.10f * s) },
        { cx - (int)(0.32f * s), cy - (int)(0.75f * s) },
        { cx + (int)(0.32f * s), cy - (int)(0.75f * s) },
        { cx + (int)(0.30f * s), cy + (int)(0.10f * s) }
    };
    setfillcolor(body);
    solidpolygon(bodyPts, 5);

    // 座舱
    setfillcolor(cockpit);
    solidellipse(cx - (int)(0.16f * s), cy - (int)(0.35f * s),
        cx + (int)(0.16f * s), cy + (int)(0.05f * s));
}

// 绘制 BOSS
void drawBoss(int cx, int cy, int frame, float hpRatio, bool flash)
{
    const float s = 70.0f;
    COLORREF wingCol = flash ? RGB(255, 255, 255) : RGB(70, 70, 95);
    COLORREF hullCol = flash ? RGB(255, 255, 255) : RGB(95, 100, 130);

    // 左右侧翼
    setfillcolor(wingCol);
    POINT lw[3] = {
        { cx - (int)(1.40f * s), cy },
        { cx - (int)(0.55f * s), cy - (int)(0.30f * s) },
        { cx - (int)(0.45f * s), cy + (int)(0.40f * s) }
    };
    solidpolygon(lw, 3);
    POINT rw[3] = {
        { cx + (int)(1.40f * s), cy },
        { cx + (int)(0.55f * s), cy - (int)(0.30f * s) },
        { cx + (int)(0.45f * s), cy + (int)(0.40f * s) }
    };
    solidpolygon(rw, 3);

    // 主体
    setfillcolor(hullCol);
    POINT hull[6] = {
        { cx - (int)(0.70f * s), cy - (int)(0.35f * s) },
        { cx + (int)(0.70f * s), cy - (int)(0.35f * s) },
        { cx + (int)(0.85f * s), cy + (int)(0.15f * s) },
        { cx + (int)(0.35f * s), cy + (int)(0.55f * s) },
        { cx - (int)(0.35f * s), cy + (int)(0.55f * s) },
        { cx - (int)(0.85f * s), cy + (int)(0.15f * s) }
    };
    solidpolygon(hull, 6);

    // 双炮管
    setfillcolor(RGB(40, 40, 55));
    solidrectangle(cx - (int)(0.90f * s), cy + (int)(0.10f * s),
        cx - (int)(0.72f * s), cy + (int)(0.55f * s));
    solidrectangle(cx + (int)(0.72f * s), cy + (int)(0.10f * s),
        cx + (int)(0.90f * s), cy + (int)(0.55f * s));

    // 能量核心（脉动发光，颜色随血量由绿转红）
    COLORREF coreCol = hpRatio > 0.5f ? RGB(90, 255, 130)
        : hpRatio > 0.25f ? RGB(255, 220, 80)
        : RGB(255, 80, 80);
    int pulse = (int)(6 * sin(frame * 0.2));
    setfillcolor(coreCol);
    solidcircle(cx, cy, 18 + pulse);
    setfillcolor(RGB(255, 255, 255));
    solidcircle(cx, cy, 8 + pulse / 2);
}

// 绘制玩家子弹（青白激光）
void drawPlayerBullet(int x, int y)
{
    setfillcolor(RGB(40, 120, 255));               // 外发光
    solidcircle(x, y, 5);
    setfillcolor(RGB(190, 235, 255));              // 内核
    solidellipse(x - 2, y - 9, x + 2, y + 5);
}

// 绘制敌方子弹（红色能量弹）
void drawEnemyBullet(int x, int y)
{
    setfillcolor(RGB(255, 70, 70));
    solidcircle(x, y, 5);
    setfillcolor(RGB(255, 220, 120));
    solidcircle(x, y, 2);
}

// 绘制道具（回血 / 升级 / 炸弹）
void drawItem(int type, int cx, int cy)
{
    // 外圈光环
    setfillcolor(RGB(30, 40, 60));
    solidcircle(cx, cy, 14);

    if (type == ITEM_HEALTH)
    {
        // 医疗包：白底红十字
        setfillcolor(RGB(240, 240, 240));
        solidcircle(cx, cy, 11);
        setfillcolor(RGB(220, 50, 50));
        solidrectangle(cx - 6, cy - 2, cx + 6, cy + 2);
        solidrectangle(cx - 2, cy - 6, cx + 2, cy + 6);
    }
    else if (type == ITEM_POWER)
    {
        // 火力升级：蓝底黄色向上箭头
        setfillcolor(RGB(60, 110, 220));
        solidcircle(cx, cy, 11);
        setfillcolor(RGB(255, 230, 90));
        POINT up[3] = {
            { cx,     cy - 7 },
            { cx - 7, cy + 2 },
            { cx + 7, cy + 2 }
        };
        solidpolygon(up, 3);
        solidrectangle(cx - 3, cy + 2, cx + 3, cy + 7);
    }
    else // ITEM_BOMB
    {
        // 炸弹：黑球 + 引线 + 火花
        setfillcolor(RGB(30, 30, 40));
        solidcircle(cx, cy + 2, 10);
        setfillcolor(RGB(120, 120, 130));
        solidcircle(cx - 3, cy - 1, 3);            // 高光
        setlinecolor(RGB(180, 140, 60));
        setlinestyle(PS_SOLID, 2);
        line(cx + 4, cy - 6, cx + 8, cy - 11);     // 引线
        setlinestyle(PS_SOLID, 1);
        setfillcolor(RGB(255, 200, 60));
        solidcircle(cx + 8, cy - 12, 3);           // 火花
    }
}

// 绘制爆炸特效
void drawExplosion(const Explosion& e)
{
    float t = (float)e.frame / e.maxFrame;         // 0 -> 1 生命进度

    // 冲击波圆环
    int r = (int)(e.radius * t);
    setlinecolor(lerpColor(RGB(255, 240, 180), RGB(120, 40, 10), t));
    setlinestyle(PS_SOLID, 2 + (int)((1 - t) * 2));
    circle(e.x, e.y, r > 2 ? r : 2);

    // 明亮内核（逐渐缩小）
    int coreR = (int)((1 - t) * e.radius * 0.4f);
    if (coreR > 0)
    {
        setfillcolor(lerpColor(RGB(255, 255, 240), RGB(255, 120, 30), t));
        solidcircle(e.x, e.y, coreR);
    }

    // 飞散粒子
    COLORREF pc = lerpColor(RGB(255, 220, 120), RGB(120, 30, 0), t);
    setfillcolor(pc);
    for (int i = 0; i < Explosion::PN; i++)
    {
        float d = e.pspd[i] * e.frame;
        int px = e.x + (int)(cos(e.pang[i]) * d);
        int py = e.y + (int)(sin(e.pang[i]) * d);
        solidcircle(px, py, (int)((1 - t) * 3) + 1);
    }
    setlinestyle(PS_SOLID, 1);                     // 复位线宽
}

/*==============================================================================
 *  五、游戏主类：封装全部状态、逻辑与渲染
 *============================================================================*/
class Game
{
public:
    Game() { init(); }

    // 运行主循环
    void run();

private:
    /*------------------------ 成员数据 ------------------------*/
    GameState state = STATE_MENU;   // 当前状态
    int  frameCount = 0;            // 全局帧计数（用于动画）
    int  score = 0;            // 当前分数
    int  highScore = 0;            // 最高分（会话内保存）
    int  level = 1;            // 当前关卡
    int  killCount = 0;            // 击杀敌机总数
    int  bossThreshold = 20;        // 触发 BOSS 所需击杀数
    int  spawnTimer = 60;           // 敌机生成计时
    int  bossWarnTimer = 0;         // "BOSS 来袭" 提示计时

    Player                 player;   // 玩家
    Boss                   boss;     // BOSS
    vector<Star>           stars;     // 背景星星
    vector<Bullet>         bullets;   // 玩家子弹
    vector<Bullet>         eBullets;  // 敌方子弹（含 BOSS）
    vector<Enemy>          enemies;   // 敌机
    vector<Item>           items;     // 道具
    vector<Explosion>      booms;     // 爆炸

    IMAGE bgImg;                      // 预生成的渐变背景底图

    /*------------------------ 初始化 ------------------------*/

    // 首次初始化（创建背景底图与星空）
    void init()
    {
        // 生成竖向渐变背景底图（顶部深蓝 -> 底部近黑）
        bgImg.Resize(WIN_W, WIN_H);
        DWORD* buf = GetImageBuffer(&bgImg);
        for (int y = 0; y < WIN_H; y++)
        {
            float t = (float)y / WIN_H;
            int r = (int)(12 + (2 - 12) * t);
            int g = (int)(14 + (4 - 14) * t);
            int b = (int)(38 + (14 - 38) * t);
            DWORD c = BGR(RGB(r, g, b));
            for (int x = 0; x < WIN_W; x++)
                buf[y * WIN_W + x] = c;
        }

        // 初始化三层星空
        stars.clear();
        for (int i = 0; i < 110; i++)
        {
            Star s;
            int layer = randInt(0, 2);                 // 层级决定速度与亮度
            s.x = (float)randInt(0, WIN_W);
            s.y = (float)randInt(0, WIN_H);
            s.speed = 0.6f + layer * 0.9f;
            s.size = layer == 2 ? 2 : 1;
            s.bright = 90 + layer * 55 + randInt(0, 30);
            stars.push_back(s);
        }
    }

    // 每局重置（进入 STATE_PLAY 前调用）
    void reset()
    {
        score = 0; level = 1; killCount = 0;
        bossThreshold = 20; spawnTimer = 60; bossWarnTimer = 0;

        player.x = WIN_W / 2.0f;
        player.y = WIN_H - 90.0f;
        player.w = 26; player.h = 30;
        player.hp = player.maxHp = 5;
        player.power = 1;
        player.bombs = 2;
        player.fireTimer = 0;
        player.invincible = 90;      // 出生保护
        player.alive = true;

        boss.active = false;

        bullets.clear();
        eBullets.clear();
        enemies.clear();
        items.clear();
        booms.clear();
    }

    /*------------------------ 输入处理 ------------------------*/

    // 读取本帧输入（方向为持续检测，功能键为边沿检测）
    Input readInput()
    {
        // 记录上一帧的功能键状态，用于检测"刚按下"
        static bool pP = false, pX = false, pSpace = false, pEsc = false;
        Input in{};

        in.left = (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState('A') & 0x8000);
        in.right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
        in.up = (GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000);
        in.down = (GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000);

        bool nP = (GetAsyncKeyState('P') & 0x8000) != 0;
        bool nX = (GetAsyncKeyState('X') & 0x8000) != 0;
        bool nSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        bool nEsc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        in.pauseEdge = nP && !pP;      pP = nP;
        in.bombEdge = nX && !pX;      pX = nX;
        in.startEdge = nSpace && !pSpace;  pSpace = nSpace;
        in.quitEdge = nEsc && !pEsc;    pEsc = nEsc;
        return in;
    }

    /*------------------------ 生成逻辑 ------------------------*/

    // 生成一架敌机（类型随关卡加权）
    void spawnEnemy()
    {
        Enemy e{};
        int roll = randInt(0, 99);
        // 关卡越高，中/大型出现概率越大
        int t = roll < (60 - level * 3) ? 0
            : roll < (88 - level * 2) ? 1 : 2;

        e.type = t;
        e.alive = true;
        e.hitFlash = 0;
        e.fireTimer = randInt(60, 120);

        if (t == 0) { e.size = 15; e.hp = 1;  e.score = 10; e.vy = 3.0f + level * 0.30f; }
        else if (t == 1) { e.size = 21; e.hp = 4;  e.score = 30; e.vy = 2.0f + level * 0.22f; }
        else { e.size = 30; e.hp = 12; e.score = 60; e.vy = 1.3f + level * 0.15f; }

        e.maxHp = e.hp;
        e.w = e.size * 1.8f;
        e.h = e.size * 1.6f;
        e.vx = randFloat(-0.6f, 0.6f);             // 轻微横向漂移
        e.x = randFloat(e.w, WIN_W - e.w);
        e.y = -e.h;
        enemies.push_back(e);
    }

    // 生成 BOSS
    void spawnBoss()
    {
        boss.size = 70;
        boss.w = 200; boss.h = 120;
        boss.maxHp = boss.hp = 120 + (level - 1) * 60;
        boss.x = WIN_W / 2.0f;
        boss.y = -80;
        boss.vx = 2.2f + level * 0.2f;
        boss.fireTimer = 60;
        boss.phase = 0;
        boss.hitFlash = 0;
        boss.entering = true;
        boss.active = true;
        bossWarnTimer = 120;                       // 显示 2 秒提示
    }

    // 玩家开火（弹幕形态随火力等级变化）
    void playerFire()
    {
        float px = player.x;
        float py = player.y - 26;                  // 从机头喷出
        auto add = [&](float x, float y, float vx, float vy)
            {
                Bullet b{ x, y, vx, vy, 1, true };
                bullets.push_back(b);
            };

        switch (player.power)
        {
        case 1:
            add(px, py, 0, -13);
            break;
        case 2:
            add(px - 9, py, 0, -13);
            add(px + 9, py, 0, -13);
            break;
        case 3:
            add(px, py, 0, -13);
            add(px - 12, py, -2, -12.5f);
            add(px + 12, py, 2, -12.5f);
            break;
        default: // 4 级及以上
            add(px - 14, py, 0, -13);
            add(px, py, 0, -14);
            add(px + 14, py, 0, -13);
            add(px - 10, py, -3, -12);
            add(px + 10, py, 3, -12);
            break;
        }
    }

    // 在指定位置产生一次爆炸
    void spawnBoom(float x, float y, float radius, int maxFrame)
    {
        Explosion e{};
        e.x = x; e.y = y;
        e.frame = 0; e.maxFrame = maxFrame;
        e.radius = radius; e.alive = true;
        for (int i = 0; i < Explosion::PN; i++)
        {
            e.pang[i] = randFloat(0, 2 * (float)PI);
            e.pspd[i] = randFloat(radius * 0.10f, radius * 0.22f);
        }
        booms.push_back(e);
    }

    // 敌机死亡时按概率掉落道具
    void maybeDropItem(float x, float y, int enemyType)
    {
        int roll = randInt(0, 99);
        int bonus = enemyType * 4;                 // 越大的敌机掉率越高
        int type = -1;
        if (roll < 6 + bonus)                type = ITEM_HEALTH;
        else if (roll < 18 + bonus)          type = ITEM_POWER;
        else if (roll < 26 + bonus)          type = ITEM_BOMB;
        if (type < 0) return;

        Item it{ x, y, 2.2f, type, 0, true };
        items.push_back(it);
    }

    // 使用炸弹：清屏并对全场敌人造成伤害
    void useBomb()
    {
        if (player.bombs <= 0) return;
        player.bombs--;

        // 清除所有敌方子弹
        eBullets.clear();

        // 摧毁所有普通敌机并计分
        for (auto& e : enemies)
        {
            if (!e.alive) continue;
            spawnBoom(e.x, e.y, 34, 18);
            score += e.score;
            killCount++;
            e.alive = false;
        }

        // 对 BOSS 造成大量固定伤害
        if (boss.active)
        {
            boss.hp -= 40;
            boss.hitFlash = 8;
            spawnBoom(boss.x, boss.y, 80, 22);
        }
        // 屏幕四处开花，增强打击感
        for (int i = 0; i < 6; i++)
            spawnBoom(randFloat(40, WIN_W - 40), randFloat(60, WIN_H - 120), 40, 20);
    }

    /*------------------------ 更新逻辑 ------------------------*/

    // 更新背景星空（持续滚动，越界重生）
    void updateStars()
    {
        for (auto& s : stars)
        {
            s.y += s.speed;
            if (s.y > WIN_H)
            {
                s.y = 0;
                s.x = (float)randInt(0, WIN_W);
            }
        }
    }

    // 更新玩家：移动、边界、开火、无敌计时、炸弹
    void updatePlayer(const Input& in)
    {
        float sp = 6.0f;
        player.x += (in.right - in.left) * sp;
        player.y += (in.down - in.up) * sp;
        player.x = clampf(player.x, 24, WIN_W - 24);
        player.y = clampf(player.y, 40, WIN_H - 30);

        if (player.invincible > 0) player.invincible--;

        // 自动开火
        if (--player.fireTimer <= 0)
        {
            playerFire();
            player.fireTimer = 7;                  // 射速
        }

        // 释放炸弹
        if (in.bombEdge) useBomb();
    }

    // 更新玩家子弹
    void updateBullets()
    {
        for (auto& b : bullets)
        {
            b.x += b.vx; b.y += b.vy;
            if (b.y < -20 || b.x < -20 || b.x > WIN_W + 20) b.alive = false;
        }
        removeDead(bullets);
    }

    // 更新敌方子弹
    void updateEnemyBullets()
    {
        for (auto& b : eBullets)
        {
            b.x += b.vx; b.y += b.vy;
            if (b.y > WIN_H + 20 || b.y < -20 || b.x < -20 || b.x > WIN_W + 20)
                b.alive = false;
        }
        removeDead(eBullets);
    }

    // 更新敌机：移动、大型敌机射击、越界移除
    void updateEnemies()
    {
        for (auto& e : enemies)
        {
            e.x += e.vx; e.y += e.vy;
            // 碰到左右边界则反弹横向速度
            if (e.x < e.w / 2 || e.x > WIN_W - e.w / 2) e.vx = -e.vx;
            if (e.hitFlash > 0) e.hitFlash--;

            // 大型敌机定期向下开火
            if (e.type == 2 && e.y > 0)
            {
                if (--e.fireTimer <= 0)
                {
                    Bullet b{ e.x, e.y + e.h / 2, 0, 4.0f, 1, true };
                    eBullets.push_back(b);
                    e.fireTimer = randInt(70, 110);
                }
            }

            if (e.y - e.h > WIN_H) e.alive = false; // 飞出屏幕（不扣分，友好设计）
        }
        removeDead(enemies);
    }

    // 更新道具：下落 + 上下浮动动画
    void updateItems()
    {
        for (auto& it : items)
        {
            it.frame++;
            it.y += it.vy;
            if (it.y > WIN_H + 20) it.alive = false;
        }
        removeDead(items);
    }

    // 更新爆炸
    void updateBooms()
    {
        for (auto& e : booms)
        {
            e.frame++;
            if (e.frame >= e.maxFrame) e.alive = false;
        }
        removeDead(booms);
    }

    // 更新 BOSS：入场、移动、弹幕、死亡结算
    void updateBoss()
    {
        if (!boss.active) return;

        // 入场：先下降到固定高度
        if (boss.entering)
        {
            boss.y += 2.0f;
            if (boss.y >= 100) { boss.y = 100; boss.entering = false; }
            return;
        }

        if (boss.hitFlash > 0) boss.hitFlash--;

        // 左右往返移动
        boss.x += boss.vx;
        if (boss.x < boss.w / 2 || boss.x > WIN_W - boss.w / 2) boss.vx = -boss.vx;

        // 弹幕：扇形 + 瞄准玩家交替
        if (--boss.fireTimer <= 0)
        {
            boss.phase = (boss.phase + 1) % 2;
            if (boss.phase == 0)
            {
                // 扇形五连发
                for (int i = -2; i <= 2; i++)
                {
                    float ang = (90 + i * 16) * (float)PI / 180.0f;
                    float sp = 5.0f;
                    Bullet b{ boss.x, boss.y + 30,
                              cos(ang) * sp, sin(ang) * sp, 1, true };
                    eBullets.push_back(b);
                }
            }
            else
            {
                // 瞄准玩家发射
                float dx = player.x - boss.x;
                float dy = player.y - boss.y;
                float len = sqrt(dx * dx + dy * dy) + 0.0001f;
                float sp = 6.0f;
                Bullet b{ boss.x, boss.y + 30,
                          dx / len * sp, dy / len * sp, 1, true };
                eBullets.push_back(b);
            }
            boss.fireTimer = 40;
        }

        // 死亡结算
        if (boss.hp <= 0)
        {
            for (int i = 0; i < 10; i++)
                spawnBoom(boss.x + randFloat(-60, 60),
                    boss.y + randFloat(-40, 40), 60, 26);
            score += 500 * level;
            boss.active = false;
            level++;
            bossThreshold = killCount + 20 + level * 2;   // 下一关阈值
            // BOSS 战胜利额外奖励
            player.bombs++;
            items.push_back({ boss.x, boss.y, 2.0f, ITEM_POWER, 0, true });
        }
    }

    // 生成节奏控制：普通敌机 + BOSS 触发
    void updateSpawning()
    {
        if (bossWarnTimer > 0) bossWarnTimer--;

        if (!boss.active)
        {
            // 达到击杀阈值则召唤 BOSS
            if (killCount >= bossThreshold)
            {
                spawnBoss();
                return;
            }
            // 常规刷怪，间隔随关卡缩短
            if (--spawnTimer <= 0)
            {
                spawnEnemy();
                int base = 55 - level * 4;
                if (base < 18) base = 18;
                spawnTimer = randInt(base, base + 25);
            }
        }
        else
        {
            // BOSS 战期间偶尔补充小型敌机增加压力
            if (--spawnTimer <= 0)
            {
                if (randInt(0, 2) == 0) spawnEnemy();
                spawnTimer = randInt(50, 90);
            }
        }
    }

    /*------------------------ 碰撞处理 ------------------------*/

    void handleCollisions()
    {
        // 1) 玩家子弹 vs 敌机
        for (auto& b : bullets)
        {
            if (!b.alive) continue;
            for (auto& e : enemies)
            {
                if (!e.alive) continue;
                if (hitAABB(b.x, b.y, 10, 14, e.x, e.y, e.w, e.h))
                {
                    e.hp -= b.dmg;
                    e.hitFlash = 3;
                    b.alive = false;
                    if (e.hp <= 0)
                    {
                        spawnBoom(e.x, e.y, e.size * 1.6f, 18);
                        score += e.score;
                        killCount++;
                        maybeDropItem(e.x, e.y, e.type);
                        e.alive = false;
                    }
                    break;
                }
            }
        }

        // 2) 玩家子弹 vs BOSS
        if (boss.active && !boss.entering)
        {
            for (auto& b : bullets)
            {
                if (!b.alive) continue;
                if (hitAABB(b.x, b.y, 10, 14, boss.x, boss.y, boss.w, boss.h))
                {
                    boss.hp -= b.dmg;
                    boss.hitFlash = 2;
                    b.alive = false;
                    if (randInt(0, 3) == 0)
                        spawnBoom(b.x, b.y, 16, 10);
                }
            }
        }

        // 3) 敌机 vs 玩家（撞击）
        if (player.invincible == 0)
        {
            for (auto& e : enemies)
            {
                if (!e.alive) continue;
                if (hitAABB(player.x, player.y, player.w, player.h,
                    e.x, e.y, e.w, e.h))
                {
                    spawnBoom(e.x, e.y, e.size * 1.6f, 18);
                    e.alive = false;
                    hurtPlayer();
                    break;
                }
            }
        }

        // 4) 敌方子弹 vs 玩家
        if (player.invincible == 0)
        {
            for (auto& b : eBullets)
            {
                if (!b.alive) continue;
                if (hitAABB(player.x, player.y, player.w, player.h, b.x, b.y, 10, 10))
                {
                    b.alive = false;
                    hurtPlayer();
                    break;
                }
            }
        }

        // 5) BOSS vs 玩家（碰撞）
        if (player.invincible == 0 && boss.active && !boss.entering)
        {
            if (hitAABB(player.x, player.y, player.w, player.h,
                boss.x, boss.y, boss.w * 0.7f, boss.h * 0.7f))
            {
                hurtPlayer();
            }
        }

        // 6) 道具 vs 玩家（拾取）
        for (auto& it : items)
        {
            if (!it.alive) continue;
            if (hitAABB(player.x, player.y, player.w + 6, player.h + 6, it.x, it.y, 26, 26))
            {
                applyItem(it.type);
                it.alive = false;
            }
        }

        // 清理死亡对象
        removeDead(bullets);
        removeDead(eBullets);
        removeDead(enemies);
        removeDead(items);
    }

    // 玩家受伤
    void hurtPlayer()
    {
        player.hp--;
        player.invincible = 90;                    // 受击后短暂无敌闪烁
        spawnBoom(player.x, player.y, 30, 16);
        if (player.hp <= 0)
        {
            player.alive = false;
            spawnBoom(player.x, player.y, 55, 26);
            if (score > highScore) highScore = score;
            state = STATE_OVER;
        }
    }

    // 应用道具效果
    void applyItem(int type)
    {
        if (type == ITEM_HEALTH)
        {
            if (player.hp < player.maxHp) player.hp++;
        }
        else if (type == ITEM_POWER)
        {
            if (player.power < 4) player.power++;
        }
        else // ITEM_BOMB
        {
            player.bombs++;
        }
    }

    // 通用：移除容器中 alive == false 的元素
    template <typename T>
    void removeDead(vector<T>& v)
    {
        v.erase(remove_if(v.begin(), v.end(),
            [](const T& o) { return !o.alive; }),
            v.end());
    }

    /*------------------------ 渲染 ------------------------*/

    // 绘制背景（渐变底图 + 星空）
    void renderBackground()
    {
        putimage(0, 0, &bgImg);
        for (const auto& s : stars)
        {
            setfillcolor(RGB(s.bright, s.bright, min(255, s.bright + 20)));
            if (s.size <= 1) putpixel((int)s.x, (int)s.y,
                RGB(s.bright, s.bright, s.bright));
            else             solidcircle((int)s.x, (int)s.y, s.size);
        }
    }

    // 绘制所有游戏实体
    void renderEntities()
    {
        // 道具（含浮动）
        for (const auto& it : items)
        {
            int bob = (int)(3 * sin(it.frame * 0.15));
            drawItem(it.type, (int)it.x, (int)it.y + bob);
        }

        // 敌方子弹
        for (const auto& b : eBullets)
            drawEnemyBullet((int)b.x, (int)b.y);

        // 敌机
        for (const auto& e : enemies)
        {
            COLORREF body, wing, cockpit;
            if (e.type == 0) { body = RGB(225, 70, 70);  wing = RGB(150, 40, 40);  cockpit = RGB(255, 200, 120); }
            else if (e.type == 1) { body = RGB(180, 90, 220); wing = RGB(110, 50, 160); cockpit = RGB(230, 180, 255); }
            else { body = RGB(90, 180, 100); wing = RGB(50, 110, 60);  cockpit = RGB(200, 255, 200); }
            drawEnemyShip((int)e.x, (int)e.y, e.size, body, wing, cockpit, e.hitFlash > 0);

            // 中/大型敌机显示血条
            if (e.type >= 1 && e.hp < e.maxHp)
                drawMiniHpBar(e.x, e.y - e.h / 2 - 6, e.size * 1.6f,
                    (float)e.hp / e.maxHp);
        }

        // BOSS
        if (boss.active)
        {
            drawBoss((int)boss.x, (int)boss.y, frameCount,
                (float)boss.hp / boss.maxHp, boss.hitFlash > 0);
        }

        // 玩家子弹
        for (const auto& b : bullets)
            drawPlayerBullet((int)b.x, (int)b.y);

        // 玩家（无敌时隔帧闪烁）
        if (player.alive && !(player.invincible > 0 && (frameCount / 4) % 2 == 0))
            drawPlayerShip((int)player.x, (int)player.y, frameCount);

        // 爆炸（画在最上层）
        for (const auto& e : booms)
            drawExplosion(e);
    }

    // 敌机小血条
    void drawMiniHpBar(float cx, float cy, float w, float ratio)
    {
        int left = (int)(cx - w / 2), right = (int)(cx + w / 2);
        setfillcolor(RGB(60, 60, 60));
        solidrectangle(left, (int)cy, right, (int)cy + 3);
        setfillcolor(ratio > 0.4f ? RGB(120, 220, 120) : RGB(230, 120, 80));
        solidrectangle(left, (int)cy, left + (int)(w * ratio), (int)cy + 3);
    }

    // 顶部 HUD 信息栏
    void renderHUD()
    {
        // 半透明感的深色顶栏
        setfillcolor(RGB(10, 12, 28));
        solidrectangle(0, 0, WIN_W, 44);

        setbkmode(TRANSPARENT);
        TCHAR buf[64];

        // 分数
        settextstyle(20, 0, _T("Consolas"));
        settextcolor(RGB(230, 230, 240));
        _stprintf_s(buf, _T("分数 %d"), score);
        outtextxy(10, 6, buf);

        // 关卡 + 最高分
        settextstyle(16, 0, _T("Consolas"));
        settextcolor(RGB(160, 200, 255));
        _stprintf_s(buf, _T("第 %d 关"), level);
        outtextxy(WIN_W - 90, 4, buf);
        settextcolor(RGB(150, 150, 170));
        _stprintf_s(buf, _T("最高 %d"), highScore);
        outtextxy(WIN_W - 90, 24, buf);

        // 生命（红心）
        for (int i = 0; i < player.maxHp; i++)
        {
            COLORREF c = i < player.hp ? RGB(230, 60, 70) : RGB(70, 60, 65);
            drawHeart(20 + i * 22, 34, 7, c);
        }

        // 炸弹数量
        drawItem(ITEM_BOMB, WIN_W / 2 - 20, 26);
        settextstyle(18, 0, _T("Consolas"));
        settextcolor(RGB(255, 220, 120));
        _stprintf_s(buf, _T("x%d"), player.bombs);
        outtextxy(WIN_W / 2 - 4, 16, buf);

        // BOSS 血条
        if (boss.active && !boss.entering)
        {
            int bx1 = 40, bx2 = WIN_W - 40, by = 50;
            setfillcolor(RGB(50, 20, 20));
            solidrectangle(bx1, by, bx2, by + 10);
            float ratio = (float)boss.hp / boss.maxHp;
            setfillcolor(lerpColor(RGB(255, 60, 60), RGB(255, 200, 60), ratio));
            solidrectangle(bx1, by, bx1 + (int)((bx2 - bx1) * ratio), by + 10);
            setlinecolor(RGB(200, 200, 210));
            rectangle(bx1, by, bx2, by + 10);
        }

        // BOSS 来袭提示
        if (bossWarnTimer > 0 && (bossWarnTimer / 6) % 2 == 0)
            drawCenterText(WIN_H / 2 - 20, _T("⚠  BOSS 来袭  ⚠"), 34, RGB(255, 90, 90));
    }

    // 居中绘制一行文字
    void drawCenterText(int y, LPCTSTR s, int fontH, COLORREF col)
    {
        settextstyle(fontH, 0, _T("微软雅黑"));
        settextcolor(col);
        setbkmode(TRANSPARENT);
        int w = textwidth(s);
        outtextxy((WIN_W - w) / 2, y, s);
    }

    // 菜单界面
    void renderMenu()
    {
        drawCenterText(150, _T("飞 机 大 战"), 52, RGB(255, 255, 255));
        drawCenterText(215, _T("PLANE  WAR"), 22, RGB(120, 180, 255));

        // 演示战机
        drawPlayerShip(WIN_W / 2, 330, frameCount);

        drawCenterText(420, _T("按 [空格] 开始游戏"), 26, RGB(255, 230, 120));

        settextstyle(18, 0, _T("微软雅黑"));
        settextcolor(RGB(180, 190, 210));
        setbkmode(TRANSPARENT);
        const TCHAR* tips[] = {
            _T("方向键 / WASD  —  移动"),
            _T("自动开火  |  X  —  释放炸弹"),
            _T("P  —  暂停     ESC  —  退出"),
            _T("拾取道具：红心回血 / 蓝箭升级 / 炸弹")
        };
        for (int i = 0; i < 4; i++)
        {
            int w = textwidth(tips[i]);
            outtextxy((WIN_W - w) / 2, 480 + i * 28, tips[i]);
        }
    }

    // 结束界面
    void renderOver()
    {
        // 半透明黑色遮罩
        setfillcolor(RGB(0, 0, 0));
        for (int y = 0; y < WIN_H; y += 2)          // 用间隔线条模拟半透明
            line(0, y, WIN_W, y);

        drawCenterText(230, _T("游 戏 结 束"), 48, RGB(255, 90, 90));

        TCHAR buf[64];
        _stprintf_s(buf, _T("本局得分：%d"), score);
        drawCenterText(320, buf, 28, RGB(255, 255, 255));
        _stprintf_s(buf, _T("历史最高：%d"), highScore);
        drawCenterText(360, buf, 24, RGB(180, 200, 255));
        _stprintf_s(buf, _T("抵达关卡：第 %d 关"), level);
        drawCenterText(396, buf, 22, RGB(180, 200, 180));

        if ((frameCount / 20) % 2 == 0)             // 提示闪烁
            drawCenterText(470, _T("按 [空格] 再来一局"), 26, RGB(255, 230, 120));
        drawCenterText(520, _T("按 [ESC] 返回菜单"), 20, RGB(160, 170, 190));
    }

    // 暂停遮罩
    void renderPauseOverlay()
    {
        setfillcolor(RGB(0, 0, 0));
        for (int y = 0; y < WIN_H; y += 2)
            line(0, y, WIN_W, y);
        drawCenterText(WIN_H / 2 - 30, _T("暂  停"), 46, RGB(255, 255, 255));
        drawCenterText(WIN_H / 2 + 30, _T("按 [P] 继续"), 24, RGB(200, 210, 230));
    }

    /*------------------------ 各状态更新 ------------------------*/

    // 游戏进行中的完整一帧
    void updatePlay(const Input& in)
    {
        if (in.pauseEdge) { state = STATE_PAUSE; return; }
        if (in.quitEdge) { state = STATE_MENU;  return; }

        updatePlayer(in);
        updateSpawning();
        updateEnemies();
        updateBullets();
        updateEnemyBullets();
        updateBoss();
        updateItems();
        updateBooms();
        handleCollisions();
    }
};

/*==============================================================================
 *  六、主循环实现
 *============================================================================*/
void Game::run()
{
    while (true)
    {
        DWORD frameStart = GetTickCount();         // 帧起始时间（用于限帧）
        frameCount++;

        Input in = readInput();

        // ------- 状态更新 -------
        updateStars();                             // 星空在任何状态都滚动

        switch (state)
        {
        case STATE_MENU:
            if (in.startEdge) { reset(); state = STATE_PLAY; }
            if (in.quitEdge)  return;              // 菜单按 ESC 退出程序
            break;

        case STATE_PLAY:
            updatePlay(in);
            break;

        case STATE_PAUSE:
            if (in.pauseEdge) state = STATE_PLAY;  // 再按 P 继续
            if (in.quitEdge)  state = STATE_MENU;
            break;

        case STATE_OVER:
            if (in.startEdge) { reset(); state = STATE_PLAY; }
            if (in.quitEdge)  state = STATE_MENU;
            break;
        }

        // ------- 渲染 -------
        BeginBatchDraw();                          // 开启双缓冲，防止闪烁
        renderBackground();

        if (state == STATE_MENU)
        {
            renderMenu();
        }
        else
        {
            renderEntities();
            renderHUD();
            if (state == STATE_PAUSE) renderPauseOverlay();
            if (state == STATE_OVER)  renderOver();
        }

        FlushBatchDraw();                          // 一次性刷新到屏幕
        EndBatchDraw();

        // ------- 限帧 -------
        DWORD elapsed = GetTickCount() - frameStart;
        if (elapsed < (DWORD)FRAME_MS) Sleep(FRAME_MS - elapsed);
    }
}

/*==============================================================================
 *  七、程序入口
 *============================================================================*/
int main()
{
    srand((unsigned)time(NULL));                   // 初始化随机种子

    initgraph(WIN_W, WIN_H);                        // 创建绘图窗口
    SetWindowText(GetHWnd(), _T("飞机大战 - Plane War"));

    Game game;                                      // 创建游戏对象
    game.run();                                      // 进入主循环

    closegraph();                                    // 关闭绘图窗口
    return 0;
}
