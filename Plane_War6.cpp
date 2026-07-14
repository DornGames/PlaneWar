/******************************************************************************
 *  项目名称：飞机大战 (Plane War)
 *  开发环境：Windows + Visual Studio + EasyX 图形库
 *  编程语言：C++ (C++11 及以上)
 *  文件说明：单文件实现的 2D 纵版飞行射击游戏，覆盖课程设计的全部基础功能
 *            与多项能力提升功能。
 *
 *  ------------------------------------------------------------------------
 *  已实现功能一览：
 *    【基础功能】
 *      · 图形窗口、稳定游戏循环、双缓冲无闪烁绘制、帧率控制
 *      · 玩家飞机 WASD / 方向键移动，边界限制
 *      · 生命值系统（3 条命）、被击中后短暂无敌闪烁
 *      · 敌机定时随机生成、自动下移、越界消失
 *      · 多种敌机类型（普通 / 快速 / 射击 / 蛇形）与矩形碰撞检测
 *      · 玩家射击、击中判定、爆炸效果、按类型计分、分数/生命实时显示
 *    【能力提升】
 *      · 道具系统：火力增强、生命恢复、护盾、炸弹（B 键清屏）
 *      · 关卡与难度递增：3 关，敌机频率/速度随关卡提升，新增敌机类型
 *      · Boss 战：每关末尾出现 Boss，带血条与环形/散射弹幕
 *      · 敌机 AI：蛇形（正弦）移动、敌机向下射击
 *      · 视觉效果：爆炸粒子、星空滚动背景、被击中屏幕震动
 *      · 存档与排行榜：文件持久化，记录并显示历史前 10 名
 *      · 游戏状态机：菜单 / 游戏中 / 暂停 / 游戏结束 / 通关胜利
 *
 *  ------------------------------------------------------------------------
 *  编译提示：
 *    1. 需要正确安装 EasyX（graphics.h）。
 *    2. 建议将源文件保存为 UTF-8 编码，并在项目属性中开启 /utf-8，
 *       或使用“多字节字符集”，以保证界面中文正常显示。
 *    3. 若需开启音效，将下方 ENABLE_SOUND 置为 1，并准备对应 .wav 资源，
 *       此时需链接 winmm.lib（代码中已通过 #pragma 自动链接）。
 *****************************************************************************/

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include <graphics.h>   // EasyX 图形库
#include <conio.h>      // 控制台输入（备用）
#include <windows.h>    // GetAsyncKeyState / GetTickCount / Sleep 等
#include <tchar.h>      // TCHAR 与 _T() / _stprintf_s，兼容 Unicode / 多字节
#include <cstdio>
#include <cstdlib>      // rand / srand / RAND_MAX
#include <cstring>      // memcpy
#include <cmath>
#include <ctime>
#include <vector>
#include <string>
#include <utility>          // std::pair（用于以比例坐标描述飞机造型）
#include <initializer_list>
#include <algorithm>
#include <fstream>
#include <sstream>

 /*==========================================================================*/
 /*                              全局配置常量                                 */
 /*==========================================================================*/

#define ENABLE_SOUND 0                     // 1 = 开启音效；0 = 关闭（默认，免资源亦可编译运行）
#include <mmsystem.h>                       // timeBeginPeriod/timeGetTime（高精度计时）+ PlaySound（音效）
#pragma comment(lib, "winmm.lib")          // 链接多媒体库：高精度帧率计时器与音效均依赖它

const int   WIN_W = 480;             // 窗口宽度（像素）
const int   WIN_H = 720;             // 窗口高度（像素）
const int   FPS = 60;              // 目标帧率
const DWORD FRAME_MS = 1000 / FPS;      // 每帧目标时长（毫秒）

const int   PLAYER_W = 44;              // 玩家飞机宽
const int   PLAYER_H = 50;              // 玩家飞机高
const int   PLAYER_SPD = 6;               // 玩家移动速度（像素/帧）
const int   START_LIVES = 3;              // 初始生命数
const int   INVINCIBLE_FRAMES = 90;        // 受击后无敌时长（帧，约 1.5 秒）
const int   SHOOT_CD = 8;               // 玩家射击冷却（帧）
const int   FIRE_BUFF_FRAMES = 600;        // 火力增强持续时长（帧，约 10 秒）
const int   MAX_BOMBS = 3;               // 炸弹数量上限
const int   MAX_LIVES_CAP = 5;             // 生命值上限（永久道具最多提升到 5）
const int   BULLET_DMG_MAX = 4;            // 子弹攻击力上限（永久道具最多提升到 4）
const int   TOTAL_LEVEL = 3;               // 总关卡数
const int   LEVEL_FRAMES = 60 * 22;        // 每关刷小怪时长（帧），之后出现 Boss

/*----------------------- 新技能相关常量 -----------------------*/
const int   ENERGY_MAX = 100;             // 能量上限（集满可放激光大招）
const int   ENERGY_KILL = 8;               // 每击毁一架敌机获得的能量
const int   ULT_FRAMES = 78;              // 激光大招持续帧数（约 1.3 秒）
const int   ULT_HALF = 26;              // 激光大招半宽（像素）
const int   DASH_SPD = 15;              // 闪避冲刺速度（像素/帧）
const int   DASH_FRAMES = 8;               // 闪避冲刺持续帧数
const int   DASH_CD = 70;              // 闪避冷却帧数
const int   BOSS_BEAM_HALF = 15;           // Boss 激光半宽（像素）

/*----------------------------- 颜色定义 -----------------------------------*/
const COLORREF COL_BG = RGB(10, 12, 30);    // 深空背景色
const COLORREF COL_PLAYER = RGB(90, 220, 255);  // 玩家飞机主色
const COLORREF COL_PLAYER_2 = RGB(40, 150, 210);  // 玩家飞机暗部
const COLORREF COL_PBULLET = RGB(180, 255, 200); // 玩家子弹
const COLORREF COL_EBULLET = RGB(255, 120, 120); // 敌方子弹
const COLORREF COL_E_NORMAL = RGB(230, 80, 80);   // 普通敌机
const COLORREF COL_E_FAST = RGB(250, 210, 60);  // 快速敌机
const COLORREF COL_E_SHOOTER = RGB(190, 110, 240); // 射击敌机
const COLORREF COL_E_ZIGZAG = RGB(120, 230, 160); // 蛇形敌机
const COLORREF COL_E_TANK = RGB(150, 165, 120); // 装甲重机（橄榄）
const COLORREF COL_E_DIVER = RGB(255, 120, 60);  // 俯冲自爆机（橙红）
const COLORREF COL_E_STRAFER = RGB(90, 205, 200);  // 横掠射击机（青蓝）
const COLORREF COL_BOSS = RGB(240, 90, 120);  // Boss

/*==========================================================================*/
/*                              枚举类型定义                                 */
/*==========================================================================*/

/** @brief 游戏总体状态（状态机） */
enum GameState {
    ST_MENU,        // 主菜单
    ST_PLAYING,     // 游戏进行中
    ST_PAUSED,      // 暂停
    ST_GAMEOVER,    // 游戏失败
    ST_VICTORY      // 通关胜利
};

/** @brief 敌机类型 */
enum EnemyType {
    E_NORMAL,       // 普通敌机：直线下移
    E_FAST,         // 快速敌机：速度更快
    E_SHOOTER,      // 射击敌机：向下发射子弹
    E_ZIGZAG,       // 蛇形敌机：正弦波左右摆动下移
    E_TANK,         // 装甲重机：血厚、移动慢、分值高（第 2 关起）
    E_DIVER,        // 俯冲自爆机：先缓降后锁定玩家高速俯冲（第 3 关起）
    E_STRAFER       // 横掠射击机：降到一定高度后横向扫场并向下射击（第 3 关起）
};

/** @brief 道具类型 */
enum ItemType {
    IT_FIRE,        // 火力增强（临时）
    IT_LIFE,        // 生命恢复（临时性：恢复 1 点，不超过当前上限）
    IT_SHIELD,      // 护盾（临时）
    IT_BOMB,        // 炸弹补给（临时）
    IT_MAXLIFE,     // 【永久】生命上限 +1 并回满 1 点（稀有）
    IT_POWERUP      // 【永久】子弹伤害 +1（稀有）
};

/** @brief 判断某道具是否为永久性增益 */
inline bool isPermanentItem(ItemType t) { return t == IT_MAXLIFE || t == IT_POWERUP; }

/** @brief 护甲减伤：实际伤害 = 伤害 - 护甲，但至少造成 1 点 */
inline int effDamage(int dmg, int armor) { int d = dmg - armor; return d < 1 ? 1 : d; }

/** @brief 敌机中文名称（用于属性面板） */
inline const TCHAR* enemyName(EnemyType t) {
    switch (t) {
    case E_NORMAL:  return _T("普通机");
    case E_FAST:    return _T("快速机");
    case E_SHOOTER: return _T("射击机");
    case E_ZIGZAG:  return _T("蛇形机");
    case E_TANK:    return _T("装甲机");
    case E_DIVER:   return _T("俯冲机");
    case E_STRAFER: return _T("横掠机");
    }
    return _T("?");
}

/** @brief 敌机攻击特征描述（用于属性面板） */
inline const TCHAR* enemyFeature(EnemyType t) {
    switch (t) {
    case E_NORMAL:  return _T("直线下移");
    case E_FAST:    return _T("高速直冲");
    case E_SHOOTER: return _T("下移+射击");
    case E_ZIGZAG:  return _T("正弦蛇形");
    case E_TANK:    return _T("厚甲慢速");
    case E_DIVER:   return _T("锁定俯冲");
    case E_STRAFER: return _T("横掠扫射");
    }
    return _T("");
}

/** @brief 敌机护甲值（同时决定属性面板的“防御力”与实际减伤） */
inline int enemyArmor(EnemyType t) {
    switch (t) {
    case E_SHOOTER: return 1;
    case E_TANK:    return 2;
    case E_STRAFER: return 1;
    default:        return 0;
    }
}

/** @brief 敌机生命值（部分类型随关卡提升，供生成与属性面板共用以保持一致） */
inline int enemyHp(EnemyType t, int level) {
    switch (t) {
    case E_NORMAL:  return 1;
    case E_FAST:    return 1;
    case E_SHOOTER: return 3;
    case E_ZIGZAG:  return 2;
    case E_TANK:    return 6 + level;
    case E_DIVER:   return 2;
    case E_STRAFER: return 3;
    }
    return 1;
}

/** @brief Boss 生命值（供生成与属性面板共用） */
inline int bossHp(int level) { return 60 + level * 50; }

/** @brief Boss 招式类型（每次随机选取，尽量不与上一招重复，招式更华丽多变） */
enum BossAtk {
    BA_RING,        // 旋转环形弹幕：连发数圈、彼此错开如花瓣
    BA_AIM,         // 瞄准扇形散射：朝玩家方向多发扇形弹
    BA_SPIRAL,      // 螺旋弹幕：双臂持续旋转喷射
    BA_PETAL,       // 十字花瓣：四个斜向弧形弹群
    BA_LASER,       // 预警激光：先亮红色警戒线，再射出粗激光（第 2 关起）
    BA_SUMMON,      // 召唤援军：唤出数架快速敌机（第 3 关起）
    BA_COUNT        // 招式总数（计数用）
};

/*==========================================================================*/
/*                              通用工具函数                                 */
/*==========================================================================*/

/**
 * @brief  返回 [a, b] 区间内的随机整数
 * @param  a 下界（含）
 * @param  b 上界（含）
 * @return 随机整数
 */
inline int randRange(int a, int b) {
    if (b <= a) return a;
    return a + rand() % (b - a + 1);
}

/**
 * @brief  返回 [0.0, 1.0] 区间内的随机浮点数
 */
inline double randFloat() {
    return (double)rand() / RAND_MAX;
}

/**
 * @brief  矩形（AABB）碰撞检测，坐标均以物体中心点表示
 * @param  x1,y1,w1,h1 物体 A 的中心坐标与宽高
 * @param  x2,y2,w2,h2 物体 B 的中心坐标与宽高
 * @return 两矩形相交返回 true
 * @note   对应课程提示：abs(x1-x2) < width && abs(y1-y2) < height
 */
inline bool rectHit(double x1, double y1, int w1, int h1,
    double x2, double y2, int w2, int h2) {
    return fabs(x1 - x2) < (w1 + w2) / 2.0 &&
        fabs(y1 - y2) < (h1 + h2) / 2.0;
}

/**
 * @brief  按比例把颜色向背景色淡出，用于粒子渐隐
 * @param  c 原始颜色
 * @param  ratio 保留比例 [0,1]，0 表示完全变为背景色
 * @return 淡出后的颜色
 */
COLORREF fadeColor(COLORREF c, double ratio) {
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    int r = (int)(GetRValue(c) * ratio + GetRValue(COL_BG) * (1 - ratio));
    int g = (int)(GetGValue(c) * ratio + GetGValue(COL_BG) * (1 - ratio));
    int b = (int)(GetBValue(c) * ratio + GetBValue(COL_BG) * (1 - ratio));
    return RGB(r, g, b);
}

/** @brief 将整数限制在 [lo, hi] 区间 */
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/** @brief 提亮颜色（各通道加 amt），用于绘制高光 */
inline COLORREF lighten(COLORREF c, int amt) {
    return RGB(clampi(GetRValue(c) + amt, 0, 255),
        clampi(GetGValue(c) + amt, 0, 255),
        clampi(GetBValue(c) + amt, 0, 255));
}

/** @brief 压暗颜色（各通道减 amt），用于绘制暗部/描边 */
inline COLORREF darken(COLORREF c, int amt) { return lighten(c, -amt); }

/*==========================================================================*/
/*                              游戏对象结构体                               */
/*==========================================================================*/

/** @brief 子弹：玩家子弹与敌方子弹共用同一结构，通过速度方向与颜色区分 */
struct Bullet {
    double   x, y;          // 中心坐标
    double   vx, vy;        // 速度分量
    int      radius;        // 半径（用于绘制与碰撞）
    int      damage;        // 伤害值
    bool     fromPlayer;    // true = 玩家子弹；false = 敌方子弹
    COLORREF color;         // 颜色
    bool     alive;         // 是否存活（越界或命中后置 false 以便移除）
};

/** @brief 敌机 */
struct Enemy {
    double    x, y;         // 中心坐标
    double    vx, vy;       // 速度分量
    int       w, h;         // 宽高
    int       hp;           // 生命值
    int       maxHp;        // 最大生命值
    int       armor;        // 护甲（减免子弹伤害，最低仍造成 1 点）
    int       score;        // 击毁得分
    EnemyType type;         // 类型
    int       shootCd;      // 射击冷却计时（射击敌机使用）
    double    baseX;        // 蛇形移动的中心 x（用于计算正弦偏移）
    double    phase;        // 蛇形移动的相位（横掠机复用为目标高度）
    int       aiState;      // 通用 AI 状态位（俯冲/横掠等分阶段行为）
    int       hitFlash;     // 受击白闪计时
    bool      alive;        // 是否存活
};

/** @brief Boss（独立管理，仅存在一个） */
struct Boss {
    double    x, y;         // 中心坐标
    double    vx;           // 水平速度（左右巡逻）
    int       w, h;         // 宽高
    int       hp;           // 当前生命值
    int       maxHp;        // 最大生命值
    int       attackTimer;  // 距下一次“选招”的间歇计时
    int       hitFlash;     // 受击白闪计时
    bool      active;       // 是否已登场
    /*---- 招式状态机 ----*/
    int       castType;     // 当前施放的招式（BossAtk），-1 表示间歇中
    int       castTimer;    // 当前招式剩余持续帧
    int       lastType;     // 上一招（用于避免连续重复）
    double    castAngle;    // 环形/螺旋类招式的累计角度
    int       laserX;       // 激光目标横坐标（施放瞬间锁定）
    int       laserWarn;    // 激光预警剩余帧
    int       laserFire;    // 激光开火剩余帧
};

/** @brief 道具 */
struct Item {
    double   x, y;          // 中心坐标
    double   vy;            // 下落速度
    int      size;          // 尺寸
    ItemType type;          // 类型
    bool     alive;         // 是否存活
};

/** @brief 爆炸/受击粒子 */
struct Particle {
    double   x, y;          // 中心坐标
    double   vx, vy;        // 速度
    int      life;          // 剩余寿命（帧）
    int      maxLife;       // 初始寿命
    double   size;          // 尺寸
    COLORREF color;         // 颜色
};

/** @brief 背景星星（视差滚动） */
struct Star {
    double   x, y;          // 坐标
    double   speed;         // 下落速度
    int      bright;        // 亮度（灰度值）
};

/** @brief 排行榜条目 */
struct RankItem {
    std::basic_string<TCHAR> name;   // 玩家昵称
    int score;                       // 分数
};

/*==========================================================================*/
/*                              游戏主类                                     */
/*==========================================================================*/

/**
 * @brief 游戏主类，集中管理全部游戏状态、数据与逻辑。
 *        通过 run() 进入主循环；内部按状态机分发输入、更新、渲染。
 */
class Game {
public:
    Game() { srand((unsigned)time(NULL)); }

    /** @brief 程序入口：初始化图形环境并进入主循环 */
    void run();

private:
    /*------------------------- 核心流程 -------------------------*/
    void resetGame();                       // 重置为一局新游戏的初始状态
    void pollInput();                       // 采样本帧键盘状态
    void handleInput();                     // 根据当前状态处理输入
    void update();                          // 根据当前状态更新逻辑
    void render();                          // 根据当前状态渲染画面

    /*------------------------- 逻辑更新 -------------------------*/
    void updatePlaying();                   // 游戏进行中的逻辑更新
    void spawnEnemy();                      // 生成一架敌机
    void playerShoot();                     // 玩家发射子弹
    void updateBullets();                   // 更新所有子弹
    void updateEnemies();                   // 更新所有敌机
    void updateBoss();                      // 更新 Boss
    void updateItems();                     // 更新道具
    void updateParticles();                 // 更新粒子
    void updateStars();                     // 更新背景星空
    void handleCollisions();                // 统一处理各类碰撞
    void hitPlayer();                       // 玩家受到一次伤害
    void killEnemy(Enemy& e);               // 击毁敌机的统一处理（计分、掉落、爆炸）
    void spawnBoss();                       // 生成 Boss
    void updateBossCast();                  // Boss 招式状态机（选招与施放）
    void spawnEnemyBullet(double x, double y, double vx, double vy,
        COLORREF c, int r = 6);       // 生成一发敌方子弹
    void activateUltimate();                // 释放激光大招
    void updateUltimate();                  // 大招每帧结算（伤害/清弹）
    void dropItem(double x, double y, bool fromBoss = false); // 在指定位置概率掉落道具
    void applyItem(ItemType t);             // 应用道具效果
    void useBomb();                         // 使用炸弹
    void spawnExplosion(double x, double y, COLORREF c, int count); // 生成爆炸粒子
    void nextLevelOrWin();                  // Boss 被击败后进入下一关或胜利

    /*------------------------- 画面渲染 -------------------------*/
    void drawBackground();                  // 星空背景
    void drawPlayer();                      // 玩家飞机
    void drawEnemies();                     // 敌机
    void drawBoss();                        // Boss 与血条
    void drawBullets();                     // 子弹
    void drawItems();                       // 道具
    void drawParticles();                   // 粒子
    void drawHUD();                         // 分数、生命、状态等信息
    void drawMenu();                        // 主菜单
    void drawPaused();                      // 暂停界面
    void drawInfoPanel();                   // 属性图鉴面板（暂停时按 I 打开）
    void drawGameOver();                    // 失败界面
    void drawVictory();                     // 胜利界面
    void drawEnemyShape(const Enemy& e);    // 绘制单个敌机造型
    void drawUltimate();                    // 绘制激光大招光束
    void drawHeart(int cx, int cy, int r, COLORREF c); // 绘制爱心（表示生命）

    /*------------------------- 数据存档 -------------------------*/
    void loadLeaderboard();                 // 从文件读取排行榜
    void saveLeaderboard();                 // 写回排行榜文件
    void tryRecordScore();                  // 若成绩进入前 10 则记录

    /*------------------------- 输入辅助 -------------------------*/
    bool keyDown(int vk)     const { return curKey[vk]; }               // 按键是否按住
    bool justPressed(int vk) const { return curKey[vk] && !prevKey[vk]; } // 按键是否本帧刚按下

#if ENABLE_SOUND
    void playSound(LPCTSTR file) { PlaySound(file, NULL, SND_FILENAME | SND_ASYNC); }
#endif

private:
    /*------------------------- 状态数据 -------------------------*/
    bool      running = true;               // 主循环是否继续
    GameState state = ST_MENU;            // 当前状态
    bool      infoPanel = false;            // 暂停时是否显示属性面板

    bool curKey[256] = { 0 };                 // 本帧按键状态
    bool prevKey[256] = { 0 };                // 上一帧按键状态

    /*------------------------- 玩家数据 -------------------------*/
    double px, py;                          // 玩家中心坐标
    int    lives;                           // 当前生命数
    int    maxLives;                        // 生命上限（可被永久道具提升）
    int    bulletDmg;                       // 子弹伤害（可被永久道具提升）
    int    invincible;                      // 无敌剩余帧数
    int    shootCd;                         // 射击冷却计时
    int    fireLevel;                       // 火力等级（1/2/3）
    int    fireTimer;                       // 火力增强剩余帧数
    bool   shield;                          // 是否持有护盾
    int    bombs;                           // 炸弹数量
    int    score;                           // 当前分数
    /*---- 新技能：激光大招 与 闪避冲刺 ----*/
    int    energy;                          // 能量值（0..ENERGY_MAX，集满可放大招）
    int    ultTimer;                        // 激光大招剩余帧数（>0 表示正在释放）
    int    dashCd;                          // 闪避冷却剩余帧
    int    dashTimer;                       // 闪避冲刺剩余帧
    double dashVX, dashVY;                  // 闪避冲刺方向（单位向量）

    /*------------------------- 关卡数据 -------------------------*/
    int    level;                           // 当前关卡（1..TOTAL_LEVEL）
    int    levelTimer;                      // 本关计时器
    int    spawnTimer;                      // 敌机生成计时器
    bool   bossPhase;                       // 是否进入 Boss 阶段
    int    shakeTimer;                      // 屏幕震动剩余帧数
    int    animTick = 0;                    // 全局动画计时（用于呼吸/脉冲等特效，持续递增）

    /*------------------------- 对象容器 -------------------------*/
    std::vector<Bullet>   bullets;          // 全部子弹
    std::vector<Enemy>    enemies;          // 全部敌机
    std::vector<Item>     items;            // 全部道具
    std::vector<Particle> particles;        // 全部粒子
    std::vector<Star>     stars;            // 背景星空
    Boss   boss;                            // Boss（单例）

    /*------------------------- 排行榜 -------------------------*/
    std::vector<RankItem> ranks;            // 排行榜（降序，最多 10 条）
    const TCHAR* RANK_FILE = _T("leaderboard.txt"); // 存档文件名
};

/*==========================================================================*/
/*                              主循环实现                                   */
/*==========================================================================*/

void Game::run() {
    // 创建图形窗口并开启批量绘图（双缓冲），保证无闪烁刷新
    initgraph(WIN_W, WIN_H);
    setbkcolor(COL_BG);
    setbkmode(TRANSPARENT);                 // 文字背景透明
    BeginBatchDraw();

    // 将系统计时器精度提升到 1ms。Windows 默认精度约 15.6ms，会导致 Sleep
    // 过冲、帧率被压到 30 左右；提升后配合 timeGetTime 即可稳定跑满 60 FPS。
    timeBeginPeriod(1);

    // 初始化背景星空（无论何种状态都持续滚动）
    stars.clear();
    for (int i = 0; i < 80; ++i) {
        Star s;
        s.x = randRange(0, WIN_W);
        s.y = randRange(0, WIN_H);
        s.speed = 0.6 + randFloat() * 2.4;  // 不同速度形成视差
        s.bright = randRange(60, 255);
        stars.push_back(s);
    }

    loadLeaderboard();                       // 读取历史排行榜
    resetGame();                             // 准备数据
    state = ST_MENU;                         // 从菜单开始

    // 主循环：处理输入 -> 更新逻辑 -> 渲染画面 -> 帧率控制
    while (running) {
        DWORD frameStart = timeGetTime();    // 高精度毫秒计时（受 timeBeginPeriod 影响）

        pollInput();
        handleInput();
        update();
        render();
        FlushBatchDraw();                    // 将后台缓冲一次性刷新到屏幕

        // 复制本帧按键状态，供下一帧做“边沿检测”
        memcpy(prevKey, curKey, sizeof(curKey));

        // 帧率控制：补足本帧剩余时间，使每帧稳定在 FRAME_MS（约 16ms → 60 FPS）
        DWORD used = timeGetTime() - frameStart;
        if (used < FRAME_MS) Sleep(FRAME_MS - used);
    }

    timeEndPeriod(1);                        // 归还计时器精度
    EndBatchDraw();
    closegraph();
}

/*==========================================================================*/
/*                              初始化与重置                                 */
/*==========================================================================*/

void Game::resetGame() {
    // 玩家初始置于屏幕底部中央
    px = WIN_W / 2.0;
    py = WIN_H - 90;
    lives = START_LIVES;
    maxLives = START_LIVES;
    bulletDmg = 1;
    invincible = 0;
    shootCd = 0;
    fireLevel = 1;
    fireTimer = 0;
    shield = false;
    bombs = 1;
    score = 0;
    energy = 0;
    ultTimer = 0;
    dashCd = 0;
    dashTimer = 0;
    dashVX = dashVY = 0;

    level = 1;
    levelTimer = 0;
    spawnTimer = 0;
    bossPhase = false;
    shakeTimer = 0;
    infoPanel = false;

    bullets.clear();
    enemies.clear();
    items.clear();
    particles.clear();
    boss.active = false;
}

/*==========================================================================*/
/*                              输入处理                                     */
/*==========================================================================*/

void Game::pollInput() {
    // 逐一采样所需按键的“按下”状态（最高位为 1 表示当前按住）
    for (int i = 0; i < 256; ++i)
        curKey[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
}

void Game::handleInput() {
    switch (state) {
    case ST_MENU:
        // 回车 / 空格开始游戏；Esc 退出
        if (justPressed(VK_RETURN) || justPressed(VK_SPACE)) {
            resetGame();
            state = ST_PLAYING;
        }
        if (justPressed(VK_ESCAPE)) running = false;
        break;

    case ST_PLAYING: {
        // ---- 移动：WASD 与方向键均可，支持斜向移动 ----
        int rdx = 0, rdy = 0;                          // 原始输入方向（-1/0/1）
        if (keyDown('A') || keyDown(VK_LEFT))  rdx -= 1;
        if (keyDown('D') || keyDown(VK_RIGHT)) rdx += 1;
        if (keyDown('W') || keyDown(VK_UP))    rdy -= 1;
        if (keyDown('S') || keyDown(VK_DOWN))  rdy += 1;
        double dx = rdx, dy = rdy;
        // 斜向归一化，避免对角线速度更快
        if (dx != 0 && dy != 0) { dx *= 0.7071; dy *= 0.7071; }
        px += dx * PLAYER_SPD;
        py += dy * PLAYER_SPD;

        // ---- 闪避冲刺：R 键，朝当前输入方向瞬移并短暂无敌 ----
        if (justPressed('R') && dashCd <= 0 && (rdx != 0 || rdy != 0)) {
            double len = sqrt((double)(rdx * rdx + rdy * rdy));
            dashVX = rdx / len; dashVY = rdy / len;
            dashTimer = DASH_FRAMES;
            dashCd = DASH_CD;
            if (invincible < DASH_FRAMES + 4) invincible = DASH_FRAMES + 4; // 冲刺期间无敌
            spawnExplosion(px, py, RGB(120, 220, 255), 6);                  // 冲刺残影
        }
        if (dashTimer > 0) { px += dashVX * DASH_SPD; py += dashVY * DASH_SPD; }

        // 边界处理：飞机不能移出屏幕
        double hw = PLAYER_W / 2.0, hh = PLAYER_H / 2.0;
        if (px < hw)          px = hw;
        if (px > WIN_W - hw)  px = WIN_W - hw;
        if (py < hh)          py = hh;
        if (py > WIN_H - hh)  py = WIN_H - hh;

        // ---- 射击：按住空格连发 ----
        if (keyDown(VK_SPACE) && shootCd <= 0) {
            playerShoot();
            shootCd = SHOOT_CD;
        }
        // ---- 激光大招：X 键，能量集满时释放 ----
        if (justPressed('X') && energy >= ENERGY_MAX && ultTimer <= 0)
            activateUltimate();
        // ---- 炸弹：B 键，边沿触发一次 ----
        if (justPressed('B')) useBomb();
        // ---- 暂停：P 键 ----
        if (justPressed('P')) state = ST_PAUSED;
        break;
    }

    case ST_PAUSED:
        if (justPressed('I')) infoPanel = !infoPanel;  // 切换属性面板
        if (justPressed('P') || justPressed(VK_RETURN)) { state = ST_PLAYING; infoPanel = false; }
        if (justPressed(VK_ESCAPE)) { state = ST_MENU; infoPanel = false; } // 放弃本局回到菜单
        break;

    case ST_GAMEOVER:
    case ST_VICTORY:
        // 回车回到菜单
        if (justPressed(VK_RETURN) || justPressed(VK_SPACE))
            state = ST_MENU;
        if (justPressed(VK_ESCAPE)) running = false;
        break;
    }
}

/*==========================================================================*/
/*                              逻辑更新分发                                 */
/*==========================================================================*/

void Game::update() {
    ++animTick;             // 全局动画计时器持续推进（用于脉冲/呼吸特效）
    updateStars();          // 星空在任何状态下都滚动，作为动态背景
    updateParticles();      // 粒子亦持续更新（菜单等界面无粒子时无影响）
    if (state == ST_PLAYING) updatePlaying();
}

void Game::updatePlaying() {
    if (shootCd > 0) --shootCd;
    if (invincible > 0) --invincible;
    if (shakeTimer > 0) --shakeTimer;
    if (dashCd > 0) --dashCd;
    if (dashTimer > 0) --dashTimer;
    if (fireTimer > 0) { if (--fireTimer == 0) fireLevel = 1; } // 火力增强到期

    // ---- 关卡节奏控制 ----
    if (!bossPhase) {
        ++levelTimer;
        // 敌机生成：间隔随关卡缩短（难度递增）
        int interval = 46 - level * 8;                 // 基础间隔
        interval -= levelTimer / 240;                  // 随时间进一步加快
        if (interval < 16) interval = 16;
        if (++spawnTimer >= interval) {
            spawnTimer = 0;
            spawnEnemy();
        }
        // 到达本关时长后，清场刷小怪并召唤 Boss
        if (levelTimer >= LEVEL_FRAMES) spawnBoss();
    }

    updateBullets();
    updateEnemies();
    updateBoss();
    updateItems();
    if (ultTimer > 0) updateUltimate();                // 激光大招结算
    handleCollisions();
}

/*==========================================================================*/
/*                              敌机生成与更新                               */
/*==========================================================================*/

void Game::spawnEnemy() {
    Enemy e;
    e.alive = true;
    e.hitFlash = 0;
    e.shootCd = randRange(60, 120);
    e.phase = randFloat() * 6.28;
    e.aiState = 0;

    // 根据关卡决定可用敌机类型（越往后类型越丰富、越强）
    int roll = randRange(0, 99);
    if (level == 1) {
        e.type = (roll < 72) ? E_NORMAL : E_FAST;
    }
    else if (level == 2) {
        e.type = (roll < 40) ? E_NORMAL : (roll < 62) ? E_FAST
            : (roll < 82) ? E_ZIGZAG : E_TANK;
    }
    else {
        e.type = (roll < 26) ? E_NORMAL : (roll < 44) ? E_FAST
            : (roll < 58) ? E_ZIGZAG : (roll < 70) ? E_TANK
            : (roll < 82) ? E_SHOOTER : (roll < 92) ? E_STRAFER : E_DIVER;
    }

    double speedScale = 1.0 + (level - 1) * 0.25;      // 速度随关卡提升
    // 生命值与护甲统一由查询函数决定，保证与属性面板显示一致
    e.hp = e.maxHp = enemyHp(e.type, level);
    e.armor = enemyArmor(e.type);

    switch (e.type) {
    case E_NORMAL:
        e.w = 40; e.h = 36; e.score = 10;
        e.vy = (1.6 + randFloat() * 0.6) * speedScale; e.vx = 0;
        break;
    case E_FAST:
        e.w = 32; e.h = 34; e.score = 20;
        e.vy = (3.2 + randFloat() * 0.8) * speedScale; e.vx = 0;
        break;
    case E_SHOOTER:
        e.w = 46; e.h = 42; e.score = 30;
        e.vy = (1.2 + randFloat() * 0.4) * speedScale; e.vx = 0;
        break;
    case E_ZIGZAG:
        e.w = 38; e.h = 36; e.score = 25;
        e.vy = (1.8 + randFloat() * 0.5) * speedScale;
        e.vx = 2.4;                                    // 蛇形水平摆幅速度
        break;
    case E_TANK:                                       // 装甲重机：血厚、慢、分高
        e.w = 54; e.h = 48; e.score = 60;
        e.vy = (0.9 + randFloat() * 0.3) * speedScale; e.vx = 0;
        break;
    case E_DIVER:                                      // 俯冲自爆机
        e.w = 34; e.h = 40; e.score = 35;
        e.vy = 1.5 * speedScale; e.vx = 0;
        break;
    case E_STRAFER:                                    // 横掠射击机
        e.w = 46; e.h = 34; e.score = 40;
        e.vy = 2.2 * speedScale;
        // 进入横掠阶段后的水平方向：靠左则向右扫，反之向左
        e.x = randRange(e.w / 2, WIN_W - e.w / 2);
        e.vx = (e.x < WIN_W / 2 ? 1 : -1) * (2.6 + level * 0.2);
        e.y = -e.h; e.baseX = e.x;
        enemies.push_back(e);
        return;                                        // 横掠机 x 已定，直接入列
    }

    e.x = randRange(e.w / 2, WIN_W - e.w / 2);
    e.y = -e.h;                                        // 从屏幕上方进入
    e.baseX = e.x;
    enemies.push_back(e);
}

void Game::updateEnemies() {
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.hitFlash > 0) --e.hitFlash;

        // ---- 各类型移动逻辑 ----
        switch (e.type) {
        case E_ZIGZAG:                                 // 蛇形（正弦波）移动
            e.phase += 0.06;
            e.y += e.vy;
            e.x = e.baseX + sin(e.phase) * 70.0;
            if (e.baseX < 60)          e.baseX = 60;
            if (e.baseX > WIN_W - 60)  e.baseX = WIN_W - 60;
            break;
        case E_DIVER:                                  // 俯冲：先缓降，进入攻击线后锁定玩家高速俯冲
            if (e.aiState == 0) {
                e.y += e.vy;
                if (e.y > 170) { e.aiState = 1; spawnExplosion(e.x, e.y, COL_E_DIVER, 4); }
            }
            else {
                double dir = (px > e.x) ? 1.0 : -1.0;  // 朝玩家横向偏转
                e.vx += dir * 0.28;
                if (e.vx > 4.2) e.vx = 4.2; if (e.vx < -4.2) e.vx = -4.2;
                e.vy += 0.10; if (e.vy > 8.5) e.vy = 8.5;  // 越冲越快
                e.x += e.vx; e.y += e.vy;
            }
            break;
        case E_STRAFER:                                // 横掠：降到指定高度后横向扫场
            if (e.aiState == 0) {
                e.y += e.vy;
                if (e.y >= 120 + randRange(0, 20)) { e.aiState = 1; }
            }
            else {
                e.x += e.vx;
                if (e.x < -e.w || e.x > WIN_W + e.w) e.alive = false; // 掠出屏幕消失
            }
            break;
        default:                                       // 普通 / 快速 / 射击 / 装甲：直线
            e.x += e.vx; e.y += e.vy;
            break;
        }

        // ---- 射击：射击机与横掠机会向下开火 ----
        bool canShoot = (e.type == E_SHOOTER && e.y > 0 && e.y < WIN_H - 120) ||
            (e.type == E_STRAFER && e.aiState == 1);
        if (canShoot && --e.shootCd <= 0) {
            e.shootCd = (e.type == E_STRAFER) ? randRange(40, 70) : randRange(70, 110);
            spawnEnemyBullet(e.x, e.y + e.h / 2.0, 0, 4.0 + level * 0.5, COL_EBULLET, 5);
        }

        // 越界（移出屏幕底部）则消失
        if (e.y - e.h / 2 > WIN_H) e.alive = false;
    }
    // 移除已消亡的敌机
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const Enemy& e) { return !e.alive; }), enemies.end());
}

/*==========================================================================*/
/*                              Boss 逻辑                                    */
/*==========================================================================*/

void Game::spawnBoss() {
    if (boss.active) return;
    bossPhase = true;
    // 清空当前小怪，营造 Boss 登场氛围
    for (auto& e : enemies) { spawnExplosion(e.x, e.y, COL_E_NORMAL, 8); }
    enemies.clear();

    boss.active = true;
    boss.w = 150; boss.h = 90;
    boss.x = WIN_W / 2.0;
    boss.y = -boss.h;                                  // 从上方缓缓进入
    boss.vx = 2.0 + level * 0.4;
    boss.maxHp = bossHp(level);                        // 血量分阶段递增：110 / 160 / 210
    boss.hp = boss.maxHp;
    boss.attackTimer = 90;
    boss.hitFlash = 0;
    // 招式状态机初始化
    boss.castType = -1;
    boss.castTimer = 0;
    boss.lastType = -1;
    boss.castAngle = 0;
    boss.laserWarn = 0;
    boss.laserFire = 0;
    boss.laserX = WIN_W / 2;
}

/**
 * @brief  生成一发敌方子弹（供 Boss 各种弹幕复用）
 */
void Game::spawnEnemyBullet(double x, double y, double vx, double vy, COLORREF c, int r) {
    Bullet b;
    b.x = x; b.y = y; b.vx = vx; b.vy = vy;
    b.radius = r; b.damage = 1; b.fromPlayer = false;
    b.color = c; b.alive = true;
    bullets.push_back(b);
}

void Game::updateBoss() {
    if (!boss.active) return;
    if (boss.hitFlash > 0) --boss.hitFlash;

    // 登场下滑，随后在顶部左右巡逻
    if (boss.y < 110) {
        boss.y += 1.5;
        return;                                        // 入场期间不攻击
    }
    boss.x += boss.vx;
    if (boss.x < boss.w / 2) { boss.x = boss.w / 2;          boss.vx = -boss.vx; }
    if (boss.x > WIN_W - boss.w / 2) { boss.x = WIN_W - boss.w / 2;  boss.vx = -boss.vx; }

    updateBossCast();                                  // 招式选取与施放
}

/**
 * @brief  Boss 招式状态机：间歇 -> 选招 -> 逐帧施放。招式共 6 套，随机选取且
 *         尽量不与上一招重复，配合关卡逐步解锁激光与召唤，攻击更华丽多变。
 */
void Game::updateBossCast() {
    const double bx = boss.x, by = boss.y + boss.h / 2.0;  // 弹幕发射口（机体下沿）
    const double bs = 1.0 + (level - 1) * 0.16;            // 弹速随关卡递增（Boss 强度提升）

    // ---------- 激光子系统（独立计时，先预警后开火） ----------
    if (boss.castType == BA_LASER) {
        if (boss.laserWarn > 0) {
            --boss.laserWarn;
        }
        else if (boss.laserFire > 0) {
            --boss.laserFire;
            // 激光命中判定：玩家横向落在光束内、且位于 Boss 下方即受击
            if (fabs(px - boss.laserX) < BOSS_BEAM_HALF + PLAYER_W * 0.3 && py > boss.y) {
                hitPlayer();
                if (state != ST_PLAYING) return;
            }
        }
    }

    // ---------- 正在施放某招：逐帧发射 ----------
    if (boss.castTimer > 0) {
        int elapsed = 0;                               // 用于按节奏分批发射
        switch (boss.castType) {
        case BA_RING: {                                // 旋转环形：每 8 帧一圈，逐圈旋转错开
            elapsed = 26 - boss.castTimer;
            if (elapsed % 8 == 0) {
                int n = 16 + level * 2;                 // 弹数随关卡增加
                for (int i = 0; i < n; ++i) {
                    double a = boss.castAngle + 6.2832 * i / n;
                    spawnEnemyBullet(bx, by, cos(a) * 3.1 * bs, sin(a) * 3.1 * bs + 0.6, RGB(255, 170, 90));
                }
                boss.castAngle += 0.35;                // 下一圈整体旋转，形成花瓣
            }
            break;
        }
        case BA_AIM: {                                 // 瞄准扇形：每 9 帧一波 7 发
            elapsed = 30 - boss.castTimer;
            if (elapsed % 9 == 0) {
                double base = atan2(py - by, px - bx);
                for (int i = -3; i <= 3; ++i)
                    spawnEnemyBullet(bx, by, cos(base + i * 0.14) * 4.4 * bs,
                        sin(base + i * 0.14) * 4.4 * bs, COL_EBULLET);
            }
            break;
        }
        case BA_SPIRAL: {                              // 螺旋双臂：每 4 帧喷两发对称弹
            elapsed = 96 - boss.castTimer;
            if (elapsed % 4 == 0) {
                double a = boss.castAngle;
                spawnEnemyBullet(bx, by, cos(a) * 2.9 * bs, sin(a) * 2.9 * bs + 0.8, RGB(120, 210, 255));
                spawnEnemyBullet(bx, by, cos(a + 3.1416) * 2.9 * bs, sin(a + 3.1416) * 2.9 * bs + 0.8, RGB(120, 210, 255));
                boss.castAngle += 0.30 - level * 0.03; // 关卡越高螺旋越密
            }
            break;
        }
        case BA_PETAL: {                               // 十字花瓣：两波，四个斜向弧
            elapsed = 22 - boss.castTimer;
            if (elapsed == 0 || elapsed == 11) {
                const double dirs[4] = { 0.9, 2.24, 3.94, 5.28 }; // 四个斜下方向
                for (double d : dirs)
                    for (int i = -2; i <= 2; ++i)
                        spawnEnemyBullet(bx, by, cos(d + i * 0.16) * 3.6 * bs,
                            sin(d + i * 0.16) * 3.6 * bs, RGB(255, 130, 200));
            }
            break;
        }
        case BA_LASER:  break;                         // 激光由上方子系统处理
        case BA_SUMMON:                                // 召唤援军：入场瞬间刷 3 架
            if (boss.castTimer == 6) for (int i = 0; i < 3; ++i) spawnEnemy();
            break;
        }
        if (--boss.castTimer <= 0) {                   // 本招结束，进入间歇
            boss.castType = -1;
            boss.attackTimer = 46 - level * 6;         // 关卡越高间歇越短（40/34/28）
        }
        return;
    }

    // ---------- 间歇：冷却结束后随机挑选下一招 ----------
    if (--boss.attackTimer > 0) return;

    // 依关卡组装可用招式池
    int pool[BA_COUNT]; int np = 0;
    pool[np++] = BA_RING; pool[np++] = BA_AIM;
    pool[np++] = BA_SPIRAL; pool[np++] = BA_PETAL;
    if (level >= 2) pool[np++] = BA_LASER;
    if (level >= 3) pool[np++] = BA_SUMMON;

    int t = pool[randRange(0, np - 1)];                // 随机挑一招
    for (int guard = 0; t == boss.lastType && guard < 4; ++guard)
        t = pool[randRange(0, np - 1)];                // 尽量不与上一招重复
    boss.lastType = t;
    boss.castType = t;
    boss.castAngle = randFloat() * 6.2832;             // 环形/螺旋随机起始角

    switch (t) {                                       // 设定各招持续时长
    case BA_RING:   boss.castTimer = 26; break;
    case BA_AIM:    boss.castTimer = 30; break;
    case BA_SPIRAL: boss.castTimer = 96; break;
    case BA_PETAL:  boss.castTimer = 22; break;
    case BA_SUMMON: boss.castTimer = 6;  break;
    case BA_LASER:                                     // 激光：预警 + 开火
        boss.laserX = (int)px;                      // 锁定当前玩家横坐标
        boss.laserWarn = 36;
        boss.laserFire = 42;
        boss.castTimer = 36 + 42;
        break;
    }
}

/*==========================================================================*/
/*                              子弹更新                                     */
/*==========================================================================*/

void Game::playerShoot() {
    // 根据火力等级发射不同数量/角度的子弹
    auto makeBullet = [&](double ox, double vx) {
        Bullet b;
        b.x = px + ox; b.y = py - PLAYER_H / 2.0;
        b.vx = vx; b.vy = -13.0;
        b.radius = 4; b.damage = bulletDmg; b.fromPlayer = true;
        b.color = COL_PBULLET; b.alive = true;
        bullets.push_back(b);
        };
    if (fireLevel == 1) {
        makeBullet(0, 0);                              // 单发
    }
    else if (fireLevel == 2) {
        makeBullet(-12, 0); makeBullet(12, 0);         // 双发
    }
    else {
        makeBullet(0, 0);                              // 三发（中路 + 两侧斜射）
        makeBullet(-14, -2.2); makeBullet(14, 2.2);
    }
#if ENABLE_SOUND
    playSound(_T("shoot.wav"));
#endif
}

void Game::updateBullets() {
    for (auto& b : bullets) {
        b.x += b.vx; b.y += b.vy;
        // 越出画面则回收
        if (b.y < -20 || b.y > WIN_H + 20 || b.x < -20 || b.x > WIN_W + 20)
            b.alive = false;
    }
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [](const Bullet& b) { return !b.alive; }), bullets.end());
}

/*==========================================================================*/
/*                              道具与粒子更新                               */
/*==========================================================================*/

void Game::dropItem(double x, double y, bool fromBoss) {
    // 触发概率：普通敌机约 20%；Boss 掉落必定触发（外部会多次调用形成道具簇）
    int chance = fromBoss ? 100 : 20;
    if (randRange(0, 99) >= chance) return;

    // 永久增益概率：明显低于临时增益；Boss 掉落显著更高；且随关卡（难度）递增
    int permPct = fromBoss ? (28 + level * 10)     // Boss：38% / 48% / 58%
        : (4 + level * 3);      // 普通：7% / 10% / 13%

    Item it;
    it.x = x; it.y = y; it.vy = 2.0; it.size = 26; it.alive = true;

    if (randRange(0, 99) < permPct) {              // 永久增益（稀有）
        it.type = (randRange(0, 1) == 0) ? IT_MAXLIFE : IT_POWERUP;
    }
    else {                                       // 临时增益
        int r = randRange(0, 99);
        it.type = (r < 40) ? IT_FIRE : (r < 65) ? IT_SHIELD
            : (r < 85) ? IT_LIFE : IT_BOMB;
    }
    items.push_back(it);
}

void Game::updateItems() {
    for (auto& it : items) {
        it.y += it.vy;
        if (it.y - it.size / 2 > WIN_H) it.alive = false;
    }
    items.erase(std::remove_if(items.begin(), items.end(),
        [](const Item& it) { return !it.alive; }), items.end());
}

void Game::applyItem(ItemType t) {
    switch (t) {
    case IT_FIRE:                                      // 火力增强：升级并刷新计时
        if (fireLevel < 3) ++fireLevel;
        fireTimer = FIRE_BUFF_FRAMES;
        break;
    case IT_LIFE:                                      // 生命恢复：不超过当前上限
        if (lives < maxLives) ++lives;
        break;
    case IT_SHIELD:                                    // 护盾：可抵挡一次伤害
        shield = true;
        break;
    case IT_BOMB:                                      // 炸弹补给：不超过上限
        if (bombs < MAX_BOMBS) ++bombs;
        break;
    case IT_MAXLIFE:                                   // 【永久】生命上限 +1（封顶 5），并回 1 点
        if (maxLives < MAX_LIVES_CAP) { ++maxLives; ++lives; }
        else if (lives < maxLives) { ++lives; }      // 已达上限则改为回血，不浪费
        spawnExplosion(px, py, RGB(255, 120, 160), 16);
        break;
    case IT_POWERUP:                                   // 【永久】子弹伤害 +1（封顶 4）
        if (bulletDmg < BULLET_DMG_MAX) ++bulletDmg;
        spawnExplosion(px, py, RGB(200, 140, 255), 16);
        break;
    }
#if ENABLE_SOUND
    playSound(_T("powerup.wav"));
#endif
}

void Game::useBomb() {
    if (bombs <= 0) return;
    --bombs;
    // 清屏：击毁全部敌机并计分、清空敌方子弹
    for (auto& e : enemies) {
        score += e.score;
        spawnExplosion(e.x, e.y, COL_E_NORMAL, 10);
    }
    enemies.clear();
    for (auto& b : bullets) if (!b.fromPlayer) b.alive = false;
    // 对 Boss 造成一次大额伤害；若因此击败 Boss 则立即结算
    if (boss.active) {
        boss.hp -= 20; boss.hitFlash = 6;
        if (boss.hp <= 0) { nextLevelOrWin(); }
    }
    shakeTimer = 12;
#if ENABLE_SOUND
    playSound(_T("bomb.wav"));
#endif
}

void Game::spawnExplosion(double x, double y, COLORREF c, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        double ang = randFloat() * 6.2832;
        double spd = 1.0 + randFloat() * 4.0;
        p.x = x; p.y = y;
        p.vx = cos(ang) * spd; p.vy = sin(ang) * spd;
        p.maxLife = p.life = randRange(18, 34);
        p.size = 2.0 + randFloat() * 3.0;
        p.color = c;
        particles.push_back(p);
    }
}

void Game::updateParticles() {
    for (auto& p : particles) {
        p.x += p.vx; p.y += p.vy;
        p.vx *= 0.94; p.vy *= 0.94;                    // 阻尼，逐渐减速
        --p.life;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return p.life <= 0; }), particles.end());
}

void Game::updateStars() {
    for (auto& s : stars) {
        s.y += s.speed;
        if (s.y > WIN_H) { s.y = 0; s.x = randRange(0, WIN_W); } // 循环滚动
    }
}

/*==========================================================================*/
/*                              碰撞与伤害                                   */
/*==========================================================================*/

void Game::killEnemy(Enemy& e) {
    e.alive = false;
    score += e.score;
    energy = clampi(energy + ENERGY_KILL, 0, ENERGY_MAX);   // 击杀积攒大招能量
    spawnExplosion(e.x, e.y, RGB(255, 180, 80), 12);
    dropItem(e.x, e.y);
#if ENABLE_SOUND
    playSound(_T("explode.wav"));
#endif
}

/**
 * @brief  释放激光大招：清空能量、进入持续释放状态，并触发一次震屏。
 *         效果见 updateUltimate（逐帧结算）。
 */
void Game::activateUltimate() {
    energy = 0;
    ultTimer = ULT_FRAMES;
    shakeTimer = 10;
    spawnExplosion(px, py - PLAYER_H / 2.0, RGB(160, 240, 255), 20);
#if ENABLE_SOUND
    playSound(_T("laser.wav"));
#endif
}

/**
 * @brief  激光大招逐帧结算：以玩家为中心向上打出一道贯穿光束，
 *         秒杀光束覆盖范围内的敌机、抵消敌方子弹，并对处于同一纵列的
 *         Boss 造成持续伤害。是应对 Boss 的强力新武器。
 */
void Game::updateUltimate() {
    --ultTimer;
    // 秒杀光束覆盖的敌机
    for (auto& e : enemies) {
        if (e.alive && e.y < py && fabs(e.x - px) < ULT_HALF + e.w / 2.0)
            killEnemy(e);
    }
    // 抵消光束覆盖内的敌方子弹
    for (auto& b : bullets) {
        if (!b.fromPlayer && b.y < py && fabs(b.x - px) < ULT_HALF + b.radius)
            b.alive = false;
    }
    // 对同一纵列的 Boss 每 3 帧造成 1 点伤害
    if (boss.active && fabs(boss.x - px) < ULT_HALF + boss.w / 2.0) {
        if (ultTimer % 3 == 0) {
            boss.hp -= 1; boss.hitFlash = 2;
            if (boss.hp <= 0) { nextLevelOrWin(); return; }
        }
    }
    // 光束前端火花
    if (ultTimer % 2 == 0)
        spawnExplosion(px + randRange(-ULT_HALF, ULT_HALF), randRange(0, (int)py),
            RGB(180, 250, 255), 1);
}

void Game::hitPlayer() {
    if (invincible > 0) return;                        // 无敌期间免疫
    if (shield) {                                      // 护盾优先抵挡
        shield = false;
        invincible = 40;
        spawnExplosion(px, py, RGB(120, 200, 255), 14);
        shakeTimer = 8;
        return;
    }
    --lives;
    invincible = INVINCIBLE_FRAMES;                    // 受击后短暂无敌闪烁
    shakeTimer = 14;
    fireLevel = 1; fireTimer = 0;                      // 受击掉火力
    spawnExplosion(px, py, COL_PLAYER, 18);
#if ENABLE_SOUND
    playSound(_T("hit.wav"));
#endif
    if (lives <= 0) {                                  // 生命耗尽 -> 游戏结束
        tryRecordScore();
        state = ST_GAMEOVER;
    }
}

void Game::handleCollisions() {
    // 1) 玩家子弹 vs 敌机
    for (auto& b : bullets) {
        if (!b.alive || !b.fromPlayer) continue;
        for (auto& e : enemies) {
            if (!e.alive) continue;
            if (rectHit(b.x, b.y, b.radius * 2, b.radius * 2, e.x, e.y, e.w, e.h)) {
                b.alive = false;
                e.hp -= effDamage(b.damage, e.armor);  // 护甲减伤
                e.hitFlash = 4;
                if (e.hp <= 0) killEnemy(e);
                break;
            }
        }
    }

    // 2) 玩家子弹 vs Boss
    if (boss.active) {
        for (auto& b : bullets) {
            if (!b.alive || !b.fromPlayer) continue;
            if (rectHit(b.x, b.y, b.radius * 2, b.radius * 2, boss.x, boss.y, boss.w, boss.h)) {
                b.alive = false;
                boss.hp -= b.damage;
                boss.hitFlash = 3;
                energy = clampi(energy + 1, 0, ENERGY_MAX);   // 打 Boss 也能积攒能量
                if (boss.hp <= 0) { nextLevelOrWin(); return; }
            }
        }
    }

    // 3) 敌机 vs 玩家（撞机）
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (rectHit(px, py, PLAYER_W, PLAYER_H, e.x, e.y, e.w, e.h)) {
            killEnemy(e);
            hitPlayer();
            if (state != ST_PLAYING) return;
        }
    }

    // 4) 敌方/Boss 子弹 vs 玩家
    for (auto& b : bullets) {
        if (!b.alive || b.fromPlayer) continue;
        if (rectHit(px, py, PLAYER_W - 10, PLAYER_H - 10, b.x, b.y, b.radius * 2, b.radius * 2)) {
            b.alive = false;
            hitPlayer();
            if (state != ST_PLAYING) return;
        }
    }

    // 5) Boss 机体 vs 玩家
    if (boss.active && rectHit(px, py, PLAYER_W, PLAYER_H, boss.x, boss.y, boss.w, boss.h)) {
        hitPlayer();
        if (state != ST_PLAYING) return;
    }

    // 6) 道具 vs 玩家（拾取）
    for (auto& it : items) {
        if (!it.alive) continue;
        if (rectHit(px, py, PLAYER_W, PLAYER_H, it.x, it.y, it.size, it.size)) {
            it.alive = false;
            applyItem(it.type);
        }
    }
}

void Game::nextLevelOrWin() {
    // Boss 被击败：大爆炸、加分
    spawnExplosion(boss.x, boss.y, RGB(255, 200, 100), 40);
    spawnExplosion(boss.x, boss.y, RGB(255, 120, 120), 30);
    score += 500 * level;
    shakeTimer = 20;
    // Boss 掉落道具簇：数量与永久增益概率都更高（fromBoss=true）
    int nDrops = 3 + level;
    for (int i = 0; i < nDrops; ++i)
        dropItem(boss.x + randRange(-55, 55), boss.y + randRange(-20, 30), true);
    boss.active = false;
    bossPhase = false;
    ultTimer = 0;                                      // 停止大招光束（Boss 已消失）

    if (level >= TOTAL_LEVEL) {                         // 通关
        tryRecordScore();
        state = ST_VICTORY;
    }
    else {                                            // 进入下一关
        ++level;
        levelTimer = 0;
        spawnTimer = 0;
        bullets.clear();                                // 清屏进入新关卡
    }
}

/*==========================================================================*/
/*                              渲染分发                                     */
/*==========================================================================*/

void Game::render() {
    cleardevice();                                      // 以背景色清屏（后台缓冲）

    // 屏幕震动：通过设置逻辑原点让世界整体抖动，绘制 HUD 前再复位
    int ox = 0, oy = 0;
    if (shakeTimer > 0) { ox = randRange(-6, 6); oy = randRange(-6, 6); }
    setorigin(ox, oy);

    drawBackground();

    switch (state) {
    case ST_MENU:
        setorigin(0, 0);
        drawMenu();
        break;
    case ST_PLAYING:
    case ST_PAUSED:
        drawItems();
        drawEnemies();
        drawBoss();
        drawBullets();
        if (ultTimer > 0) drawUltimate();               // 激光大招（在玩家之下）
        drawPlayer();
        drawParticles();
        setorigin(0, 0);                                // HUD 不随震动
        drawHUD();
        if (state == ST_PAUSED) { if (infoPanel) drawInfoPanel(); else drawPaused(); }
        break;
    case ST_GAMEOVER:
        drawParticles();
        setorigin(0, 0);
        drawGameOver();
        break;
    case ST_VICTORY:
        drawParticles();
        setorigin(0, 0);
        drawVictory();
        break;
    }
    setorigin(0, 0);
}

/*==========================================================================*/
/*                              背景与粒子绘制                               */
/*==========================================================================*/

void Game::drawBackground() {
    // 滚动星空：不同亮度与速度形成层次感
    for (const auto& s : stars) {
        COLORREF c = RGB(s.bright, s.bright, s.bright);
        putpixel((int)s.x, (int)s.y, c);
        if (s.speed > 2.0) putpixel((int)s.x, (int)s.y + 1, c); // 快星拖尾
    }
}

void Game::drawParticles() {
    for (const auto& p : particles) {
        double ratio = (double)p.life / p.maxLife;      // 剩余寿命比例
        setfillcolor(fadeColor(p.color, ratio));
        int r = (int)(p.size * ratio) + 1;
        solidcircle((int)p.x, (int)p.y, r);
    }
}

/*==========================================================================*/
/*                              玩家绘制                                     */
/*==========================================================================*/

void Game::drawPlayer() {
    // 无敌期间隔帧闪烁（不绘制），形成受击闪烁效果
    if (invincible > 0 && (invincible / 4) % 2 == 0) return;

    int cx = (int)px, cy = (int)py;
    int w = PLAYER_W, h = PLAYER_H;

    // 机身主体（多边形拼出一架朝上的战机）
    POINT body[] = {
        { cx,              cy - h / 2 },                  // 机头
        { cx + w * 18 / 100,   cy - h * 10 / 100 },
        { cx + w / 2,        cy + h * 25 / 100 },             // 右翼尖
        { cx + w * 18 / 100,   cy + h * 15 / 100 },
        { cx + w * 28 / 100,   cy + h / 2 },                  // 右尾
        { cx - w * 28 / 100,   cy + h / 2 },                  // 左尾
        { cx - w * 18 / 100,   cy + h * 15 / 100 },
        { cx - w / 2,        cy + h * 25 / 100 },             // 左翼尖
        { cx - w * 18 / 100,   cy - h * 10 / 100 },
    };
    setfillcolor(COL_PLAYER);
    solidpolygon(body, 9);

    // 机身暗部（中轴）
    setfillcolor(COL_PLAYER_2);
    solidrectangle(cx - 4, cy - h / 2 + 6, cx + 4, cy + h / 2 - 4);

    // 座舱
    setfillcolor(RGB(230, 250, 255));
    solidcircle(cx, cy - 6, 5);

    // 尾焰（随机跳动）
    int flame = randRange(6, 14);
    setfillcolor(RGB(255, 160, 60));
    POINT fire[] = {
        { cx - 6, cy + h / 2 - 4 },
        { cx + 6, cy + h / 2 - 4 },
        { cx,     cy + h / 2 - 4 + flame },
    };
    solidpolygon(fire, 3);

    // 护盾绘制：环绕飞机的青色圆环
    if (shield) {
        setlinecolor(RGB(120, 210, 255));
        setlinestyle(PS_SOLID, 2);
        circle(cx, cy, PLAYER_W / 2 + 8);
        setlinestyle(PS_SOLID, 1);
    }
}

/**
 * @brief  绘制激光大招光束：以玩家为中心，向上贯穿至屏幕顶部的多层辉光光束，
 *         宽度随时间脉动，前端带一圈汇聚光环。
 */
void Game::drawUltimate() {
    int cx = (int)px, top = 0, bot = (int)(py - PLAYER_H / 2 + 4);
    double pulse = 0.6 + 0.4 * sin(animTick * 0.6);
    int hw = (int)(ULT_HALF * pulse);
    // 外层柔光
    setfillcolor(fadeColor(RGB(120, 230, 255), 0.35));
    solidrectangle(cx - hw - 6, top, cx + hw + 6, bot);
    // 主光束
    setfillcolor(RGB(120, 230, 255));
    solidrectangle(cx - hw, top, cx + hw, bot);
    // 高亮核心
    setfillcolor(RGB(240, 255, 255));
    solidrectangle(cx - hw / 3, top, cx + hw / 3, bot);
    // 汇聚光环
    setfillcolor(RGB(200, 250, 255));
    solidcircle(cx, bot, hw + 4);
    setfillcolor(RGB(255, 255, 255));
    solidcircle(cx, bot, hw / 2 + 2);
}

/*==========================================================================*/
/*                              敌机绘制                                     */
/*==========================================================================*/

void Game::drawEnemyShape(const Enemy& e) {
    // 说明：敌机机头朝下（指向下方的玩家）。造型采用“尾焰 + 机翼 + 机身 +
    //       高光 + 座舱”多层叠加，与玩家飞机同等精细度。坐标以中心为原点、
    //       按机体宽高的比例表达，便于统一缩放。
    const int cx = (int)e.x, cy = (int)e.y, w = e.w, h = e.h;
    const bool flash = e.hitFlash > 0;      // 受击瞬间白闪

    // 依类型选取配色（主色 / 尾焰色 / 座舱色）
    COLORREF base, glow, cockpit;
    switch (e.type) {
    case E_NORMAL:  base = COL_E_NORMAL;  glow = RGB(255, 130, 90);  cockpit = RGB(255, 235, 190); break;
    case E_FAST:    base = COL_E_FAST;    glow = RGB(255, 190, 70);  cockpit = RGB(255, 255, 220); break;
    case E_SHOOTER: base = COL_E_SHOOTER; glow = RGB(205, 130, 255); cockpit = RGB(235, 215, 255); break;
    case E_TANK:    base = COL_E_TANK;    glow = RGB(200, 200, 150); cockpit = RGB(240, 245, 210); break;
    case E_DIVER:   base = COL_E_DIVER;   glow = RGB(255, 220, 120); cockpit = RGB(255, 240, 200); break;
    case E_STRAFER: base = COL_E_STRAFER; glow = RGB(150, 255, 250); cockpit = RGB(225, 255, 255); break;
    default:        base = COL_E_ZIGZAG;  glow = RGB(130, 255, 190); cockpit = RGB(225, 255, 240); break;
    }
    if (flash) { base = RGB(255, 255, 255); cockpit = RGB(255, 255, 255); }
    const COLORREF hi = lighten(base, 50);  // 高光
    const COLORREF dk = darken(base, 60);   // 暗部 / 机翼

    // 以比例坐标绘制多边形的辅助 lambda
    auto poly = [&](std::initializer_list<std::pair<double, double>> pts, COLORREF c) {
        std::vector<POINT> v; v.reserve(pts.size());
        for (const auto& p : pts)
            v.push_back(POINT{ (long)(cx + p.first * w), (long)(cy + p.second * h) });
        setfillcolor(c);
        solidpolygon(v.data(), (int)v.size());
        };
    // 以比例坐标绘制椭圆的辅助 lambda（fx,fy 为中心比例，rw,rh 为半径比例）
    auto ell = [&](double fx, double fy, double rw, double rh, COLORREF c) {
        setfillcolor(c);
        solidellipse((int)(cx + (fx - rw) * w), (int)(cy + (fy - rh) * h),
            (int)(cx + (fx + rw) * w), (int)(cy + (fy + rh) * h));
        };
    // 尾焰（机体上方，随机跳动）
    auto flame = [&](double fx) {
        int fl = randRange(0, 3);
        setfillcolor(glow);
        solidcircle((int)(cx + fx * w), cy - h / 2 + 1, 3 + fl);
        };

    switch (e.type) {
    case E_NORMAL: {
        // 普通敌机：经典战斗机剪影
        flame(-0.16); flame(0.16);
        poly({ {-0.50,-0.05},{-0.14,-0.12},{0.14,-0.12},{0.50,-0.05},
              {0.32,0.26},{0,0.10},{-0.32,0.26} }, dk);                 // 机翼
        poly({ {-0.16,-0.42},{0.16,-0.42},{0.21,0.06},
              {0.09,0.48},{-0.09,0.48},{-0.21,0.06} }, base);          // 机身
        poly({ {-0.06,-0.34},{0.06,-0.34},{0.04,0.34},{-0.04,0.34} }, hi); // 中轴高光
        poly({ {-0.16,-0.42},{-0.30,-0.30},{-0.14,-0.18} }, dk);        // 左尾翼
        poly({ {0.16,-0.42},{0.30,-0.30},{0.14,-0.18} }, dk);           // 右尾翼
        ell(0, -0.12, 0.11, 0.13, cockpit);                          // 座舱
        break;
    }
    case E_FAST: {
        // 快速敌机：纤细的截击机，机翼后掠、造型凌厉
        flame(-0.10); flame(0.10);
        poly({ {-0.50,-0.20},{-0.10,-0.02},{-0.10,0.12},{-0.26,0.02} }, dk); // 左后掠翼
        poly({ {0.50,-0.20},{0.10,-0.02},{0.10,0.12},{0.26,0.02} }, dk);     // 右后掠翼
        poly({ {-0.11,-0.50},{0.11,-0.50},{0.17,0.10},
              {0,0.50},{-0.17,0.10} }, base);                          // 机身
        poly({ {-0.04,-0.42},{0.04,-0.42},{0,0.40} }, hi);              // 高光
        poly({ {-0.20,-0.30},{-0.06,-0.24},{-0.10,-0.10} }, hi);       // 左鸭翼
        poly({ {0.20,-0.30},{0.06,-0.24},{0.10,-0.10} }, hi);          // 右鸭翼
        ell(0, -0.14, 0.08, 0.12, cockpit);                          // 座舱
        break;
    }
    case E_SHOOTER: {
        // 射击敌机：厚重炮艇，双主炮下指、两侧武器荚舱
        flame(-0.18); flame(0); flame(0.18);
        poly({ {-0.52,-0.10},{-0.16,-0.16},{0.16,-0.16},{0.52,-0.10},
              {0.40,0.22},{0,0.06},{-0.40,0.22} }, dk);                // 宽机翼
        poly({ {-0.24,-0.40},{0.24,-0.40},{0.30,-0.10},{0.24,0.30},
              {0.10,0.46},{-0.10,0.46},{-0.24,0.30},{-0.30,-0.10} }, base); // 装甲机身
        ell(-0.30, 0.02, 0.10, 0.22, dk);                            // 左荚舱
        ell(0.30, 0.02, 0.10, 0.22, dk);                             // 右荚舱
        setfillcolor(dk);                                            // 双主炮（下指）
        solidrectangle((int)(cx - 0.18 * w) - 2, cy + (int)(0.30 * h), (int)(cx - 0.18 * w) + 2, cy + (int)(0.62 * h));
        solidrectangle((int)(cx + 0.18 * w) - 2, cy + (int)(0.30 * h), (int)(cx + 0.18 * w) + 2, cy + (int)(0.62 * h));
        setfillcolor(RGB(255, 230, 120));                            // 炮口辉光
        solidcircle((int)(cx - 0.18 * w), cy + (int)(0.60 * h), 3);
        solidcircle((int)(cx + 0.18 * w), cy + (int)(0.60 * h), 3);
        poly({ {-0.10,-0.32},{0.10,-0.32},{0.06,0.16},{-0.06,0.16} }, hi); // 中轴高光
        ell(0, -0.10, 0.15, 0.16, cockpit);                          // 座舱
        break;
    }
    case E_TANK: {
        // 装甲重机：厚重方正的机体、履带侧板、双联主炮，一看就“肉”
        setfillcolor(dk);                                            // 两侧履带板
        solidrectangle((int)(cx - 0.52 * w), (int)(cy - 0.34 * h), (int)(cx - 0.34 * w), (int)(cy + 0.40 * h));
        solidrectangle((int)(cx + 0.34 * w), (int)(cy - 0.34 * h), (int)(cx + 0.52 * w), (int)(cy + 0.40 * h));
        setfillcolor(darken(dk, 30));                                // 履带纹路
        for (int i = -3; i <= 3; ++i) {
            int yy = cy + (int)(i * 0.11 * h);
            solidrectangle((int)(cx - 0.52 * w), yy - 1, (int)(cx - 0.34 * w), yy + 1);
            solidrectangle((int)(cx + 0.34 * w), yy - 1, (int)(cx + 0.52 * w), yy + 1);
        }
        poly({ {-0.36,-0.40},{0.36,-0.40},{0.36,0.34},{0.18,0.48},
              {-0.18,0.48},{-0.36,0.34} }, base);                     // 装甲主体
        poly({ {-0.26,-0.30},{0.26,-0.30},{0.20,0.08},{-0.20,0.08} }, hi); // 装甲高光板
        setfillcolor(darken(base, 40));                              // 双联主炮
        solidrectangle((int)(cx - 0.16 * w) - 3, cy + (int)(0.30 * h), (int)(cx - 0.16 * w) + 3, cy + (int)(0.58 * h));
        solidrectangle((int)(cx + 0.16 * w) - 3, cy + (int)(0.30 * h), (int)(cx + 0.16 * w) + 3, cy + (int)(0.58 * h));
        ell(0, -0.06, 0.16, 0.16, cockpit);                          // 指挥舱
        break;
    }
    case E_DIVER: {
        // 俯冲自爆机：尖锐箭形、下端聚能，攻击性强；俯冲时机身泛红警示
        bool diving = (e.aiState == 1);
        setfillcolor(diving ? RGB(255, 80, 40) : glow);              // 尾部聚能焰
        solidcircle(cx, cy - h / 2 + 2, diving ? 5 + randRange(0, 3) : 3 + randRange(0, 2));
        poly({ {-0.46,-0.28},{-0.10,0.02},{-0.14,0.20} }, dk);         // 左翼
        poly({ {0.46,-0.28},{0.10,0.02},{0.14,0.20} }, dk);            // 右翼
        poly({ {-0.14,-0.46},{0.14,-0.46},{0.22,0.02},
              {0,0.52},{-0.22,0.02} }, base);                         // 箭形机身
        poly({ {-0.05,-0.36},{0.05,-0.36},{0,0.46} }, hi);             // 高光
        ell(0, -0.10, 0.10, 0.12, cockpit);                          // 座舱
        setfillcolor(RGB(255, 235, 150));                            // 头部聚能尖
        solidcircle(cx, cy + (int)(0.44 * h), 3);
        break;
    }
    case E_STRAFER: {
        // 横掠射击机：扁平宽体、两侧引擎荚舱、腹部下指机炮
        flame(-0.34); flame(0.34);
        poly({ {-0.50,-0.22},{0.50,-0.22},{0.42,0.10},{-0.42,0.10} }, dk); // 宽机翼
        poly({ {-0.30,-0.34},{0.30,-0.34},{0.36,0.06},{0.14,0.42},
              {-0.14,0.42},{-0.36,0.06} }, base);                     // 扁平机身
        ell(-0.42, -0.06, 0.09, 0.16, darken(base, 40));             // 左引擎荚
        ell(0.42, -0.06, 0.09, 0.16, darken(base, 40));              // 右引擎荚
        poly({ {-0.16,-0.26},{0.16,-0.26},{0.10,0.10},{-0.10,0.10} }, hi); // 高光
        setfillcolor(darken(base, 50));                              // 腹部机炮
        solidrectangle(cx - 3, cy + (int)(0.30 * h), cx + 3, cy + (int)(0.50 * h));
        setfillcolor(RGB(180, 255, 255));                            // 炮口辉光
        solidcircle(cx, cy + (int)(0.48 * h), 3);
        ell(0, -0.08, 0.13, 0.14, cockpit);                          // 座舱
        break;
    }
    default: { // E_ZIGZAG
        // 蛇形敌机：碟形/有机造型，暗示其左右摆动的飞行方式
        flame(-0.20); flame(0.20);
        poly({ {-0.55,-0.10},{-0.20,-0.02},{-0.20,0.14},{-0.42,0.10} }, dk); // 左侧翼
        poly({ {0.55,-0.10},{0.20,-0.02},{0.20,0.14},{0.42,0.10} }, dk);     // 右侧翼
        ell(0, 0.12, 0.44, 0.30, base);                              // 碟形下体
        ell(0, -0.04, 0.24, 0.32, hi);                               // 上部穹顶
        // 碟体上的三盏航行灯
        setfillcolor(cockpit);
        solidcircle((int)(cx - 0.26 * w), cy + (int)(0.18 * h), 2);
        solidcircle((int)(cx), cy + (int)(0.22 * h), 2);
        solidcircle((int)(cx + 0.26 * w), cy + (int)(0.18 * h), 2);
        ell(0, -0.02, 0.13, 0.15, glow);                             // 中央能量核
        break;
    }
    }

    // 血量 > 1 的敌机在头顶绘制小血条
    if (e.maxHp > 1) {
        int bw = w;
        setfillcolor(RGB(40, 40, 55));
        solidroundrect(cx - bw / 2, cy - h / 2 - 9, cx + bw / 2, cy - h / 2 - 4, 3, 3);
        int cur = bw * e.hp / e.maxHp;
        setfillcolor(RGB(90, 230, 120));
        solidroundrect(cx - bw / 2, cy - h / 2 - 9, cx - bw / 2 + cur, cy - h / 2 - 4, 3, 3);
    }
}

void Game::drawEnemies() {
    for (const auto& e : enemies) if (e.alive) drawEnemyShape(e);
}

/*==========================================================================*/
/*                              Boss 绘制                                    */
/*==========================================================================*/

void Game::drawBoss() {
    if (!boss.active) return;
    const int cx = (int)boss.x, cy = (int)boss.y, w = boss.w, h = boss.h;
    const bool flash = boss.hitFlash > 0;

    COLORREF base = flash ? RGB(255, 255, 255) : COL_BOSS;
    const COLORREF hi = lighten(base, 45);
    const COLORREF dk = darken(base, 70);
    const COLORREF steel = RGB(70, 70, 95);            // 金属结构色

    // 与敌机相同的比例坐标辅助 lambda
    auto poly = [&](std::initializer_list<std::pair<double, double>> pts, COLORREF c) {
        std::vector<POINT> v; v.reserve(pts.size());
        for (const auto& p : pts)
            v.push_back(POINT{ (long)(cx + p.first * w), (long)(cy + p.second * h) });
        setfillcolor(c);
        solidpolygon(v.data(), (int)v.size());
        };
    auto ell = [&](double fx, double fy, double rw, double rh, COLORREF c) {
        setfillcolor(c);
        solidellipse((int)(cx + (fx - rw) * w), (int)(cy + (fy - rh) * h),
            (int)(cx + (fx + rw) * w), (int)(cy + (fy + rh) * h));
        };

    // 呼吸脉冲：核心与灯光随时间明暗起伏
    double pulse = 0.5 + 0.5 * sin(animTick * 0.15);

    // 0) 激光：先画预警线，再画粗光束（位于机体之下方，先绘制以便机体覆盖发射口）
    if (boss.castType == BA_LASER) {
        int lx = boss.laserX;
        if (boss.laserWarn > 0) {
            // 预警：闪烁的红色细线，提示玩家躲避
            if ((boss.laserWarn / 3) % 2 == 0) {
                setfillcolor(RGB(255, 70, 70));
                solidrectangle(lx - 2, (int)boss.y, lx + 2, WIN_H);
            }
        }
        else if (boss.laserFire > 0) {
            // 开火：多层辉光光束
            setfillcolor(fadeColor(RGB(255, 90, 90), 0.4));
            solidrectangle(lx - BOSS_BEAM_HALF - 6, (int)boss.y, lx + BOSS_BEAM_HALF + 6, WIN_H);
            setfillcolor(RGB(255, 90, 110));
            solidrectangle(lx - BOSS_BEAM_HALF, (int)boss.y, lx + BOSS_BEAM_HALF, WIN_H);
            setfillcolor(RGB(255, 230, 240));
            solidrectangle(lx - BOSS_BEAM_HALF / 3, (int)boss.y, lx + BOSS_BEAM_HALF / 3, WIN_H);
        }
    }

    // 1) 顶部推进器辉光（4 组，随机跳动）
    setfillcolor(RGB(255, 170, 90));
    for (double fx : { -0.30, -0.10, 0.10, 0.30 })
        solidcircle((int)(cx + fx * w), cy - h / 2 + 2, 4 + randRange(0, 3));

    // 2) 后掠尾翼（机体后方）
    poly({ {-0.56,-0.34},{-0.30,-0.20},{-0.30,0.16},{-0.56,0.32} }, dk);
    poly({ {0.56,-0.34},{0.30,-0.20},{0.30,0.16},{0.56,0.32} }, dk);

    // 3) 主装甲机体（大八边形）
    poly({ {-0.40,-0.42},{0.40,-0.42},{0.50,-0.05},{0.40,0.30},
          {0.18,0.48},{-0.18,0.48},{-0.40,0.30},{-0.50,-0.05} }, base);

    // 4) 装甲高光板 + 分色装甲线
    poly({ {-0.30,-0.34},{0.30,-0.34},{0.22,0.06},{-0.22,0.06} }, hi);
    poly({ {-0.14,0.10},{0.14,0.10},{0.10,0.40},{-0.10,0.40} }, dk);

    // 5) 两侧武器荚舱 + 下指主炮 + 炮口辉光
    ell(-0.40, 0.05, 0.12, 0.34, steel);
    ell(0.40, 0.05, 0.12, 0.34, steel);
    setfillcolor(dk);
    solidrectangle((int)(cx - 0.40 * w) - 3, cy + (int)(0.20 * h), (int)(cx - 0.40 * w) + 3, cy + (int)(0.60 * h));
    solidrectangle((int)(cx + 0.40 * w) - 3, cy + (int)(0.20 * h), (int)(cx + 0.40 * w) + 3, cy + (int)(0.60 * h));
    setfillcolor(RGB(255, 220, 120));
    solidcircle((int)(cx - 0.40 * w), cy + (int)(0.58 * h), 4);
    solidcircle((int)(cx + 0.40 * w), cy + (int)(0.58 * h), 4);

    // 6) 前部尖刺（机头下方）
    poly({ {-0.12,0.40},{0.12,0.40},{0,0.62} }, dk);

    // 7) 舰桥座舱
    ell(0, -0.12, 0.18, 0.18, RGB(180, 220, 255));
    ell(0, -0.12, 0.11, 0.12, RGB(240, 250, 255));

    // 8) 中央能量核（脉冲式多层圆环）
    int coreR = 12 + (int)(pulse * 8);
    setfillcolor(darken(RGB(255, 90, 60), 20));
    solidcircle(cx, cy + (int)(0.14 * h), coreR + 4);
    setfillcolor(RGB(255, 150, 60));
    solidcircle(cx, cy + (int)(0.14 * h), coreR);
    setfillcolor(lighten(RGB(255, 230, 140), (int)(pulse * 30)));
    solidcircle(cx, cy + (int)(0.14 * h), coreR - 5);

    // 9) 翼尖闪烁航行灯（与脉冲反相）
    setfillcolor(pulse > 0.5 ? RGB(255, 255, 180) : RGB(120, 90, 60));
    solidcircle((int)(cx - 0.50 * w), cy, 3);
    solidcircle((int)(cx + 0.50 * w), cy, 3);

    // ---------------- Boss 血条（屏幕顶部横贯，带边框与高光） ----------------
    int barX = 36, barY = 16, barW = WIN_W - 72, barH = 14;
    setfillcolor(RGB(20, 20, 30));                     // 底槽
    solidroundrect(barX - 2, barY - 2, barX + barW + 2, barY + barH + 2, 6, 6);
    int cur = barW * boss.hp / boss.maxHp;
    if (cur < 0) cur = 0;
    setfillcolor(RGB(210, 50, 70));                    // 血量填充
    solidroundrect(barX, barY, barX + cur, barY + barH, 5, 5);
    setfillcolor(RGB(255, 130, 150));                  // 顶部高光条
    if (cur > 4) solidrectangle(barX + 2, barY + 2, barX + cur - 2, barY + 4);
    setlinecolor(RGB(120, 120, 140));                  // 边框
    setlinestyle(PS_SOLID, 1);
    rectangle(barX - 2, barY - 2, barX + barW + 2, barY + barH + 2);
    settextcolor(WHITE);
    settextstyle(14, 0, _T("微软雅黑"));
    TCHAR bt[24];
    _stprintf_s(bt, _countof(bt), _T("BOSS  第%d关"), level);
    outtextxy(barX, barY - 17, bt);
}

/*==========================================================================*/
/*                              子弹与道具绘制                               */
/*==========================================================================*/

void Game::drawBullets() {
    for (const auto& b : bullets) {
        setfillcolor(b.color);
        if (b.fromPlayer) {
            // 玩家子弹：细长光弹
            solidroundrect((int)b.x - b.radius, (int)b.y - b.radius * 2,
                (int)b.x + b.radius, (int)b.y + b.radius * 2, 4, 4);
        }
        else {
            solidcircle((int)b.x, (int)b.y, b.radius); // 敌弹：圆形
        }
    }
}

void Game::drawItems() {
    // 呼吸脉冲：光晕大小随时间起伏
    double pulse = 0.5 + 0.5 * sin(animTick * 0.18);

    for (const auto& it : items) {
        const int cx = (int)it.x, cy = (int)it.y, s = it.size;
        const int r = s / 2;

        // 依类型选取胶囊主色
        COLORREF bg;
        switch (it.type) {
        case IT_FIRE:    bg = RGB(255, 140, 40);  break;
        case IT_LIFE:    bg = RGB(235, 70, 95);   break;
        case IT_SHIELD:  bg = RGB(70, 160, 255);  break;
        case IT_MAXLIFE: bg = RGB(255, 80, 150);  break;   // 永久：粉红
        case IT_POWERUP: bg = RGB(180, 110, 255); break;   // 永久：紫
        default:         bg = RGB(150, 155, 170); break;   // 炸弹
        }
        const bool perm = isPermanentItem(it.type);

        // 1) 外发光光晕（永久增益额外套一圈醒目的金色旋转光环）
        if (perm) {
            setfillcolor(fadeColor(RGB(255, 215, 90), 0.5));
            solidcircle(cx, cy, r + 8 + (int)(pulse * 4));
            setlinecolor(RGB(255, 225, 120));
            setlinestyle(PS_SOLID, 2);
            circle(cx, cy, r + 6);
            setlinestyle(PS_SOLID, 1);
        }
        setfillcolor(fadeColor(bg, 0.35));
        solidcircle(cx, cy, r + 4 + (int)(pulse * 3));

        // 2) 胶囊本体：深色描边 + 主色 + 左上高光，营造立体质感
        setfillcolor(darken(bg, 70));
        solidroundrect(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, 9, 9);
        setfillcolor(bg);
        solidroundrect(cx - r, cy - r, cx + r, cy + r, 8, 8);
        setfillcolor(lighten(bg, 70));
        solidellipse(cx - r + 3, cy - r + 3, cx - 2, cy - 2);   // 左上高光

        // 3) 依类型绘制图标（纯图形，不用文字）
        switch (it.type) {
        case IT_FIRE: {                                        // 火焰
            setfillcolor(RGB(255, 210, 60));
            POINT f1[] = { {cx, cy - r + 4}, {cx + 6, cy + 2}, {cx, cy + r - 4}, {cx - 6, cy + 2} };
            solidpolygon(f1, 4);
            setfillcolor(RGB(255, 255, 200));
            POINT f2[] = { {cx, cy - 2}, {cx + 3, cy + 3}, {cx, cy + r - 6}, {cx - 3, cy + 3} };
            solidpolygon(f2, 4);
            break;
        }
        case IT_LIFE:                                          // 爱心（生命）
            drawHeart(cx, cy - 1, r - 3, RGB(255, 245, 245));
            break;
        case IT_MAXLIFE:                                       // 永久生命：爱心 + 金色小加号
            drawHeart(cx, cy - 1, r - 3, RGB(255, 250, 250));
            setfillcolor(RGB(255, 220, 90));
            solidrectangle(cx - 5, cy - 8, cx - 3, cy - 2);
            solidrectangle(cx - 7, cy - 6, cx - 1, cy - 4);
            break;
        case IT_POWERUP: {                                     // 永久火力：向上四角星
            setfillcolor(RGB(255, 250, 210));
            POINT st[] = { {cx, cy - r + 3}, {cx + 4, cy}, {cx + r - 4, cy},
                           {cx + 5, cy + 4}, {cx + 7, cy + r - 4}, {cx, cy + 6},
                           {cx - 7, cy + r - 4}, {cx - 5, cy + 4}, {cx - r + 4, cy},
                           {cx - 4, cy} };
            solidpolygon(st, 10);
            break;
        }
        case IT_SHIELD: {                                      // 盾牌
            setfillcolor(RGB(235, 245, 255));
            POINT sh[] = { {cx, cy - r + 3}, {cx + r - 4, cy - r + 6},
                           {cx + r - 5, cy + 2}, {cx, cy + r - 3},
                           {cx - r + 5, cy + 2}, {cx - r + 4, cy - r + 6} };
            solidpolygon(sh, 6);
            setfillcolor(bg);                                  // 盾面内嵌一道横纹
            solidrectangle(cx - r + 6, cy - 1, cx + r - 6, cy + 1);
            break;
        }
        default: {                                            // 炸弹
            setfillcolor(RGB(45, 45, 55));
            solidcircle(cx, cy + 2, r - 4);
            setfillcolor(RGB(110, 110, 130));                  // 球体高光
            solidcircle(cx - 3, cy - 1, 2);
            setlinecolor(RGB(180, 150, 90));                   // 引线
            setlinestyle(PS_SOLID, 2);
            line(cx + r - 7, cy - r + 6, cx + r - 3, cy - r + 2);
            setlinestyle(PS_SOLID, 1);
            setfillcolor((pulse > 0.5) ? RGB(255, 230, 120)    // 引线火花闪烁
                : RGB(255, 150, 60));
            solidcircle(cx + r - 3, cy - r + 2, 2);
            break;
        }
        }
    }
}

/*==========================================================================*/
/*                              HUD / 界面绘制                               */
/*==========================================================================*/

/**
 * @brief 绘制一颗爱心（用于生命值显示）
 */
void Game::drawHeart(int cx, int cy, int r, COLORREF c) {
    setfillcolor(c);
    solidcircle(cx - r / 2, cy - r / 3, r / 2);
    solidcircle(cx + r / 2, cy - r / 3, r / 2);
    POINT p[] = { { cx - r, cy - r / 4 }, { cx + r, cy - r / 4 }, { cx, cy + r } };
    solidpolygon(p, 3);
}

void Game::drawHUD() {
    TCHAR buf[64];
    const int yStatus = WIN_H - 76;    // 状态行：火力 / 护盾 / 闪避 / 炸弹
    const int yMain = WIN_H - 50;    // 主行：分数 / 关卡 / 生命
    const int yEnergy = WIN_H - 14;    // 能量条

    // ===== 状态行 =====
    settextstyle(18, 0, _T("微软雅黑"));
    // 火力（左）——移到底部后不再被 Boss 血条遮挡
    settextcolor(RGB(255, 200, 120));
    _stprintf_s(buf, _countof(buf), _T("火力Lv.%d"), fireLevel);
    outtextxy(12, yStatus, buf);
    // 弹药（右）
    settextcolor(RGB(210, 210, 210));
    _stprintf_s(buf, _countof(buf), _T("炸弹x%d(B)"), bombs);
    outtextxy(WIN_W - textwidth(buf) - 12, yStatus, buf);
    // 中部状态旗标：护盾 / 闪避（颜色区分是否可用）
    int sx = 130;
    if (shield) {
        settextcolor(RGB(120, 210, 255));
        outtextxy(sx, yStatus, _T("护盾"));
        sx += textwidth(_T("护盾")) + 14;
    }
    settextcolor(dashCd <= 0 ? RGB(120, 230, 160) : RGB(110, 110, 130));
    outtextxy(sx, yStatus, dashCd <= 0 ? _T("闪避") : _T("闪避冷却"));

    // ===== 主行 =====
    settextcolor(WHITE);
    settextstyle(20, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("SCORE %d"), score);
    outtextxy(12, yMain, buf);
    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(220, 230, 255));
    _stprintf_s(buf, _countof(buf), _T("第%d/%d关"), level, TOTAL_LEVEL);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, yMain, buf);
    // 生命（右）：数量少用爱心，多则用“♥ xN”避免溢出
    if (lives <= 6) {
        for (int i = 0; i < lives; ++i)
            drawHeart(WIN_W - 20 - i * 24, yMain + 10, 8, RGB(235, 70, 95));
    }
    else {
        drawHeart(WIN_W - 70, yMain + 10, 8, RGB(235, 70, 95));
        settextstyle(18, 0, _T("Consolas"));
        settextcolor(RGB(240, 120, 140));
        _stprintf_s(buf, _countof(buf), _T("x%d"), lives);
        outtextxy(WIN_W - 56, yMain, buf);
    }

    // ===== 能量条（激光大招）：标签在左、条带在右，避开分数 =====
    int ebX = 66, ebW = WIN_W - ebX - 12, ebH = 9;
    setfillcolor(RGB(28, 32, 48));
    solidroundrect(ebX, yEnergy, ebX + ebW, yEnergy + ebH, 4, 4);
    int ecur = ebW * energy / ENERGY_MAX;
    bool full = (energy >= ENERGY_MAX);
    setfillcolor(full ? ((animTick / 6) % 2 ? RGB(190, 255, 255) : RGB(90, 220, 255))
        : RGB(70, 170, 230));
    if (ecur > 0) solidroundrect(ebX, yEnergy, ebX + ecur, yEnergy + ebH, 4, 4);
    settextstyle(17, 0, _T("微软雅黑"));
    settextcolor(full ? RGB(200, 255, 255) : RGB(150, 185, 220));
    outtextxy(10, yEnergy - 4, full ? _T("大招X") : _T("能量"));
}

void Game::drawMenu() {
    TCHAR buf[64];
    settextcolor(RGB(120, 230, 255));
    settextstyle(56, 0, _T("微软雅黑"));
    const TCHAR* title = _T("飞 机 大 战");
    outtextxy(WIN_W / 2 - textwidth(title) / 2, 150, title);

    settextcolor(WHITE);
    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* tips[] = {
        _T("WASD / 方向键  移动"),
        _T("空格 射击      R 闪避冲刺"),
        _T("X 激光大招(集满能量)   B 炸弹"),
        _T("P 暂停          Esc 退出"),
        _T("金色道具=永久增益(生命上限/火力)"),
        _T("回车 / 空格  开始游戏"),
    };
    int y = 300;
    for (auto t : tips) {
        outtextxy(WIN_W / 2 - textwidth(t) / 2, y, t);
        y += 34;
    }

    // 历史最高分
    settextcolor(RGB(255, 210, 120));
    settextstyle(20, 0, _T("Consolas"));
    int best = ranks.empty() ? 0 : ranks.front().score;
    _stprintf_s(buf, _countof(buf), _T("HIGH SCORE: %d"), best);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, 520, buf);

    // 排行榜前 5
    settextcolor(RGB(200, 220, 255));
    settextstyle(15, 0, _T("微软雅黑"));
    outtextxy(WIN_W / 2 - 90, 560, _T("—— 排行榜 ——"));
    settextstyle(14, 0, _T("Consolas"));
    for (int i = 0; i < (int)ranks.size() && i < 5; ++i) {
        _stprintf_s(buf, _countof(buf), _T("%d. %-8s  %d"),
            i + 1, ranks[i].name.c_str(), ranks[i].score);
        outtextxy(WIN_W / 2 - 90, 585 + i * 20, buf);
    }
}

void Game::drawPaused() {
    settextcolor(WHITE);
    settextstyle(40, 0, _T("微软雅黑"));
    const TCHAR* t = _T("暂  停");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, WIN_H / 2 - 70, t);
    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* t2 = _T("P / 回车 继续      Esc 返回菜单");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, WIN_H / 2 - 6, t2);
    settextcolor(RGB(150, 220, 255));
    const TCHAR* t3 = _T("按 I 查看属性面板");
    outtextxy(WIN_W / 2 - textwidth(t3) / 2, WIN_H / 2 + 26, t3);
}

/**
 * @brief  属性图鉴面板（暂停时按 I 打开）。
 *         上半部分显示我方战机的实时属性与增益（BUFF），下半部分以表格列出
 *         【当前关卡】会出现的敌机（含 Boss）的生命/攻击/防御/攻击特征。
 *         敌机列表依据当前 level 动态生成，进入下一关会自动更新。
 */
void Game::drawInfoPanel() {
    TCHAR buf[80];
    // 面板背景与边框
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinecolor(RGB(70, 110, 160));
    setlinestyle(PS_SOLID, 2);
    roundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 标题
    settextcolor(RGB(120, 230, 255));
    settextstyle(30, 0, _T("微软雅黑"));
    const TCHAR* title = _T("属 性 面 板");
    outtextxy(WIN_W / 2 - textwidth(title) / 2, 26, title);

    /*---------------- 我方战机 ----------------*/
    int y = 68;
    settextcolor(RGB(150, 230, 180));
    settextstyle(22, 0, _T("微软雅黑"));
    outtextxy(28, y, _T("【我方战机】"));
    y += 32;
    settextstyle(20, 0, _T("微软雅黑"));
    settextcolor(RGB(230, 230, 240));

    _stprintf_s(buf, _countof(buf), _T("攻击力：%d  (上限 %d)"), bulletDmg, BULLET_DMG_MAX);
    outtextxy(40, y, buf); y += 28;
    _stprintf_s(buf, _countof(buf), _T("防御力：%s"),
        shield ? _T("护盾中(挡1次)  受击-1命") : _T("无  受击-1命"));
    outtextxy(40, y, buf); y += 28;
    _stprintf_s(buf, _countof(buf), _T("生命值：%d / %d  (上限 %d)"), lives, maxLives, MAX_LIVES_CAP);
    outtextxy(40, y, buf); y += 30;

    // BUFF 一览
    settextcolor(RGB(255, 220, 140));
    outtextxy(40, y, _T("BUFF："));
    int bx = 40 + textwidth(_T("BUFF：")), by = y;
    auto tag = [&](const TCHAR* s, COLORREF c) {
        settextcolor(c);
        if (bx + textwidth(s) > WIN_W - 40) { bx = 92; by += 26; }  // 自动换行
        outtextxy(bx, by, s); bx += textwidth(s) + 12;
        };
    if (fireTimer > 0) {
        _stprintf_s(buf, _countof(buf), _T("火力Lv.%d(%ds)"), fireLevel, fireTimer / FPS + 1);
        tag(buf, RGB(255, 190, 120));
    }
    else {
        tag(_T("火力Lv.1"), RGB(200, 200, 200));
    }
    if (shield) tag(_T("护盾"), RGB(120, 210, 255));
    _stprintf_s(buf, _countof(buf), _T("炸弹x%d"), bombs);           tag(buf, RGB(210, 210, 210));
    _stprintf_s(buf, _countof(buf), _T("能量%d%%"), energy * 100 / ENERGY_MAX); tag(buf, RGB(120, 210, 255));
    tag(dashCd <= 0 ? _T("闪避就绪") : _T("闪避冷却"), dashCd <= 0 ? RGB(120, 230, 160) : RGB(140, 140, 150));
    y = by + 40;

    /*---------------- 本关敌机（随关卡更新） ----------------*/
    settextcolor(RGB(255, 170, 150));
    settextstyle(22, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("【本关敌机 · 第%d关】"), level);
    outtextxy(28, y, buf); y += 34;

    // 列坐标：外形图标 / 名称 / 生命 / 攻击 / 防御 / 攻击特征
    const int cIcon = 46, cName = 72, cHp = 158, cAtk = 210, cDef = 258, cFeat = 306;
    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(150, 170, 210));
    outtextxy(cIcon - 16, y, _T("外形")); outtextxy(cName, y, _T("名称"));
    outtextxy(cHp, y, _T("生命"));        outtextxy(cAtk, y, _T("攻击"));
    outtextxy(cDef, y, _T("防御"));       outtextxy(cFeat, y, _T("特征"));
    y += 26;
    setlinecolor(RGB(60, 80, 110));
    line(28, y, WIN_W - 28, y);
    y += 8;

    // 绘制某类型敌机小图标：复用真实绘制函数，外观与实战完全一致
    auto icon = [&](EnemyType t, int cx, int cy) {
        Enemy e{};
        e.type = t; e.x = cx; e.y = cy; e.w = 34; e.h = 28;
        e.hp = 1; e.maxHp = 1;             // maxHp=1 以跳过图标上的血条
        e.armor = enemyArmor(t); e.aiState = 0; e.hitFlash = 0; e.alive = true;
        drawEnemyShape(e);
        };
    // Boss 小徽标
    auto bossIcon = [&](int cx, int cy) {
        setfillcolor(COL_BOSS);
        solidroundrect(cx - 16, cy - 9, cx + 16, cy + 9, 6, 6);
        setfillcolor(RGB(150, 60, 90));
        solidcircle(cx - 12, cy, 4); solidcircle(cx + 12, cy, 4);
        setfillcolor(RGB(255, 220, 120));
        solidcircle(cx, cy, 5);
        };

    // 依据当前关卡组装敌机名单（与 spawnEnemy 的解锁规则一致）
    std::vector<EnemyType> roster = { E_NORMAL, E_FAST };
    if (level >= 2) { roster.push_back(E_ZIGZAG); roster.push_back(E_TANK); }
    if (level >= 3) { roster.push_back(E_SHOOTER); roster.push_back(E_STRAFER); roster.push_back(E_DIVER); }

    const int rowH = 32;
    settextstyle(18, 0, _T("微软雅黑"));
    for (EnemyType t : roster) {
        int cy = y + rowH / 2 - 2;
        icon(t, cIcon, cy);
        settextcolor(RGB(225, 225, 235));
        outtextxy(cName, y + 4, enemyName(t));
        _stprintf_s(buf, _countof(buf), _T("%d"), enemyHp(t, level));   outtextxy(cHp, y + 4, buf);
        outtextxy(cAtk, y + 4, _T("1"));                                // 敌方每次命中固定扣 1 命
        _stprintf_s(buf, _countof(buf), _T("%d"), enemyArmor(t));       outtextxy(cDef, y + 4, buf);
        outtextxy(cFeat, y + 4, enemyFeature(t));
        y += rowH;
    }
    // Boss 行
    {
        int cy = y + rowH / 2 - 2;
        bossIcon(cIcon, cy);
        settextcolor(RGB(255, 150, 175));
        outtextxy(cName, y + 4, _T("BOSS"));
        _stprintf_s(buf, _countof(buf), _T("%d"), bossHp(level)); outtextxy(cHp, y + 4, buf);
        outtextxy(cAtk, y + 4, _T("1"));  outtextxy(cDef, y + 4, _T("0"));
        outtextxy(cFeat, y + 4, _T("弹幕/激光"));
        y += rowH + 6;
    }

    // 提示
    settextcolor(RGB(150, 170, 200));
    settextstyle(16, 0, _T("微软雅黑"));
    outtextxy(28, y, _T("说明：攻击力越高越能击穿护甲(至少造成1点)"));
    y += 24;
    settextcolor(RGB(150, 220, 255));
    outtextxy(28, y, _T("按 I 返回暂停界面    P 继续游戏"));
}

void Game::drawGameOver() {
    TCHAR buf[64];
    settextcolor(RGB(255, 90, 90));
    settextstyle(48, 0, _T("微软雅黑"));
    const TCHAR* t = _T("游戏结束");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 220, t);

    settextcolor(WHITE);
    settextstyle(24, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("SCORE: %d"), score);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, 300, buf);

    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* t2 = _T("回车 返回菜单        Esc 退出");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, 380, t2);
}

void Game::drawVictory() {
    TCHAR buf[64];
    settextcolor(RGB(255, 220, 120));
    settextstyle(48, 0, _T("微软雅黑"));
    const TCHAR* t = _T("恭喜通关！");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 220, t);

    settextcolor(WHITE);
    settextstyle(24, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("SCORE: %d"), score);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, 300, buf);

    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* t2 = _T("回车 返回菜单        Esc 退出");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, 380, t2);
}

/*==========================================================================*/
/*                              排行榜存档                                   */
/*==========================================================================*/

void Game::loadLeaderboard() {
    ranks.clear();
    // 使用与字符集匹配的文件流类型，兼容 Unicode / 多字节
    std::basic_ifstream<TCHAR> fin(RANK_FILE);
    if (!fin) return;
    std::basic_string<TCHAR> line;
    // 每行格式：分数<空格>昵称（昵称允许含空格，取首个空格后的剩余部分）
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::basic_istringstream<TCHAR> ss(line);
        int sc = 0;
        ss >> sc;
        std::basic_string<TCHAR> name;
        std::getline(ss, name);
        // 去掉前导空格
        size_t p = name.find_first_not_of(_T(' '));
        if (p != std::basic_string<TCHAR>::npos) name = name.substr(p);
        if (name.empty()) name = _T("Player");
        ranks.push_back({ name, sc });
    }
    std::sort(ranks.begin(), ranks.end(),
        [](const RankItem& a, const RankItem& b) { return a.score > b.score; });
    if (ranks.size() > 10) ranks.resize(10);
}

void Game::saveLeaderboard() {
    std::basic_ofstream<TCHAR> fout(RANK_FILE);
    if (!fout) return;
    for (const auto& r : ranks)
        fout << r.score << _T(' ') << r.name << std::endl;
}

void Game::tryRecordScore() {
    // 判断当前分数是否能进入前 10
    bool qualifies = (int)ranks.size() < 10 ||
        (!ranks.empty() && score > ranks.back().score);
    if (score <= 0 || !qualifies) return;

    // 弹出输入框获取玩家昵称（EasyX 提供 InputBox）
    TCHAR name[32] = _T("");
    InputBox(name, 32, _T("你的成绩进入排行榜！请输入昵称："),
        _T("排行榜记录"), _T("Player"), 0, 0, false);
    if (_tcslen(name) == 0) _tcscpy_s(name, _countof(name), _T("Player"));

    ranks.push_back({ std::basic_string<TCHAR>(name), score });
    std::sort(ranks.begin(), ranks.end(),
        [](const RankItem& a, const RankItem& b) { return a.score > b.score; });
    if (ranks.size() > 10) ranks.resize(10);
    saveLeaderboard();
}

/*==========================================================================*/
/*                                  main                                     */
/*==========================================================================*/

/**
 * @brief 程序入口：创建 Game 对象并运行。
 */
int main() {
    Game game;
    game.run();
    return 0;
}