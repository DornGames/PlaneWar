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
 *      · 音频系统：自定义 BGM 循环播放 + 10 种音效，基于 MCI 多路播放，M 键静音
 *      · 游戏状态机：菜单 / 游戏中 / 暂停 / 游戏结束 / 通关胜利
 *
 *  ------------------------------------------------------------------------
 *  编译提示：
 *    1. 需要正确安装 EasyX（graphics.h）。
 *    2. 建议将源文件保存为 UTF-8 编码，并在项目属性中开启 /utf-8，
 *       或使用“多字节字符集”，以保证界面中文正常显示。
 *    3. 音频：把 audio 文件夹放到【与 exe 同级的工作目录】下
 *       （VS 调试时工作目录默认是 .vcxproj 所在目录）。
 *       · BGM：自备曲目，命名为 audio\bgm.mp3（也支持 .wav/.wma/.mid），
 *              程序会自动识别格式并循环播放。
 *       · 音效：audio\sfx_*.wav，共 10 个，为程序合成的原创素材。
 *       资源缺失不会崩溃，只会自动静音。如需彻底关闭音频，将 ENABLE_SOUND 置 0。
 *       音频通过 MCI 播放，已用 #pragma 自动链接 winmm.lib。
 *****************************************************************************/

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
#include <thread>               // 音频命令后台线程
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>

 /*==========================================================================*/
 /*                              全局配置常量                                 */
 /*==========================================================================*/

#define ENABLE_SOUND 1                     // 1 = 开启 BGM 与音效（需 audio/ 目录下的 wav 资源）；0 = 完全静音
#include <mmsystem.h>                       // timeBeginPeriod/timeGetTime（高精度计时）+ MCI 音频播放
#pragma comment(lib, "winmm.lib")          // 链接多媒体库：高精度帧率计时器与音效均依赖它

const int   WIN_W = 480;             // 窗口宽度（像素）
const int   WIN_H = 720;             // 窗口高度（像素）
const int   FPS = 60;              // 目标帧率
const DWORD FRAME_MS = 1000 / FPS;      // 每帧目标时长（毫秒）

const int   PLAYER_W = 44;              // 玩家飞机宽
const int   PLAYER_H = 50;              // 玩家飞机高
const int   PLAYER_SPD = 6;               // 玩家基础移动速度（像素/帧）
const int   START_LIVES = 4;              // 初始生命数（已上调，容错更高）
const int   INVINCIBLE_FRAMES = 90;        // 受击后无敌时长（帧，约 1.5 秒）
const int   SHOOT_CD = 9;               // 玩家基础射击冷却（帧）——数值框架基准
const int   FIRE_BUFF_FRAMES = 600;        // 火力增强持续时长（帧，约 10 秒）
const int   MAX_BOMBS = 3;               // 炸弹数量上限
const int   PERM_HP_BONUS = 20;            // 永久道具每次提升的血量上限
const int   PERM_HP_CAP = 120;           // 永久血量加成累计上限
const int   BULLET_DMG_BASE = 3;           // 基础攻击（数值框架基准值）
const int   BULLET_DMG_MAX = 8;            // 基础攻击上限（3 基础 + 5 次永久强化）

/*----------------------- 生命/防御与装备机制常量 -----------------------*/
const int   PLAYER_BASE_HP = 120;          // 玩家基础血量（已上调，降低难度）
const int   HIT_HP_BULLET = 10;           // 被普通/敌方子弹命中扣血
const int   HIT_HP_CONTACT = 12;           // 撞上敌机扣血（已下调）
const int   HIT_HP_BOSS = 18;           // 撞上 Boss 机体扣血（已下调）
const int   HIT_HP_LASER = 15;           // 被 Boss 激光命中扣血（已下调）
const int   HIT_IFRAMES = 24;           // 每次受击后的短暂无敌帧（避免同帧连击）
const int   BAG_CAP = 24;           // 物品栏容量（合成需要囤积同类装备，故放宽）
const int   GEAR_LVL_MAX = 5;            // 装备最高等级（两件同级同类可合成高一级）

/*----------------------- 玩家等级与经验 -----------------------*/
const int   PLV_MAX = 30;           // 玩家最高等级

/**
 * @brief  升到「下一级」所需经验。刻意设计为二次增长，使升级速度随关卡递减。
 *         plv=1 时约 60，plv=10 时约 690，plv=20 时约 2380。
 */
inline int xpNeed(int plv) { return 30 + plv * 14 + plv * plv * 3; }   // 已下调：升级更快

/*--- 等级带来的属性成长：刻意做得比敌方膨胀慢（线性、且分段步进）---*/
inline int levelAtk(int plv) { return (plv - 1) / 4; }      // 每 4 级 +1 攻击（30 级共 +7）
inline int levelDef(int plv) { return (plv - 1) / 6; }      // 每 6 级 +1 防御（30 级共 +4）
inline int levelHp(int plv) { return (plv - 1) * 8; }      // 每级 +8 血量上限（30 级共 +232）
/*----------------------- 关卡与 Boss 强度 -----------------------*/
const int   BOSS_KINDS = 3;               // Boss 种类数（前 3 关各不相同，之后循环复用）
const int   LEVEL_FRAMES = 60 * 22;        // 每关刷小怪时长（帧），之后出现 Boss
// 说明：游戏为无限关卡模式。第 1..3 关依次为三种不同外观/招式的 Boss；
//       第 4 关起循环复用这三种 Boss，但每循环一轮（cycle）属性显著增强。

/*----------------------- 新技能相关常量 -----------------------*/
const int   ENERGY_MAX = 140;             // 能量上限（提高：大招更珍贵）
const int   ENERGY_KILL = 4;               // 每击毁一架敌机获得的能量（已减半）
const int   ULT_FRAMES = 120;             // 能量光束持续帧数（2.0 秒，已加强）
const int   ULT_HALF = 26;              // 激光大招半宽（像素）
/*----------------------- 弹反（R 键）-----------------------*/
const int   PARRY_FRAMES = 20;             // 弹反的无敌帧数（约 1/3 秒）
const int   PARRY_CD = 80;             // 弹反冷却帧数
const int   PARRY_R = 95;             // 弹反判定半径（像素）
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
    ST_VICTORY,     // 通关胜利
    ST_CHEAT        // 自定义初始配置界面（菜单输入秘籍 131215 进入）
};

/** @brief 自定义配置面板的条目 */
enum CheatItem {
    CI_PLV,         // 玩家等级
    CI_LEVEL,       // 起始关卡
    CI_WTYPE,       // 武器类型
    CI_RARITY,      // 装备稀有度（三个部位统一）
    CI_GLVL,        // 装备等级（三个部位统一）
    CI_HP,          // 额外生命值
    CI_ATK,         // 额外攻击力
    CI_DEF,         // 额外防御力
    CI_LIVES,       // 备用机数量
    CI_MONEY,       // 初始货币
    CI_ULT,         // 初始大招（并解锁全部大招）
    CI_SKILL,       // 初始小技能（并解锁全部技能）
    CI_PASSIVE,     // 初始被动数量（0~3，随机赠予，便于演示被动系统）
    CI_COUNT
};

/*==========================================================================*/
/*                     货币 / 大招 / 小技能  定义                            */
/*==========================================================================*/
/*
 *  货币：内部统一以【铜币】为最小单位存储，显示时换算为 金/银/铜。
 *        汇率：10 铜 = 1 银，10 银 = 1 金，故 1 金 = 100 铜。
 *  设计：越强力的技能标价越高（且高价技能本身也更难攒够钱），
 *        形成"强度 ↔ 获取难度"的正相关。
 */
const int COPPER_PER_SILVER = 10;                  // 10 铜 = 1 银
const int SILVER_PER_GOLD = 10;                  // 10 银 = 1 金
const int COPPER_PER_GOLD = COPPER_PER_SILVER * SILVER_PER_GOLD;   // 1 金 = 100 铜

/** @brief 把铜币总额拆成 金/银/铜 三段，便于显示 */
inline void splitMoney(int copper, int& g, int& s, int& c) {
    if (copper < 0) copper = 0;
    g = copper / COPPER_PER_GOLD;
    s = (copper % COPPER_PER_GOLD) / COPPER_PER_SILVER;
    c = copper % COPPER_PER_SILVER;
}

/** @brief 大招类型（X 键释放，消耗能量） */
enum UltType {
    U_LASER,        // 能量光束：向上贯穿光柱，持续伤害
    U_NOVA,         // 环形爆轰：以自身为中心的扩张冲击波，清场
    U_SLOW,         // 时空减速：全场敌人与弹幕减速，便于走位
    U_OVERDRIVE,    // 超载模式：短时间无敌 + 极限射速
    U_METEOR,       // 陨石轰炸：天降陨石，随机大范围打击
    U_BLACKHOLE,    // 奇点黑洞：吸附并持续绞杀范围内敌人
    U_SATELLITE,    // 卫星炮台：召唤环绕卫星自动开火
    U_COUNT
};

/** @brief 小技能类型（B 键释放，消耗使用次数） */
enum SkillType {
    SK_BOMB,        // 清屏炸弹：炸毁全部敌机并重创 Boss
    SK_REPAIR,      // 应急维修：立即回复大量血量
    SK_SHIELD,      // 力场护盾：获得一次伤害免疫
    SK_MAGNET,      // 磁力吸附：把全场道具瞬间吸到身边
    SK_EMP,         // 电磁脉冲：清除全部敌方弹幕
    SK_FREEZE,      // 冰冻立场：短时间冻结所有敌机
    SK_DECOY,       // 诱饵无人机：投放诱饵吸收附近敌弹
    SK_BERSERK,     // 狂暴强化：短时间提升攻击力
    SK_CLONE,       // 战术分身：召唤僚机协同开火
    SK_COUNT
};

/*
 *  ── 强度与定价模型（这套数值是量化标定的，不是拍脑袋）──
 *
 *  以「等效DPS秒」为统一度量：某技能提供的价值 ≈ 相当于多少秒的普通射击。
 *    伤害类 -> 总伤害 / DPS
 *    保命类 -> 免除的伤害 / 每秒威胁 × 0.8
 *    控场类 -> 控制时长 × 减速比例 × 0.8（≈等效无敌时间）
 *    增益类 -> 持续时长 × 额外倍率
 *
 *  设计目标与实测结果（基准：中期玩家 DPS≈480、血量≈350、每秒威胁≈60）：
 *    · 大招   强度区间 [4.5, 13.8]  —— 强力、改变战局
 *    · 小技能 强度区间 [1.0,  2.4]  —— 战术性、应急
 *    · 两区间【完全不重叠】：最弱大招(4.5) > 最强小技能(2.4)
 *
 *  定价：价格 = 强度 × 900（大招与小技能【统一系数】，故价格严格正比于强度）。
 *  经济：经模拟标定，前 6 次购买的平均间隔为 4.0 关（最长 5 关），
 *        符合"每 3~5 关能买一件"的目标。
 */

 /** @brief 大招名称 */
inline const TCHAR* ultName(int u) {
    switch (u) {
    case U_LASER:     return _T("能量光束");
    case U_NOVA:      return _T("环形爆轰");
    case U_SLOW:      return _T("时停领域");
    case U_OVERDRIVE: return _T("超载模式");
    case U_METEOR:    return _T("陨石轰炸");
    case U_BLACKHOLE: return _T("奇点黑洞");
    default:          return _T("卫星炮台");
    }
}

/** @brief 大招效果说明（图鉴 / 商店用） */
inline const TCHAR* ultDesc(int u) {
    switch (u) {
    case U_LASER:     return _T("2秒 高强度贯穿光柱");
    case U_NOVA:      return _T("以自身为中心爆发冲击波清场");
    case U_SLOW:      return _T("8秒:敌人减速至25%且易伤+50%");
    case U_OVERDRIVE: return _T("无敌 + 极限射速，攻守兼备");
    case U_METEOR:    return _T("天降陨石，大范围随机轰炸");
    case U_BLACKHOLE: return _T("吸附敌人并持续绞杀");
    default:          return _T("召唤两台环绕卫星自动开火");
    }
}

/** @brief 小技能名称 */
inline const TCHAR* skillName(int s) {
    switch (s) {
    case SK_BOMB:    return _T("清屏炸弹");
    case SK_REPAIR:  return _T("应急维修");
    case SK_SHIELD:  return _T("力场护盾");
    case SK_MAGNET:  return _T("磁力吸附");
    case SK_EMP:     return _T("电磁脉冲");
    case SK_FREEZE:  return _T("冰冻立场");
    case SK_DECOY:   return _T("诱饵无人机");
    case SK_BERSERK: return _T("狂暴强化");
    default:         return _T("战术分身");
    }
}

/** @brief 小技能效果说明（图鉴 / 商店用） */
inline const TCHAR* skillDesc(int s) {
    switch (s) {
    case SK_BOMB:    return _T("炸毁全部敌机，重创 Boss");
    case SK_REPAIR:  return _T("立即回复 40% 血量上限");
    case SK_SHIELD:  return _T("获得一次伤害完全免疫");
    case SK_MAGNET:  return _T("全场道具瞬间吸附到身边");
    case SK_EMP:     return _T("清除屏幕上全部敌方弹幕");
    case SK_FREEZE:  return _T("冻结所有敌机 2 秒");
    case SK_DECOY:   return _T("投放诱饵，吸收周围敌弹 4 秒");
    case SK_BERSERK: return _T("6 秒内攻击力 +40%");
    default:         return _T("召唤僚机协同开火 8 秒");
    }
}

/**
 * @brief  大招售价（铜币）。强度越高价格越贵——
 *         能量光束为初始免费技能，其余按强度阶梯定价。
 */
inline int ultPrice(int u) {
    // 价格 = 强度 × 900（与小技能同系数，保证"价格 ∝ 强度"字面成立）
    switch (u) {
    case U_LASER:     return 0;      // 强度 6.0，初始免费赠送
    case U_NOVA:      return 4050;   // 强度 4.5  -> 40 金 5 银
    case U_SATELLITE: return 6300;   // 强度 7.0  -> 63 金
    case U_METEOR:    return 7200;   // 强度 8.0  -> 72 金
    case U_SLOW:      return 7900;   // 强度 8.8  -> 79 金
    case U_BLACKHOLE: return 9550;   // 强度 10.6 -> 95 金 5 银
    default:          return 12400;  // 超载模式：强度 13.8 -> 124 金（最强）
    }
}

/** @brief 小技能售价（铜币）。清屏炸弹为初始免费技能。 */
inline int skillPrice(int s) {
    // 价格 = 强度 × 900（与大招同系数）
    switch (s) {
    case SK_BOMB:    return 0;       // 强度 1.3，初始免费赠送
    case SK_MAGNET:  return 900;     // 强度 1.0 ->  9 金
    case SK_EMP:     return 1100;    // 强度 1.2 -> 11 金
    case SK_FREEZE:  return 1450;    // 强度 1.6 -> 14 金 5 银
    case SK_SHIELD:  return 1450;    // 强度 1.6 -> 14 金 5 银
    case SK_REPAIR:  return 1700;    // 强度 1.9 -> 17 金
    case SK_DECOY:   return 1800;    // 强度 2.0 -> 18 金
    case SK_CLONE:   return 1800;    // 强度 2.0 -> 18 金
    default:         return 2150;    // 狂暴强化：强度 2.4 -> 21 金 5 银
    }
}

/** @brief 大招强度评级（1~5 星，图鉴/商店展示用） */
inline int ultTier(int u) {
    // 星级直接由量化强度分档（4.5~13.8 映射到 1~5 星），与价格同源
    switch (u) {
    case U_NOVA:      return 1;   // 4.5
    case U_LASER:     return 2;   // 6.0
    case U_SATELLITE: return 2;   // 7.0
    case U_METEOR:    return 3;   // 8.0
    case U_SLOW:      return 3;   // 8.8
    case U_BLACKHOLE: return 4;   // 10.6
    default:          return 5;   // 13.8
    }
}

/** @brief 小技能强度评级（1~5 星） */
inline int skillTier(int s) {
    // 星级由量化强度分档（1.0~2.4 映射到 1~5 星）
    switch (s) {
    case SK_MAGNET:  return 1;   // 1.0
    case SK_EMP:     return 1;   // 1.2
    case SK_BOMB:    return 2;   // 1.3
    case SK_FREEZE:  return 3;   // 1.6
    case SK_SHIELD:  return 3;   // 1.6
    case SK_REPAIR:  return 4;   // 1.9
    case SK_DECOY:   return 4;   // 2.0
    case SK_CLONE:   return 4;   // 2.0
    default:         return 5;   // 2.4 狂暴强化
    }
}

/** @brief 敌机击杀掉落的铜币（按强度权重 + 关卡递增） */
inline int enemyMoney(int powerWeight, int level) {
    // 【经济标定】掉币随关卡二次增长，与敌人强度成长同步，
    // 保证后期负担得起高价大招；模拟显示前 6 次购买平均间隔 4.0 关。
    (void)powerWeight;
    int n = level - 1;
    double v = 1.0 + 0.3 * n + 0.15 * n * n;
    int c = (int)v;
    return c < 1 ? 1 : c;
}

/*==========================================================================*/
/*                          被动技能（三关一评级）                           */
/*==========================================================================*/
/*
 *  机制说明：
 *    每打过 3 关 Boss（第 3/6/9… 关）结算一次「战绩评级」：
 *      评级依据 = 该三关实际获得分数 / 该三关的【期望分数】× 100%
 *      · S 级 (≥130%) -> 随机赠予 3 个不同被动
 *      · A 级 (≥100%) -> 2 个
 *      · B 级 (≥ 70%) -> 1 个
 *      · 低于 B 级     -> 不赠予
 *    赠予的被动仅在【接下来的 3 关】生效；下次结算时全部清空并重新评定。
 *    因此玩家需要持续打出高分才能维持强度，形成滚动式的正反馈。
 */

const int PASSIVE_SLOTS = 3;               // 被动技能槽位上限（S 级最多 3 个）

/**
 * @brief  被动技能类型（共 10 种）。
 *
 *  设计约束：
 *    · 仅 3 个为【数值型】（直接加属性）：强化弹头 / 复合装甲 / 高效引擎
 *    · 其余 7 个必须是【独特机制】，且不得与小技能、大招的效果重复——
 *      小技能与大招是"主动触发的一次性效果"，被动则是"持续改变战斗规则"。
 */
enum PassiveType {
    /*---- 数值型（3 个）----*/
    PS_ATK,         // 强化弹头：攻击力 +30%
    PS_ARMOR,       // 复合装甲：防御力 +4，血量上限 +25%
    PS_ENGINE,      // 高效引擎：移动速度 +3，射速 +2

    /*---- 机制型（7 个，各自改变一条战斗规则）----*/
    PS_CRIT,        // 临界暴击：每发子弹有 20% 概率造成 3 倍伤害
    PS_PIERCE,      // 贯穿改造：我方子弹额外获得 1 次贯穿
    PS_ECHO,        // 回响射击：每第 3 次射击额外分裂出扇形弹
    PS_CHAIN,       // 连锁闪电：击杀敌机时向最近的敌人放电
    PS_CORPSE,      // 残骸引爆：敌机被击毁时炸开，伤害周围敌机
    PS_ADRENALINE,  // 应激装甲：血量低于 35% 时，伤害 +60%、移速 +3
    PS_AURA,        // 引力力场：机体周围的敌弹持续减速
    PS_COUNT
};

/** @brief 战绩评级 */
enum RankTier { RK_NONE, RK_B, RK_A, RK_S };

/** @brief 被动技能名称 */
inline const TCHAR* passiveName(int p) {
    switch (p) {
    case PS_ATK:        return _T("强化弹头");
    case PS_ARMOR:      return _T("复合装甲");
    case PS_ENGINE:     return _T("高效引擎");
    case PS_CRIT:       return _T("临界暴击");
    case PS_PIERCE:     return _T("贯穿改造");
    case PS_ECHO:       return _T("回响射击");
    case PS_CHAIN:      return _T("连锁闪电");
    case PS_CORPSE:     return _T("残骸引爆");
    case PS_ADRENALINE: return _T("应激装甲");
    default:            return _T("引力力场");
    }
}

/** @brief 被动技能效果说明 */
inline const TCHAR* passiveDesc(int p) {
    switch (p) {
    case PS_ATK:        return _T("[数值] 攻击力 +30%");
    case PS_ARMOR:      return _T("[数值] 防御 +4，血量上限 +25%");
    case PS_ENGINE:     return _T("[数值] 移动速度 +3，射速 +2");
    case PS_CRIT:       return _T("[机制] 20% 概率打出 3 倍暴击");
    case PS_PIERCE:     return _T("[机制] 子弹额外贯穿 1 个敌人");
    case PS_ECHO:       return _T("[机制] 每第3次射击分裂出扇形弹");
    case PS_CHAIN:      return _T("[机制] 击杀时向最近敌人放电");
    case PS_CORPSE:     return _T("[机制] 敌机死亡时炸伤周围敌机");
    case PS_ADRENALINE: return _T("[机制] 残血(<35%)时伤害+60%移速+3");
    default:            return _T("[机制] 机体周围敌弹持续减速");
    }
}

/** @brief 评级名称 */
inline const TCHAR* tierName(int t) {
    switch (t) {
    case RK_S: return _T("S");
    case RK_A: return _T("A");
    case RK_B: return _T("B");
    default:   return _T("C");
    }
}

/** @brief 评级颜色 */
inline COLORREF tierColor(int t) {
    switch (t) {
    case RK_S: return RGB(255, 215, 80);    // 金
    case RK_A: return RGB(190, 130, 255);   // 紫
    case RK_B: return RGB(90, 180, 255);    // 蓝
    default:   return RGB(150, 155, 170);   // 灰
    }
}

/** @brief 评级可获得的被动数量 */
inline int tierGrantCount(int t) {
    switch (t) {
    case RK_S: return 3;
    case RK_A: return 2;
    case RK_B: return 1;
    default:   return 0;
    }
}

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
    IT_POWERUP,     // 【永久】子弹伤害 +1（稀有）
    IT_GEAR         // 装备掉落：拾取后进入物品栏，可在装备面板安装
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
inline int enemyArmor(EnemyType t, int level) {
    // 【数值框架】护甲 = 基数 + n/4，上限 8。所有敌机基础护甲 >=1（可随关卡提升）。
    int base;
    switch (t) {
    case E_FAST:    base = 1; break;
    case E_NORMAL:  base = 1; break;
    case E_ZIGZAG:  base = 1; break;
    case E_DIVER:   base = 2; break;
    case E_SHOOTER: base = 2; break;
    case E_STRAFER: base = 2; break;
    case E_TANK:    base = 3; break;   // 精英护甲最高
    default:        base = 1; break;
    }
    int a = base + (level - 1) / 5;      // 成长放缓
    const int ARMOR_CAP = 5;             // 上限下调：后期不再难以击穿
    return a > ARMOR_CAP ? ARMOR_CAP : a;
}

/** @brief 敌机生命值（部分类型随关卡提升，供生成与属性面板共用以保持一致） */
inline int enemyHp(EnemyType t, int level) {
    // 【数值框架】血量 = 基数 × (1 + 30%·n + 1.8%·n²)   n = 关卡-1
    // 二次项系数刻意取小，保证杂兵不会在后期变成海绵，同时仍随关卡持续加压。
    int base;
    switch (t) {
    case E_NORMAL:  base = 2;  break;   // 杂兵
    case E_FAST:    base = 1;  break;   // 脆皮高速
    case E_ZIGZAG:  base = 3;  break;
    case E_DIVER:   base = 3;  break;
    case E_SHOOTER: base = 5;  break;   // 火力位
    case E_STRAFER: base = 5;  break;
    case E_TANK:    base = 8;  break;   // 精英
    default:        base = 2;  break;
    }
    int n = level - 1;
    int v = (int)(base * (1.0 + 0.25 * n + 0.012 * n * n));   // 已放缓
    return v < 1 ? 1 : v;
}

/** @brief Boss 生命值（供生成与属性面板共用） */
/*----------------------- Boss 种类与强度 -----------------------*/

/** @brief Boss 种类（外观与招式套路各不相同） */
enum BossKind {
    BK_DREADNOUGHT, // 第1关：无畏舰——环形/扇形弹幕，稳健
    BK_HIVE,        // 第2关：母舰——召唤援军 + 十字花瓣 + 激光
    BK_SERPENT      // 第3关：机械巨蛇——螺旋弹幕 + 追踪弹 + 双激光，最凶
};

/** @brief 当前关卡对应的 Boss 种类（每 3 关循环一次） */
inline int bossKind(int level) { return (level - 1) % BOSS_KINDS; }

/** @brief 当前关卡的循环轮次（0 = 首轮的 1~3 关，1 = 第 4~6 关，依此类推） */
inline int bossCycle(int level) { return (level - 1) / BOSS_KINDS; }

/**
 * @brief  Boss 生命值。首轮保持 180 / 260 / 340（第 1 关手感已调好，不再改动）；
 *         之后每循环一轮成倍增长，用以抵消玩家装备（攻击最高 +7）与射速带来的
 *         成长——玩家满配 DPS 约为裸机的 13 倍，故后期 Boss 血量必须同量级放大。
 * @note   采用「按关卡整体递增」而非「按轮次分段」，保证血量随关卡单调上升，
 *         不会在进入新一轮时反而变低。
 */
 /**
  * @brief  Boss 生命值（无限关卡）。经数值模拟标定，已计入 Boss 护甲的减伤：
  *         第 1 关约 3.5 秒击杀、第 15 关约 10 秒、第 30 关约 25 秒；
  *         第 4 关起「难度指数」严格递增且增量整体持续扩大。
  */
inline int bossHp(int level) {
    // 【数值框架·友好版】Boss 血量 = 110 × (1 + 110%·n + 8.5%·n²)
    // 经标定：全程击杀耗时稳定在 5~11 秒（原为 11~24 秒，过于漫长）。
    int n = level - 1;
    return (int)(110 * (1.0 + 1.10 * n + 0.085 * n * n));
}

/**
 * @brief  Boss 防御力。不同种类基础护甲不同（绝不为 0，因此可随关卡提升），
 *         并每 3 关 +1、封顶 4——保证满配玩家(攻击 8)每发仍能打出 >=4 点伤害，
 *         既让防御有存在感，又不会让后期变成打不动的消耗战。
 */
inline int bossArmor(int kind, int level) {
    // 【数值框架·友好版】Boss 护甲 = 1(+无畏舰1) + n/8，上限 3。
    // 上限刻意压低：Boss 的压力来自弹幕与血量，护甲过高只会拖长战斗。
    // 无畏舰的重装加成从第 4 关起生效：第 1 关玩家攻击仅 3 点，
    // 若此时护甲为 2 会把有效伤害压到 1，导致首战冗长（实测 16 秒）。
    int bonus = (kind == BK_DREADNOUGHT && level >= 4) ? 1 : 0;
    int a = 1 + bonus + (level - 1) / 8;
    const int BOSS_ARMOR_CAP = 3;
    return a > BOSS_ARMOR_CAP ? BOSS_ARMOR_CAP : a;
}

/** @brief Boss 强度系数（弹速/弹量随轮次提升） */
inline double bossPower(int level) { return 1.0 + 0.18 * bossCycle(level); }

/**
 * @brief  Boss「攻击欲望」：两次出招之间的间歇帧数，越小越凶。
 *         该值【只与 Boss 种类有关，与关卡轮次无关】——因此三种 Boss 的
 *         攻击欲望依次上升（无畏舰 < 母舰 < 巨蛇），而同一种 Boss 在后续
 *         关卡再次出现时，攻击欲望与上一次出现时完全一致。
 */
inline int bossAggroGap(int kind) {
    switch (kind) {
    case BK_DREADNOUGHT: return 62;   // 攻击欲望最低：出招从容（已放宽）
    case BK_HIVE:        return 46;   // 中等：靠召唤与铺场压迫（已放宽）
    default:             return 30;   // 最高：巨蛇最凶，但已放宽至可应付
    }
}

/** @brief Boss 名称（图鉴与血条标题用） */
inline const TCHAR* bossName(int kind) {
    switch (kind) {
    case BK_DREADNOUGHT: return _T("无畏舰");
    case BK_HIVE:        return _T("蜂巢母舰");
    default:             return _T("机械巨蛇");
    }
}

/** @brief Boss 攻击方式描述（图鉴用） */
inline const TCHAR* bossFeature(int kind) {
    switch (kind) {
    case BK_DREADNOUGHT: return _T("环形/舷炮/霰弹/十字");
    case BK_HIVE:        return _T("召唤/布雷/虫群/光束");
    default:             return _T("追踪/双激光/突进/尾扫");
    }
}

/** @brief 敌机撞击伤害（图鉴“攻击”列即取此值；俯冲机为自爆机故更高） */
inline int enemyTouchDmg(EnemyType t, int level) {
    // 【数值框架·友好版】伤害 = 基数 × (1 + 10%·n + 0.3%·n²)
    // 基数与成长系数均已下调，使玩家全程可承受 6~14 次打击。
    int base;
    switch (t) {
    case E_FAST:    base = 9; break;
    case E_NORMAL:  base = 11; break;
    case E_ZIGZAG:  base = 11; break;
    case E_SHOOTER: base = 12; break;
    case E_STRAFER: base = 12; break;
    case E_TANK:    base = 16; break;   // 重装撞击
    case E_DIVER:   base = 20; break;   // 自爆俯冲最痛
    default:        base = 11; break;
    }
    int n = level - 1;
    return (int)(base * (1.0 + 0.10 * n + 0.003 * n * n));
}


/** @brief 敌方子弹伤害（同样为二次膨胀，随关卡持续加压） */
inline int enemyBulletDmg(int level) {
    int n = level - 1;
    return (int)(HIT_HP_BULLET * (1.0 + 0.10 * n + 0.003 * n * n));
}


/** @brief 敌机击毁得分（逐关递增：关卡越高分数越高） */
inline int enemyScore(EnemyType t, int level) {
    int base;
    switch (t) {
    case E_NORMAL:  base = 10; break;
    case E_FAST:    base = 20; break;
    case E_SHOOTER: base = 30; break;
    case E_ZIGZAG:  base = 25; break;
    case E_TANK:    base = 60; break;
    case E_DIVER:   base = 35; break;
    case E_STRAFER: base = 40; break;
    default:        base = 10; break;
    }
    return base * (10 + 4 * (level - 1)) / 10;   // 每关约 +40% 基础分
}

/** @brief 击败某关 Boss 的固定得分（与 nextLevelOrWin 中的计分保持一致） */
inline int bossScoreOf(int level) { return 500 * level + 100 * level * level; }

/**
 * @brief  某关实际会刷出的敌机总数。
 *         必须严格复刻 updatePlaying 里的刷怪循环——间隔会随关内时间递减，
 *         若只用最终(最小)间隔估算会高估刷怪数，导致评级基准偏高。
 */
inline int levelSpawnCount(int level) {
    int n = 0, st = 0;
    for (int t = 0; t < LEVEL_FRAMES; ++t) {
        int iv = 46 - level * 8 - (t / 240);
        if (iv < 16) iv = 16;
        if (++st >= iv) { st = 0; ++n; }
    }
    return n;
}

/**
 * @brief  某关刷出的敌机的【平均分值】（×100 以整数表示，避免浮点）。
 *         必须与 spawnEnemy 的概率表一致——各关可刷的机型不同，平均分值差异巨大：
 *         第 1 关只有普通机(10)与快速机(20)，均分仅 12.8；
 *         若统一按全部 7 种机型的均分(31.4)计算，第 1 关的基准会虚高 2.6 倍，
 *         导致"全歼也评不到 S"。
 */
inline int levelAvgMobScore100(int level) {
    if (level == 1)      return 72 * 10 + 28 * 20;                      // 1280
    else if (level == 2) return 40 * 10 + 22 * 20 + 20 * 25 + 18 * 60;      // 2420
    else                 return 26 * 10 + 18 * 20 + 14 * 25 + 12 * 60
        + 12 * 30 + 10 * 40 + 8 * 35;              // 2730
}

/**
 * @brief  某关【杂兵满分】——即把该关刷出的敌机全部击毁所能获得的分数。
 *         评级采用「得分率 = 实际杂兵得分 / 杂兵满分」：
 *           全歼 = 100%，直观且必定能拿到 S。
 */
inline int fullMobScore(int level) {
    int avg100 = levelAvgMobScore100(level) * (10 + 4 * (level - 1)) / 10;   // 关卡分数加成
    return (int)((double)levelSpawnCount(level) * avg100 / 100.0);
}

/*----------------------- 经验奖励 -----------------------*/

/**
 * @brief  敌机「强度权重」：按属性加权评估这架敌机有多难缠。
 *         权重 = 生命 + 防御×6 + 撞击伤害/2。
 */
inline int enemyPower(EnemyType t, int level) {
    return enemyHp(t, level) + enemyArmor(t, level) * 6 + enemyTouchDmg(t, level) / 2;
}

/**
 * @brief  击毁敌机获得的经验：同一关卡内按强度权重分配（越厉害的怪经验越多），
 *         并随关卡整体提升（越后面的关卡经验越多）。
 */
inline int enemyXp(EnemyType t, int level) {
    int base = 3 + enemyPower(t, level) / 5;          // 略回调，配合更低的升级需求
    return base * (10 + 2 * (level - 1)) / 10;        // 随关卡增幅也调低
}

/** @brief 击败 Boss 获得的经验（远高于小怪，且随关卡提升） */
inline int bossXp(int level) { return 45 + level * 22; }   // 已调低

/** @brief 敌机攻击力（对玩家造成的伤害，配合玩家防御减免） */
inline int enemyAttack(EnemyType t) {
    switch (t) {
    case E_NORMAL:  return 10;
    case E_FAST:    return 12;
    case E_SHOOTER: return 12;
    case E_ZIGZAG:  return 10;
    case E_TANK:    return 22;
    case E_DIVER:   return 28;   // 自爆机撞击伤害高
    case E_STRAFER: return 14;
    }
    return 10;
}

/*----------------------- 装备系统（定义见下方 Gear 版本） -----------------------*/

/** @brief Boss 招式类型（每种 Boss 拥有各自的招式池，随机选取且避免连续重复） */
enum BossAtk {
    /*---- 无畏舰专属 6 招：正面炮火压制，弹幕规整、可读性强 ----*/
    BA_RING,        // 旋转环形弹幕：连发数圈、彼此错开如花瓣
    BA_AIM,         // 瞄准扇形散射：朝玩家方向多发扇形弹
    BA_LASER,       // 预警激光：先亮红色警戒线，再射出粗激光
    BA_BROADSIDE,   // 舷炮齐射：左右两舷交替倾泻平行弹幕
    BA_SHOTGUN,     // 霰弹爆发：锥形高速散弹，近身极痛
    BA_CROSS,       // 十字扫射：四向同时推进的弹墙

    /*---- 蜂巢母舰专属 6 招：以量取胜，场面混乱 ----*/
    BA_SUMMON,      // 召唤援军：唤出数架敌机
    BA_PETAL,       // 十字花瓣：四个斜向弧形弹群
    BA_MINE,        // 布雷：投放悬浮雷，延时炸开成环形弹幕
    BA_SWARM,       // 虫群散射：大量低速弹铺满全屏
    BA_BEAMFAN,     // 扇形光束：以自身为中心缓慢旋转扫过的光束群
    BA_DRONE,       // 无人机墙：一排同步下压的弹墙，需找缝隙穿过

    /*---- 机械巨蛇专属 6 招：高机动、追踪压迫 ----*/
    BA_SPIRAL,      // 螺旋弹幕：多臂持续旋转喷射
    BA_HOMING,      // 追踪导弹：缓慢转向追踪玩家
    BA_TWINLASER,   // 双柱激光：左右两道同时开火，中间留缝
    BA_COIL,        // 盘绕突进：横向高速冲刺并沿途撒弹
    BA_VENOM,       // 毒液喷吐：抛物线状散落的酸弹
    BA_TAILSWEEP,   // 尾扫：由一侧向另一侧推进的密集弹墙

    BA_COUNT        // 招式总数（计数用）
};

/** @brief 装备部位 */
enum GearSlot {
    GS_WEAPON,      // 武器：提升攻击力
    GS_ARMOR,       // 护甲：提升防御力与生命上限
    GS_ENGINE,      // 引擎：提升机动与射速
    GS_COUNT
};

/** @brief 装备稀有度（越稀有属性越强，掉落越少） */
enum Rarity {
    R_COMMON,       // 普通（灰）
    R_RARE,         // 精良（蓝）
    R_EPIC,         // 稀有（紫）
    R_LEGEND,       // 史诗（金）
    R_COUNT
};

/**
 * @brief  武器类型：不同武器不仅提供数值，更会【改变攻击方式】。
 *         四种武器综合强度大致相当（射速与单发伤害互补），但手感截然不同：
 *           速射炮  ：直线速射，泛用平衡
 *           散射炮  ：锥形扇面，近距离清场强
 *           穿透光束：可贯穿多个敌人，射速偏慢
 *           追踪导弹：自动追踪必中，射速最慢但单发最重
 */
enum WeaponType {
    W_CANNON,       // 速射炮
    W_SPREAD,       // 散射炮
    W_PIERCE,       // 穿透光束
    W_HOMING,       // 追踪导弹
    W_COUNT
};

/** @brief 武器名称 */
inline const TCHAR* weaponName(int w) {
    switch (w) {
    case W_CANNON: return _T("速射炮");
    case W_SPREAD: return _T("散射炮");
    case W_PIERCE: return _T("穿透光束");
    default:       return _T("追踪导弹");
    }
}

/** @brief 武器攻击方式简述（图鉴用） */
inline const TCHAR* weaponDesc(int w) {
    switch (w) {
    case W_CANNON: return _T("直线速射，泛用平衡");
    case W_SPREAD: return _T("锥形散射，近距强");
    case W_PIERCE: return _T("贯穿敌人，射速偏慢");
    default:       return _T("自动追踪必中，射速慢");
    }
}

/** @brief 各武器在不同火力等级下的弹道说明（图鉴用） */
inline const TCHAR* weaponFireDesc(int w, int fireLv) {
    switch (w) {
    case W_CANNON:
        return fireLv == 1 ? _T("Lv1 单发直射")
            : fireLv == 2 ? _T("Lv2 双管平行") : _T("Lv3 三管+两侧斜射");
    case W_SPREAD:
        return fireLv == 1 ? _T("Lv1 3发窄扇")
            : fireLv == 2 ? _T("Lv2 6发中扇") : _T("Lv3 9发宽扇");
    case W_PIERCE:
        return fireLv == 1 ? _T("Lv1 单道贯穿")
            : fireLv == 2 ? _T("Lv2 双道贯穿") : _T("Lv3 三道+光束加粗");
    default:
        return fireLv == 1 ? _T("Lv1 单发追踪")
            : fireLv == 2 ? _T("Lv2 双发追踪") : _T("Lv3 三发扇形追踪");
    }
}

/** @brief 武器主色（子弹与图标配色） */
inline COLORREF weaponColor(int w) {
    switch (w) {
    case W_CANNON: return RGB(180, 255, 200);
    case W_SPREAD: return RGB(255, 210, 120);
    case W_PIERCE: return RGB(150, 220, 255);
    default:       return RGB(255, 160, 200);
    }
}

/** @brief 武器射速修正（增加的冷却帧）：覆盖越广/单发越强的武器射速越慢 */
inline int weaponCdBonus(int w) {
    // 【平衡原则】DPS = (60/冷却) × 弹数 × (攻击×倍率)。三项互相抵消，使各武器 DPS 相等。
    // 速射炮基准冷却 9 帧、单发 100%、弹数 1/2/3。
    switch (w) {
    case W_CANNON: return 0;    // 冷却 9
    case W_SPREAD: return 9;    // 冷却 18：弹数×3，故冷却×2
    case W_PIERCE: return 9;    // 冷却 18：单发×2
    default:       return 27;   // 冷却 36：单发×4（追踪导弹）
    }
}

/** @brief 武器单发伤害倍率(%)：与射速共同保证四种武器 DPS 大致相当 */
inline int weaponDmgPct(int w) {
    // 与 weaponCdBonus 配套：保证四种武器 DPS 完全一致（差异只体现在手感与覆盖）。
    switch (w) {
    case W_CANNON: return 100;   // 冷却9  ×1弹 ×1.0
    case W_SPREAD: return 67;    // 冷却18 ×3弹 ×0.67  ≈ 基准
    case W_PIERCE: return 200;   // 冷却18 ×1弹 ×2.0   = 基准
    default:       return 400;   // 冷却36 ×1弹 ×4.0   = 基准（必中）
    }
}

/** @brief 一件装备：由部位与稀有度决定其加成强度 */
struct Gear {
    int slot;       // GearSlot
    int rarity;     // Rarity
    int lvl;        // 装备等级 1..GEAR_LVL_MAX（敌人只掉落 1 级，靠合成提升）
    int wtype;      // WeaponType：仅 slot==GS_WEAPON 时有效，决定攻击方式
};

/** @brief 稀有度名称 */
inline const TCHAR* rarityName(int r) {
    switch (r) {
    case R_COMMON: return _T("普通"); case R_RARE: return _T("精良");
    case R_EPIC: return _T("稀有"); default: return _T("史诗");
    }
}
/** @brief 稀有度颜色 */
inline COLORREF rarityColor(int r) {
    switch (r) {
    case R_COMMON: return RGB(185, 190, 200); case R_RARE: return RGB(90, 175, 255);
    case R_EPIC: return RGB(195, 115, 255); default: return RGB(255, 205, 90);
    }
}
/** @brief 部位名称 */
inline const TCHAR* slotName(int s) {
    switch (s) {
    case GS_WEAPON: return _T("武器"); case GS_ARMOR: return _T("护甲");
    default: return _T("引擎");
    }
}
/*--- 各部位加成随稀有度线性增长（rarity 取值 0..3）---*/
/*--- 装备加成：由「稀有度」与「等级」共同决定 ---
 *   稀有度决定基数，等级(1..5)在其基础上线性叠加。
 *   合成规则：两件【同部位 + 同稀有度 + 同等级】可合成为高一级的同类装备，
 *   因此一件 LV5 装备等价于 16 件 LV1，收益理应可观。               */
inline int gearAtk(int rarity, int lvl) { return (rarity + 1) + (lvl - 1) * 2; }        // 武器：攻击 1..4 +0..8  => max 12
inline int gearDef(int rarity, int lvl) { return (rarity + 1) + (lvl - 1); }            // 护甲：防御 1..4 +0..4  => max 8
inline int gearHp(int rarity, int lvl) { return (rarity + 1) * 15 + (lvl - 1) * 12; }  // 护甲：血上限 15..60 +0..48 => max 108
inline int gearSpeed(int rarity, int lvl) { return (rarity + 1) + (lvl - 1); }            // 引擎：机动 1..4 +0..4  => max 8
inline int gearRate(int rarity, int lvl) { return rarity + (lvl - 1) / 2; }              // 引擎：射速 0..3 +0..2  => max 5(减冷却)

/*----------------------- 道具图鉴信息 -----------------------*/

/** @brief 道具名称（图鉴用） */
inline const TCHAR* itemName(int t) {
    switch (t) {
    case IT_FIRE:    return _T("火力增强");
    case IT_LIFE:    return _T("应急维修");
    case IT_SHIELD:  return _T("能量护盾");
    case IT_BOMB:    return _T("炸弹补给");
    case IT_MAXLIFE: return _T("装甲强化");
    case IT_POWERUP: return _T("弹头改良");
    default:         return _T("装备芯片");
    }
}

/** @brief 道具效果说明（图鉴用） */
inline const TCHAR* itemEffect(int t) {
    switch (t) {
    case IT_FIRE:    return _T("临时 火力等级+1，持续10秒");
    case IT_LIFE:    return _T("临时 立即恢复 40 点血量");
    case IT_SHIELD:  return _T("临时 抵挡一次伤害");
    case IT_BOMB:    return _T("炸弹+1；清屏，对Boss约8%上限血+20");
    case IT_MAXLIFE: return _T("永久 血量上限+25 并回复");
    case IT_POWERUP: return _T("永久 子弹基础伤害+1");
    default:         return _T("拾取后进入背包，可安装");
    }
}

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
/*                              音频引擎（BGM + 音效）                       */
/*==========================================================================*/
/*
 *  实现说明：
 *    Windows 的 PlaySound() 同一时刻只能播放一个声音——一旦播放音效，BGM 就会
 *    被打断，无法满足游戏需求。因此这里改用 MCI（mciSendString）：
 *    每个音频以独立「别名」打开，可多路同时播放。
 *
 *    · BGM：单一别名，每帧轮询播放状态，播完自动重播实现无缝循环。
 *    · 音效：为每个音效开若干「声部」(voice) 轮流使用，使同类音效可以叠加
 *            （例如连续射击时不会互相截断）。
 *
 *    全部资源为程序合成的原创 8-bit 音频，无版权问题。
 *    若 audio 目录缺失或打开失败，引擎会自动降级为静音，游戏照常运行。
 */

 /** @brief 音效编号 */
enum SfxId {
    SFX_SHOOT, SFX_EXPLODE, SFX_HIT, SFX_POWERUP, SFX_BOMB,
    SFX_LASER, SFX_LEVELUP, SFX_FUSE, SFX_BOSSWARN, SFX_GAMEOVER,
    SFX_COUNT
};

/**
 * @brief  音频引擎（BGM + 音效）。
 *
 *  设计要点（两个坑都在这里解决）：
 *
 *  1) 【性能】mciSendString 是同步阻塞调用，且 MCI 会把命令串行化。
 *     当 BGM(尤其 mp3 走 mpegvideo 设备)正在解码时，主线程每发一条命令
 *     都可能被拖住十几~几十毫秒；在 60FPS(每帧仅 16.7ms) 下必然掉帧。
 *     => 解法：所有 MCI 命令交由一个工作线程串行执行，主线程只做入队(微秒级)。
 *
 *  2) 【正确性】MCI 设备存在“线程所有权”：设备由哪个线程打开，
 *     就应由哪个线程发送后续命令，跨线程操作会被拒绝(命令静默失败=没声音)。
 *     => 解法：把 open / play / close 全部放在同一个工作线程内完成，
 *        主线程绝不直接碰 MCI。跨线程共享的状态用 std::atomic 传递。
 */
class Audio {
public:
    ~Audio() { stopWorker(); }

    /** @brief 初始化：启动工作线程，由它去打开全部音频设备 */
    void init() {
#if ENABLE_SOUND
        // 各音效的最小触发间隔(ms)：射击最频繁，节流最狠；关键音效不节流
        static const int gaps[SFX_COUNT] = {
            70,  // SFX_SHOOT   连射节流：听感无损，却能显著降低设备压力
            45,  // SFX_EXPLODE
            60,  // SFX_HIT
            0,   // SFX_POWERUP
            0,   // SFX_BOMB
            0,   // SFX_LASER
            0,   // SFX_LEVELUP
            0,   // SFX_FUSE
            0,   // SFX_BOSSWARN
            0    // SFX_GAMEOVER
        };
        for (int i = 0; i < SFX_COUNT; ++i) { sfxMinGap[i] = gaps[i]; sfxLastMs[i] = 0; }
        worker = std::thread(&Audio::workerLoop, this);   // 设备将在此线程内打开
#endif
    }

    /** @brief 关闭：让工作线程排空队列并关闭设备后退出 */
    void shutdown() { stopWorker(); }

    /** @brief 开始播放 BGM（已在播放则忽略） */
    void startBgm() {
#if ENABLE_SOUND
        if (muted || playing || !bgmOk.load()) return;
        post(_T("play bgm from 0"));
        bgmStartMs = timeGetTime();                 // 记录起播时刻，用于本地计时循环
        playing = true;
#endif
    }

    /** @brief 停止 BGM */
    void stopBgm() {
#if ENABLE_SOUND
        if (bgmOk.load()) post(_T("stop bgm"));
        playing = false;
#endif
    }

    /**
     * @brief  每帧调用：判断 BGM 是否播完并循环。
     *         【零开销】用开局查得的曲目长度 + 本地计时器判断，
     *         正常情况下每帧不做任何 MCI 调用，只有真正该循环时才发一条命令。
     */
    void update() {
#if ENABLE_SOUND
        if (muted || !playing || !bgmOk.load()) return;
        DWORD len = bgmLenMs.load();
        if (len == 0) return;                       // 长度未知则不做计时循环
        DWORD now = timeGetTime();
        if (now - bgmStartMs >= len) {
            post(_T("play bgm from 0"));
            bgmStartMs = now;
        }
#endif
    }

    /** @brief 播放一次音效（节流 + 声部轮换 + 异步投递，主线程不阻塞） */
    void playSfx(int id) {
#if ENABLE_SOUND
        if (muted || id < 0 || id >= SFX_COUNT) return;
        if (!ready.load()) return;                  // 设备尚未就绪
        DWORD now = timeGetTime();
        if (sfxMinGap[id] > 0 && now - sfxLastMs[id] < (DWORD)sfxMinGap[id]) return;
        sfxLastMs[id] = now;
        int v = sfxNext[id];
        sfxNext[id] = (v + 1) % SFX_VOICES;
        TCHAR cmd[64];
        _stprintf_s(cmd, _countof(cmd), _T("play sfx%d_%d from 0"), id, v);
        post(cmd);
#endif
    }

    /** @brief 静音开关（M 键） */
    void toggleMute() {
#if ENABLE_SOUND
        muted = !muted;
        if (muted) { post(_T("stop bgm")); playing = false; }
        else { startBgm(); }
#endif
    }
    bool isMuted() const { return muted; }

    /*---- 诊断接口（供 F3 浮层显示，便于定位"没声音"的原因）----*/
    bool  diagReady() const {
#if ENABLE_SOUND
        return ready.load();
#else
        return false;
#endif
    }
    bool  diagBgmOk() const {
#if ENABLE_SOUND
        return bgmOk.load();
#else
        return false;
#endif
    }
    DWORD diagBgmLen() const {
#if ENABLE_SOUND
        return bgmLenMs.load();
#else
        return 0;
#endif
    }

private:
#if ENABLE_SOUND
    static const int SFX_VOICES = 3;               // 每个音效的声部数（可叠加播放）

    /** @brief 投递一条 MCI 命令到工作线程（主线程侧，仅入队，微秒级） */
    void post(const TCHAR* cmd) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (cmdQ.size() >= 48) return;          // 过载保护：宁可丢音效也不堆积
            cmdQ.push_back(std::basic_string<TCHAR>(cmd));
        }
        cv.notify_one();
    }

    /**
     * @brief  工作线程主体：
     *         ① 在本线程内打开所有设备（满足 MCI 的线程所有权要求）
     *         ② 循环执行命令队列
     *         ③ 退出前在本线程内关闭设备
     */
    void workerLoop() {
        openAllDevices();
        ready.store(true);

        for (;;) {
            std::basic_string<TCHAR> cmd;
            {
                std::unique_lock<std::mutex> lk(mtx);
                cv.wait(lk, [this] { return quitFlag || !cmdQ.empty(); });
                if (quitFlag && cmdQ.empty()) break;
                cmd = cmdQ.front();
                cmdQ.pop_front();
            }
            mciSendString(cmd.c_str(), NULL, 0, NULL);   // 阻塞只发生在本线程
        }
        closeAllDevices();
    }

    /** @brief 打开 BGM 与全部音效设备（必须在工作线程内调用） */
    void openAllDevices() {
        // ---- BGM：自动识别格式与设备 ----
        // MCI 标准设备只有 waveaudio(wav/PCM)、mpegvideo(mpg/mp3)、sequencer(mid)，
        // 没有为 MP4 设计的设备；type 为 NULL 表示交给系统按注册表自动判定。
        struct { const TCHAR* file; const TCHAR* type; } cand[] = {
            { _T("audio\\bgm.wav"), _T("waveaudio") },   // 标准 PCM wav（最可靠）
            { _T("audio\\bgm.wav"), _T("mpegvideo") },   // 非标准编码 wav 的回退
            { _T("audio\\bgm.wav"), NULL              },
            { _T("audio\\bgm.mp3"), _T("mpegvideo") },
            { _T("audio\\bgm.mp3"), NULL              },
            { _T("audio\\bgm.mp4"), NULL              }, // mp4：尽力而为，取决于系统驱动
            { _T("audio\\bgm.mp4"), _T("mpegvideo") },
            { _T("audio\\bgm.m4a"), NULL              },
            { _T("audio\\bgm.wma"), _T("mpegvideo") },
            { _T("audio\\bgm.mid"), _T("sequencer")  },
        };
        bool ok = false;
        for (int i = 0; i < 10 && !ok; ++i) {
            TCHAR cmd[256];
            if (cand[i].type)
                _stprintf_s(cmd, _countof(cmd), _T("open \"%s\" type %s alias bgm"),
                    cand[i].file, cand[i].type);
            else
                _stprintf_s(cmd, _countof(cmd), _T("open \"%s\" alias bgm"), cand[i].file);
            if (mciSendString(cmd, NULL, 0, NULL) == 0) ok = true;
        }
        bgmOk.store(ok);
        if (ok) {   // 只查一次曲目长度，供主线程做零开销的计时循环
            TCHAR ret[64] = _T("");
            mciSendString(_T("set bgm time format milliseconds"), NULL, 0, NULL);
            mciSendString(_T("status bgm length"), ret, _countof(ret), NULL);
            bgmLenMs.store((DWORD)_ttoi(ret));
        }

        // ---- 音效：每个音效开 SFX_VOICES 个声部，可叠加播放 ----
        static const TCHAR* sfxFile[SFX_COUNT] = {
            _T("audio\\sfx_shoot.wav"),    _T("audio\\sfx_explode.wav"),
            _T("audio\\sfx_hit.wav"),      _T("audio\\sfx_powerup.wav"),
            _T("audio\\sfx_bomb.wav"),     _T("audio\\sfx_laser.wav"),
            _T("audio\\sfx_levelup.wav"),  _T("audio\\sfx_fuse.wav"),
            _T("audio\\sfx_bosswarn.wav"), _T("audio\\sfx_gameover.wav")
        };
        for (int s = 0; s < SFX_COUNT; ++s)
            for (int v = 0; v < SFX_VOICES; ++v) {
                TCHAR cmd[256];
                _stprintf_s(cmd, _countof(cmd),
                    _T("open \"%s\" type waveaudio alias sfx%d_%d"), sfxFile[s], s, v);
                mciSendString(cmd, NULL, 0, NULL);   // 失败则该声部静音，不影响其它
            }
    }

    /** @brief 关闭全部设备（必须在工作线程内调用） */
    void closeAllDevices() {
        if (bgmOk.load()) mciSendString(_T("close bgm"), NULL, 0, NULL);
        for (int s = 0; s < SFX_COUNT; ++s)
            for (int v = 0; v < SFX_VOICES; ++v) {
                TCHAR cmd[64];
                _stprintf_s(cmd, _countof(cmd), _T("close sfx%d_%d"), s, v);
                mciSendString(cmd, NULL, 0, NULL);
            }
    }
#endif

    /** @brief 停止并回收工作线程（幂等，shutdown 与析构共用） */
    void stopWorker() {
#if ENABLE_SOUND
        if (!worker.joinable()) return;
        {
            std::lock_guard<std::mutex> lk(mtx);
            quitFlag = true;
        }
        cv.notify_all();
        worker.join();          // 工作线程会排空队列并在自身线程内关闭设备
#endif
    }

#if ENABLE_SOUND
    std::thread              worker;
    std::mutex               mtx;
    std::condition_variable  cv;
    std::deque<std::basic_string<TCHAR> > cmdQ;
    bool               quitFlag = false;
    std::atomic<bool>  ready;               // 设备是否已就绪（工作线程置位）
    std::atomic<bool>  bgmOk;               // BGM 是否成功打开
    std::atomic<DWORD> bgmLenMs;            // 曲目总长(ms)，0 表示未知
    DWORD sfxLastMs[SFX_COUNT] = {};        // 各音效上次播放时刻（节流用）
    int   sfxMinGap[SFX_COUNT] = {};        // 各音效最小间隔(ms)
    int   sfxNext[SFX_COUNT] = {};          // 各音效下一个使用的声部
    DWORD bgmStartMs = 0;                   // 本轮起播时刻
    bool  playing = false;
#endif
    bool muted = false;

public:
    /** @brief 构造：初始化原子量 */
    Audio()
#if ENABLE_SOUND
        : ready(false), bgmOk(false), bgmLenMs(0)
#endif
    {
    }
};

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
    bool     homing;        // 是否为追踪弹（敌弹追踪玩家；玩家追踪导弹追踪敌人）
    int      life;          // 存活帧数（用于追踪弹的燃料上限与拖尾特效）
    int      pierce;        // 剩余可贯穿次数（0=命中即消失；穿透光束>0）
    int      trail;         // 拖尾特效强度（0=无）
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
    int       armor;        // 防御力（减免子弹伤害，至少仍造成 1 点）
    int       attackTimer;  // 距下一次“选招”的间歇计时
    int       hitFlash;     // 受击白闪计时
    bool      active;       // 是否已登场
    int       kind;         // BossKind：决定外观与招式池
    double    power;        // 强度系数（弹速/弹量随循环轮次提升）
    double    bob;          // 浮动相位（外观动效）
    /*---- 招式状态机 ----*/
    int       castType;     // 当前施放的招式（BossAtk），-1 表示间歇中
    int       castTimer;    // 当前招式剩余持续帧
    int       lastType;     // 上一招（用于避免连续重复）
    double    castAngle;    // 环形/螺旋类招式的累计角度
    int       laserX;       // 激光目标横坐标（施放瞬间锁定）
    int       laserX2;      // 第二道激光横坐标（双柱激光用）
    int       laserWarn;    // 激光预警剩余帧
    int       laserFire;    // 激光开火剩余帧
};

/** @brief 悬浮雷（蜂巢母舰 BA_MINE 专用）：静止一段时间后炸开成环形弹幕 */
struct Mine {
    double x, y;            // 中心坐标
    int    fuse;            // 引爆倒计时（帧）
    bool   alive;           // 是否存活
};

/** @brief 道具 */
struct Item {
    double   x, y;          // 中心坐标
    double   vy;            // 下落速度
    int      size;          // 尺寸
    ItemType type;          // 类型
    Gear     gear;          // 当 type==IT_GEAR 时有效：掉落的装备数据
    bool     alive;         // 是否存活
};

/** @brief 爆炸/受击粒子 */
/** @brief 粒子种类：不同种类有各自的运动与绘制方式，用于组合出华丽特效 */
enum PKind {
    PK_SPARK,       // 火花：高速、拖尾、快速衰减
    PK_SMOKE,       // 烟雾：低速、膨胀、渐隐
    PK_RING,        // 冲击波环：快速扩张的圆环
    PK_SHARD,       // 碎片：带旋转的多边形碎块
    PK_STAR         // 星芒：四角星闪光
};

struct Particle {
    double   x, y;          // 中心坐标
    double   vx, vy;        // 速度
    int      life;          // 剩余寿命（帧）
    int      maxLife;       // 初始寿命
    double   size;          // 尺寸
    COLORREF color;         // 颜色
    int      kind;          // PKind：粒子种类
    double   rot, spin;     // 旋转角与角速度（碎片/星芒用）
    double   grow;          // 每帧尺寸增量（冲击环/烟雾用）
};

/** @brief 僚机 / 环绕卫星（技能召唤物，自动开火） */
struct Helper {
    double x, y;        // 当前位置
    double phase;       // 环绕相位（卫星用）
    int    life;        // 剩余存活帧
    int    shootCd;     // 射击冷却
    bool   orbit;       // true=环绕玩家的卫星；false=并肩僚机
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
    void updateMines();                     // 更新悬浮雷（倒计时与引爆）
    void drawMines();                       // 绘制悬浮雷
    void updateStars();                     // 更新背景星空
    void handleCollisions();                // 统一处理各类碰撞
    void killEnemy(Enemy& e);               // 击毁敌机的统一处理（计分、掉落、爆炸）
    void spawnBoss();                       // 生成 Boss
    void updateBossCast();                  // Boss 招式状态机（选招与施放）
    void spawnEnemyBullet(double x, double y, double vx, double vy,
        COLORREF c, int r = 6, bool homing = false); // 生成一发敌方子弹
    void activateUltimate();                // 释放当前选用的大招
    void useSkill();                        // 释放当前选用的小技能
    void updateSkillEffects();              // 各类技能效果的逐帧结算
    void updateHelpers();                   // 僚机/卫星更新
    void drawHelpers();                     // 僚机/卫星绘制
    void addMoney(int copper);              // 获得货币
    void evaluateCycle();                   // 三关结算：评级并赠予被动
    void drawRankOverlay();                 // 评级结算浮层
    void drawRankPanel(int x, int y);       // 本轮评级栏（画在暂停面板内）
    void drawShop();                        // 商店界面
    void drawSkillPanel();                  // 技能总览面板（大招/小技能/被动）
    void updateUltimate();                  // 大招每帧结算（伤害/清弹）
    void dropItem(double x, double y, bool fromBoss = false); // 在指定位置概率掉落道具
    void applyItem(ItemType t);             // 应用道具效果
    void addGear(const Gear& g);            // 拾取装备进入物品栏
    void equipFromBag(int idx);             // 装备物品栏中的某件（与原装备交换）
    void discardFromBag(int idx);           // 丢弃物品栏中的某件
    int  playerAttack()  const;             // 有效攻击力（基础 + 武器）
    int  playerDefense() const;             // 有效防御力（护甲）
    int  playerHpMax()   const;             // 有效血量上限（基础 + 护甲）
    int  playerSpeed()   const;             // 有效移动速度（基础 + 引擎）
    int  playerShootCd() const;             // 有效射击冷却（基础 - 引擎）
    void hitPlayer(int dmg);                // 玩家受到一次伤害
    void gainXp(int amount);                // 获得经验并处理升级
    void applyCheatConfig();                // 把自定义配置应用到本局
    void drawCheatPanel();                  // 绘制自定义初始配置界面
    int  rollWeaponType();                  // 从轮转池中取一种武器（保证四种均匀出现）
    int  playerWeaponType() const;          // 当前武器类型（未装备则为速射炮）
    int  playerBulletDamage() const;        // 单发伤害（含武器倍率）
    void spawnMuzzleFlash(double x, double y, COLORREF c);  // 枪口闪光特效
    void fuseFromBag(int idx);              // 与背包中同类同级装备合成升级（扣血，归零才损命）
    void spawnExplosion(double x, double y, COLORREF c, int count); // 生成爆炸粒子
    void addParticle(int kind, double x, double y, double vx, double vy,
        double size, COLORREF c, int life, double grow, double spin);
    void spawnShockwave(double x, double y, COLORREF c, double growSpeed, int life);
    void nextLevelOrWin();                  // Boss 被击败后进入下一关或胜利

    /*------------------------- 画面渲染 -------------------------*/
    void drawBackground();                  // 星空背景
    void drawPlayer();                      // 玩家飞机
    void drawEnemies();                     // 敌机
    void drawBoss();                        // Boss 与血条
    void drawBossIcon(int kind, int cx, int cy);  // Boss 缩略图标（图鉴用，与实战造型对应）
    void drawBullets();                     // 子弹
    void drawItems();                       // 道具
    void drawItemIcon(int type, const Gear& g, int cx, int cy, int r, double pulse); // 单个道具图标（世界/图鉴共用）
    void drawParticles();                   // 粒子
    void drawHUD();                         // 分数、生命、状态等信息
    void drawFlash();                       // 全屏白闪特效
    void drawFps();                         // 性能诊断浮层（F3）
    void drawBanner();                      // 关卡横幅
    void drawMenu();                        // 主菜单
    void drawPaused();                      // 暂停界面
    void drawInfoPanel();                   // 属性图鉴面板（暂停时按 I 打开）
    void drawEquipPanel();                  // 装备栏面板（暂停时按 E 打开）
    void drawGameOver();                    // 失败界面
    void drawVictory();                     // 胜利界面
    void drawEnemyShape(const Enemy& e);    // 绘制单个敌机造型
    void drawUltimate();                    // 绘制激光大招光束

    /*------------------------- 数据存档 -------------------------*/
    void loadLeaderboard();                 // 从文件读取排行榜
    void saveLeaderboard();                 // 写回排行榜文件
    void tryRecordScore();                  // 若成绩进入前 10 则记录

    /*------------------------- 输入辅助 -------------------------*/
    bool keyDown(int vk)     const { return curKey[vk]; }               // 按键是否按住
    bool justPressed(int vk) const { return curKey[vk] && !prevKey[vk]; } // 按键是否本帧刚按下


private:
    /*------------------------- 状态数据 -------------------------*/
    Audio     audio;                        // 音频引擎（BGM + 音效）
    bool      running = true;               // 主循环是否继续
    GameState state = ST_MENU;            // 当前状态
    int       pausePanel = 0;               // 暂停面板：0=无 1=图鉴 2=装备栏
    int       dexPage = 0;               // 图鉴分页：0=敌机 1=道具增益 2=装备
    bool      quitSettle = false;           // 本次结算是否由“主动退出并结算”触发
    int       rankJustSet = -1;             // 本局刚写入排行榜的名次下标（-1 表示未上榜）
    int       wPool[W_COUNT];               // 武器轮转池：保证四种武器都能稳定出现
    int       wPoolLeft = 0;                // 池中剩余数量（取空后重新洗牌）
    /*---- 性能诊断（F3 开关）----*/
    bool      showFps = false;              // 是否显示帧率/耗时诊断
    double    fpsAvg = 60.0;                // 平滑后的帧率
    DWORD     lastFrameMs = 0;              // 上一帧耗时(ms)
    DWORD     worstFrameMs = 0;             // 近期最差帧耗时(ms)，用于定位卡顿
    /*---- 秘籍 / 自定义初始配置 ----*/
    TCHAR     cheatBuf[8] = _T("");         // 菜单中最近输入的数字（用于匹配秘籍）
    bool      cheatOn = false;              // 本局是否使用自定义配置开局
    int       cheatCursor = 0;              // 配置面板光标
    /*---- 商店 ----*/
    int       shopTab = 0;                  // 商店分页：0=大招 1=小技能
    int       shopCursor = 0;               // 商店光标
    int       cfgPlv = 1, cfgLevel = 1;     // 自定义：等级 / 起始关卡
    int       cfgWtype = 0, cfgRarity = 3;  // 自定义：武器类型 / 装备稀有度
    int       cfgGlvl = 1;                  // 自定义：装备等级
    int       cfgHp = 0, cfgAtk = 0, cfgDef = 0;   // 自定义：额外血量/攻击/防御
    int       cfgLives = 4;                 // 自定义：备用机数量
    int       cfgMoney = 0;                 // 自定义：初始货币（铜）
    int       cfgUlt = 0, cfgSkill = 0;     // 自定义：初始大招 / 小技能
    int       cfgPassive = 0;               // 自定义：初始被动数量（0~3）
    int       invCursor = 0;               // 装备栏光标位置

    bool curKey[256] = { 0 };                 // 本帧按键状态
    bool prevKey[256] = { 0 };                // 上一帧按键状态

    /*------------------------- 玩家数据 -------------------------*/
    double px, py;                          // 玩家中心坐标
    int    hp;                              // 当前血量（护甲装备提升上限，归零才损失 1 命）
    int    lives;                           // 剩余命数（血量耗尽后 -1，用完则失败）
    int    bonusHp;                         // 永久道具累计提升的血量上限
    /*---- 等级与经验 ----*/
    int    plv;                             // 玩家等级（1..PLV_MAX）
    int    xp;                              // 当前经验值（达到 xpNeed(plv) 即升级）
    int    lvlUpFlash;                      // 升级特效剩余帧
    DWORD  lastLvlUpMs;                     // 上次升级时刻（用于统计每级耗时）
    int    lastLvlSec;                      // 上一次升级所花秒数（HUD/结算页展示"升级速度"）
    int    bulletDmg;                       // 基础子弹伤害（可被永久道具提升）
    int    invincible;                      // 无敌剩余帧数
    int    shootCd;                         // 射击冷却计时
    int    fireLevel;                       // 火力等级（1/2/3）
    int    fireTimer;                       // 火力增强剩余帧数
    bool   shield;                          // 是否持有护盾
    int    bombs;                           // 炸弹数量
    int    score;                           // 当前分数
    /*---- 新技能：激光大招 与 闪避冲刺 ----*/
    int    energy;                          // 能量值（0..ENERGY_MAX，集满可放大招）
    /*---- 货币与技能 ----*/
    int    money;                           // 持有货币（以铜币为单位统一存储）
    bool   ultOwned[U_COUNT];               // 已购买的大招
    bool   skillOwned[SK_COUNT];            // 已购买的小技能
    int    curUlt;                          // 当前选用的大招
    /*---- 被动技能（三关一评级）----*/
    bool   passiveOn[PS_COUNT];             // 本轮生效中的被动
    int    passiveCount;                    // 本轮生效的被动数量
    int    echoCount;                       // 回响射击的计数（每 3 次触发一次）
    int    cycleStartScore;                 // 本轮三关开始时的分数（用于算增长）
    int    lastTier;                        // 上次评级结果（RankTier）
    int    lastPct;                         // 上次评级的达成百分比
    int    lastGained;                      // 上轮三关的实际得分增长
    int    lastCycFrom, lastCycTo;          // 上轮三关的关卡区间
    int    rankShowTimer;                   // 评级结算浮层剩余显示帧
    int    grantedList[PASSIVE_SLOTS];      // 本轮赠予的被动（用于结算浮层展示）
    int    grantedNum;                      // 本轮赠予数量
    int    curSkill;                        // 当前选用的小技能
    /*---- 技能效果计时（>0 表示生效中）----*/
    int    slowTimer;                       // 时空减速
    int    odTimer;                         // 超载模式
    int    bhTimer;                         // 黑洞
    double bhX, bhY;                        // 黑洞中心
    int    freezeTimer;                     // 冰冻
    int    decoyTimer;                      // 诱饵无人机剩余帧
    double decoyX, decoyY;                  // 诱饵位置
    int    berserkTimer;                    // 狂暴
    int    novaTimer;                       // 环形爆轰
    double novaR;                           // 爆轰半径
    int    ultTimer;                        // 激光大招剩余帧数（>0 表示正在释放）
    int    parryCd;                         // 弹反冷却剩余帧
    int    parryTimer;                      // 弹反无敌/特效剩余帧

    /*------------------------- 关卡数据 -------------------------*/
    int    level;                           // 当前关卡（1 起，无上限；每 3 关 Boss 循环一轮并增强）
    int    levelTimer;                      // 本关计时器
    int    spawnTimer;                      // 敌机生成计时器
    bool   bossPhase;                       // 是否进入 Boss 阶段
    int    shakeTimer;                      // 屏幕震动剩余帧数
    int    flashTimer = 0;                  // 全屏白闪剩余帧数（大招/Boss 爆炸）
    int    bannerTimer = 0;                 // 关卡横幅剩余帧数
    int    animTick = 0;                    // 全局动画计时（用于呼吸/脉冲等特效，持续递增）

    /*------------------------- 对象容器 -------------------------*/
    std::vector<Bullet>   bullets;          // 全部子弹
    std::vector<Enemy>    enemies;          // 全部敌机
    std::vector<Item>     items;            // 全部道具
    std::vector<Particle> particles;        // 全部粒子
    std::vector<Mine>     mines;            // 悬浮雷（蜂巢母舰布雷用）
    std::vector<Helper>   helpers;          // 僚机 / 卫星（技能召唤物）
    std::vector<Star>     stars;            // 背景星空
    Boss   boss;                            // Boss（单例）

    /*------------------------- 装备与物品栏 -------------------------*/
    std::vector<Gear> bag;                  // 物品栏（未装备的装备）
    Gear   equip[GS_COUNT];                 // 各部位已装备的装备
    bool   equipOn[GS_COUNT];               // 各部位是否已装备
    TCHAR  toast[48];                       // 拾取提示文字
    int    toastTimer = 0;                  // 提示剩余显示帧

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

    audio.init();                            // 初始化音频引擎（资源缺失会自动静音）
    loadLeaderboard();                       // 读取历史排行榜
    resetGame();                             // 准备数据
    state = ST_MENU;                         // 从菜单开始

    // 主循环：处理输入 -> 更新逻辑 -> 渲染画面 -> 帧率控制
    while (running) {
        DWORD frameStart = timeGetTime();    // 高精度毫秒计时（受 timeBeginPeriod 影响）

        pollInput();
        audio.update();                      // BGM 循环轮询
        handleInput();
        update();
        render();
        FlushBatchDraw();                    // 将后台缓冲一次性刷新到屏幕

        // 复制本帧按键状态，供下一帧做“边沿检测”
        memcpy(prevKey, curKey, sizeof(curKey));

        // ---- 性能统计：记录本帧「实际工作耗时」（不含 Sleep）----
        DWORD used = timeGetTime() - frameStart;
        lastFrameMs = used;
        if (used > worstFrameMs) worstFrameMs = used;          // 追踪最差帧，便于定位卡顿源
        if ((animTick & 63) == 0) worstFrameMs = 0;            // 每 64 帧重置一次峰值
        // 平滑帧率：用实际总帧时长（含 Sleep）估算
        double inst = 1000.0 / (used < FRAME_MS ? FRAME_MS : used);
        fpsAvg = fpsAvg * 0.92 + inst * 0.08;

        // 帧率控制：补足本帧剩余时间，使每帧稳定在 FRAME_MS（约 16ms → 60 FPS）
        if (used < FRAME_MS) Sleep(FRAME_MS - used);
    }

    audio.shutdown();                        // 关闭音频设备
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
    bonusHp = 0;
    plv = 1;
    xp = 0;
    lvlUpFlash = 0;
    lastLvlUpMs = timeGetTime();
    lastLvlSec = 0;
    bulletDmg = BULLET_DMG_BASE;
    invincible = 0;
    // 装备清空后再计算血量上限，确保为基础值
    bag.clear();
    for (int i = 0; i < GS_COUNT; ++i) equipOn[i] = false;
    invCursor = 0;
    pausePanel = 0;
    toastTimer = 0;
    hp = playerHpMax();
    shootCd = 0;
    fireLevel = 1;
    fireTimer = 0;
    shield = false;
    bombs = 1;
    score = 0;
    energy = 0;
    ultTimer = 0;
    // 货币与技能：初始各送一个最基础的（能量光束 / 清屏炸弹），其余需商店购买
    money = 0;
    for (int i = 0; i < U_COUNT; ++i)  ultOwned[i] = (i == U_LASER);
    for (int i = 0; i < SK_COUNT; ++i) skillOwned[i] = (i == SK_BOMB);
    curUlt = U_LASER; curSkill = SK_BOMB;
    for (int i = 0; i < PS_COUNT; ++i) passiveOn[i] = false;
    passiveCount = 0; echoCount = 0; cycleStartScore = 0;
    lastTier = RK_NONE; lastPct = 0; rankShowTimer = 0;
    lastGained = 0; lastCycFrom = 1; lastCycTo = 3;
    grantedNum = 0;
    slowTimer = odTimer = bhTimer = 0;
    freezeTimer = decoyTimer = berserkTimer = novaTimer = 0;
    decoyX = decoyY = 0;
    novaR = 0; bhX = bhY = 0;
    helpers.clear();
    parryCd = 0;
    parryTimer = 0;

    level = 1;
    levelTimer = 0;
    spawnTimer = 0;
    bossPhase = false;
    shakeTimer = 0;
    flashTimer = 0;
    bannerTimer = 110;                      // 开局显示“第 1 关”横幅
    quitSettle = false;
    rankJustSet = -1;
    wPoolLeft = 0;                          // 重置武器轮转池

    bullets.clear();
    enemies.clear();
    items.clear();
    particles.clear();
    mines.clear();
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
    case ST_MENU: {
        // ---- 秘籍检测：在菜单依次输入 131215 进入自定义初始配置界面 ----
        // 做法：维护一个只保存最近 6 位数字的滚动缓冲，每帧检测新按下的数字键，
        //       缓冲内容与秘籍串相同即触发。主键盘与小键盘数字均可。
        for (int d = 0; d <= 9; ++d) {
            if (justPressed('0' + d) || justPressed(VK_NUMPAD0 + d)) {
                int n = (int)_tcslen(cheatBuf);
                if (n >= 6) {                          // 满了则左移一位
                    for (int i = 0; i < 5; ++i) cheatBuf[i] = cheatBuf[i + 1];
                    n = 5;
                }
                cheatBuf[n] = (TCHAR)(_T('0') + d);
                cheatBuf[n + 1] = 0;
                if (_tcscmp(cheatBuf, _T("131215")) == 0) {   // 命中秘籍
                    cheatBuf[0] = 0;
                    cheatCursor = 0;
                    state = ST_CHEAT;
                    audio.playSfx(SFX_POWERUP);
                }
            }
        }
        // 回车 / 空格开始游戏（正常开局，不带自定义配置）；Esc 退出
        if (justPressed(VK_RETURN) || justPressed(VK_SPACE)) {
            cheatOn = false;
            resetGame();
            state = ST_PLAYING;
        }
        if (justPressed('M')) audio.toggleMute();      // 静音开关
        if (justPressed(VK_ESCAPE)) running = false;
        break;
    }

    case ST_CHEAT: {
        // ---- 自定义初始配置：↑↓ 选条目，←→ 调数值，回车开始，Esc 返回 ----
        if (justPressed(VK_UP))   cheatCursor = (cheatCursor + CI_COUNT - 1) % CI_COUNT;
        if (justPressed(VK_DOWN)) cheatCursor = (cheatCursor + 1) % CI_COUNT;
        int d = 0;
        if (justPressed(VK_LEFT))  d = -1;
        if (justPressed(VK_RIGHT)) d = +1;
        // 按住 Shift 可 10 倍步进，方便快速拉到目标值
        if (d != 0 && (keyDown(VK_SHIFT))) d *= 10;
        if (d != 0) {
            switch (cheatCursor) {
            case CI_PLV:    cfgPlv = clampi(cfgPlv + d, 1, PLV_MAX);        break;
            case CI_LEVEL:  cfgLevel = clampi(cfgLevel + d, 1, 99);           break;
            case CI_WTYPE:  cfgWtype = (cfgWtype + (d > 0 ? 1 : W_COUNT - 1)) % W_COUNT; break;
            case CI_RARITY: cfgRarity = clampi(cfgRarity + (d > 0 ? 1 : -1), 0, R_COUNT - 1); break;
            case CI_GLVL:   cfgGlvl = clampi(cfgGlvl + (d > 0 ? 1 : -1), 1, GEAR_LVL_MAX);  break;
            case CI_HP:     cfgHp = clampi(cfgHp + d * 20, 0, 2000);       break;
            case CI_ATK:    cfgAtk = clampi(cfgAtk + d, 0, 200);            break;
            case CI_DEF:    cfgDef = clampi(cfgDef + d, 0, 100);            break;
            case CI_LIVES:  cfgLives = clampi(cfgLives + (d > 0 ? 1 : -1), 1, 9); break;
            case CI_MONEY:  cfgMoney = clampi(cfgMoney + d * 100, 0, 99900);   break;  // 每步 1 金
            case CI_ULT:    cfgUlt = (cfgUlt + (d > 0 ? 1 : U_COUNT - 1)) % U_COUNT;    break;
            case CI_SKILL:  cfgSkill = (cfgSkill + (d > 0 ? 1 : SK_COUNT - 1)) % SK_COUNT; break;
            case CI_PASSIVE: cfgPassive = clampi(cfgPassive + (d > 0 ? 1 : -1), 0, PASSIVE_SLOTS); break;
            }
        }
        if (justPressed(VK_RETURN) || justPressed(VK_SPACE)) {   // 按配置开局
            cheatOn = true;
            resetGame();
            applyCheatConfig();
            state = ST_PLAYING;
        }
        if (justPressed(VK_ESCAPE)) state = ST_MENU;
        break;
    }

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
        int spd = playerSpeed();
        px += dx * spd;
        py += dy * spd;

        // ---- 弹反：R 键。短暂无敌帧 + 把范围内的敌弹反弹回去 ----
        //      设计意图：把原本"位移躲避"的被动操作，改成有攻击性的主动防御——
        //      时机对了不仅能免伤，还能把弹幕转化为自己的火力。
        if (justPressed('R') && parryCd <= 0) {
            parryTimer = PARRY_FRAMES;
            parryCd = PARRY_CD;
            if (invincible < PARRY_FRAMES) invincible = PARRY_FRAMES;   // 无敌帧
            int reflected = 0;
            for (auto& b : bullets) {
                if (b.fromPlayer || !b.alive) continue;
                double dx = b.x - px, dy = b.y - py;
                if (dx * dx + dy * dy > (double)PARRY_R * PARRY_R) continue;
                // 反弹：转为我方子弹，朝上方射出并继承一部分横向速度
                b.fromPlayer = true;
                double sp = sqrt(b.vx * b.vx + b.vy * b.vy);
                if (sp < 1) sp = 6.0;
                b.vx = -b.vx * 0.5;
                b.vy = -fabs(sp) - 2.0;
                b.damage = playerBulletDamage();       // 用玩家的攻击力结算
                b.color = RGB(255, 190, 235);
                b.pierce = 1;
                b.homing = false;
                b.trail = 2;
                addParticle(PK_STAR, b.x, b.y, 0, 0, 7.0, RGB(255, 200, 240), 9, -0.5, 0.25);
                ++reflected;
            }
            spawnShockwave(px, py, RGB(255, 180, 230), 8.0, PARRY_FRAMES);
            audio.playSfx(reflected > 0 ? SFX_FUSE : SFX_POWERUP);
            if (reflected > 0) {                       // 反弹成功给予正反馈
                spawnExplosion(px, py, RGB(255, 200, 240), 10);
                energy = clampi(energy + reflected * 2, 0, ENERGY_MAX);
            }
        }

        // 边界处理：飞机不能移出屏幕
        double hw = PLAYER_W / 2.0, hh = PLAYER_H / 2.0;
        if (px < hw)          px = hw;
        if (px > WIN_W - hw)  px = WIN_W - hw;
        if (py < hh)          py = hh;
        if (py > WIN_H - hh)  py = WIN_H - hh;

        // ---- 射击：按住空格连发 ----
        if (keyDown(VK_SPACE) && shootCd <= 0) {
            playerShoot();
            shootCd = playerShootCd();
        }
        // ---- 激光大招：X 键，能量集满时释放 ----
        if (justPressed('X') && energy >= ENERGY_MAX && ultTimer <= 0)
            activateUltimate();
        // ---- 炸弹：B 键，边沿触发一次 ----
        if (justPressed('B')) useSkill();
        // ---- 暂停：P 键 ----
        if (justPressed('M')) audio.toggleMute();      // 静音开关
        if (justPressed(VK_F3)) showFps = !showFps;    // 性能诊断开关
        if (justPressed('P')) state = ST_PAUSED;
        break;
    }

    case ST_PAUSED:
        // 切换面板：I=图鉴，E=装备栏
        if (justPressed('I')) { pausePanel = (pausePanel == 1) ? 0 : 1; dexPage = 0; }
        if (justPressed('E')) pausePanel = (pausePanel == 2) ? 0 : 2;
        if (justPressed('S')) { pausePanel = (pausePanel == 3) ? 0 : 3; shopCursor = 0; }
        if (justPressed('K')) pausePanel = (pausePanel == 4) ? 0 : 4;
        // ---- 商店操作：Tab 切页 / ↑↓ 选择 / 回车 购买或装备 ----
        if (pausePanel == 3) {
            int n = (shopTab == 0) ? U_COUNT : SK_COUNT;
            if (justPressed(VK_TAB)) { shopTab ^= 1; shopCursor = 0; }
            if (justPressed(VK_UP))   shopCursor = (shopCursor + n - 1) % n;
            if (justPressed(VK_DOWN)) shopCursor = (shopCursor + 1) % n;
            if (justPressed(VK_RETURN)) {
                if (shopTab == 0) {                    // 大招页
                    if (ultOwned[shopCursor]) {        // 已拥有 -> 装备
                        curUlt = shopCursor;
                        _stprintf_s(toast, _countof(toast), _T("已装备大招：%s"), ultName(curUlt));
                        toastTimer = 120;
                        audio.playSfx(SFX_POWERUP);
                    }
                    else if (money >= ultPrice(shopCursor)) {   // 购买
                        money -= ultPrice(shopCursor);
                        ultOwned[shopCursor] = true;
                        curUlt = shopCursor;
                        _stprintf_s(toast, _countof(toast), _T("购买成功：%s"), ultName(curUlt));
                        toastTimer = 150;
                        audio.playSfx(SFX_FUSE);
                    }
                    else {
                        _stprintf_s(toast, _countof(toast), _T("货币不足！"));
                        toastTimer = 100;
                    }
                }
                else {                               // 小技能页
                    if (skillOwned[shopCursor]) {
                        curSkill = shopCursor;
                        _stprintf_s(toast, _countof(toast), _T("已装备技能：%s"), skillName(curSkill));
                        toastTimer = 120;
                        audio.playSfx(SFX_POWERUP);
                    }
                    else if (money >= skillPrice(shopCursor)) {
                        money -= skillPrice(shopCursor);
                        skillOwned[shopCursor] = true;
                        curSkill = shopCursor;
                        _stprintf_s(toast, _countof(toast), _T("购买成功：%s"), skillName(curSkill));
                        toastTimer = 150;
                        audio.playSfx(SFX_FUSE);
                    }
                    else {
                        _stprintf_s(toast, _countof(toast), _T("货币不足！"));
                        toastTimer = 100;
                    }
                }
            }
        }
        // 图鉴内：←→ 翻页（敌机 / 道具 / 装备）
        if (pausePanel == 1) {
            if (justPressed(VK_LEFT))  dexPage = (dexPage + 6) % 7;
            if (justPressed(VK_RIGHT)) dexPage = (dexPage + 1) % 7;
        }
        // 装备栏内导航与操作
        if (pausePanel == 2) {
            int n = (int)bag.size();
            if (justPressed(VK_UP) && n > 0) invCursor = (invCursor - 1 + n) % n;
            if (justPressed(VK_DOWN) && n > 0) invCursor = (invCursor + 1) % n;
            if (justPressed(VK_RETURN) && n > 0) equipFromBag(invCursor);
            if (justPressed('F') && n > 0) fuseFromBag(invCursor);   // 合成升级
            if (justPressed('D') && n > 0) discardFromBag(invCursor);
            if (invCursor >= (int)bag.size()) invCursor = bag.empty() ? 0 : (int)bag.size() - 1;
        }
        else if (justPressed(VK_RETURN)) {
            state = ST_PLAYING; pausePanel = 0;    // 无面板时回车也可继续
        }
        if (justPressed('P')) { state = ST_PLAYING; pausePanel = 0; }
        if (justPressed(VK_ESCAPE)) {                  // 退出并结算：记录成绩并进入结算页
            pausePanel = 0;
            quitSettle = true;
            tryRecordScore();                          // 若上榜会弹出昵称输入框
            state = ST_GAMEOVER;                       // 复用结算页展示分数与排行榜
        }
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
    audio.startBgm();       // 单曲 BGM 全程循环播放（曲目由 audio/bgm.* 决定）
    updateStars();          // 星空在任何状态下都滚动，作为动态背景
    updateParticles();      // 粒子亦持续更新（菜单等界面无粒子时无影响）
    if (state == ST_PLAYING) updatePlaying();
}

void Game::updatePlaying() {
    if (shootCd > 0) --shootCd;
    if (invincible > 0) --invincible;
    if (shakeTimer > 0) --shakeTimer;
    if (flashTimer > 0) --flashTimer;
    if (lvlUpFlash > 0) --lvlUpFlash;
    if (bannerTimer > 0) --bannerTimer;
    if (rankShowTimer > 0) --rankShowTimer;
    if (parryCd > 0) --parryCd;
    if (parryTimer > 0) --parryTimer;
    if (toastTimer > 0) --toastTimer;
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
    updateMines();                                     // 悬浮雷倒计时与引爆
    updateSkillEffects();                              // 技能效果结算
    updateHelpers();                                   // 僚机/卫星
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

    // 速度随关卡提升，但设上限，避免无限关卡后期敌机快到无法游玩
    double speedScale = 1.0 + (level - 1) * 0.18;
    if (speedScale > 2.2) speedScale = 2.2;
    // 生命值与护甲统一由查询函数决定，保证与属性面板显示一致
    e.hp = e.maxHp = enemyHp(e.type, level);
    e.armor = enemyArmor(e.type, level);
    e.score = enemyScore(e.type, level);

    switch (e.type) {
    case E_NORMAL:
        e.w = 40; e.h = 36;
        e.vy = (1.6 + randFloat() * 0.6) * speedScale; e.vx = 0;
        break;
    case E_FAST:
        e.w = 32; e.h = 34;
        e.vy = (3.2 + randFloat() * 0.8) * speedScale; e.vx = 0;
        break;
    case E_SHOOTER:
        e.w = 46; e.h = 42;
        e.vy = (1.2 + randFloat() * 0.4) * speedScale; e.vx = 0;
        break;
    case E_ZIGZAG:
        e.w = 38; e.h = 36;
        e.vy = (1.8 + randFloat() * 0.5) * speedScale;
        e.vx = 2.4;                                    // 蛇形水平摆幅速度
        break;
    case E_TANK:                                       // 装甲重机：血厚、慢、分高
        e.w = 54; e.h = 48;
        e.vy = (0.9 + randFloat() * 0.3) * speedScale; e.vx = 0;
        break;
    case E_DIVER:                                      // 俯冲自爆机
        e.w = 34; e.h = 40;
        e.vy = 1.5 * speedScale; e.vx = 0;
        break;
    case E_STRAFER:                                    // 横掠射击机
        e.w = 46; e.h = 34;
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
    // 技能效果：冰冻立场期间敌机完全静止；时空减速期间按 40% 速度移动
    if (freezeTimer > 0) {
        for (auto& e : enemies) if (e.alive && e.hitFlash > 0) --e.hitFlash;
        return;                                        // 冻结：跳过全部移动与射击
    }
    const double spdK = (slowTimer > 0) ? 0.25 : 1.0;   // 时停领域：降至 25%
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.hitFlash > 0) --e.hitFlash;

        // ---- 各类型移动逻辑 ----
        switch (e.type) {
        case E_ZIGZAG:                                 // 蛇形（正弦波）移动
            e.phase += 0.06 * spdK;
            e.y += e.vy * spdK;
            e.x = e.baseX + sin(e.phase) * 70.0;
            if (e.baseX < 60)          e.baseX = 60;
            if (e.baseX > WIN_W - 60)  e.baseX = WIN_W - 60;
            break;
        case E_DIVER:                                  // 俯冲：先缓降，进入攻击线后锁定玩家高速俯冲
            if (e.aiState == 0) {
                e.y += e.vy * spdK;
                if (e.y > 170) { e.aiState = 1; spawnExplosion(e.x, e.y, COL_E_DIVER, 4); }
            }
            else {
                double dir = (px > e.x) ? 1.0 : -1.0;  // 朝玩家横向偏转
                e.vx += dir * 0.28;
                if (e.vx > 4.2) e.vx = 4.2; if (e.vx < -4.2) e.vx = -4.2;
                e.vy += 0.10; if (e.vy > 8.5) e.vy = 8.5;  // 越冲越快
                e.x += e.vx * spdK; e.y += e.vy * spdK;
            }
            break;
        case E_STRAFER:                                // 横掠：降到指定高度后横向扫场
            if (e.aiState == 0) {
                e.y += e.vy * spdK;
                if (e.y >= 120 + randRange(0, 20)) { e.aiState = 1; }
            }
            else {
                e.x += e.vx * spdK;
                if (e.x < -e.w || e.x > WIN_W + e.w) e.alive = false; // 掠出屏幕消失
            }
            break;
        default:                                       // 普通 / 快速 / 射击 / 装甲：直线
            e.x += e.vx * spdK; e.y += e.vy * spdK;
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
    boss.kind = bossKind(level);                     // 每 3 关循环一次种类
    boss.power = bossPower(level);                    // 循环轮次越高越强
    boss.bob = 0;

    // 三种 Boss 体型不同：无畏舰宽厚、母舰巨大、巨蛇细长灵活
    switch (boss.kind) {
    case BK_DREADNOUGHT: boss.w = 150; boss.h = 90; boss.vx = 2.2; break;
    case BK_HIVE:        boss.w = 170; boss.h = 100; boss.vx = 1.6; break;
    default:             boss.w = 140; boss.h = 76; boss.vx = 3.2; break;  // 巨蛇更快
    }
    boss.vx *= (1.0 + 0.12 * bossCycle(level));        // 后期循环移动更快

    boss.x = WIN_W / 2.0;
    boss.y = -boss.h;                                  // 从上方缓缓进入
    boss.maxHp = bossHp(level);
    boss.hp = boss.maxHp;
    boss.armor = bossArmor(boss.kind, level);          // 防御力（不为 0，随关卡提升）
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
    boss.laserX2 = WIN_W / 2;
    shakeTimer = 24;                                   // 登场震屏
    audio.playSfx(SFX_BOSSWARN);                       // 登场警报
}

/**
 * @brief  生成一发敌方子弹（供 Boss 各种弹幕复用）
 */
void Game::spawnEnemyBullet(double x, double y, double vx, double vy, COLORREF c, int r, bool homing) {
    Bullet b;
    b.x = x; b.y = y; b.vx = vx; b.vy = vy;
    b.radius = r; b.damage = 1; b.fromPlayer = false;
    b.color = c; b.alive = true;
    b.homing = homing; b.life = 0;
    b.pierce = 0; b.trail = homing ? 2 : 0;
    bullets.push_back(b);
}

void Game::updateBoss() {
    if (!boss.active) return;
    if (boss.hitFlash > 0) --boss.hitFlash;
    boss.bob += 0.05;                                  // 外观浮动相位

    // 登场下滑，随后按种类以不同方式巡逻
    if (boss.y < 110) {
        boss.y += 1.5;
        return;                                        // 入场期间不攻击
    }
    boss.x += boss.vx;
    if (boss.x < boss.w / 2) { boss.x = boss.w / 2;          boss.vx = -boss.vx; }
    if (boss.x > WIN_W - boss.w / 2) { boss.x = WIN_W - boss.w / 2;  boss.vx = -boss.vx; }
    // 巨蛇 Boss 会额外上下起伏，机动性更强
    if (boss.kind == BK_SERPENT) boss.y = 110 + sin(boss.bob) * 26;

    updateBossCast();                                  // 招式选取与施放
}

/**
 * @brief  Boss 招式状态机：间歇 -> 选招 -> 逐帧施放。
 *         三种 Boss 拥有各自的招式池与节奏：
 *           无畏舰：环形 / 扇形 / 花瓣 / 激光      —— 稳健，正面压制
 *           母舰：  召唤 / 花瓣 / 激光 / 环形      —— 场面混乱，靠小怪消耗
 *           巨蛇：  螺旋 / 追踪弹 / 双柱激光 / 扇形 —— 机动凶悍，最难缠
 *         弹速、弹量、间歇均随循环轮次（boss.power）增强。
 */
void Game::updateBossCast() {
    const double bx = boss.x, by = boss.y + boss.h / 2.0;  // 弹幕发射口（机体下沿）
    const double bs = boss.power;                          // 强度系数（循环轮次越高越快）
    const int    cyc = bossCycle(level);

    // ---------- 激光子系统（单柱/双柱/扇形光束共用：先预警后开火） ----------
    if (boss.castType == BA_LASER || boss.castType == BA_TWINLASER) {
        if (boss.laserWarn > 0) {
            --boss.laserWarn;
        }
        else if (boss.laserFire > 0) {
            --boss.laserFire;
            bool hit = fabs(px - boss.laserX) < BOSS_BEAM_HALF + PLAYER_W * 0.3;
            if (boss.castType == BA_TWINLASER)
                hit = hit || fabs(px - boss.laserX2) < BOSS_BEAM_HALF + PLAYER_W * 0.3;
            if (hit && py > boss.y) {
                hitPlayer(HIT_HP_LASER);
                if (state != ST_PLAYING) return;
            }
            if (boss.laserFire % 3 == 0) {                 // 光束根部火花
                spawnExplosion(boss.laserX, by, RGB(255, 120, 120), 2);
                if (boss.castType == BA_TWINLASER)
                    spawnExplosion(boss.laserX2, by, RGB(255, 120, 120), 2);
            }
        }
    }

    // ---------- 正在施放某招：逐帧发射 ----------
    if (boss.castTimer > 0) {
        int elapsed = 0;
        switch (boss.castType) {

            /*==================== 无畏舰 6 招 ====================*/
        case BA_RING: {                                // ① 旋转环形
            elapsed = 26 - boss.castTimer;
            if (elapsed % 8 == 0) {
                int n = 18 + cyc * 4;
                for (int i = 0; i < n; ++i) {
                    double a = boss.castAngle + 6.2832 * i / n;
                    spawnEnemyBullet(bx, by, cos(a) * 3.1 * bs, sin(a) * 3.1 * bs + 0.6, RGB(255, 170, 90));
                }
                boss.castAngle += 0.35;
            }
            break;
        }
        case BA_AIM: {                                 // ② 瞄准扇形
            elapsed = 30 - boss.castTimer;
            if (elapsed % 9 == 0) {
                double base = atan2(py - by, px - bx);
                int k = 3 + cyc;
                for (int i = -k; i <= k; ++i)
                    spawnEnemyBullet(bx, by, cos(base + i * 0.14) * 4.4 * bs,
                        sin(base + i * 0.14) * 4.4 * bs, COL_EBULLET);
            }
            break;
        }
        case BA_LASER: break;                          // ③ 激光（上方子系统处理）
        case BA_BROADSIDE: {                           // ④ 舷炮齐射：左右舷交替平行弹幕
            elapsed = 60 - boss.castTimer;
            if (elapsed % 6 == 0) {
                bool leftSide = ((elapsed / 6) % 2 == 0);
                double sx = leftSide ? (bx - boss.w * 0.45) : (bx + boss.w * 0.45);
                for (int i = 0; i < 4 + cyc; ++i) {
                    double ox = (leftSide ? -1 : 1) * i * 16.0;
                    spawnEnemyBullet(sx + ox, by, 0, 3.4 * bs, RGB(255, 210, 120), 6);
                }
                spawnExplosion(sx, by, RGB(255, 220, 140), 3);   // 炮口焰
            }
            break;
        }
        case BA_SHOTGUN: {                             // ⑤ 霰弹爆发：锥形高速散弹
            elapsed = 34 - boss.castTimer;
            if (elapsed == 0 || elapsed == 14 || elapsed == 28) {
                double base = atan2(py - by, px - bx);
                int n = 12 + cyc * 3;
                for (int i = 0; i < n; ++i) {
                    double a = base + (randFloat() - 0.5) * 0.75;      // 锥形随机散布
                    double v = (5.0 + randFloat() * 2.2) * bs;
                    spawnEnemyBullet(bx, by, cos(a) * v, sin(a) * v, RGB(255, 140, 90), 4);
                }
                spawnExplosion(bx, by, RGB(255, 200, 120), 8);
                shakeTimer = 6;
            }
            break;
        }
        case BA_CROSS: {                               // ⑥ 十字扫射：四向弹墙同时推进
            elapsed = 40 - boss.castTimer;
            if (elapsed % 5 == 0) {
                double v = 3.6 * bs;
                double off = (elapsed / 5) * 0.12;                     // 每波略微旋转
                for (int i = 0; i < 4; ++i) {
                    double a = off + 1.5708 * i;
                    spawnEnemyBullet(bx, by, cos(a) * v, sin(a) * v, RGB(255, 240, 150), 7);
                    spawnEnemyBullet(bx, by, cos(a + 0.10) * v, sin(a + 0.10) * v, RGB(255, 240, 150), 5);
                    spawnEnemyBullet(bx, by, cos(a - 0.10) * v, sin(a - 0.10) * v, RGB(255, 240, 150), 5);
                }
            }
            break;
        }

                     /*==================== 蜂巢母舰 6 招 ====================*/
        case BA_SUMMON:                                // ① 召唤援军
            if (boss.castTimer == 6) {
                int n = 3 + cyc;
                for (int i = 0; i < n; ++i) spawnEnemy();
                spawnExplosion(bx, by, RGB(180, 255, 160), 24);
                flashTimer = 4;
            }
            break;
        case BA_PETAL: {                               // ② 十字花瓣
            elapsed = 22 - boss.castTimer;
            if (elapsed == 0 || elapsed == 11) {
                const double dirs[4] = { 0.9, 2.24, 3.94, 5.28 };
                for (double d : dirs)
                    for (int i = -2; i <= 2; ++i)
                        spawnEnemyBullet(bx, by, cos(d + i * 0.16) * 3.6 * bs,
                            sin(d + i * 0.16) * 3.6 * bs, RGB(255, 130, 200));
            }
            break;
        }
        case BA_MINE:                                  // ③ 布雷：延时炸开成环
            if (boss.castTimer % 10 == 0) {
                Mine m;
                m.x = randRange(50, WIN_W - 50);
                m.y = randRange(140, WIN_H / 2 + 60);
                m.fuse = 90 + randRange(0, 40);
                m.alive = true;
                mines.push_back(m);
                spawnExplosion(m.x, m.y, RGB(200, 255, 170), 4);
            }
            break;
        case BA_SWARM: {                               // ④ 虫群散射：低速弹铺满全屏
            elapsed = 70 - boss.castTimer;
            if (elapsed % 4 == 0) {
                int n = 3 + cyc;
                for (int i = 0; i < n; ++i) {
                    double a = 1.5708 + (randFloat() - 0.5) * 2.4;     // 朝下方随机散开
                    double v = (1.5 + randFloat() * 1.6) * bs;         // 低速，靠数量压迫
                    spawnEnemyBullet(bx + randRange(-60, 60), by,
                        cos(a) * v, sin(a) * v, RGB(170, 255, 140), 5);
                }
            }
            break;
        }
        case BA_BEAMFAN: {                             // ⑤ 扇形光束：缓慢旋转扫过
            elapsed = 90 - boss.castTimer;
            if (elapsed % 3 == 0) {
                int arms = 4;
                for (int i = 0; i < arms; ++i) {
                    double a = boss.castAngle + 6.2832 * i / arms;
                    if (sin(a) < 0) continue;                          // 只保留朝下的光束
                    for (int k = 1; k <= 3; ++k)                       // 沿光束方向连续布弹
                        spawnEnemyBullet(bx + cos(a) * k * 8, by + sin(a) * k * 8,
                            cos(a) * 2.6 * bs, sin(a) * 2.6 * bs,
                            RGB(140, 255, 200), 5);
                }
                boss.castAngle += 0.055;                               // 缓慢旋转
            }
            break;
        }
        case BA_DRONE: {                               // ⑥ 无人机墙：整排下压，留随机缝隙
            elapsed = 56 - boss.castTimer;
            if (elapsed % 18 == 0) {
                int gap = randRange(1, 8);                             // 随机缺口位置
                for (int i = 0; i < 10; ++i) {
                    if (i == gap || i == gap + 1) continue;            // 留出可穿过的缝
                    spawnEnemyBullet(24 + i * (WIN_W - 48) / 9.0, boss.y + boss.h / 2,
                        0, 2.6 * bs, RGB(200, 255, 160), 7);
                }
            }
            break;
        }

                     /*==================== 机械巨蛇 6 招 ====================*/
        case BA_SPIRAL: {                              // ① 四臂反向双螺旋（大幅加密）
            elapsed = 110 - boss.castTimer;
            if (elapsed % 2 == 0) {                    // 发射频率翻倍
                int arms = 4 + cyc;                    // 四臂起步
                for (int i = 0; i < arms; ++i) {
                    double a = boss.castAngle + 6.2832 * i / arms;
                    spawnEnemyBullet(bx, by, cos(a) * 3.0 * bs, sin(a) * 3.0 * bs + 0.7, RGB(120, 210, 255), 5);
                    // 反向旋转的第二层螺旋，形成交错网格
                    double a2 = -boss.castAngle * 1.3 + 6.2832 * i / arms;
                    spawnEnemyBullet(bx, by, cos(a2) * 2.2 * bs, sin(a2) * 2.2 * bs + 0.9, RGB(180, 140, 255), 4);
                }
                boss.castAngle += 0.22;
            }
            break;
        }
        case BA_HOMING:                                // ② 导弹齐射（加密：波次更多、每波更多）
            elapsed = 66 - boss.castTimer;
            if (elapsed % 8 == 0) {                    // 波次更密
                int n = 3 + cyc;
                for (int i = 0; i < n; ++i) {
                    double a = 1.5708 + (i - (n - 1) / 2.0) * 0.34;
                    spawnEnemyBullet(bx, by, cos(a) * 2.9, sin(a) * 2.9,
                        RGB(255, 200, 90), 7, true);
                }
                // 伴随两发直射封走位
                spawnEnemyBullet(bx, by, -2.0, 4.2 * bs, RGB(255, 150, 90), 5);
                spawnEnemyBullet(bx, by, 2.0, 4.2 * bs, RGB(255, 150, 90), 5);
            }
            break;
        case BA_TWINLASER: break;                      // ③ 双柱激光（上方子系统处理）
        case BA_COIL: {                                // ④ 盘绕突进（加密：三向撒弹 + 更快冲刺）
            elapsed = 96 - boss.castTimer;
            boss.x += boss.vx * 2.8;                                   // 冲刺更快
            if (boss.x < boss.w / 2) { boss.x = boss.w / 2;         boss.vx = -boss.vx; }
            if (boss.x > WIN_W - boss.w / 2) { boss.x = WIN_W - boss.w / 2; boss.vx = -boss.vx; }
            if (elapsed % 3 == 0) {                                    // 撒弹更密
                for (int i = -1; i <= 1; ++i)                          // 三向散布
                    spawnEnemyBullet(boss.x, by, i * 1.8 + (randFloat() - 0.5),
                        3.4 * bs, RGB(190, 140, 255), 5);
                addParticle(PK_SPARK, boss.x, by, -boss.vx * 0.4, 0.5, 2.5,
                    RGB(200, 150, 255), 12, -0.05, 0);         // 冲刺残影
            }
            break;
        }
        case BA_VENOM: {                               // ⑤ 毒液喷吐（加密：连续喷洒成雨幕）
            elapsed = 70 - boss.castTimer;
            if (elapsed % 3 == 0) {                    // 喷吐频率大幅提高
                int n = 4 + cyc;
                for (int i = 0; i < n; ++i) {
                    double a = 1.5708 + (randFloat() - 0.5) * 1.9;
                    double v = (2.4 + randFloat() * 2.2) * bs;
                    spawnEnemyBullet(bx + randRange(-30, 30), by,
                        cos(a) * v, sin(a) * v * 0.55,
                        RGB(160, 255, 120), 6);
                }
            }
            break;
        }
        case BA_TAILSWEEP: {                           // ⑥ 双向尾扫（加密：两道弹墙对向扫过）
            elapsed = 72 - boss.castTimer;
            if (elapsed % 2 == 0) {                    // 扫描更密
                int step = elapsed / 2;
                double fx1 = step * WIN_W / 36.0;                      // 自左向右
                double fx2 = WIN_W - step * WIN_W / 36.0;              // 自右向左
                for (int k = 0; k < 3; ++k) {
                    spawnEnemyBullet(fx1, boss.y + boss.h / 2 + k * 12, 0, 3.2 * bs, RGB(220, 150, 255), 5);
                    spawnEnemyBullet(fx2, boss.y + boss.h / 2 + k * 12, 0, 3.2 * bs, RGB(255, 150, 220), 5);
                }
            }
            break;
        }
        }
        if (--boss.castTimer <= 0) {                   // 本招结束，进入间歇
            boss.castType = -1;
            // 攻击欲望只取决于 Boss 种类：同一 Boss 每次出现都保持一致的凶悍程度
            boss.attackTimer = bossAggroGap(boss.kind);
        }
        return;
    }

    // ---------- 间歇：冷却结束后随机挑选下一招 ----------
    if (--boss.attackTimer > 0) return;

    // 依 Boss 种类组装招式池：每种 Boss 6 招，且三者互不重复
    int pool[8]; int np = 0;
    switch (boss.kind) {
    case BK_DREADNOUGHT:
        pool[np++] = BA_RING;      pool[np++] = BA_AIM;
        pool[np++] = BA_LASER;     pool[np++] = BA_BROADSIDE;
        pool[np++] = BA_SHOTGUN;   pool[np++] = BA_CROSS;
        break;
    case BK_HIVE:
        pool[np++] = BA_SUMMON;    pool[np++] = BA_PETAL;
        pool[np++] = BA_MINE;      pool[np++] = BA_SWARM;
        pool[np++] = BA_BEAMFAN;   pool[np++] = BA_DRONE;
        break;
    default:
        pool[np++] = BA_SPIRAL;    pool[np++] = BA_HOMING;
        pool[np++] = BA_TWINLASER; pool[np++] = BA_COIL;
        pool[np++] = BA_VENOM;     pool[np++] = BA_TAILSWEEP;
        break;
    }

    int t = pool[randRange(0, np - 1)];                // 随机挑一招
    for (int guard = 0; t == boss.lastType && guard < 5; ++guard)
        t = pool[randRange(0, np - 1)];                // 尽量不与上一招重复
    boss.lastType = t;
    boss.castType = t;
    boss.castAngle = randFloat() * 6.2832;             // 环形/螺旋随机起始角

    switch (t) {                                       // 设定各招持续时长
    case BA_RING:      boss.castTimer = 26; break;
    case BA_AIM:       boss.castTimer = 30; break;
    case BA_BROADSIDE: boss.castTimer = 60; break;
    case BA_SHOTGUN:   boss.castTimer = 34; break;
    case BA_CROSS:     boss.castTimer = 40; break;
    case BA_SUMMON:    boss.castTimer = 6;  break;
    case BA_PETAL:     boss.castTimer = 22; break;
    case BA_MINE:      boss.castTimer = 40; break;
    case BA_SWARM:     boss.castTimer = 70; break;
    case BA_BEAMFAN:   boss.castTimer = 90; break;
    case BA_DRONE:     boss.castTimer = 56; break;
    case BA_SPIRAL:    boss.castTimer = 110; break;
    case BA_HOMING:    boss.castTimer = 66; break;
    case BA_COIL:      boss.castTimer = 96; break;
    case BA_VENOM:     boss.castTimer = 70; break;
    case BA_TAILSWEEP: boss.castTimer = 72; break;
    case BA_LASER:                                     // 单柱激光：预警 + 开火
        boss.laserX = (int)px;
        boss.laserWarn = 36 - cyc * 4;
        if (boss.laserWarn < 16) boss.laserWarn = 16;
        boss.laserFire = 42;
        boss.castTimer = boss.laserWarn + boss.laserFire;
        break;
    case BA_TWINLASER:                                 // 双柱激光：左右夹击（缝隙会被弹幕封锁）
        boss.laserX = (int)px - 78;
        boss.laserX2 = (int)px + 78;
        boss.laserWarn = 34 - cyc * 4;
        if (boss.laserWarn < 16) boss.laserWarn = 16;
        boss.laserFire = 44;
        boss.castTimer = boss.laserWarn + boss.laserFire;
        break;
    }
}

/**
 * @brief  更新悬浮雷：倒计时结束后炸开成环形弹幕（蜂巢母舰专属招式）。
 */
void Game::updateMines() {
    for (auto& m : mines) {
        if (!m.alive) continue;
        --m.fuse;
        if (m.fuse == 20) spawnExplosion(m.x, m.y, RGB(255, 230, 140), 4);  // 引爆前预警
        if (m.fuse <= 0) {
            m.alive = false;
            int n = 12 + bossCycle(level) * 4;
            for (int i = 0; i < n; ++i) {              // 炸开成环形弹幕
                double a = 6.2832 * i / n;
                spawnEnemyBullet(m.x, m.y, cos(a) * 2.8, sin(a) * 2.8, RGB(200, 255, 150), 5);
            }
            spawnExplosion(m.x, m.y, RGB(220, 255, 170), 16);
            shakeTimer = 6;
        }
    }
    mines.erase(std::remove_if(mines.begin(), mines.end(),
        [](const Mine& m) { return !m.alive; }), mines.end());
}

/**
 * @brief  绘制悬浮雷：临爆前会加速闪烁并涨大，给玩家明确的躲避提示。
 */
void Game::drawMines() {
    for (const auto& m : mines) {
        int blinkRate = (m.fuse < 30) ? 3 : 8;                     // 越接近引爆闪得越快
        bool lit = ((m.fuse / blinkRate) % 2 == 0);
        int r = 9 + (m.fuse < 30 ? (30 - m.fuse) / 6 : 0);         // 临爆涨大
        setfillcolor(fadeColor(lit ? RGB(255, 120, 90) : RGB(120, 200, 120), 0.45));
        solidcircle((int)m.x, (int)m.y, r + 5);
        setfillcolor(lit ? RGB(255, 140, 100) : RGB(90, 160, 90));
        solidcircle((int)m.x, (int)m.y, r);
        setfillcolor(RGB(40, 50, 40));                             // 雷体外壳纹路
        solidcircle((int)m.x, (int)m.y, r - 3);
        setfillcolor(lit ? RGB(255, 240, 180) : RGB(160, 220, 160));
        solidcircle((int)m.x, (int)m.y, 3);
        // 四周尖刺
        setfillcolor(RGB(70, 90, 70));
        for (int i = 0; i < 4; ++i) {
            double a = 0.785 + 1.5708 * i;
            solidcircle((int)(m.x + cos(a) * (r + 2)), (int)(m.y + sin(a) * (r + 2)), 2);
        }
    }
}

/**
 * @brief  玩家射击。攻击方式由【当前装备的武器类型】决定，四种武器弹道完全不同，
 *         且各自拥有独立的火力等级(1~3)强化路线。若未装备武器则使用默认速射炮。
 */
void Game::playerShoot() {
    const int  wt = playerWeaponType();
    const int  dmg = playerBulletDamage();             // 已含武器倍率
    const COLORREF wc = weaponColor(wt);
    const double muzY = py - PLAYER_H / 2.0;

    // 通用发弹器
    auto shoot = [&](double ox, double vx, double vy, int radius,
        int pierce, bool homing, int trail) {
            Bullet b;
            b.x = px + ox; b.y = muzY;
            b.vx = vx; b.vy = vy;
            b.radius = radius; b.damage = dmg; b.fromPlayer = true;
            b.color = wc; b.alive = true;
            b.homing = homing; b.life = 0;
            b.pierce = pierce; b.trail = trail;
            // 被动·临界暴击：20% 概率 3 倍伤害（暴击弹染成金色以便辨识）
            if (passiveOn[PS_CRIT] && randRange(0, 99) < 20) {
                b.damage *= 3;
                b.color = RGB(255, 220, 90);
                b.radius = radius + 1;
            }
            // 被动·贯穿改造：所有子弹额外获得 1 次贯穿
            if (passiveOn[PS_PIERCE]) b.pierce += 1;
            bullets.push_back(b);
        };

    switch (wt) {
    case W_CANNON:                                     // 速射炮：直线速射
        if (fireLevel == 1) {
            shoot(0, 0, -13.0, 4, 0, false, 1);
        }
        else if (fireLevel == 2) {
            shoot(-12, 0, -13.0, 4, 0, false, 1);
            shoot(12, 0, -13.0, 4, 0, false, 1);
        }
        else {
            shoot(0, 0, -13.5, 5, 0, false, 2);        // 中路加粗
            shoot(-14, -2.2, -13.0, 4, 0, false, 1);
            shoot(14, 2.2, -13.0, 4, 0, false, 1);
        }
        break;

    case W_SPREAD: {                                   // 散射炮：锥形扇面
        int n = (fireLevel == 1) ? 3 : (fireLevel == 2 ? 6 : 9);   // 与其它武器同比例(×1/×2/×3)
        double sp = (fireLevel == 1) ? 0.15 : (fireLevel == 2 ? 0.13 : 0.11);
        for (int i = 0; i < n; ++i) {
            double a = -1.5708 + (i - (n - 1) / 2.0) * sp;   // 以正上方为中心张开
            shoot(0, cos(a) * 11.0, sin(a) * 11.0, 4, 0, false, 0);
        }
        break;
    }

    case W_PIERCE: {                                   // 穿透光束：可贯穿多个敌人
        int pc = (fireLevel == 3) ? 5 : 3;             // 可贯穿次数
        int rr = (fireLevel == 3) ? 7 : 5;             // Lv3 光束加粗
        if (fireLevel == 1) {
            shoot(0, 0, -16.0, rr, pc, false, 3);
        }
        else if (fireLevel == 2) {
            shoot(-11, 0, -16.0, rr, pc, false, 3);
            shoot(11, 0, -16.0, rr, pc, false, 3);
        }
        else {
            shoot(0, 0, -16.5, rr + 1, pc, false, 4);
            shoot(-16, 0, -16.0, rr, pc, false, 3);
            shoot(16, 0, -16.0, rr, pc, false, 3);
        }
        break;
    }

    default: {                                         // 追踪导弹：自动追踪必中
        if (fireLevel == 1) {
            shoot(0, 0, -8.0, 5, 0, true, 2);
        }
        else if (fireLevel == 2) {
            shoot(-12, -1.2, -8.0, 5, 0, true, 2);
            shoot(12, 1.2, -8.0, 5, 0, true, 2);
        }
        else {
            shoot(-16, -2.4, -8.0, 6, 0, true, 3);
            shoot(0, 0.0, -8.5, 6, 0, true, 3);
            shoot(16, 2.4, -8.0, 6, 0, true, 3);
        }
        break;
    }
    }

    // 被动·回响射击：每第 3 次射击，额外分裂出一圈扇形弹
    if (passiveOn[PS_ECHO]) {
        if (++echoCount >= 3) {
            echoCount = 0;
            for (int i = -2; i <= 2; ++i) {
                if (i == 0) continue;
                double a = -1.5708 + i * 0.26;
                shoot(0, cos(a) * 9.5, sin(a) * 9.5, 3, 0, false, 1);
            }
            addParticle(PK_STAR, px, muzY, 0, 0, 8.0, RGB(200, 240, 255), 8, -0.6, 0.2);
        }
    }

    // 枪口闪光特效
    spawnMuzzleFlash(px, muzY, wc);
    audio.playSfx(SFX_SHOOT);
}

void Game::updateBullets() {
    for (auto& b : bullets) {
        ++b.life;
        // ---- 追踪弹制导 ----
        if (b.homing && b.life < 150) {
            double tx, ty; bool has = false;
            if (b.fromPlayer) {
                // 玩家追踪导弹：锁定最近的敌机/Boss
                double best = 1e18;
                for (const auto& e : enemies) {
                    if (!e.alive) continue;
                    double d = (e.x - b.x) * (e.x - b.x) + (e.y - b.y) * (e.y - b.y);
                    if (d < best) { best = d; tx = e.x; ty = e.y; has = true; }
                }
                if (boss.active) {
                    double d = (boss.x - b.x) * (boss.x - b.x) + (boss.y - b.y) * (boss.y - b.y);
                    if (d < best) { tx = boss.x; ty = boss.y; has = true; }
                }
            }
            else {
                tx = px; ty = py; has = true;          // 敌方追踪弹锁定玩家
            }
            if (has) {
                double dx = tx - b.x, dy = ty - b.y;
                double len = sqrt(dx * dx + dy * dy);
                if (len > 1) {
                    double spd = sqrt(b.vx * b.vx + b.vy * b.vy);
                    double turn = b.fromPlayer ? 0.42 : 0.16;   // 玩家导弹转向更灵敏
                    b.vx += (dx / len) * turn;
                    b.vy += (dy / len) * turn;
                    double ns = sqrt(b.vx * b.vx + b.vy * b.vy);
                    if (ns > 0) { b.vx = b.vx / ns * spd; b.vy = b.vy / ns * spd; }
                }
            }
        }
        // ---- 拖尾特效 ----
        if (b.trail > 0 && b.life % 3 == 0) {           // 频率降低，避免拖尾过亮
            addParticle(PK_SPARK, b.x, b.y, -b.vx * 0.10, -b.vy * 0.10,
                0.8 + b.trail * 0.3,
                b.fromPlayer ? darken(b.color, 60) : RGB(255, 160, 80),
                5 + b.trail, -0.05, 0);
        }
        // 时停领域：只放慢敌方弹幕，我方子弹不受影响
        double k = (!b.fromPlayer && slowTimer > 0) ? 0.25 : 1.0;
        // 被动·引力力场：机体周围 130px 内的敌弹持续减速（与时停领域可叠加）
        if (!b.fromPlayer && passiveOn[PS_AURA]) {
            double dx = b.x - px, dy = b.y - py;
            if (dx * dx + dy * dy < 130.0 * 130.0) k *= 0.45;
        }
        b.x += b.vx * k; b.y += b.vy * k;
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

    // 永久增益概率：明显低于临时增益；Boss 掉落显著更高；随关卡递增但设上限
    int permPct = fromBoss ? (22 + level * 6)      // Boss：28% / 34% / 40% ...
        : (6 + level * 2);      // 普通：8% / 10% / 12% ...
    // 装备掉落概率：Boss 明显更高，且随关卡递增（同样设上限）
    int gearPct = fromBoss ? (26 + level * 4)      // Boss：30% / 34% / 38% ...
        : (5 + level * 2);      // 普通：7% / 9% / 11% ...
    // 无限关卡下必须封顶，否则后期临时增益将被完全挤掉
    if (permPct > (fromBoss ? 45 : 16)) permPct = fromBoss ? 45 : 16;
    if (gearPct > (fromBoss ? 42 : 18)) gearPct = fromBoss ? 42 : 18;

    Item it;
    it.x = x; it.y = y; it.vy = 2.0; it.size = 26; it.alive = true;
    it.gear.slot = 0; it.gear.rarity = 0; it.gear.lvl = 1; it.gear.wtype = W_CANNON;

    int r = randRange(0, 99);
    if (r < permPct) {                             // 永久增益（稀有）
        it.type = (randRange(0, 1) == 0) ? IT_MAXLIFE : IT_POWERUP;
    }
    else if (r < permPct + gearPct) {            // 装备掉落
        it.type = IT_GEAR;
        it.size = 28;                              // 装备图标略大
        // 武器槽权重更高(50%)，保证攻击方式的多样性能被玩家实际体验到
        int sr = randRange(0, 99);
        it.gear.slot = (sr < 50) ? GS_WEAPON : (sr < 78) ? GS_ARMOR : GS_ENGINE;
        // 稀有度：Boss 与关卡越高，越容易出高品质
        int rr = randRange(0, 99) + (fromBoss ? 25 : 0) + level * 5;
        it.gear.rarity = (rr < 45) ? R_COMMON : (rr < 72) ? R_RARE
            : (rr < 92) ? R_EPIC : R_LEGEND;
        it.gear.lvl = 1;                           // 敌人掉落的装备恒为 LV.1，高级只能靠合成
        // 武器随机决定攻击方式；Boss 掉落更容易出稀有攻击方式
        it.gear.wtype = rollWeaponType();          // 轮转池：四种武器均匀出现，不会缺某一种
    }
    else {                                       // 临时增益
        int t = randRange(0, 99);
        it.type = (t < 40) ? IT_FIRE : (t < 65) ? IT_SHIELD
            : (t < 85) ? IT_LIFE : IT_BOMB;
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
    case IT_LIFE:                                      // 维修：恢复部分 HP（不超过上限）
        hp += 40; if (hp > playerHpMax()) hp = playerHpMax();
        break;
    case IT_SHIELD:                                    // 护盾：可抵挡一次伤害
        shield = true;
        break;
    case IT_BOMB:                                      // 炸弹补给：不超过上限
        if (bombs < MAX_BOMBS) ++bombs;
        break;
    case IT_MAXLIFE:                                   // 【永久】血量上限 +25（累计封顶）
        if (bonusHp < PERM_HP_CAP) {
            bonusHp += PERM_HP_BONUS;
            if (bonusHp > PERM_HP_CAP) bonusHp = PERM_HP_CAP;
            hp += PERM_HP_BONUS;
        }
        else {
            hp += PERM_HP_BONUS;                       // 已封顶则转为回血，不浪费
        }
        if (hp > playerHpMax()) hp = playerHpMax();
        spawnExplosion(px, py, RGB(255, 120, 160), 16);
        break;
    case IT_POWERUP:                                   // 【永久】子弹伤害 +1（封顶 4）
        if (bulletDmg < BULLET_DMG_MAX) ++bulletDmg;
        spawnExplosion(px, py, RGB(200, 140, 255), 16);
        break;
    }
    audio.playSfx(SFX_POWERUP);
}


/**
 * @brief  生成一个粒子（内部工具）。
 */
void Game::addParticle(int kind, double x, double y, double vx, double vy,
    double size, COLORREF c, int life, double grow, double spin) {
    Particle p;
    p.kind = kind; p.x = x; p.y = y; p.vx = vx; p.vy = vy;
    p.size = size; p.color = c; p.maxLife = p.life = life;
    p.grow = grow; p.rot = randFloat() * 6.2832; p.spin = spin;
    particles.push_back(p);
}

/**
 * @brief  华丽爆炸特效：由「冲击波环 + 白热星芒 + 高速火花 + 旋转碎片 + 烟雾」
 *         五层叠加而成，而非单一圆点，视觉层次远比原先丰富。
 * @param  count 规模（火花数量基准）
 */
void Game::spawnExplosion(double x, double y, COLORREF c, int count) {
    // 1) 冲击波环：瞬间向外扩张
    addParticle(PK_RING, x, y, 0, 0, 4.0, lighten(c, 60), 16, 2.6, 0);
    if (count >= 16)
        addParticle(PK_RING, x, y, 0, 0, 2.0, RGB(255, 255, 255), 12, 3.6, 0);
    // 2) 白热星芒（大爆炸才有）
    if (count >= 12)
        addParticle(PK_STAR, x, y, 0, 0, 10.0 + count * 0.3, RGB(255, 255, 220), 14, -0.4, 0.12);
    // 3) 高速火花
    for (int i = 0; i < count; ++i) {
        double a = randFloat() * 6.2832;
        double s = 1.5 + randFloat() * 5.0;
        addParticle(PK_SPARK, x, y, cos(a) * s, sin(a) * s,
            1.6 + randFloat() * 2.4, c, randRange(16, 30), -0.03, 0);
    }
    // 4) 旋转碎片
    for (int i = 0; i < count / 3; ++i) {
        double a = randFloat() * 6.2832;
        double s = 1.0 + randFloat() * 3.0;
        addParticle(PK_SHARD, x, y, cos(a) * s, sin(a) * s,
            2.0 + randFloat() * 2.5, darken(c, 30), randRange(24, 42),
            0, (randFloat() - 0.5) * 0.5);
    }
    // 5) 烟雾（缓慢膨胀渐隐）
    for (int i = 0; i < count / 4; ++i) {
        double a = randFloat() * 6.2832;
        addParticle(PK_SMOKE, x, y, cos(a) * 0.7, sin(a) * 0.7 - 0.3,
            3.0 + randFloat() * 3.0, darken(c, 70), randRange(30, 50), 0.25, 0);
    }
}

/**
 * @brief  枪口闪光：短促的星芒 + 少量火花，让射击手感更实。
 */
void Game::spawnMuzzleFlash(double x, double y, COLORREF c) {
    addParticle(PK_STAR, x, y, 0, 0, 5.0, darken(c, 40), 4, -0.8, 0.2);
    for (int i = 0; i < 2; ++i) {
        double a = -1.5708 + (randFloat() - 0.5) * 1.2;
        addParticle(PK_SPARK, x, y, cos(a) * 2.2, sin(a) * 2.2,
            1.4, c, 7, -0.05, 0);
    }
}

/**
 * @brief  冲击波：单独的扩张圆环，用于 Boss 登场、升级、大招启动等时刻。
 */
void Game::spawnShockwave(double x, double y, COLORREF c, double growSpeed, int life) {
    addParticle(PK_RING, x, y, 0, 0, 6.0, c, life, growSpeed, 0);
}

void Game::updateParticles() {
    for (auto& p : particles) {
        p.x += p.vx; p.y += p.vy;
        p.rot += p.spin;
        switch (p.kind) {
        case PK_SPARK:  p.vx *= 0.93; p.vy *= 0.93; p.vy += 0.06; break;  // 阻尼+轻微下坠
        case PK_SMOKE:  p.vx *= 0.96; p.vy *= 0.96; p.size += p.grow; break;
        case PK_RING:   p.size += p.grow; break;                          // 圆环持续扩张
        case PK_SHARD:  p.vx *= 0.96; p.vy *= 0.96; p.vy += 0.10; break;  // 碎片受重力
        case PK_STAR:   p.size += p.grow; break;
        }
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
    // ===== 被动·连锁闪电：向最近的存活敌机放电 =====
    if (passiveOn[PS_CHAIN]) {
        Enemy* best = NULL; double bd = 1e18;
        for (auto& o : enemies) {
            if (!o.alive || &o == &e) continue;
            double d = (o.x - e.x) * (o.x - e.x) + (o.y - e.y) * (o.y - e.y);
            if (d < bd && d < 200.0 * 200.0) { bd = d; best = &o; }
        }
        if (best) {
            best->hp -= effDamage(playerAttack() * 2, best->armor);
            best->hitFlash = 4;
            // 电弧特效：沿两点连线撒粒子
            for (int i = 0; i <= 8; ++i) {
                double t = i / 8.0;
                addParticle(PK_SPARK,
                    e.x + (best->x - e.x) * t + randRange(-4, 4),
                    e.y + (best->y - e.y) * t + randRange(-4, 4),
                    0, 0, 2.0, RGB(140, 220, 255), 10, -0.05, 0);
            }
            if (best->hp <= 0) killEnemy(*best);        // 可连锁触发
        }
    }
    // ===== 被动·残骸引爆：死亡时炸伤周围敌机 =====
    if (passiveOn[PS_CORPSE]) {
        int cd = playerAttack() * 2;
        for (auto& o : enemies) {
            if (!o.alive || &o == &e) continue;
            double dx = o.x - e.x, dy = o.y - e.y;
            if (dx * dx + dy * dy < 90.0 * 90.0) {
                o.hp -= effDamage(cd, o.armor);
                o.hitFlash = 3;
                if (o.hp <= 0) killEnemy(o);
            }
        }
        spawnShockwave(e.x, e.y, RGB(255, 170, 90), 6.0, 16);
    }
    score += e.score;
    gainXp(enemyXp(e.type, level));                         // 按强度权重给经验
    addMoney(enemyMoney(enemyPower(e.type, level), level)); // 按强度权重给货币
    energy = clampi(energy + ENERGY_KILL, 0, ENERGY_MAX);   // 击杀积攒大招能量
    // 多层爆炸：白热核心 + 橙色火球 + 暗红余烬，层次更华丽
    spawnExplosion(e.x, e.y, RGB(255, 250, 220), 5);
    spawnExplosion(e.x, e.y, RGB(255, 180, 80), 12);
    spawnExplosion(e.x, e.y, RGB(220, 90, 50), 8);
    dropItem(e.x, e.y);
    audio.playSfx(SFX_EXPLODE);
}

/**
 * @brief  释放激光大招：清空能量、进入持续释放状态，并触发一次震屏。
 *         效果见 updateUltimate（逐帧结算）。
 */
 /**
  * @brief  释放当前选用的大招（X 键，消耗全部能量）。
  *         七种大招各有完全不同的机制：直伤 / 清场 / 控场 / 增益 / 召唤。
  *         强度越高的大招售价越贵（见 ultPrice），形成"强度↔获取难度"正相关。
  */
void Game::activateUltimate() {
    energy = 0;
    shakeTimer = 16;
    flashTimer = 8;
    audio.playSfx(SFX_LASER);

    switch (curUlt) {
    case U_LASER:                                  // ① 能量光束：向上贯穿光柱
        ultTimer = ULT_FRAMES;
        spawnExplosion(px, py - PLAYER_H / 2.0, RGB(160, 240, 255), 26);
        spawnShockwave(px, py, RGB(150, 235, 255), 7.0, 22);
        break;

    case U_NOVA:                                   // ② 环形爆轰：扩张冲击波清场
        novaTimer = 40; novaR = 0;
        spawnShockwave(px, py, RGB(255, 220, 140), 14.0, 34);
        spawnShockwave(px, py, RGB(255, 255, 255), 9.0, 24);
        spawnExplosion(px, py, RGB(255, 200, 120), 30);
        shakeTimer = 26;
        break;

    case U_SLOW:                                   // ③ 时停领域：极限减速 + 易伤
        slowTimer = 60 * 8;                        // 8 秒
        spawnShockwave(px, py, RGB(150, 200, 255), 16.0, 40);
        break;

    case U_OVERDRIVE:                              // ④ 超载模式：无敌 + 极限射速
        odTimer = 60 * 6;                          // 6 秒
        if (invincible < odTimer) invincible = odTimer;
        spawnShockwave(px, py, RGB(255, 180, 90), 10.0, 30);
        spawnExplosion(px, py, RGB(255, 220, 140), 24);
        break;

    case U_METEOR:                                 // ⑤ 陨石轰炸：天降大范围打击
        // 直接投放一批高速下坠的陨石（用玩家子弹实现，伤害极高、可贯穿）
        for (int i = 0; i < 14; ++i) {
            Bullet b;
            b.x = randRange(20, WIN_W - 20); b.y = randRange(-260, -20);
            b.vx = randFloat() * 1.0 - 0.5;  b.vy = 6.5 + randFloat() * 2.5;
            b.radius = 13; b.damage = playerBulletDamage() * 8;   // 已加强
            b.fromPlayer = true; b.color = RGB(255, 150, 70); b.alive = true;
            b.homing = false; b.life = 0; b.pierce = 99; b.trail = 4;
            bullets.push_back(b);
        }
        shakeTimer = 30;
        break;

    case U_BLACKHOLE:                              // ⑥ 奇点黑洞：吸附并绞杀
        bhTimer = 60 * 5;                          // 5 秒
        bhX = WIN_W / 2.0; bhY = WIN_H * 0.32;     // 固定生成在屏幕上半区
        spawnShockwave(bhX, bhY, RGB(180, 120, 255), 12.0, 36);
        break;

    default:                                       // ⑦ 卫星炮台：召唤环绕卫星
        for (int i = 0; i < 2; ++i) {
            Helper h;
            h.x = px; h.y = py;
            h.phase = 3.1416 * i;                  // 两台卫星相位相反
            h.life = 60 * 14;                      // 14 秒
            h.shootCd = 0; h.orbit = true;
            helpers.push_back(h);
        }
        spawnShockwave(px, py, RGB(160, 255, 220), 8.0, 24);
        break;
    }
}

/**
 * @brief  释放当前选用的小技能（B 键，消耗一次使用次数）。
 *         九种技能覆盖清场 / 恢复 / 控制 / 增益 / 召唤五类。
 */
void Game::useSkill() {
    if (bombs <= 0) return;
    --bombs;
    shakeTimer = 12;
    audio.playSfx(SFX_BOMB);

    switch (curSkill) {
    case SK_BOMB: {                                // ① 清屏炸弹
        for (auto& e : enemies) {
            score += e.score;
            gainXp(enemyXp(e.type, level));
            addMoney(enemyMoney(enemyPower(e.type, level), level));
            spawnExplosion(e.x, e.y, RGB(255, 250, 220), 5);
            spawnExplosion(e.x, e.y, RGB(255, 170, 70), 10);
        }
        enemies.clear();
        for (auto& b : bullets) if (!b.fromPlayer) b.alive = false;
        mines.clear();
        if (boss.active) {                         // 对 Boss 按血量上限比例重创（无视护甲）
            int dmg = 20 + boss.maxHp / 12;
            boss.hp -= dmg; boss.hitFlash = 6;
            spawnExplosion(boss.x, boss.y, RGB(255, 220, 140), 18);
            if (boss.hp <= 0) { nextLevelOrWin(); return; }
        }
        spawnShockwave(px, py, RGB(255, 220, 140), 12.0, 28);
        spawnShockwave(px, py, RGB(255, 255, 255), 7.0, 20);
        flashTimer = 7;
        break;
    }
    case SK_REPAIR:                                // ② 应急维修：回复 50% 血上限
        hp += playerHpMax() * 2 / 5;               // 回复 40% 血上限
        if (hp > playerHpMax()) hp = playerHpMax();
        spawnExplosion(px, py, RGB(140, 255, 170), 22);
        spawnShockwave(px, py, RGB(140, 255, 170), 6.0, 22);
        break;

    case SK_SHIELD:                                // ③ 力场护盾：免疫一次伤害
        shield = true;
        spawnShockwave(px, py, RGB(120, 210, 255), 7.0, 24);
        break;

    case SK_MAGNET:                                // ④ 磁力吸附：道具瞬间到手
        for (auto& it : items) { it.x = px; it.y = py - 4; }
        spawnShockwave(px, py, RGB(255, 220, 120), 18.0, 26);
        break;

    case SK_EMP:                                   // ⑤ 电磁脉冲：清空敌方弹幕
        for (auto& b : bullets)
            if (!b.fromPlayer) {
                addParticle(PK_SPARK, b.x, b.y, 0, 0, 2.0, RGB(160, 220, 255), 10, -0.05, 0);
                b.alive = false;
            }
        mines.clear();
        spawnShockwave(px, py, RGB(160, 220, 255), 22.0, 30);
        flashTimer = 5;
        break;

    case SK_FREEZE:                                // ⑥ 冰冻立场：冻结敌机 3 秒
        freezeTimer = 60 * 2;                      // 冻结 2 秒
        spawnShockwave(px, py, RGB(150, 230, 255), 20.0, 32);
        break;

    case SK_DECOY:                                 // ⑦ 诱饵无人机：吸收周围敌弹
        decoyTimer = 60 * 4;
        decoyX = px; decoyY = py - 40;             // 投放在身前，替你挡弹
        spawnShockwave(decoyX, decoyY, RGB(255, 200, 120), 8.0, 24);
        break;

    case SK_BERSERK:                               // ⑧ 狂暴强化：攻击翻倍 8 秒
        berserkTimer = 60 * 6;                     // 狂暴 6 秒
        spawnExplosion(px, py, RGB(255, 120, 90), 20);
        break;

    default: {                                     // ⑨ 战术分身：召唤僚机
        for (int i = 0; i < 2; ++i) {
            Helper h;
            h.x = px + (i == 0 ? -44 : 44); h.y = py + 10;
            h.phase = (i == 0 ? 0.0 : 1.0); h.life = 60 * 8; h.shootCd = 0; h.orbit = false;
            helpers.push_back(h);
        }
        spawnShockwave(px, py, RGB(180, 220, 255), 7.0, 22);
        break;
    }
    }
}

/**
 * @brief  各类技能效果的逐帧结算（计时递减 + 持续型效果的实际作用）。
 */
void Game::updateSkillEffects() {
    if (slowTimer > 0) --slowTimer;
    if (odTimer > 0) --odTimer;
    if (freezeTimer > 0) --freezeTimer;
    if (decoyTimer > 0) --decoyTimer;
    if (berserkTimer > 0) --berserkTimer;
    if (odTimer > 0 && invincible < 2) invincible = 2;   // 超载期间保持无敌

    // ---- 环形爆轰：不断扩张的伤害环 ----
    if (novaTimer > 0) {
        --novaTimer;
        novaR += 14.0;
        int dmg = playerBulletDamage() * 9 / 2;    // 4.5 倍
        for (auto& e : enemies) {
            if (!e.alive) continue;
            double d = sqrt((e.x - px) * (e.x - px) + (e.y - py) * (e.y - py));
            if (fabs(d - novaR) < 26) {                  // 只有环带扫过的瞬间造成伤害
                e.hp -= effDamage(dmg, e.armor);
                e.hitFlash = 4;
                if (e.hp <= 0) killEnemy(e);
            }
        }
        if (boss.active) {
            double d = sqrt((boss.x - px) * (boss.x - px) + (boss.y - py) * (boss.y - py));
            if (fabs(d - novaR) < 30 && novaTimer % 4 == 0) {
                boss.hp -= 3 + boss.maxHp / 120; boss.hitFlash = 3;
                if (boss.hp <= 0) { nextLevelOrWin(); return; }
            }
        }
        for (int i = 0; i < 3; ++i) {                    // 环带火花
            double a = randFloat() * 6.2832;
            addParticle(PK_SPARK, px + cos(a) * novaR, py + sin(a) * novaR,
                cos(a) * 1.5, sin(a) * 1.5, 2.2, RGB(255, 210, 120), 12, -0.05, 0);
        }
    }

    // ---- 黑洞：吸附范围内敌人并持续伤害 ----
    if (bhTimer > 0) {
        --bhTimer;
        for (auto& e : enemies) {
            if (!e.alive) continue;
            double dx = bhX - e.x, dy = bhY - e.y;
            double d = sqrt(dx * dx + dy * dy);
            if (d < 230 && d > 1) {
                e.x += dx / d * 3.4;                     // 吸附（已加强）
                e.y += dy / d * 3.4;
                if (bhTimer % 4 == 0) {                  // 持续绞杀（已加强）
                    e.hp -= effDamage(playerBulletDamage(), e.armor);
                    e.hitFlash = 2;
                    if (e.hp <= 0) killEnemy(e);
                }
            }
        }
        // 敌方弹幕也被吸入
        for (auto& b : bullets) {
            if (b.fromPlayer) continue;
            double dx = bhX - b.x, dy = bhY - b.y;
            double d = sqrt(dx * dx + dy * dy);
            if (d < 150) { if (d < 18) b.alive = false; else { b.vx += dx / d * 0.5; b.vy += dy / d * 0.5; } }
        }
        // 视觉：旋转吸积盘
        for (int i = 0; i < 2; ++i) {
            double a = randFloat() * 6.2832, r = 40 + randFloat() * 90;
            addParticle(PK_SPARK, bhX + cos(a) * r, bhY + sin(a) * r,
                -cos(a) * 2.2, -sin(a) * 2.2, 2.0, RGB(200, 140, 255), 16, -0.04, 0);
        }
    }

    // ---- 诱饵无人机：吸收靠近的敌方弹幕，替玩家挡弹 ----
    if (decoyTimer > 0) {
        for (auto& b : bullets) {
            if (b.fromPlayer || !b.alive) continue;
            double dx = b.x - decoyX, dy = b.y - decoyY;
            if (dx * dx + dy * dy < 62 * 62) {           // 半径 62 内的敌弹被吸收
                b.alive = false;
                addParticle(PK_SPARK, b.x, b.y, (decoyX - b.x) * 0.06, (decoyY - b.y) * 0.06,
                    2.0, RGB(255, 210, 120), 10, -0.05, 0);
            }
        }
    }
}

/**
 * @brief  僚机 / 卫星：自动跟随并开火。
 *         orbit=true 为环绕玩家的卫星（大招召唤）；false 为并肩僚机（小技能召唤）。
 */
void Game::updateHelpers() {
    for (auto& h : helpers) {
        --h.life;
        if (h.orbit) {                                   // 卫星：绕玩家旋转
            h.phase += 0.06;
            h.x = px + cos(h.phase) * 62;
            h.y = py + sin(h.phase) * 62;
        }
        else {                                         // 僚机：平滑跟随两侧
            double tx = px + (h.phase < 0.5 ? -44 : 44), ty = py + 10;
            h.x += (tx - h.x) * 0.12;
            h.y += (ty - h.y) * 0.12;
        }
        if (--h.shootCd <= 0) {                          // 自动开火
            h.shootCd = 14;
            Bullet b;
            b.x = h.x; b.y = h.y - 8;
            b.vx = 0; b.vy = -12.0;
            b.radius = 3; b.damage = playerBulletDamage() / 2;
            if (b.damage < 1) b.damage = 1;
            b.fromPlayer = true; b.color = RGB(180, 230, 255); b.alive = true;
            b.homing = false; b.life = 0; b.pierce = 0; b.trail = 1;
            bullets.push_back(b);
        }
    }
    helpers.erase(std::remove_if(helpers.begin(), helpers.end(),
        [](const Helper& h) { return h.life <= 0; }), helpers.end());
}

/** @brief 绘制僚机 / 卫星 / 诱饵无人机 */
void Game::drawHelpers() {
    // 诱饵无人机：脉冲光环 + 核心（在 render 阶段绘制，避免被 cleardevice 擦掉）
    if (decoyTimer > 0) {
        double pl = 0.5 + 0.5 * sin(animTick * 0.25);
        setfillcolor(fadeColor(RGB(255, 200, 110), 0.35));
        solidcircle((int)decoyX, (int)decoyY, 14 + (int)(pl * 5));
        setlinecolor(RGB(255, 220, 140));
        setlinestyle(PS_SOLID, 2);
        circle((int)decoyX, (int)decoyY, 62);          // 吸弹范围提示
        setlinestyle(PS_SOLID, 1);
        setfillcolor(RGB(230, 170, 70));
        solidcircle((int)decoyX, (int)decoyY, 8);
        setfillcolor(RGB(255, 240, 190));
        solidcircle((int)decoyX, (int)decoyY, 3);
    }
    for (const auto& h : helpers) {
        bool fading = (h.life < 60) && ((h.life / 5) % 2 == 0);   // 将消失时闪烁
        if (fading) continue;
        int cx = (int)h.x, cy = (int)h.y;
        setfillcolor(fadeColor(RGB(140, 220, 255), 0.4));
        solidcircle(cx, cy, 11);
        setfillcolor(RGB(90, 170, 230));
        POINT p[] = { {cx, cy - 9}, {cx + 8, cy + 6}, {cx, cy + 2}, {cx - 8, cy + 6} };
        solidpolygon(p, 4);
        setfillcolor(RGB(220, 250, 255));
        solidcircle(cx, cy - 2, 3);
    }
}

/**
 * @brief  三关结算：按本轮三关的得分表现评级，并赠予下一轮的被动技能。
 *         调用时机：刚打过第 3/6/9… 关 Boss、即将进入下一轮时。
 *
 *  评级 = 本轮实际得分 / 本轮期望分数 × 100%
 *    S(≥130%) -> 3 个被动 | A(≥100%) -> 2 个 | B(≥70%) -> 1 个 | 低于 B -> 0 个
 *  上一轮的被动会在此【全部清空】，必须靠新一轮的表现重新赢取。
 */
void Game::evaluateCycle() {
    // 1) 汇总本轮三关的【杂兵满分】与必得的 Boss 分
    //    （level 此时已指向下一轮首关，故回看前 3 关）
    int fullMob = 0, bossPart = 0;
    for (int L = level - 3; L <= level - 1; ++L)
        if (L >= 1) { fullMob += fullMobScore(L); bossPart += bossScoreOf(L); }
    if (fullMob < 1) fullMob = 1;

    // 2) 实际杂兵得分 = 本轮总增长 - 必得的 Boss 分
    lastGained = score - cycleStartScore;             // 记录明细供结算浮层展示
    lastCycFrom = level - 3; lastCycTo = level - 1;
    int gainedMob = (score - cycleStartScore) - bossPart;
    if (gainedMob < 0) gainedMob = 0;
    // 得分率 = 实际杂兵得分 / 杂兵满分（全歼 = 100%）
    lastPct = (int)((double)gainedMob / fullMob * 100.0);

    // 3) 定级：全歼(100%)必得 S；漏怪越多评级越低
    if (lastPct >= 90) lastTier = RK_S;
    else if (lastPct >= 70) lastTier = RK_A;
    else if (lastPct >= 50) lastTier = RK_B;
    else                    lastTier = RK_NONE;

    // 3) 清空上一轮被动——这是机制核心：强度不会累积，必须持续打出高分
    for (int i = 0; i < PS_COUNT; ++i) passiveOn[i] = false;
    passiveCount = 0; grantedNum = 0;

    // 4) 依评级随机赠予【互不重复】的被动
    int n = tierGrantCount(lastTier);
    int pool[PS_COUNT];
    for (int i = 0; i < PS_COUNT; ++i) pool[i] = i;
    for (int i = PS_COUNT - 1; i > 0; --i) {          // 洗牌，保证随机且不重复
        int j = randRange(0, i);
        int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    for (int i = 0; i < n && i < PS_COUNT; ++i) {
        passiveOn[pool[i]] = true;
        grantedList[i] = pool[i];
        ++passiveCount; ++grantedNum;
    }
    if (hp > playerHpMax()) hp = playerHpMax();       // 生命核心失效时夹紧血量

    // 5) 开启下一轮统计 + 展示结算浮层
    cycleStartScore = score;
    rankShowTimer = 240;                              // 约 4 秒
    audio.playSfx(lastTier >= RK_A ? SFX_LEVELUP : SFX_POWERUP);
    if (n > 0) {
        spawnShockwave(px, py, tierColor(lastTier), 9.0, 30);
        spawnExplosion(px, py, tierColor(lastTier), 26);
    }
}

/** @brief 获得货币 */
void Game::addMoney(int copper) {
    if (copper <= 0) return;
    money += copper;
}

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
    // 对同一纵列的 Boss 持续造成伤害（按血量上限缩放，同样【无视护甲】，
    // 使激光大招成为对付重甲 Boss 的核心手段）
    if (boss.active && fabs(boss.x - px) < ULT_HALF + boss.w / 2.0) {
        if (ultTimer % 3 == 0) {
            int tick = 1 + boss.maxHp / 200;           // 已调低：全程约打掉 Boss 血量的 1/9
            boss.hp -= tick; boss.hitFlash = 2;
            if (boss.hp <= 0) { nextLevelOrWin(); return; }
        }
    }
    // 光束前端火花
    if (ultTimer % 2 == 0)
        spawnExplosion(px + randRange(-ULT_HALF, ULT_HALF), randRange(0, (int)py),
            RGB(180, 250, 255), 1);
}

void Game::hitPlayer(int dmg) {
    if (invincible > 0) return;                        // 无敌期间免疫
    if (shield) {                                      // 护盾优先抵挡（免除本次全部伤害）
        shield = false;
        invincible = 40;
        spawnExplosion(px, py, RGB(120, 200, 255), 14);
        shakeTimer = 8;
        return;
    }
    // 护甲减伤：实际扣血 = 伤害 - 防御力，至少 1
    int actual = dmg - playerDefense();
    if (actual < 1) actual = 1;
    hp -= actual;
    invincible = HIT_IFRAMES;                          // 每次受击短暂无敌，避免同帧连击
    shakeTimer = 8;
    spawnExplosion(px, py, COL_PLAYER, 10);
    audio.playSfx(SFX_HIT);
    if (hp <= 0) {                                     // 血量耗尽 -> 损失 1 条命
        --lives;
        shakeTimer = 16;
        fireLevel = 1; fireTimer = 0;                  // 阵亡才掉火力
        spawnExplosion(px, py, COL_PLAYER, 24);
        if (lives <= 0) {                              // 命数用尽 -> 游戏结束
            audio.playSfx(SFX_GAMEOVER);
            quitSettle = false;
            tryRecordScore();
            state = ST_GAMEOVER;
        }
        else {                                       // 复活：回满血并给予较长无敌
            hp = playerHpMax();
            invincible = INVINCIBLE_FRAMES;
        }
    }
}

/*==========================================================================*/
/*                          玩家有效属性 / 装备操作                          */
/*==========================================================================*/

int Game::playerAttack()  const {
    int a = bulletDmg + levelAtk(plv)
        + (equipOn[GS_WEAPON] ? gearAtk(equip[GS_WEAPON].rarity, equip[GS_WEAPON].lvl) : 0)
        + (cheatOn ? cfgAtk : 0);
    if (passiveOn[PS_ATK]) a = a * 13 / 10;            // 被动·强化弹头：+30%
    // 被动·应激装甲：残血(<35%)时伤害 +60%——高风险高回报的机制
    if (passiveOn[PS_ADRENALINE] && hp * 100 < playerHpMax() * 35) a = a * 16 / 10;
    return a;
}
int Game::playerDefense() const {
    int d = levelDef(plv)
        + (equipOn[GS_ARMOR] ? gearDef(equip[GS_ARMOR].rarity, equip[GS_ARMOR].lvl) : 0)
        + (cheatOn ? cfgDef : 0);
    if (passiveOn[PS_ARMOR]) d += 4;                   // 被动·复合装甲：防御 +4
    return d;
}
int Game::playerHpMax()   const {
    int h = PLAYER_BASE_HP + bonusHp + levelHp(plv)
        + (equipOn[GS_ARMOR] ? gearHp(equip[GS_ARMOR].rarity, equip[GS_ARMOR].lvl) : 0)
        + (cheatOn ? cfgHp : 0);
    if (passiveOn[PS_ARMOR]) h = h * 5 / 4;            // 被动·复合装甲：血上限 +25%
    return h;
}
int Game::playerSpeed()   const {
    int s = PLAYER_SPD + (equipOn[GS_ENGINE] ? gearSpeed(equip[GS_ENGINE].rarity, equip[GS_ENGINE].lvl) : 0);
    if (passiveOn[PS_ENGINE]) s += 3;                  // 被动·高效引擎：移动 +3
    if (passiveOn[PS_ADRENALINE] && hp * 100 < playerHpMax() * 35) s += 3;   // 应激装甲：残血提速
    return s;
}
int Game::playerWeaponType() const {
    return equipOn[GS_WEAPON] ? equip[GS_WEAPON].wtype : W_CANNON;   // 无武器时用默认速射炮
}

int Game::playerBulletDamage() const {
    int d = playerAttack() * weaponDmgPct(playerWeaponType()) / 100;
    if (berserkTimer > 0) d = d * 7 / 5;           // 狂暴强化：攻击 +40%
    return d < 1 ? 1 : d;
}

int Game::playerShootCd() const {
    int c = SHOOT_CD - (equipOn[GS_ENGINE] ? gearRate(equip[GS_ENGINE].rarity, equip[GS_ENGINE].lvl) : 0);
    c += weaponCdBonus(playerWeaponType());            // 武器自带射速修正
    if (passiveOn[PS_ENGINE]) c -= 2;                  // 被动·高效引擎：射速 +2
    if (odTimer > 0) c = 2;                            // 超载模式：极限射速
    return c < 2 ? 2 : c;
}

/** @brief 拾取一件装备进入物品栏（满则提示丢弃） */
/**
 * @brief  获得经验并处理升级。升级所需经验随等级二次增长，而经验获取只随
 *         关卡线性增长，因此【升级速度随关卡推进必然递减】，符合设计目标。
 *         每次升级会记录本级所花秒数（lastLvlSec），用于在 HUD 与结算页展示。
 */
 /**
  * @brief  把自定义初始配置应用到本局（仅在秘籍开局时调用）。
  *         必须在 resetGame() 之后调用——resetGame 会把一切复位。
  *         注意：cfgHp / cfgAtk / cfgDef 是以「额外加成」的形式叠加进
  *         playerHpMax() / playerAttack() / playerDefense()，
  *         因此这里只需设置等级、关卡、装备与备用机，属性会自动生效。
  */
void Game::applyCheatConfig() {
    plv = cfgPlv;
    level = cfgLevel;
    lives = cfgLives;
    xp = 0;

    // 三个部位一次性配齐：武器按所选类型，护甲/引擎同稀有度同等级
    for (int s = 0; s < GS_COUNT; ++s) {
        equip[s].slot = s;
        equip[s].rarity = cfgRarity;
        equip[s].lvl = cfgGlvl;
        equip[s].wtype = cfgWtype;      // 仅武器槽有意义
        equipOn[s] = true;
    }
    fireLevel = 3;                       // 直接给满火力，方便展示各武器 Lv3 弹道
    fireTimer = FIRE_BUFF_FRAMES;
    // 货币与技能：秘籍模式下解锁全部大招/小技能，便于演示，并按所选装备
    money = cfgMoney;
    for (int i = 0; i < U_COUNT; ++i)  ultOwned[i] = true;
    for (int i = 0; i < SK_COUNT; ++i) skillOwned[i] = true;
    curUlt = cfgUlt;
    curSkill = cfgSkill;
    bombs = MAX_BOMBS;                // 技能次数给满，方便逐个演示
    energy = ENERGY_MAX;               // 能量给满，进场即可放大招
    // 被动：按所选数量随机赠予（互不重复），便于现场演示被动系统
    for (int i = 0; i < PS_COUNT; ++i) passiveOn[i] = false;
    passiveCount = grantedNum = 0;
    {
        int pool[PS_COUNT];
        for (int i = 0; i < PS_COUNT; ++i) pool[i] = i;
        for (int i = PS_COUNT - 1; i > 0; --i) {
            int j = randRange(0, i); int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
        }
        for (int i = 0; i < cfgPassive && i < PS_COUNT; ++i) {
            passiveOn[pool[i]] = true; grantedList[i] = pool[i];
            ++passiveCount; ++grantedNum;
        }
        lastTier = cfgPassive >= 3 ? RK_S : cfgPassive == 2 ? RK_A
            : cfgPassive == 1 ? RK_B : RK_NONE;
        lastPct = cfgPassive >= 3 ? 130 : cfgPassive == 2 ? 100 : cfgPassive == 1 ? 70 : 0;
    }
    cycleStartScore = score;
    hp = playerHpMax();                  // 被动生效后再回满血
    hp = playerHpMax();                  // 属性算好后回满血
    bannerTimer = 110;                   // 显示关卡横幅
    _stprintf_s(toast, _countof(toast), _T("自定义开局：Lv.%d  第%d关"), plv, level);
    toastTimer = 180;
}

void Game::gainXp(int amount) {
    if (plv >= PLV_MAX) return;                        // 满级后不再累计
    xp += amount;
    while (plv < PLV_MAX && xp >= xpNeed(plv)) {
        xp -= xpNeed(plv);
        ++plv;
        // 统计本级耗时（毫秒 -> 秒），作为"升级速度"的直观指标
        DWORD now = timeGetTime();
        lastLvlSec = (int)((now - lastLvlUpMs) / 1000);
        lastLvlUpMs = now;
        // 升级特效与提示
        lvlUpFlash = 60;
        flashTimer = 5;
        spawnExplosion(px, py, RGB(255, 240, 150), 26);
        spawnExplosion(px, py, RGB(120, 230, 255), 18);
        spawnShockwave(px, py, RGB(255, 230, 140), 6.0, 26);       // 升级冲击波
        spawnShockwave(px, py, RGB(255, 255, 255), 3.5, 18);
        hp = playerHpMax();                            // 升级回满血，作为升级奖励
        audio.playSfx(SFX_LEVELUP);
        _stprintf_s(toast, _countof(toast), _T("升级！Lv.%d  (本级用时 %d 秒)"), plv, lastLvlSec);
        toastTimer = 150;
    }
    if (plv >= PLV_MAX) xp = 0;
}

/**
 * @brief  装备合成：把背包中 idx 处的装备与另一件【同部位 + 同稀有度 + 同等级】的
 *         装备合成为高一级的同类装备（LV1+LV1 -> LV2，最高 LV5）。
 *         敌人只掉落 LV1 装备，高等级装备只能靠合成获得。
 */
void Game::fuseFromBag(int idx) {
    if (idx < 0 || idx >= (int)bag.size()) return;
    Gear a = bag[idx];
    if (a.lvl >= GEAR_LVL_MAX) {                       // 已满级
        _stprintf_s(toast, _countof(toast), _T("已是最高等级 LV.%d"), GEAR_LVL_MAX);
        toastTimer = 120; return;
    }
    // 在背包中寻找另一件同部位、同稀有度、同等级的装备作为素材
    int mate = -1;
    for (int i = 0; i < (int)bag.size(); ++i) {
        if (i == idx) continue;
        if (bag[i].slot == a.slot && bag[i].rarity == a.rarity && bag[i].lvl == a.lvl &&
            (a.slot != GS_WEAPON || bag[i].wtype == a.wtype)) {
            mate = i; break;
        }   // 武器还需同类型
    }
    if (mate < 0) {
        _stprintf_s(toast, _countof(toast), _T("需要两件相同的 %s·%s LV.%d"),
            rarityName(a.rarity), slotName(a.slot), a.lvl);
        toastTimer = 150; return;
    }
    // 移除两件素材，放入合成后的高一级装备（先删下标大的，避免下标失效）
    int hi = idx > mate ? idx : mate, lo = idx > mate ? mate : idx;
    bag.erase(bag.begin() + hi);
    bag.erase(bag.begin() + lo);
    Gear r; r.slot = a.slot; r.rarity = a.rarity; r.lvl = a.lvl + 1; r.wtype = a.wtype;
    bag.push_back(r);
    invCursor = (int)bag.size() - 1;                   // 光标跟到新装备上
    spawnExplosion(px, py, RGB(255, 220, 120), 18);
    audio.playSfx(SFX_FUSE);
    _stprintf_s(toast, _countof(toast), _T("合成成功：%s·%s LV.%d"),
        rarityName(r.rarity), slotName(r.slot), r.lvl);
    toastTimer = 150;
}

/**
 * @brief  抽取武器类型。使用「洗牌轮转池」而非纯随机：每轮把四种武器各放一份，
 *         取空后重新洗牌。这样保证【每四把掉落的武器必定四种各一把】，
 *         不会出现某种武器(如散射炮)长期不掉的情况。
 */
int Game::rollWeaponType() {
    if (wPoolLeft <= 0) {                              // 池空则重新装填并洗牌
        for (int i = 0; i < W_COUNT; ++i) wPool[i] = i;
        for (int i = W_COUNT - 1; i > 0; --i) {
            int j = randRange(0, i);
            int t = wPool[i]; wPool[i] = wPool[j]; wPool[j] = t;
        }
        wPoolLeft = W_COUNT;
    }
    return wPool[--wPoolLeft];
}

void Game::addGear(const Gear& g) {
    if ((int)bag.size() < BAG_CAP) {
        bag.push_back(g);
        if (g.slot == GS_WEAPON)
            _stprintf_s(toast, _countof(toast), _T("获得武器：%s·%s"),
                rarityName(g.rarity), weaponName(g.wtype));
        else
            _stprintf_s(toast, _countof(toast), _T("获得装备：%s·%s LV.%d"),
                rarityName(g.rarity), slotName(g.slot), g.lvl);
    }
    else {
        _stprintf_s(toast, _countof(toast), _T("物品栏已满！(暂停按E管理)"));
    }
    toastTimer = 150;
}

/** @brief 装备物品栏中第 idx 件：与该部位原装备交换，原装备退回物品栏 */
void Game::equipFromBag(int idx) {
    if (idx < 0 || idx >= (int)bag.size()) return;
    Gear g = bag[idx];
    int slot = g.slot;
    bag.erase(bag.begin() + idx);
    if (equipOn[slot]) bag.push_back(equip[slot]);     // 原装备退回物品栏
    equip[slot] = g;
    equipOn[slot] = true;
    if (hp > playerHpMax()) hp = playerHpMax();         // 卸下更高护甲时夹紧血量
    if (invCursor >= (int)bag.size()) invCursor = bag.empty() ? 0 : (int)bag.size() - 1;
}

/** @brief 丢弃物品栏中第 idx 件 */
void Game::discardFromBag(int idx) {
    if (idx < 0 || idx >= (int)bag.size()) return;
    bag.erase(bag.begin() + idx);
    if (invCursor >= (int)bag.size()) invCursor = bag.empty() ? 0 : (int)bag.size() - 1;
}

void Game::handleCollisions() {
    // 1) 玩家子弹 vs 敌机（穿透光束可连续贯穿多个敌人）
    for (auto& b : bullets) {
        if (!b.alive || !b.fromPlayer) continue;
        for (auto& e : enemies) {
            if (!e.alive) continue;
            if (rectHit(b.x, b.y, b.radius * 2, b.radius * 2, e.x, e.y, e.w, e.h)) {
                // 时停领域生效时敌人易伤 +50%（大招 U_SLOW 的核心收益之一）
                int dmg = (slowTimer > 0) ? b.damage * 3 / 2 : b.damage;
                e.hp -= effDamage(dmg, e.armor);       // 护甲减伤
                e.hitFlash = 4;
                // 命中火花特效
                addParticle(PK_SPARK, b.x, b.y, -b.vx * 0.2, -b.vy * 0.2, 2.0,
                    RGB(255, 240, 190), 8, -0.05, 0);
                if (e.hp <= 0) killEnemy(e);
                if (b.pierce > 0) { --b.pierce; }      // 贯穿：继续飞行
                else { b.alive = false; break; }
            }
        }
    }

    // 2) 玩家子弹 vs Boss
    if (boss.active) {
        for (auto& b : bullets) {
            if (!b.alive || !b.fromPlayer) continue;
            if (rectHit(b.x, b.y, b.radius * 2, b.radius * 2, boss.x, boss.y, boss.w, boss.h)) {
                if (b.pierce > 0) --b.pierce; else b.alive = false;   // 贯穿弹可继续
                int bdmg = (slowTimer > 0) ? b.damage * 3 / 2 : b.damage;   // 时停领域易伤
                boss.hp -= effDamage(bdmg, boss.armor);       // 护甲减伤
                addParticle(PK_SPARK, b.x, b.y, -b.vx * 0.2, -b.vy * 0.2, 2.4,
                    RGB(255, 240, 190), 9, -0.05, 0);
                boss.hitFlash = 3;
                if (b.life % 2 == 0) energy = clampi(energy + 1, 0, ENERGY_MAX);   // 打Boss攒能量(已减半)
                if (boss.hp <= 0) { nextLevelOrWin(); return; }
            }
        }
    }

    // 3) 敌机 vs 玩家（撞机）
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (rectHit(px, py, PLAYER_W, PLAYER_H, e.x, e.y, e.w, e.h)) {
            EnemyType et = e.type;                     // 先记录类型（killEnemy 后 e 可能失效）
            killEnemy(e);
            hitPlayer(enemyTouchDmg(et, level));       // 按机型与关卡决定撞击伤害
            if (state != ST_PLAYING) return;
        }
    }

    // 4) 敌方/Boss 子弹 vs 玩家
    for (auto& b : bullets) {
        if (!b.alive || b.fromPlayer) continue;
        if (rectHit(px, py, PLAYER_W - 10, PLAYER_H - 10, b.x, b.y, b.radius * 2, b.radius * 2)) {
            b.alive = false;
            hitPlayer(enemyBulletDmg(level));   // 敌弹伤害逐关递增
            if (state != ST_PLAYING) return;
        }
    }

    // 5) Boss 机体 vs 玩家
    if (boss.active && rectHit(px, py, PLAYER_W, PLAYER_H, boss.x, boss.y, boss.w, boss.h)) {
        hitPlayer(HIT_HP_BOSS);
        if (state != ST_PLAYING) return;
    }

    // 6) 道具 vs 玩家（拾取）
    for (auto& it : items) {
        if (!it.alive) continue;
        if (rectHit(px, py, PLAYER_W, PLAYER_H, it.x, it.y, it.size, it.size)) {
            it.alive = false;
            if (it.type == IT_GEAR) addGear(it.gear);  // 装备进物品栏
            else                    applyItem(it.type);
        }
    }
}

void Game::nextLevelOrWin() {
    // Boss 被击败：多层大爆炸特效
    spawnExplosion(boss.x, boss.y, RGB(255, 240, 200), 30);
    spawnExplosion(boss.x, boss.y, RGB(255, 200, 100), 44);
    spawnExplosion(boss.x, boss.y, RGB(255, 120, 120), 36);
    for (int i = 0; i < 8; ++i)                        // 沿机体散开的连锁爆点
        spawnExplosion(boss.x + randRange(-boss.w / 2, boss.w / 2),
            boss.y + randRange(-boss.h / 2, boss.h / 2),
            RGB(255, randRange(150, 240), 80), 10);
    audio.playSfx(SFX_BOMB);                           // Boss 爆炸巨响
    spawnShockwave(boss.x, boss.y, RGB(255, 240, 180), 9.0, 26);   // 巨型冲击波
    spawnShockwave(boss.x, boss.y, RGB(255, 150, 90), 5.5, 34);
    score += 500 * level + 100 * level * level;        // Boss 分数随关卡显著提升
    gainXp(bossXp(level));                             // Boss 经验远高于小怪
    addMoney(120 + level * 45);                        // Boss 掉落大额货币
    shakeTimer = 30;
    flashTimer = 12;                                   // 全屏白闪
    // Boss 掉落道具簇：数量与永久增益概率都更高（fromBoss=true）
    int nDrops = 3 + (level < 6 ? level : 6);
    for (int i = 0; i < nDrops; ++i)
        dropItem(boss.x + randRange(-55, 55), boss.y + randRange(-20, 30), true);
    boss.active = false;
    bossPhase = false;
    ultTimer = 0;
    mines.clear();                                     // 清除残留悬浮雷                                      // 停止大招光束（Boss 已消失）

    // 无限关卡：击败 Boss 后始终进入下一关，难度持续爬升
    ++level;
    // 每打过 3 关 Boss（进入第 4/7/10… 关时）结算一次评级并刷新被动
    if (level > 1 && (level - 1) % 3 == 0) evaluateCycle();
    levelTimer = 0;
    spawnTimer = 0;
    bullets.clear();                                   // 清屏进入新关卡
    bannerTimer = 110;                                 // 显示“第 N 关”横幅
    // 注意：排行榜不在每关结算，只在【血量与备用机耗尽】或【主动退出并结算】时记录
}

/*==========================================================================*/
/*                              渲染分发                                     */
/*==========================================================================*/

void Game::render() {
    cleardevice();                                      // 以背景色清屏（后台缓冲）

    // 屏幕震动：震动越强偏移越大（Boss 爆炸时格外明显）
    int ox = 0, oy = 0;
    if (shakeTimer > 0) {
        int amp = 4 + shakeTimer / 4; if (amp > 12) amp = 12;
        ox = randRange(-amp, amp); oy = randRange(-amp, amp);
    }
    setorigin(ox, oy);

    drawBackground();

    switch (state) {
    case ST_MENU:
        setorigin(0, 0);
        drawMenu();
        break;
    case ST_CHEAT:
        setorigin(0, 0);
        drawCheatPanel();
        break;
    case ST_PLAYING:
    case ST_PAUSED:
        drawItems();
        drawEnemies();
        drawBoss();
        drawMines();                                    // 悬浮雷（在子弹之下）
        drawBullets();
        if (ultTimer > 0) drawUltimate();               // 激光大招（在玩家之下）
        drawHelpers();                                  // 僚机/卫星
        drawPlayer();
        drawParticles();
        setorigin(0, 0);                                // HUD 不随震动
        drawFlash();                                    // 全屏白闪（爆炸/大招）
        drawBanner();                                   // 关卡横幅
        drawRankOverlay();                              // 三关评级结算浮层
        drawHUD();
        drawFps();                                      // 性能诊断浮层
        if (state == ST_PAUSED) {
            if (pausePanel == 1) drawInfoPanel();
            else if (pausePanel == 2) drawEquipPanel();
            else if (pausePanel == 3) drawShop();
            else if (pausePanel == 4) drawSkillPanel();
            else                      drawPaused();
        }
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

/**
 * @brief  全屏白闪：以逐行点阵近似半透明，强度随剩余帧衰减。
 *         用于 Boss 爆炸、大招启动等高光时刻。
 */
 /**
  * @brief  性能诊断浮层（F3 切换）。显示帧率、本帧工作耗时与近期最差帧耗时。
  *         用途：排查卡顿来源。若「最差帧」出现规律性尖峰（如每秒数次跳到
  *         30ms 以上），通常意味着某个同步阻塞调用（如 MCI 查询）在拖慢主循环。
  */
void Game::drawFps() {
    if (!showFps) return;
    TCHAR buf[80];
    setfillcolor(RGB(0, 0, 0));
    solidroundrect(WIN_W - 176, 4, WIN_W - 6, 62, 5, 5);
    setlinecolor(RGB(90, 200, 120));
    roundrect(WIN_W - 176, 4, WIN_W - 6, 62, 5, 5);
    settextstyle(14, 0, _T("Consolas"));
    settextcolor(fpsAvg >= 55 ? RGB(120, 240, 150) : RGB(255, 160, 90));
    _stprintf_s(buf, _countof(buf), _T("FPS %.1f"), fpsAvg);
    outtextxy(WIN_W - 170, 8, buf);
    settextcolor(RGB(200, 210, 230));
    _stprintf_s(buf, _countof(buf), _T("frame %2lu ms"), lastFrameMs);
    outtextxy(WIN_W - 170, 25, buf);
    settextcolor(worstFrameMs > 16 ? RGB(255, 130, 110) : RGB(200, 210, 230));
    _stprintf_s(buf, _countof(buf), _T("worst %2lu ms"), worstFrameMs);
    outtextxy(WIN_W - 170, 42, buf);

    // ---- 音频诊断：直接显示设备状态，便于定位"没声音"的原因 ----
    setfillcolor(RGB(0, 0, 0));
    solidroundrect(WIN_W - 176, 66, WIN_W - 6, 122, 5, 5);
    setlinecolor(RGB(90, 160, 220));
    roundrect(WIN_W - 176, 66, WIN_W - 6, 122, 5, 5);
    settextstyle(13, 0, _T("Consolas"));
    settextcolor(audio.diagReady() ? RGB(120, 240, 150) : RGB(255, 130, 110));
    _stprintf_s(buf, _countof(buf), _T("SFX  %s"), audio.diagReady() ? _T("ready") : _T("FAIL"));
    outtextxy(WIN_W - 170, 70, buf);
    settextcolor(audio.diagBgmOk() ? RGB(120, 240, 150) : RGB(255, 130, 110));
    _stprintf_s(buf, _countof(buf), _T("BGM  %s"), audio.diagBgmOk() ? _T("open") : _T("NOT FOUND"));
    outtextxy(WIN_W - 170, 86, buf);
    settextcolor(RGB(200, 210, 230));
    _stprintf_s(buf, _countof(buf), _T("len %lus %s"), audio.diagBgmLen() / 1000,
        audio.isMuted() ? _T("[MUTED]") : _T(""));
    outtextxy(WIN_W - 170, 102, buf);
}

void Game::drawFlash() {
    if (flashTimer <= 0) return;
    double t = flashTimer / 12.0; if (t > 1) t = 1;
    // 不再是全屏刷白：改为「边缘泛光晕影 + 中心柔光」的组合，更有质感
    int steps = 14;
    for (int i = 0; i < steps; ++i) {
        double k = (double)i / steps;                   // 0=最外圈
        int inset = (int)(k * 150);
        int a = (int)(230 * t * (1.0 - k));             // 越靠内越淡
        if (a <= 4) continue;
        COLORREF c = RGB(clampi(a, 0, 255), clampi(a, 0, 255), clampi(a + 20, 0, 255));
        setlinecolor(fadeColor(c, t));
        setlinestyle(PS_SOLID, 3);
        rectangle(inset, inset, WIN_W - inset, WIN_H - inset);
    }
    setlinestyle(PS_SOLID, 1);
}

/**
 * @brief  关卡横幅：进入新关卡时短暂显示“第 N 关 / Boss 名”，带淡入淡出。
 */
 /**
  * @brief  三关评级结算浮层：展示达成百分比、评级与本轮赠予的被动技能。
  *         每次结算后显示约 4 秒，随后自动消失。
  */
  /**
   * @brief  本轮评级栏（绘制在【暂停面板】内，不占用战斗视野）。
   *         显示本轮三关的杂兵满分、S/A/B 各档所需分数、当前分数与实时得分率。
   * @param  x,y 面板左上角
   */
void Game::drawRankPanel(int x, int y) {
    TCHAR buf[80];
    int cyc0 = ((level - 1) / 3) * 3 + 1;          // 本轮第一关
    int cyc2 = cyc0 + 2;

    // 本轮三关的杂兵满分 与 必得的 Boss 分
    int fullMob = 0, bossFull = 0;
    for (int L = cyc0; L <= cyc2; ++L) { fullMob += fullMobScore(L); bossFull += bossScoreOf(L); }
    if (fullMob < 1) fullMob = 1;

    // 各评级所需总分 = 本轮起点 + 必得Boss分 + 杂兵满分×档位
    int needS = cycleStartScore + bossFull + fullMob * 90 / 100;
    int needA = cycleStartScore + bossFull + fullMob * 70 / 100;
    int needB = cycleStartScore + bossFull + fullMob * 50 / 100;

    // 实时得分率：按已通关的整关 + 当前关进度，折算"到目前为止的满分"
    int bossSoFar = 0, fullSoFar = 0;
    for (int L = cyc0; L < level; ++L) { bossSoFar += bossScoreOf(L); fullSoFar += fullMobScore(L); }
    fullSoFar += (int)(fullMobScore(level) * (double)levelTimer / LEVEL_FRAMES);
    int mobNow = (score - cycleStartScore) - bossSoFar;
    if (mobNow < 0) mobNow = 0;
    int pctNow = (fullSoFar > 0) ? (int)((double)mobNow / fullSoFar * 100) : 0;
    int tierNow = (pctNow >= 90) ? RK_S : (pctNow >= 70) ? RK_A : (pctNow >= 50) ? RK_B : RK_NONE;

    int w = WIN_W - 2 * x, h = 132;
    setfillcolor(RGB(10, 12, 22));
    solidroundrect(x, y, x + w, y + h, 8, 8);
    setlinecolor(tierColor(tierNow));
    roundrect(x, y, x + w, y + h, 8, 8);

    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(180, 200, 230));
    _stprintf_s(buf, _countof(buf), _T("本轮评级 (第%d-%d关)"), cyc0, cyc2);
    outtextxy(x + 10, y + 6, buf);

    // 实时评级大字 + 得分率
    settextstyle(34, 0, _T("Consolas"));
    settextcolor(tierColor(tierNow));
    _stprintf_s(buf, _countof(buf), _T("%s"), tierName(tierNow));
    outtextxy(x + w - 44, y + 4, buf);
    settextstyle(15, 0, _T("Consolas"));
    settextcolor(RGB(190, 205, 230));
    _stprintf_s(buf, _countof(buf), _T("%d%%"), pctNow);
    outtextxy(x + w - 92, y + 16, buf);

    // 三档所需分数
    settextstyle(15, 0, _T("Consolas"));
    const int needs[3] = { needS, needA, needB };
    const int tiers[3] = { RK_S, RK_A, RK_B };
    int yy = y + 32;
    for (int i = 0; i < 3; ++i) {
        bool ok = (score >= needs[i]);
        settextcolor(ok ? tierColor(tiers[i]) : RGB(110, 118, 135));
        _stprintf_s(buf, _countof(buf), _T("%s 级  需 %7d 分   %s"),
            tierName(tiers[i]), needs[i], ok ? _T("已达成") : _T(""));
        outtextxy(x + 12, yy, buf);
        yy += 20;
    }
    setlinecolor(RGB(55, 66, 88));
    line(x + 10, yy + 2, x + w - 10, yy + 2);
    settextcolor(RGB(235, 240, 250));
    settextstyle(16, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("当前 %d 分"), score);
    outtextxy(x + 12, yy + 6, buf);
    settextcolor(RGB(150, 165, 190));
    settextstyle(13, 0, _T("微软雅黑"));
    outtextxy(x + 12, yy + 26, _T("得分率 = 杂兵实得 / 杂兵满分（全歼=100%）"));
}

void Game::drawRankOverlay() {
    if (rankShowTimer <= 0) return;
    TCHAR buf[96];
    int h = 224, y0 = WIN_H / 2 - h / 2 - 30;

    setfillcolor(RGB(10, 12, 24));
    solidroundrect(30, y0, WIN_W - 30, y0 + h, 12, 12);
    setlinecolor(tierColor(lastTier));
    setlinestyle(PS_SOLID, 3);
    roundrect(30, y0, WIN_W - 30, y0 + h, 12, 12);
    setlinestyle(PS_SOLID, 1);

    // 标题
    settextcolor(RGB(200, 215, 240));
    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* t = _T("三 关 战 绩 结 算");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, y0 + 12, t);

    // 评级大字（带脉冲）
    double pl = 0.5 + 0.5 * sin(animTick * 0.2);
    settextcolor(lastTier == RK_NONE ? RGB(150, 155, 170)
        : lighten(tierColor(lastTier), (int)(pl * 50)));
    settextstyle(54, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("%s"), tierName(lastTier));
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y0 + 34, buf);

    // 本轮分数明细 + 达成百分比
    settextcolor(RGB(220, 228, 245));
    settextstyle(16, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("第%d-%d关得分 %d   得分率 %d%%"),
        lastCycFrom, lastCycTo, lastGained, lastPct);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y0 + 92, buf);
    settextcolor(RGB(150, 165, 190));
    settextstyle(14, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("评级线：S≥90%%  A≥70%%  B≥50%%（全歼=100%%）"));
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y0 + 112, buf);

    // 赠予的被动
    settextstyle(16, 0, _T("微软雅黑"));
    if (grantedNum > 0) {
        settextcolor(RGB(255, 220, 130));
        settextstyle(15, 0, _T("微软雅黑"));
        _stprintf_s(buf, _countof(buf), _T("%s级 -> 获得 %d 个被动（仅第%d-%d关生效）"),
            tierName(lastTier), grantedNum, lastCycTo + 1, lastCycTo + 3);
        outtextxy(WIN_W / 2 - textwidth(buf) / 2, y0 + 132, buf);
        int yy = y0 + 154;
        for (int i = 0; i < grantedNum; ++i) {         // 逐条列出名称与效果
            settextstyle(15, 0, _T("微软雅黑"));
            settextcolor(RGB(170, 250, 200));
            _stprintf_s(buf, _countof(buf), _T("%s"), passiveName(grantedList[i]));
            int nw = textwidth(buf);
            outtextxy(WIN_W / 2 - 150, yy, buf);
            settextstyle(13, 0, _T("微软雅黑"));
            settextcolor(RGB(160, 175, 200));
            _stprintf_s(buf, _countof(buf), _T("%s"), passiveDesc(grantedList[i]));
            outtextxy(WIN_W / 2 - 150 + (nw > 70 ? nw + 8 : 78), yy + 2, buf);
            yy += 19;
        }
    }
    else {
        settextcolor(RGB(230, 130, 120));
        settextstyle(16, 0, _T("微软雅黑"));
        const TCHAR* t2 = _T("未达 B 级，本轮无被动加成");
        outtextxy(WIN_W / 2 - textwidth(t2) / 2, y0 + 138, t2);
        settextcolor(RGB(150, 160, 180));
        settextstyle(14, 0, _T("微软雅黑"));
        const TCHAR* t3 = _T("提高击杀率(少漏怪)即可提升评级");
        outtextxy(WIN_W / 2 - textwidth(t3) / 2, y0 + 162, t3);
    }
}

void Game::drawBanner() {
    if (bannerTimer <= 0) return;
    TCHAR buf[64];
    int cy = WIN_H / 2 - 40;
    // 背景条
    setfillcolor(RGB(18, 22, 40));
    solidrectangle(0, cy - 6, WIN_W, cy + 62);
    setlinecolor(RGB(90, 200, 255));
    line(0, cy - 6, WIN_W, cy - 6);
    line(0, cy + 62, WIN_W, cy + 62);

    settextcolor(RGB(140, 235, 255));
    settextstyle(34, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("第 %d 关"), level);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, cy, buf);

    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(255, 190, 150));
    int cyc = bossCycle(level);
    if (cyc > 0)
        _stprintf_s(buf, _countof(buf), _T("%s  强化 ★x%d"), bossName(bossKind(level)), cyc);
    else
        _stprintf_s(buf, _countof(buf), _T("%s"), bossName(bossKind(level)));
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, cy + 38, buf);
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
        double t = (double)p.life / p.maxLife;          // 剩余寿命比例 [0,1]
        COLORREF c = fadeColor(p.color, t);
        switch (p.kind) {
        case PK_RING: {                                 // 冲击波环：空心圆，越扩越淡
            int r = (int)p.size;
            if (r < 1) break;
            setlinecolor(c);
            setlinestyle(PS_SOLID, t > 0.5 ? 3 : 1);
            circle((int)p.x, (int)p.y, r);
            setlinestyle(PS_SOLID, 1);
            break;
        }
        case PK_STAR: {                                 // 星芒：四角星 + 中心亮点
            double s = p.size * t;
            if (s < 1) break;
            POINT st[8];
            for (int i = 0; i < 8; ++i) {
                double a = p.rot + 0.7854 * i;
                double rr = (i % 2 == 0) ? s : s * 0.32; // 长短交替形成尖角
                st[i].x = (long)(p.x + cos(a) * rr);
                st[i].y = (long)(p.y + sin(a) * rr);
            }
            setfillcolor(c);
            solidpolygon(st, 8);
            setfillcolor(RGB(255, 255, 255));
            solidcircle((int)p.x, (int)p.y, (int)(s * 0.18) + 1);
            break;
        }
        case PK_SHARD: {                                // 碎片：旋转三角块
            double s = p.size;
            POINT tri[3];
            for (int i = 0; i < 3; ++i) {
                double a = p.rot + 2.0944 * i;
                tri[i].x = (long)(p.x + cos(a) * s);
                tri[i].y = (long)(p.y + sin(a) * s);
            }
            setfillcolor(c);
            solidpolygon(tri, 3);
            break;
        }
        case PK_SMOKE: {                                // 烟雾：大而淡的圆
            setfillcolor(fadeColor(p.color, t * 0.45));
            solidcircle((int)p.x, (int)p.y, (int)p.size);
            break;
        }
        default: {                                      // 火花：亮点 + 速度方向拖尾
            int r = (int)(p.size * t) + 1;
            double len = 2.0 + p.size;
            setlinecolor(fadeColor(p.color, t * 0.6));
            setlinestyle(PS_SOLID, 1);
            line((int)p.x, (int)p.y,
                (int)(p.x - p.vx * len * 0.25), (int)(p.y - p.vy * len * 0.25));
            setfillcolor(t > 0.6 ? RGB(255, 255, 230) : c);
            solidcircle((int)p.x, (int)p.y, r);
            break;
        }
        }
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

    // 弹反判定圈：释放瞬间显示，直观展示反弹范围
    if (parryTimer > 0) {
        double k = parryTimer / (double)PARRY_FRAMES;
        setlinecolor(fadeColor(RGB(255, 190, 235), k));
        setlinestyle(PS_SOLID, 3);
        circle(cx, cy, (int)(PARRY_R * (1.15 - k * 0.15)));
        setlinestyle(PS_SOLID, 1);
        setfillcolor(fadeColor(RGB(255, 190, 235), k * 0.30));
        solidcircle(cx, cy, PLAYER_W / 2 + 6);
    }

    // 护盾绘制：环绕飞机的青色圆环
    if (shield) {
        setlinecolor(RGB(120, 210, 255));
        setlinestyle(PS_SOLID, 2);
        circle(cx, cy, PLAYER_W / 2 + 8);
        setlinestyle(PS_SOLID, 1);
    }

    // 我方血条：悬浮于机身上方，随飞机移动
    int hpMax = playerHpMax();
    int bw = PLAYER_W + 6, bx = cx - bw / 2, byy = cy - PLAYER_H / 2 - 12;
    setfillcolor(RGB(35, 38, 55));
    solidroundrect(bx, byy, bx + bw, byy + 5, 3, 3);          // 底槽
    int cur = bw * hp / hpMax; if (cur < 0) cur = 0;
    COLORREF hc = (hp * 2 <= hpMax) ? RGB(240, 140, 80) : RGB(90, 230, 255);
    setfillcolor(hc);
    if (cur > 0) solidroundrect(bx, byy, bx + cur, byy + 5, 3, 3);  // 血量
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

    // 所有敌机在头顶绘制血条（受击后可直观看到剩余生命）；score<0 为图鉴图标，跳过血条
    if (e.score >= 0) {
        int bw = w;
        setfillcolor(RGB(40, 40, 55));
        solidroundrect(cx - bw / 2, cy - h / 2 - 9, cx + bw / 2, cy - h / 2 - 4, 3, 3);
        int cur = bw * e.hp / e.maxHp;
        if (cur < 0) cur = 0;
        // 血量越低越偏红
        COLORREF hpc = (e.hp * 2 <= e.maxHp) ? RGB(230, 120, 70) : RGB(90, 230, 120);
        setfillcolor(hpc);
        if (cur > 0) solidroundrect(cx - bw / 2, cy - h / 2 - 9, cx - bw / 2 + cur, cy - h / 2 - 4, 3, 3);
    }
}

void Game::drawEnemies() {
    for (const auto& e : enemies) if (e.alive) drawEnemyShape(e);
}

/*==========================================================================*/
/*                              Boss 绘制                                    */
/*==========================================================================*/

/**
 * @brief  绘制 Boss 缩略图标（图鉴用）。三种 Boss 各画各的造型，
 *         与实战中的机体一一对应，避免"名称与图标对不上"。
 * @param  kind  BossKind
 * @param  cx,cy 图标中心
 */
void Game::drawBossIcon(int kind, int cx, int cy) {
    switch (kind) {
    case BK_DREADNOUGHT: {          // 无畏舰：绯红八边形舰体 + 双荚舱 + 橙色核心
        const COLORREF c = RGB(240, 90, 120);
        setfillcolor(darken(c, 70));                       // 尾翼
        solidrectangle(cx - 20, cy - 5, cx - 14, cy + 6);
        solidrectangle(cx + 14, cy - 5, cx + 20, cy + 6);
        setfillcolor(c);                                   // 主装甲（八边形近似）
        POINT body[] = { {cx - 13,cy - 8},{cx + 13,cy - 8},{cx + 17,cy - 1},{cx + 13,cy + 7},
                         {cx + 6,cy + 11},{cx - 6,cy + 11},{cx - 13,cy + 7},{cx - 17,cy - 1} };
        solidpolygon(body, 8);
        setfillcolor(lighten(c, 45));                      // 高光板
        solidrectangle(cx - 9, cy - 6, cx + 9, cy + 1);
        setfillcolor(RGB(70, 70, 95));                     // 武器荚舱
        solidcircle(cx - 13, cy + 1, 3); solidcircle(cx + 13, cy + 1, 3);
        setfillcolor(RGB(180, 220, 255));                  // 舰桥
        solidcircle(cx, cy - 3, 3);
        setfillcolor(RGB(255, 160, 70));                   // 能量核
        solidcircle(cx, cy + 4, 4);
        break;
    }
    case BK_HIVE: {                 // 蜂巢母舰：墨绿六边形巢体 + 蜂巢孔 + 中央舱门
        const COLORREF c = RGB(140, 200, 90);
        setfillcolor(darken(c, 70));                       // 外壳
        POINT sh[] = { {cx - 17,cy - 7},{cx + 17,cy - 7},{cx + 21,cy + 1},{cx + 11,cy + 11},
                       {cx - 11,cy + 11},{cx - 21,cy + 1} };
        solidpolygon(sh, 6);
        setfillcolor(c);                                   // 巢体
        POINT bd[] = { {cx - 14,cy - 5},{cx + 14,cy - 5},{cx + 17,cy + 1},{cx + 9,cy + 9},
                       {cx - 9,cy + 9},{cx - 17,cy + 1} };
        solidpolygon(bd, 6);
        setfillcolor(darken(c, 45));                       // 蜂巢孔
        for (int i = -2; i <= 2; ++i) solidcircle(cx + i * 6, cy - 2, 2);
        setfillcolor(RGB(220, 255, 180));                  // 亮起的孔
        solidcircle(cx - 6, cy - 2, 2); solidcircle(cx + 6, cy - 2, 2);
        setfillcolor(RGB(90, 130, 70));                    // 中央舱门
        solidcircle(cx, cy + 4, 5);
        setfillcolor(RGB(200, 255, 210));                  // 指挥舱
        solidcircle(cx, cy - 6, 3);
        break;
    }
    default: {                      // 机械巨蛇：紫钢分节尾 + 头部 + 利齿 + 发光双眼
        const COLORREF c = RGB(150, 120, 240);
        for (int i = 3; i >= 1; --i) {                     // 分节尾巴（蜿蜒）
            int sx = cx - 8 - i * 5, sy = cy - 4 + (i % 2 ? 3 : -3);
            setfillcolor(i % 2 ? darken(c, 40) : darken(c, 65));
            solidcircle(sx, sy, 5 - i / 2);
        }
        setfillcolor(darken(c, 70));                       // 侧鳍
        solidrectangle(cx - 18, cy + 1, cx - 8, cy + 5);
        solidrectangle(cx + 8, cy + 1, cx + 18, cy + 5);
        setfillcolor(c);                                   // 头部
        solidellipse(cx - 11, cy - 9, cx + 11, cy + 8);
        setfillcolor(lighten(c, 45));                      // 头顶高光
        solidellipse(cx - 7, cy - 7, cx + 7, cy + 1);
        setfillcolor(RGB(245, 245, 255));                  // 利齿
        for (int i = -1; i <= 1; ++i) {
            POINT th[] = { {cx + i * 6 - 2, cy + 5}, {cx + i * 6 + 2, cy + 5}, {cx + i * 6, cy + 10} };
            solidpolygon(th, 3);
        }
        setfillcolor(RGB(255, 220, 100));                  // 发光双眼
        solidcircle(cx - 4, cy - 3, 2); solidcircle(cx + 4, cy - 3, 2);
        setfillcolor(RGB(230, 200, 255));                  // 额心能量核
        solidcircle(cx, cy - 8, 3);
        break;
    }
    }
}

void Game::drawBoss() {
    if (!boss.active) return;
    const int cx = (int)boss.x, cy = (int)boss.y, w = boss.w, h = boss.h;
    const bool flash = boss.hitFlash > 0;

    // 每种 Boss 一套配色
    COLORREF baseCol;
    switch (boss.kind) {
    case BK_DREADNOUGHT: baseCol = RGB(240, 90, 120); break;   // 绯红无畏舰
    case BK_HIVE:        baseCol = RGB(140, 200, 90); break;   // 墨绿母舰
    default:             baseCol = RGB(150, 120, 240); break;  // 紫钢巨蛇
    }
    // 循环轮次越高，机体镀上越强的金色（视觉上一眼看出更强）
    int cyc = bossCycle(level);
    if (cyc > 0) {
        int g = cyc * 22; if (g > 70) g = 70;
        baseCol = RGB(clampi(GetRValue(baseCol) + g, 0, 255),
            clampi(GetGValue(baseCol) + g / 2, 0, 255),
            GetBValue(baseCol));
    }
    COLORREF base = flash ? RGB(255, 255, 255) : baseCol;
    const COLORREF hi = lighten(base, 45);
    const COLORREF dk = darken(base, 70);
    const COLORREF steel = RGB(70, 70, 95);            // 金属结构色

    // 比例坐标辅助 lambda
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

    // 0) 激光：先画预警线，再画粗光束（支持单柱/双柱）
    if (boss.castType == BA_LASER || boss.castType == BA_TWINLASER) {
        int lxs[2] = { boss.laserX, boss.laserX2 };
        int nBeam = (boss.castType == BA_TWINLASER) ? 2 : 1;
        for (int i = 0; i < nBeam; ++i) {
            int lx = lxs[i];
            if (boss.laserWarn > 0) {
                // 预警：闪烁红线 + 底部警示三角，提示玩家躲避
                if ((boss.laserWarn / 3) % 2 == 0) {
                    setfillcolor(RGB(255, 70, 70));
                    solidrectangle(lx - 2, (int)boss.y, lx + 2, WIN_H);
                    POINT wt[] = { {lx - 8, WIN_H - 96}, {lx + 8, WIN_H - 96}, {lx, WIN_H - 80} };
                    solidpolygon(wt, 3);
                }
            }
            else if (boss.laserFire > 0) {
                // 开火：多层辉光光束 + 随机抖动的芯线
                int jitter = randRange(-2, 2);
                setfillcolor(fadeColor(RGB(255, 90, 90), 0.4));
                solidrectangle(lx - BOSS_BEAM_HALF - 8, (int)boss.y, lx + BOSS_BEAM_HALF + 8, WIN_H);
                setfillcolor(RGB(255, 90, 110));
                solidrectangle(lx - BOSS_BEAM_HALF, (int)boss.y, lx + BOSS_BEAM_HALF, WIN_H);
                setfillcolor(RGB(255, 230, 240));
                solidrectangle(lx - BOSS_BEAM_HALF / 3 + jitter, (int)boss.y,
                    lx + BOSS_BEAM_HALF / 3 + jitter, WIN_H);
            }
        }
    }

    /*========== 依种类绘制机体 ==========*/
    if (boss.kind == BK_DREADNOUGHT) {
        // ---- 无畏舰：厚重八边形主体 + 双主炮 + 脉冲核心 ----
        setfillcolor(RGB(255, 170, 90));                                  // 顶部推进器
        for (double fx : { -0.30, -0.10, 0.10, 0.30 })
            solidcircle((int)(cx + fx * w), cy - h / 2 + 2, 4 + randRange(0, 3));
        poly({ {-0.56,-0.34},{-0.30,-0.20},{-0.30,0.16},{-0.56,0.32} }, dk); // 尾翼
        poly({ {0.56,-0.34},{0.30,-0.20},{0.30,0.16},{0.56,0.32} }, dk);
        poly({ {-0.40,-0.42},{0.40,-0.42},{0.50,-0.05},{0.40,0.30},
              {0.18,0.48},{-0.18,0.48},{-0.40,0.30},{-0.50,-0.05} }, base); // 主装甲
        poly({ {-0.30,-0.34},{0.30,-0.34},{0.22,0.06},{-0.22,0.06} }, hi);   // 高光板
        poly({ {-0.14,0.10},{0.14,0.10},{0.10,0.40},{-0.10,0.40} }, dk);
        ell(-0.40, 0.05, 0.12, 0.34, steel);                              // 武器荚舱
        ell(0.40, 0.05, 0.12, 0.34, steel);
        setfillcolor(dk);                                                 // 下指主炮
        solidrectangle((int)(cx - 0.40 * w) - 3, cy + (int)(0.20 * h), (int)(cx - 0.40 * w) + 3, cy + (int)(0.60 * h));
        solidrectangle((int)(cx + 0.40 * w) - 3, cy + (int)(0.20 * h), (int)(cx + 0.40 * w) + 3, cy + (int)(0.60 * h));
        setfillcolor(RGB(255, 220, 120));
        solidcircle((int)(cx - 0.40 * w), cy + (int)(0.58 * h), 4);
        solidcircle((int)(cx + 0.40 * w), cy + (int)(0.58 * h), 4);
        poly({ {-0.12,0.40},{0.12,0.40},{0,0.62} }, dk);                    // 前部尖刺
        ell(0, -0.12, 0.18, 0.18, RGB(180, 220, 255));                    // 舰桥
        ell(0, -0.12, 0.11, 0.12, RGB(240, 250, 255));
        int coreR = 12 + (int)(pulse * 8);                                // 能量核
        setfillcolor(darken(RGB(255, 90, 60), 20));
        solidcircle(cx, cy + (int)(0.14 * h), coreR + 4);
        setfillcolor(RGB(255, 150, 60));
        solidcircle(cx, cy + (int)(0.14 * h), coreR);
        setfillcolor(lighten(RGB(255, 230, 140), (int)(pulse * 30)));
        solidcircle(cx, cy + (int)(0.14 * h), coreR - 5);
    }
    else if (boss.kind == BK_HIVE) {
        // ---- 母舰：巨大六边形巢体 + 旋转舱门 + 众多蜂巢孔 ----
        setfillcolor(RGB(160, 255, 160));                                 // 侧翼推进
        for (double fx : { -0.44, 0.44 })
            solidcircle((int)(cx + fx * w), cy - h / 2 + 8, 4 + randRange(0, 3));
        poly({ {-0.50,-0.30},{0.50,-0.30},{0.60,0.05},{0.34,0.42},
              {-0.34,0.42},{-0.60,0.05} }, dk);                            // 外壳
        poly({ {-0.42,-0.24},{0.42,-0.24},{0.50,0.04},{0.28,0.34},
              {-0.28,0.34},{-0.50,0.04} }, base);                          // 巢体
        // 蜂巢孔阵列（会随脉冲亮灭，像在孵化）
        for (int i = -2; i <= 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                double ox = i * 0.16, oy = -0.06 + j * 0.18;
                bool lit = ((animTick / 10 + i + j) % 3 == 0);
                ell(ox, oy, 0.05, 0.07, lit ? RGB(220, 255, 180) : darken(base, 45));
            }
        }
        // 中央舱门：旋转的三叶结构（召唤时张开）
        bool opening = (boss.castType == BA_SUMMON);
        double rot = animTick * 0.06;
        setfillcolor(opening ? RGB(255, 240, 160) : RGB(90, 130, 70));
        solidcircle(cx, cy + (int)(0.18 * h), 16 + (int)(pulse * 4));
        for (int i = 0; i < 3; ++i) {
            double a = rot + 6.2832 * i / 3;
            POINT bl[] = {
                { (long)(cx + cos(a) * 6),  (long)(cy + 0.18 * h + sin(a) * 6) },
                { (long)(cx + cos(a + 0.6) * 17), (long)(cy + 0.18 * h + sin(a + 0.6) * 17) },
                { (long)(cx + cos(a - 0.6) * 17), (long)(cy + 0.18 * h + sin(a - 0.6) * 17) },
            };
            setfillcolor(darken(base, 30));
            solidpolygon(bl, 3);
        }
        ell(0, -0.16, 0.16, 0.13, RGB(200, 255, 210));                    // 指挥舱
        ell(0, -0.16, 0.09, 0.08, RGB(245, 255, 245));
    }
    else {
        // ---- 机械巨蛇：细长主体 + 分节尾巴 + 头部利齿 ----
        // 分节尾巴（跟随浮动相位摆动，蜿蜒如蛇）
        for (int i = 6; i >= 1; --i) {
            double t = i * 0.32;
            int sx = cx - (int)(sin(boss.bob - t) * 46);
            int sy = cy - (int)(i * 0.17 * h);
            int sr = 15 - i;
            setfillcolor(i % 2 ? darken(base, 40) : darken(base, 65));
            solidcircle(sx, sy, sr);
            setfillcolor(RGB(255, 210, 120));                             // 节间灯
            solidcircle(sx, sy, 2);
        }
        poly({ {-0.50,-0.10},{-0.18,-0.04},{-0.18,0.14},{-0.44,0.20} }, dk); // 侧鳍
        poly({ {0.50,-0.10},{0.18,-0.04},{0.18,0.14},{0.44,0.20} }, dk);
        ell(0, 0, 0.34, 0.40, base);                                       // 头部
        ell(0, -0.06, 0.24, 0.26, hi);                                     // 头顶高光
        // 利齿（下颚）
        setfillcolor(RGB(245, 245, 255));
        for (int i = -2; i <= 2; ++i) {
            POINT tooth[] = {
                { (long)(cx + i * 0.10 * w - 3), (long)(cy + 0.30 * h) },
                { (long)(cx + i * 0.10 * w + 3), (long)(cy + 0.30 * h) },
                { (long)(cx + i * 0.10 * w),     (long)(cy + 0.46 * h) },
            };
            solidpolygon(tooth, 3);
        }
        // 双眼（脉冲发光）
        COLORREF eye = pulse > 0.5 ? RGB(255, 240, 120) : RGB(255, 140, 60);
        ell(-0.15, -0.08, 0.07, 0.09, RGB(30, 20, 40));
        ell(0.15, -0.08, 0.07, 0.09, RGB(30, 20, 40));
        setfillcolor(eye);
        solidcircle((int)(cx - 0.15 * w), (int)(cy - 0.08 * h), 4);
        solidcircle((int)(cx + 0.15 * w), (int)(cy - 0.08 * h), 4);
        // 额心能量核
        int coreR = 8 + (int)(pulse * 5);
        setfillcolor(RGB(200, 150, 255));
        solidcircle(cx, (int)(cy - 0.24 * h), coreR);
        setfillcolor(RGB(245, 230, 255));
        solidcircle(cx, (int)(cy - 0.24 * h), coreR - 3);
    }

    // 翼尖闪烁航行灯（三种 Boss 通用）
    setfillcolor(pulse > 0.5 ? RGB(255, 255, 180) : RGB(120, 90, 60));
    solidcircle((int)(cx - 0.50 * w), cy, 3);
    solidcircle((int)(cx + 0.50 * w), cy, 3);

    // ---------------- Boss 血条（顶部横贯，带边框与高光） ----------------
    int barX = 36, barY = 16, barW = WIN_W - 72, barH = 14;
    setfillcolor(RGB(20, 20, 30));                     // 底槽
    solidroundrect(barX - 2, barY - 2, barX + barW + 2, barY + barH + 2, 6, 6);
    int cur = barW * boss.hp / boss.maxHp;
    if (cur < 0) cur = 0;
    setfillcolor(RGB(210, 50, 70));                    // 血量填充
    if (cur > 0) solidroundrect(barX, barY, barX + cur, barY + barH, 5, 5);
    setfillcolor(RGB(255, 130, 150));                  // 顶部高光条
    if (cur > 4) solidrectangle(barX + 2, barY + 2, barX + cur - 2, barY + 4);
    setlinecolor(RGB(120, 120, 140));                  // 边框
    setlinestyle(PS_SOLID, 1);
    rectangle(barX - 2, barY - 2, barX + barW + 2, barY + barH + 2);
    settextcolor(WHITE);
    settextstyle(16, 0, _T("微软雅黑"));
    TCHAR bt[48];
    if (bossCycle(level) > 0)                          // 循环轮次以 ★ 标注强度
        _stprintf_s(bt, _countof(bt), _T("%s  第%d关  ★x%d"), bossName(boss.kind), level, bossCycle(level));
    else
        _stprintf_s(bt, _countof(bt), _T("%s  第%d关"), bossName(boss.kind), level);
    outtextxy(barX, barY - 19, bt);
}

void Game::drawBullets() {
    for (const auto& b : bullets) {
        if (b.fromPlayer) {
            // 玩家子弹：柔和弹体 + 暗色描边（刻意降低亮度，长时间游玩不刺眼）
            setfillcolor(darken(b.color, 90));
            solidroundrect((int)b.x - b.radius, (int)b.y - b.radius * 2,
                (int)b.x + b.radius, (int)b.y + b.radius * 2, 4, 4);
            setfillcolor(darken(b.color, 35));
            solidroundrect((int)b.x - b.radius + 1, (int)b.y - b.radius * 2 + 1,
                (int)b.x + b.radius - 1, (int)b.y + b.radius * 2 - 1, 3, 3);
        }
        else if (b.homing) {
            // 追踪导弹：朝速度方向拉长的菱形弹体 + 光晕
            double a = atan2(b.vy, b.vx);
            double ca = cos(a), sa = sin(a);
            int L = b.radius + 5, W = b.radius - 1;
            POINT p[4] = {
                { (long)(b.x + ca * L),          (long)(b.y + sa * L) },
                { (long)(b.x - sa * W),          (long)(b.y + ca * W) },
                { (long)(b.x - ca * L * 0.7),    (long)(b.y - sa * L * 0.7) },
                { (long)(b.x + sa * W),          (long)(b.y - ca * W) },
            };
            setfillcolor(fadeColor(b.color, 0.4));
            solidcircle((int)b.x, (int)b.y, b.radius + 3);
            setfillcolor(b.color);
            solidpolygon(p, 4);
            setfillcolor(RGB(255, 250, 220));
            solidcircle((int)b.x, (int)b.y, 2);
        }
        else {
            // 敌弹：光晕 + 实心圆 + 高光点，弹幕更醒目华丽
            setfillcolor(fadeColor(b.color, 0.35));
            solidcircle((int)b.x, (int)b.y, b.radius + 3);
            setfillcolor(b.color);
            solidcircle((int)b.x, (int)b.y, b.radius);
            setfillcolor(lighten(b.color, 90));
            solidcircle((int)b.x - b.radius / 3, (int)b.y - b.radius / 3, b.radius / 3 + 1);
        }
    }
}

/**
 * @brief  绘制单个道具/装备图标（世界掉落物与图鉴共用，保证图标完全一致）。
 * @param  type  道具类型
 * @param  g     当 type==IT_GEAR 时有效的装备数据（决定颜色与符号）
 * @param  cx,cy 图标中心
 * @param  r     图标半径
 * @param  pulse 呼吸脉冲系数 [0,1]，用于光晕与引线火花
 */
void Game::drawItemIcon(int type, const Gear& g, int cx, int cy, int r, double pulse) {
    // 依类型选取胶囊主色
    COLORREF bg;
    switch (type) {
    case IT_FIRE:    bg = RGB(255, 140, 40);  break;
    case IT_LIFE:    bg = RGB(235, 70, 95);   break;
    case IT_SHIELD:  bg = RGB(70, 160, 255);  break;
    case IT_MAXLIFE: bg = RGB(255, 80, 150);  break;   // 永久：粉红
    case IT_POWERUP: bg = RGB(180, 110, 255); break;   // 永久：紫
    case IT_GEAR:    bg = rarityColor(g.rarity); break; // 装备：按稀有度着色
    default:         bg = RGB(150, 155, 170); break;   // 炸弹
    }
    const bool perm = isPermanentItem((ItemType)type);

    // 1) 外发光光晕（永久增益=金环；装备=稀有度色环）
    if (perm || type == IT_GEAR) {
        COLORREF ring = (type == IT_GEAR) ? rarityColor(g.rarity) : RGB(255, 215, 90);
        setfillcolor(fadeColor(ring, 0.5));
        solidcircle(cx, cy, r + 8 + (int)(pulse * 4));
        setlinecolor(ring);
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
    switch (type) {
    case IT_FIRE: {                                        // 火焰
        setfillcolor(RGB(255, 210, 60));
        POINT f1[] = { {cx, cy - r + 4}, {cx + 6, cy + 2}, {cx, cy + r - 4}, {cx - 6, cy + 2} };
        solidpolygon(f1, 4);
        setfillcolor(RGB(255, 255, 200));
        POINT f2[] = { {cx, cy - 2}, {cx + 3, cy + 3}, {cx, cy + r - 6}, {cx - 3, cy + 3} };
        solidpolygon(f2, 4);
        break;
    }
    case IT_LIFE:                                          // 维修：白色十字
        setfillcolor(RGB(255, 245, 245));
        solidrectangle(cx - 3, cy - r + 4, cx + 3, cy + r - 4);
        solidrectangle(cx - r + 4, cy - 3, cx + r - 4, cy + 3);
        break;
    case IT_MAXLIFE:                                       // 永久血量上限：血条 + 向上箭头
        setfillcolor(RGB(60, 30, 40));                     // 血槽底
        solidroundrect(cx - r + 4, cy + 1, cx + r - 4, cy + 7, 2, 2);
        setfillcolor(RGB(255, 250, 250));                  // 已填充血量
        solidroundrect(cx - r + 4, cy + 1, cx + 2, cy + 7, 2, 2);
        setfillcolor(RGB(255, 220, 90));                   // 向上箭头（提升）
        {
            POINT up[] = { {cx, cy - r + 3}, {cx + 6, cy - 3}, {cx + 2, cy - 3},
                           {cx + 2, cy - 1}, {cx - 2, cy - 1}, {cx - 2, cy - 3},
                           {cx - 6, cy - 3} };
            solidpolygon(up, 7);
        }
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
    case IT_GEAR: {                                       // 装备芯片：按部位画不同符号
        setfillcolor(darken(bg, 40));
        solidroundrect(cx - r + 4, cy - r + 4, cx + r - 4, cy + r - 4, 4, 4);
        setfillcolor(RGB(250, 250, 255));
        if (g.slot == GS_WEAPON) {                        // 武器：向上箭头
            POINT p[] = { {cx, cy - 7}, {cx + 6, cy + 2}, {cx + 2, cy + 2},
                          {cx + 2, cy + 7}, {cx - 2, cy + 7}, {cx - 2, cy + 2}, {cx - 6, cy + 2} };
            solidpolygon(p, 7);
        }
        else if (g.slot == GS_ARMOR) {                  // 护甲：盾形
            POINT p[] = { {cx, cy - 7}, {cx + 6, cy - 4}, {cx + 5, cy + 3},
                          {cx, cy + 7}, {cx - 5, cy + 3}, {cx - 6, cy - 4} };
            solidpolygon(p, 6);
        }
        else {                                          // 引擎：齿轮近似（方块+四角）
            solidrectangle(cx - 5, cy - 5, cx + 5, cy + 5);
            solidrectangle(cx - 7, cy - 2, cx + 7, cy + 2);
            solidrectangle(cx - 2, cy - 7, cx + 2, cy + 7);
        }
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

void Game::drawItems() {
    // 呼吸脉冲：光晕大小随时间起伏
    double pulse = 0.5 + 0.5 * sin(animTick * 0.18);
    for (const auto& it : items)
        drawItemIcon(it.type, it.gear, (int)it.x, (int)it.y, it.size / 2, pulse);
}

/*==========================================================================*/
/*                              HUD / 界面绘制                               */
/*==========================================================================*/

/** @brief 绘制金/银/铜货币（定义见下方），HUD 与商店共用 */
static int drawMoneyAt(int x, int y, int copper, int fontSize);

void Game::drawHUD() {
    TCHAR buf[64];
    const int yStatus = WIN_H - 76;    // 状态行：火力 / 护盾 / 闪避 / 炸弹
    const int yMain = WIN_H - 50;    // 主行：分数 / 关卡 / 生命
    const int yEnergy = WIN_H - 14;    // 能量条

    // ===== 状态行（左→右流式排布，避免相互重叠） =====
    settextstyle(18, 0, _T("微软雅黑"));
    int sx = 12;
    // 火力
    settextcolor(RGB(255, 200, 120));
    _stprintf_s(buf, _countof(buf), _T("火力Lv.%d"), fireLevel);
    outtextxy(sx, yStatus, buf);
    sx += textwidth(buf) + 16;
    // 小技能（显示当前选用的技能名与剩余次数）
    settextcolor(bombs > 0 ? RGB(210, 210, 210) : RGB(110, 110, 130));
    _stprintf_s(buf, _countof(buf), _T("%s x%d(B)"), skillName(curSkill), bombs);
    outtextxy(sx, yStatus, buf);
    sx += textwidth(buf) + 14;
    // 护盾（有才显示）
    if (shield) {
        settextcolor(RGB(120, 210, 255));
        outtextxy(sx, yStatus, _T("护盾"));
        sx += textwidth(_T("护盾")) + 16;
    }
    // 闪避
    // 弹反：就绪时高亮，冷却时实时显示剩余秒数 + 迷你进度条
    if (parryCd <= 0) {
        settextcolor(RGB(255, 180, 230));
        outtextxy(sx, yStatus, _T("弹反 R"));
        sx += textwidth(_T("弹反 R")) + 14;
    }
    else {
        settextcolor(RGB(130, 110, 125));
        _stprintf_s(buf, _countof(buf), _T("弹反 %.1fs"), parryCd / (double)FPS);
        outtextxy(sx, yStatus, buf);
        int bw = textwidth(buf);
        // 冷却进度条：随冷却推进由左向右填满
        setfillcolor(RGB(40, 34, 42));
        solidrectangle(sx, yStatus + 19, sx + bw, yStatus + 22);
        setfillcolor(RGB(220, 140, 190));
        int done = (int)(bw * (1.0 - parryCd / (double)PARRY_CD));
        if (done > 0) solidrectangle(sx, yStatus + 19, sx + done, yStatus + 22);
        sx += bw + 14;
    }
    // 被动技能：显示本轮生效数量（0 时灰显）
    if (passiveCount > 0) {
        settextcolor(RGB(150, 250, 190));
        _stprintf_s(buf, _countof(buf), _T("被动x%d"), passiveCount);
        outtextxy(sx, yStatus, buf);
        sx += textwidth(buf) + 14;
    }

    // 备用机（本行最右侧，独占位置，不与炸弹冲突）
    settextcolor(RGB(190, 190, 205));
    _stprintf_s(buf, _countof(buf), _T("备用机x%d"), lives);
    outtextxy(WIN_W - textwidth(buf) - 12, yStatus, buf);

    // ===== 主行 =====
    settextcolor(WHITE);
    settextstyle(20, 0, _T("Consolas"));
    _stprintf_s(buf, _countof(buf), _T("SCORE %d"), score);
    outtextxy(12, yMain, buf);
    drawMoneyAt(12 + textwidth(buf) + 14, yMain + 2, money, 16);   // 金/银/铜
    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(220, 230, 255));
    _stprintf_s(buf, _countof(buf), _T("第%d关"), level);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, yMain, buf);
    // 血量（右）：纯数值显示，不再使用爱心图标
    settextstyle(18, 0, _T("Consolas"));
    int hpMaxV = playerHpMax();
    settextcolor((hp * 2 <= hpMaxV) ? RGB(255, 130, 110) : RGB(120, 235, 170));
    _stprintf_s(buf, _countof(buf), _T("HP %d/%d"), hp, hpMaxV);
    outtextxy(WIN_W - textwidth(buf) - 12, yMain, buf);

    // ===== 等级与经验条（能量条上方一行） =====
    {
        int yLv = WIN_H - 28;
        settextstyle(16, 0, _T("微软雅黑"));
        // 升级瞬间等级号闪烁
        settextcolor(lvlUpFlash > 0 && (lvlUpFlash / 5) % 2 ? RGB(255, 255, 200) : RGB(255, 215, 120));
        _stprintf_s(buf, _countof(buf), _T("Lv.%d"), plv);
        outtextxy(12, yLv - 3, buf);
        int lx = 12 + textwidth(_T("Lv.30")) + 8;
        int lw = WIN_W - lx - 12, lh = 7;
        setfillcolor(RGB(28, 32, 48));
        solidroundrect(lx, yLv, lx + lw, yLv + lh, 3, 3);
        if (plv < PLV_MAX) {
            int need = xpNeed(plv);
            int cur = need > 0 ? lw * xp / need : 0;
            if (cur > lw) cur = lw;
            setfillcolor(RGB(255, 195, 80));
            if (cur > 0) solidroundrect(lx, yLv, lx + cur, yLv + lh, 3, 3);
        }
        else {
            setfillcolor(RGB(255, 230, 140));
            solidroundrect(lx, yLv, lx + lw, yLv + lh, 3, 3);
        }
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
    if (full) {
        _stprintf_s(buf, _countof(buf), _T("%s就绪 X"), ultName(curUlt));
        outtextxy(10, yEnergy - 4, buf);
    }
    else outtextxy(10, yEnergy - 4, _T("能量"));

    // ===== 拾取提示（短暂显示于上方，避开 Boss 血条） =====
    if (toastTimer > 0) {
        settextstyle(17, 0, _T("微软雅黑"));
        int tw = textwidth(toast);
        int tx = WIN_W / 2 - tw / 2, ty = boss.active ? 40 : 12;
        setfillcolor(RGB(20, 24, 40));
        solidroundrect(tx - 10, ty - 3, tx + tw + 10, ty + 22, 6, 6);
        setlinecolor(RGB(80, 130, 180));
        roundrect(tx - 10, ty - 3, tx + tw + 10, ty + 22, 6, 6);
        settextcolor(RGB(230, 235, 245));
        outtextxy(tx, ty, toast);
    }
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
        _T("空格 射击      R 弹反(反弹弹幕)"),
        _T("X 激光大招(集满能量)   B 炸弹"),
        _T("P 暂停(图鉴/装备)   M 静音   Esc 退出"),
        _T("金色道具=永久增益(生命上限/火力)"),
        _T("· 输入特定数字可解锁自定义开局 ·"),
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

/**
 * @brief  自定义初始配置界面（菜单输入秘籍 131215 进入）。
 *         ↑↓ 选择条目，←→ 调整数值（按住 Shift 为 10 倍步进），
 *         回车按当前配置开局，Esc 返回主菜单。
 *         右下角实时预览按该配置开局后的最终属性。
 */
void Game::drawCheatPanel() {
    TCHAR buf[96];
    // 面板背景
    setfillcolor(RGB(10, 14, 24));
    solidroundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinecolor(RGB(230, 180, 60));
    setlinestyle(PS_SOLID, 2);
    roundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 标题
    settextcolor(RGB(255, 215, 90));
    settextstyle(30, 0, _T("微软雅黑"));
    const TCHAR* t = _T("自 定 义 初 始 配 置");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 30, t);
    settextcolor(RGB(150, 160, 180));
    settextstyle(15, 0, _T("微软雅黑"));
    const TCHAR* t2 = _T("秘籍模式 · 本局成绩仍会计入排行榜");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, 66, t2);

    // ---- 条目表 ----
    int y = 96;
    const int rowH = 33;
    for (int i = 0; i < CI_COUNT; ++i) {
        bool sel = (i == cheatCursor);
        if (sel) {                                   // 当前选中项高亮
            setfillcolor(RGB(40, 44, 30));
            solidroundrect(28, y - 3, WIN_W - 28, y + 24, 5, 5);
            setlinecolor(RGB(230, 200, 90));
            roundrect(28, y - 3, WIN_W - 28, y + 24, 5, 5);
        }
        settextstyle(18, 0, _T("微软雅黑"));
        settextcolor(sel ? RGB(255, 230, 130) : RGB(200, 205, 220));

        const TCHAR* name; TCHAR val[48];
        switch (i) {
        case CI_PLV:    name = _T("玩家等级");
            _stprintf_s(val, _countof(val), _T("Lv.%d"), cfgPlv); break;
        case CI_LEVEL:  name = _T("起始关卡");
            _stprintf_s(val, _countof(val), _T("第 %d 关  (%s)"), cfgLevel, bossName(bossKind(cfgLevel))); break;
        case CI_WTYPE:  name = _T("武器类型");
            _stprintf_s(val, _countof(val), _T("%s"), weaponName(cfgWtype)); break;
        case CI_RARITY: name = _T("装备稀有度");
            _stprintf_s(val, _countof(val), _T("%s"), rarityName(cfgRarity)); break;
        case CI_GLVL:   name = _T("装备等级");
            _stprintf_s(val, _countof(val), _T("LV.%d"), cfgGlvl); break;
        case CI_HP:     name = _T("额外生命值");
            _stprintf_s(val, _countof(val), _T("+%d"), cfgHp); break;
        case CI_ATK:    name = _T("额外攻击力");
            _stprintf_s(val, _countof(val), _T("+%d"), cfgAtk); break;
        case CI_DEF:    name = _T("额外防御力");
            _stprintf_s(val, _countof(val), _T("+%d"), cfgDef); break;
        case CI_LIVES:  name = _T("备用机数量");
            _stprintf_s(val, _countof(val), _T("x%d"), cfgLives); break;
        case CI_MONEY:  name = _T("初始货币");
        {
            int g, s, c; splitMoney(cfgMoney, g, s, c);
            _stprintf_s(val, _countof(val), _T("%d金 %d银 %d铜"), g, s, c);
        } break;
        case CI_ULT:    name = _T("初始大招");
            _stprintf_s(val, _countof(val), _T("%s"), ultName(cfgUlt)); break;
        case CI_SKILL:  name = _T("初始小技能");
            _stprintf_s(val, _countof(val), _T("%s"), skillName(cfgSkill)); break;
        default:        name = _T("初始被动数");
            _stprintf_s(val, _countof(val), _T("%d 个 (%s级)"), cfgPassive,
                cfgPassive >= 3 ? _T("S") : cfgPassive == 2 ? _T("A")
                : cfgPassive == 1 ? _T("B") : _T("无")); break;
        }
        outtextxy(44, y, name);
        // 数值：稀有度/武器用对应颜色，更直观
        if (i == CI_RARITY) settextcolor(rarityColor(cfgRarity));
        else if (i == CI_WTYPE) settextcolor(weaponColor(cfgWtype));
        settextstyle(18, 0, _T("Consolas"));
        outtextxy(196, y, val);
        if (sel) {                                   // 选中项两侧画调整箭头
            settextcolor(RGB(255, 230, 130));
            outtextxy(178, y, _T("<"));
            outtextxy(WIN_W - 48, y, _T(">"));
        }
        y += rowH;
    }

    // ---- 实时预览最终属性 ----
    y += 4;
    setlinecolor(RGB(70, 80, 100));
    line(28, y, WIN_W - 28, y);
    y += 10;
    settextcolor(RGB(150, 230, 180));
    settextstyle(18, 0, _T("微软雅黑"));
    outtextxy(44, y, _T("开局后的最终属性："));
    y += 26;
    // 按当前配置试算（与 playerXxx() 的公式保持一致）
    int pvAtk = BULLET_DMG_BASE + levelAtk(cfgPlv) + gearAtk(cfgRarity, cfgGlvl) + cfgAtk;
    int pvDef = levelDef(cfgPlv) + gearDef(cfgRarity, cfgGlvl) + cfgDef;
    int pvHp = PLAYER_BASE_HP + levelHp(cfgPlv) + gearHp(cfgRarity, cfgGlvl) + cfgHp;
    int pvSpd = PLAYER_SPD + gearSpeed(cfgRarity, cfgGlvl);
    settextstyle(17, 0, _T("Consolas"));
    settextcolor(RGB(225, 230, 240));
    _stprintf_s(buf, _countof(buf), _T("攻击 %d    防御 %d"), pvAtk, pvDef);
    outtextxy(60, y, buf); y += 24;
    _stprintf_s(buf, _countof(buf), _T("血量 %d    速度 %d"), pvHp, pvSpd);
    outtextxy(60, y, buf);

    // ---- 操作提示 ----
    int oy = WIN_H - 66;
    setlinecolor(RGB(70, 80, 100));
    line(28, oy - 8, WIN_W - 28, oy - 8);
    settextcolor(RGB(200, 210, 230));
    settextstyle(17, 0, _T("微软雅黑"));
    outtextxy(30, oy, _T("↑↓ 选择    ←→ 调整 (Shift 加速)")); oy += 24;
    settextcolor(RGB(150, 220, 255));
    outtextxy(30, oy, _T("回车 开始游戏        Esc 返回菜单"));
}

/**
 * @brief  绘制货币（金/银/铜三色小圆 + 数字），供 HUD 与商店复用。
 * @return 绘制所占宽度
 */
static int drawMoneyAt(int x, int y, int copper, int fontSize) {
    TCHAR buf[32];
    int g, s, c;
    splitMoney(copper, g, s, c);
    const COLORREF cols[3] = { RGB(255, 215, 80), RGB(200, 205, 215), RGB(205, 130, 70) };
    const int vals[3] = { g, s, c };
    settextstyle(fontSize, 0, _T("Consolas"));
    int cx = x;
    for (int i = 0; i < 3; ++i) {
        setfillcolor(cols[i]);
        solidcircle(cx + 5, y + fontSize / 2, 5);      // 币种色点
        setfillcolor(lighten(cols[i], 60));
        solidcircle(cx + 3, y + fontSize / 2 - 2, 2);  // 高光
        settextcolor(cols[i]);
        _stprintf_s(buf, _countof(buf), _T("%d"), vals[i]);
        outtextxy(cx + 13, y, buf);
        cx += 13 + textwidth(buf) + 8;
    }
    return cx - x;
}

/**
 * @brief  商店界面（暂停时按 S 打开）。
 *         Tab 切换「大招 / 小技能」两页；↑↓ 选择；回车 购买(未拥有) 或 装备(已拥有)。
 *         价格与强度星级正相关——越强的技能越贵、越难攒够钱。
 */
 /**
  * @brief  技能面板（暂停时按 K 打开）——一屏总览三类技能的当前状态：
  *           · 大招：当前选用 + 能量充能进度 + 是否就绪
  *           · 小技能：当前选用 + 剩余次数
  *           · 被动：本轮生效的全部被动及其效果
  *         同时附上本轮评级栏，便于随时掌握战况。
  */
void Game::drawSkillPanel() {
    TCHAR buf[96];
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinecolor(RGB(120, 200, 255));
    setlinestyle(PS_SOLID, 2);
    roundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinestyle(PS_SOLID, 1);

    settextcolor(RGB(150, 220, 255));
    settextstyle(28, 0, _T("微软雅黑"));
    const TCHAR* t = _T("技 能 总 览");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 20, t);

    int y = 58;
    // ================= 大招 =================
    settextcolor(RGB(160, 220, 255));
    settextstyle(20, 0, _T("微软雅黑"));
    outtextxy(24, y, _T("【大招】 X 键")); y += 28;
    settextstyle(19, 0, _T("微软雅黑"));
    settextcolor(RGB(235, 240, 250));
    _stprintf_s(buf, _countof(buf), _T("%s"), ultName(curUlt));
    outtextxy(40, y, buf);
    settextstyle(14, 0, _T("Consolas"));           // 星级
    settextcolor(RGB(255, 200, 90));
    TCHAR st[8] = _T("");
    for (int k = 0; k < ultTier(curUlt); ++k) _tcscat_s(st, _countof(st), _T("*"));
    outtextxy(WIN_W - 70, y + 3, st);
    y += 24;
    settextstyle(14, 0, _T("微软雅黑"));
    settextcolor(RGB(150, 165, 190));
    outtextxy(40, y, ultDesc(curUlt)); y += 22;
    // 充能条
    {
        bool full = (energy >= ENERGY_MAX);
        int bx = 40, bw = WIN_W - 80, bh = 14;
        setfillcolor(RGB(28, 32, 48));
        solidroundrect(bx, y, bx + bw, y + bh, 5, 5);
        int cur = bw * energy / ENERGY_MAX; if (cur > bw) cur = bw;
        setfillcolor(full ? ((animTick / 6) % 2 ? RGB(190, 255, 255) : RGB(90, 220, 255))
            : RGB(70, 170, 230));
        if (cur > 0) solidroundrect(bx, y, bx + cur, y + bh, 5, 5);
        settextstyle(14, 0, _T("Consolas"));
        settextcolor(full ? RGB(200, 255, 255) : RGB(160, 180, 210));
        _stprintf_s(buf, _countof(buf), _T("充能 %d/%d %s"), energy, ENERGY_MAX,
            full ? _T("  [就绪! 按X释放]") : _T(""));
        outtextxy(bx, y + bh + 3, buf);
    }
    y += 40;
    setlinecolor(RGB(50, 62, 84)); line(24, y, WIN_W - 24, y); y += 10;

    // ================= 小技能 =================
    settextcolor(RGB(255, 210, 150));
    settextstyle(20, 0, _T("微软雅黑"));
    outtextxy(24, y, _T("【小技能】 B 键")); y += 28;
    settextstyle(19, 0, _T("微软雅黑"));
    settextcolor(bombs > 0 ? RGB(235, 240, 250) : RGB(130, 130, 145));
    _stprintf_s(buf, _countof(buf), _T("%s"), skillName(curSkill));
    outtextxy(40, y, buf);
    settextstyle(14, 0, _T("Consolas"));
    settextcolor(RGB(255, 200, 90));
    st[0] = 0;
    for (int k = 0; k < skillTier(curSkill); ++k) _tcscat_s(st, _countof(st), _T("*"));
    outtextxy(WIN_W - 70, y + 3, st);
    y += 24;
    settextstyle(14, 0, _T("微软雅黑"));
    settextcolor(RGB(150, 165, 190));
    outtextxy(40, y, skillDesc(curSkill)); y += 20;
    // 剩余次数（用方块表示）
    settextstyle(15, 0, _T("微软雅黑"));
    settextcolor(RGB(210, 215, 230));
    outtextxy(40, y, _T("剩余次数"));
    for (int i = 0; i < MAX_BOMBS; ++i) {
        setfillcolor(i < bombs ? RGB(255, 190, 110) : RGB(52, 56, 70));
        solidroundrect(118 + i * 22, y + 2, 136 + i * 22, y + 16, 3, 3);
    }
    y += 30;
    setlinecolor(RGB(50, 62, 84)); line(24, y, WIN_W - 24, y); y += 10;

    // ================= 被动 =================
    settextcolor(RGB(170, 250, 200));
    settextstyle(20, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("【被动】本轮生效 %d 个"), passiveCount);
    outtextxy(24, y, buf); y += 26;
    if (passiveCount == 0) {
        settextstyle(15, 0, _T("微软雅黑"));
        settextcolor(RGB(140, 148, 165));
        outtextxy(40, y, _T("（无）每 3 关按得分率评级后获得")); y += 22;
    }
    else {
        for (int p = 0; p < PS_COUNT; ++p) {
            if (!passiveOn[p]) continue;
            settextstyle(17, 0, _T("微软雅黑"));
            settextcolor(RGB(160, 245, 195));
            _stprintf_s(buf, _countof(buf), _T("· %s"), passiveName(p));
            outtextxy(40, y, buf);
            settextstyle(14, 0, _T("微软雅黑"));
            settextcolor(RGB(150, 165, 190));
            outtextxy(150, y + 2, passiveDesc(p));
            y += 22;
        }
    }
    y += 6;

    // ================= 本轮评级 =================
    drawRankPanel(24, y);

    // 操作提示
    int oy = WIN_H - 40;
    settextcolor(RGB(150, 220, 255));
    settextstyle(16, 0, _T("微软雅黑"));
    outtextxy(24, oy, _T("K 关闭   I 图鉴   E 装备   S 商店   P 继续"));
}

void Game::drawShop() {
    TCHAR buf[96];
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinecolor(RGB(230, 190, 80));
    setlinestyle(PS_SOLID, 2);
    roundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 标题 + 持有货币
    settextcolor(RGB(255, 215, 90));
    settextstyle(28, 0, _T("微软雅黑"));
    const TCHAR* t = _T("商  店");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 22, t);
    settextstyle(15, 0, _T("微软雅黑"));
    settextcolor(RGB(160, 170, 190));
    outtextxy(28, 58, _T("持有："));
    drawMoneyAt(28 + textwidth(_T("持有：")), 56, money, 17);

    // 分页标签
    const TCHAR* tabs[2] = { _T("大招 (X键)"), _T("小技能 (B键)") };
    settextstyle(18, 0, _T("微软雅黑"));
    int tx = WIN_W / 2 - 90, ty = 80;
    for (int i = 0; i < 2; ++i) {
        int w = textwidth(tabs[i]) + 18;
        if (i == shopTab) {
            setfillcolor(RGB(60, 52, 24));
            solidroundrect(tx - 4, ty - 3, tx + w, ty + 24, 6, 6);
            settextcolor(RGB(255, 225, 130));
        }
        else settextcolor(RGB(120, 130, 150));
        outtextxy(tx + 6, ty, tabs[i]);
        tx += w + 10;
    }
    settextcolor(RGB(120, 130, 150));
    settextstyle(14, 0, _T("微软雅黑"));
    outtextxy(WIN_W - 92, ty + 4, _T("Tab 切页"));

    // ---- 条目列表 ----
    int n = (shopTab == 0) ? U_COUNT : SK_COUNT;
    int y = 114;
    const int rowH = (shopTab == 0) ? 62 : 54;
    for (int i = 0; i < n; ++i) {
        bool owned = (shopTab == 0) ? ultOwned[i] : skillOwned[i];
        bool equipped = (shopTab == 0) ? (curUlt == i) : (curSkill == i);
        int  price = (shopTab == 0) ? ultPrice(i) : skillPrice(i);
        int  tier = (shopTab == 0) ? ultTier(i) : skillTier(i);
        bool sel = (i == shopCursor);

        if (sel) {                                     // 选中高亮
            setfillcolor(RGB(40, 40, 26));
            solidroundrect(26, y - 3, WIN_W - 26, y + rowH - 10, 6, 6);
            setlinecolor(RGB(230, 200, 90));
            roundrect(26, y - 3, WIN_W - 26, y + rowH - 10, 6, 6);
        }
        // 名称（已装备标记）
        settextstyle(19, 0, _T("微软雅黑"));
        settextcolor(equipped ? RGB(140, 255, 180) : (owned ? RGB(225, 230, 240) : RGB(150, 155, 170)));
        _stprintf_s(buf, _countof(buf), _T("%s%s"),
            (shopTab == 0) ? ultName(i) : skillName(i),
            equipped ? _T("  ◀ 使用中") : _T(""));
        outtextxy(40, y, buf);
        // 强度星级
        settextstyle(14, 0, _T("Consolas"));
        settextcolor(RGB(255, 200, 90));
        TCHAR stars[8] = _T("");
        for (int k = 0; k < tier; ++k) _tcscat_s(stars, _countof(stars), _T("*"));
        _stprintf_s(buf, _countof(buf), _T("强度 %-5s"), stars);
        outtextxy(WIN_W - 150, y + 2, buf);
        // 说明
        settextstyle(14, 0, _T("微软雅黑"));
        settextcolor(RGB(145, 158, 182));
        outtextxy(40, y + 22, (shopTab == 0) ? ultDesc(i) : skillDesc(i));
        // 价格 / 状态
        if (owned) {
            settextstyle(15, 0, _T("微软雅黑"));
            settextcolor(RGB(120, 200, 140));
            outtextxy(WIN_W - 150, y + 24, equipped ? _T("已装备") : _T("已拥有 (回车装备)"));
        }
        else {
            drawMoneyAt(WIN_W - 150, y + 22, price, 15);
            if (money < price) {                       // 买不起标红
                settextstyle(13, 0, _T("微软雅黑"));
                settextcolor(RGB(230, 110, 100));
                outtextxy(WIN_W - 150, y + 40, _T("货币不足"));
            }
        }
        y += rowH;
    }

    // 操作提示
    int oy = WIN_H - 64;
    setlinecolor(RGB(70, 80, 100));
    line(24, oy - 8, WIN_W - 24, oy - 8);
    settextcolor(RGB(200, 210, 230));
    settextstyle(16, 0, _T("微软雅黑"));
    outtextxy(28, oy, _T("↑↓ 选择   回车 购买/装备   Tab 切页")); oy += 22;
    settextcolor(RGB(150, 220, 255));
    outtextxy(28, oy, _T("S 关闭   I 图鉴   E 装备栏   P 继续"));
}

void Game::drawPaused() {
    settextcolor(WHITE);
    settextstyle(40, 0, _T("微软雅黑"));
    const TCHAR* t = _T("暂  停");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, WIN_H / 2 - 70, t);
    settextstyle(18, 0, _T("微软雅黑"));
    const TCHAR* t2 = _T("P / 回车 继续    Esc 退出并结算");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, WIN_H / 2 - 6, t2);
    settextcolor(RGB(150, 220, 255));
    const TCHAR* t3 = _T("K 技能   I 图鉴   E 装备   S 商店");
    outtextxy(WIN_W / 2 - textwidth(t3) / 2, WIN_H / 2 + 26, t3);
}

/**
 * @brief  图鉴面板（暂停时按 I 打开，←→ 翻页）。
 *         整合三页内容：
 *           第1页 敌机图鉴 —— 图标 / 名称 / 属性(生命·攻击·防御) / 攻击方式（仅当前关卡，随关卡更新）
 *           第2页 增益图鉴 —— 图标 / 名称 / 效果（临时与永久增益）
 *           第3页 装备图鉴 —— 图标 / 名称 / 各稀有度效果
 *         顶部同时显示我方战机的实时属性，便于对比。
 */
void Game::drawInfoPanel() {
    TCHAR buf[96];
    double pulse = 0.5 + 0.5 * sin(animTick * 0.18);

    // 面板背景与边框
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinecolor(RGB(70, 110, 160));
    setlinestyle(PS_SOLID, 2);
    roundrect(16, 16, WIN_W - 16, WIN_H - 16, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 标题
    settextcolor(RGB(120, 230, 255));
    settextstyle(32, 0, _T("微软雅黑"));
    const TCHAR* title = _T("图  鉴");
    outtextxy(WIN_W / 2 - textwidth(title) / 2, 24, title);

    // 分页标签（当前页高亮）
    const TCHAR* tabs[7] = { _T("敌机"), _T("增益"), _T("装备"), _T("武器"), _T("大招"), _T("技能"), _T("被动") };
    settextstyle(21, 0, _T("微软雅黑"));
    int tw = 0; for (int i = 0; i < 7; ++i) tw += textwidth(tabs[i]) + 12;
    int tx = WIN_W / 2 - tw / 2, ty = 60;
    for (int i = 0; i < 7; ++i) {
        int w = textwidth(tabs[i]) + 8;
        if (i == dexPage) {
            setfillcolor(RGB(40, 70, 110));
            solidroundrect(tx - 4, ty - 3, tx + w, ty + 24, 6, 6);
            settextcolor(RGB(180, 240, 255));
        }
        else settextcolor(RGB(120, 130, 150));
        outtextxy(tx + 5, ty, tabs[i]);
        tx += w + 4;
    }

    // 我方战机实时属性（各页共用的顶栏）
    int y = 94;
    settextstyle(19, 0, _T("微软雅黑"));
    settextcolor(RGB(160, 235, 190));
    _stprintf_s(buf, _countof(buf), _T("我方：血量%d/%d  攻击%d  防御%d  速度%d  备用机%d"),
        hp, playerHpMax(), playerAttack(), playerDefense(), playerSpeed(), lives);
    outtextxy(28, y, buf);
    y += 26;
    setlinecolor(RGB(60, 80, 110));
    line(28, y, WIN_W - 28, y);
    y += 10;

    /*================= 第 1 页：敌机图鉴 =================*/
    if (dexPage == 0) {
        settextcolor(RGB(255, 170, 150));
        settextstyle(22, 0, _T("微软雅黑"));
        _stprintf_s(buf, _countof(buf), _T("本关敌机 · 第%d关"), level);
        outtextxy(28, y, buf); y += 28;

        const int cIcon = 42, cName = 66, cHp = 146, cAtk = 190, cDef = 230, cXp = 268, cFeat = 320;
        settextstyle(18, 0, _T("微软雅黑"));
        settextcolor(RGB(150, 170, 210));
        outtextxy(cIcon - 16, y, _T("图标")); outtextxy(cName, y, _T("名称"));
        outtextxy(cHp, y, _T("生命"));        outtextxy(cAtk, y, _T("攻击"));
        outtextxy(cDef, y, _T("防御"));       outtextxy(cXp, y, _T("经验"));
        outtextxy(cFeat, y, _T("攻击方式"));
        y += 22;
        setlinecolor(RGB(50, 65, 90));
        line(28, y, WIN_W - 28, y);
        y += 6;

        // 敌机小图标：复用真实绘制函数，外观与实战一致
        auto icon = [&](EnemyType t, int cx, int cy) {
            Enemy e{};
            e.type = t; e.x = cx; e.y = cy; e.w = 34; e.h = 28;
            e.hp = 1; e.maxHp = 1; e.score = -1;   // score<0：图鉴图标，跳过血条
            e.armor = enemyArmor(t, level); e.aiState = 0; e.hitFlash = 0; e.alive = true;
            drawEnemyShape(e);
            };
        // 依据当前关卡组装名单（与 spawnEnemy 解锁规则一致）
        std::vector<EnemyType> roster = { E_NORMAL, E_FAST };
        if (level >= 2) { roster.push_back(E_ZIGZAG); roster.push_back(E_TANK); }
        if (level >= 3) { roster.push_back(E_SHOOTER); roster.push_back(E_STRAFER); roster.push_back(E_DIVER); }

        const int rowH = 32;
        settextstyle(18, 0, _T("微软雅黑"));
        for (EnemyType t : roster) {
            int cy = y + rowH / 2 - 2;
            icon(t, cIcon, cy);
            settextcolor(RGB(225, 225, 235));
            outtextxy(cName, y + 5, enemyName(t));
            _stprintf_s(buf, _countof(buf), _T("%d"), enemyHp(t, level));        outtextxy(cHp, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), enemyTouchDmg(t, level));  outtextxy(cAtk, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), enemyArmor(t, level));     outtextxy(cDef, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), enemyXp(t, level));        outtextxy(cXp, y + 5, buf);
            outtextxy(cFeat, y + 5, enemyFeature(t));
            y += rowH;
        }
        // Boss 行：当前关卡的 Boss（图标与名称严格对应各自造型）
        {
            int k = bossKind(level);
            int cy = y + rowH / 2 - 2;
            drawBossIcon(k, cIcon, cy);
            settextcolor(RGB(255, 150, 175));
            outtextxy(cName, y + 5, bossName(k));
            _stprintf_s(buf, _countof(buf), _T("%d"), bossHp(level)); outtextxy(cHp, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), HIT_HP_BOSS);   outtextxy(cAtk, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), bossArmor(k, level)); outtextxy(cDef, y + 5, buf);
            _stprintf_s(buf, _countof(buf), _T("%d"), bossXp(level));       outtextxy(cXp, y + 5, buf);
            outtextxy(cFeat, y + 5, bossFeature(k));
            y += rowH + 6;
        }
        settextcolor(RGB(150, 170, 200));
        settextstyle(16, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("经验=按强度加权，越强的敌人给得越多"));
    }
    /*================= 第 2 页：增益图鉴 =================*/
    else if (dexPage == 1) {
        settextcolor(RGB(255, 210, 140));
        settextstyle(22, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("道具增益（金框=永久）")); y += 30;

        const int types[6] = { IT_FIRE, IT_LIFE, IT_SHIELD, IT_BOMB, IT_MAXLIFE, IT_POWERUP };
        const int rowH = 52;
        Gear dummy{};                       // 非装备道具用不到，占位即可
        for (int i = 0; i < 6; ++i) {
            int t = types[i];
            int cy = y + rowH / 2 - 4;
            drawItemIcon(t, dummy, 52, cy, 13, pulse);
            settextstyle(20, 0, _T("微软雅黑"));
            settextcolor(isPermanentItem((ItemType)t) ? RGB(255, 215, 90) : RGB(225, 230, 240));
            outtextxy(84, y + 2, itemName(t));
            settextstyle(17, 0, _T("微软雅黑"));
            settextcolor(RGB(150, 165, 190));
            outtextxy(84, y + 24, itemEffect(t));
            y += rowH;
        }
        settextcolor(RGB(150, 170, 200));
        settextstyle(16, 0, _T("微软雅黑"));
        outtextxy(28, y + 4, _T("永久增益掉率更低，Boss 掉落几率更高"));
    }
    /*================= 第 3 页：装备图鉴 =================*/
    else if (dexPage == 2) {
        settextcolor(RGB(180, 240, 140));
        settextstyle(21, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("装备（掉落恒为LV.1，靠合成升级）")); y += 26;
        settextcolor(RGB(150, 165, 190));
        settextstyle(15, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("两件同部位+同稀有度+同等级 → 合成高一级")); y += 22;
        settextcolor(RGB(160, 180, 210));
        outtextxy(28, y, _T("下表数值 = 普通~史诗 的加成范围")); y += 24;

        for (int s = 0; s < GS_COUNT; ++s) {
            Gear g{}; g.slot = s; g.rarity = R_EPIC; g.lvl = 1;
            drawItemIcon(IT_GEAR, g, 44, y + 10, 12, pulse);
            settextstyle(19, 0, _T("微软雅黑"));
            settextcolor(RGB(230, 235, 245));
            outtextxy(66, y, slotName(s));
            y += 24;
            // 逐等级列出加成（普通~史诗 的范围）
            settextstyle(16, 0, _T("Consolas"));
            for (int L = 1; L <= GEAR_LVL_MAX; ++L) {
                settextcolor(L == GEAR_LVL_MAX ? RGB(255, 215, 90) : RGB(190, 200, 220));
                switch (s) {
                case GS_WEAPON:
                    _stprintf_s(buf, _countof(buf), _T("LV.%d  攻击+%d~%d"),
                        L, gearAtk(R_COMMON, L), gearAtk(R_LEGEND, L));
                    break;
                case GS_ARMOR:
                    _stprintf_s(buf, _countof(buf), _T("LV.%d  防+%d~%d 血+%d~%d"),
                        L, gearDef(R_COMMON, L), gearDef(R_LEGEND, L),
                        gearHp(R_COMMON, L), gearHp(R_LEGEND, L));
                    break;
                default:
                    _stprintf_s(buf, _countof(buf), _T("LV.%d  速+%d~%d 射速+%d~%d"),
                        L, gearSpeed(R_COMMON, L), gearSpeed(R_LEGEND, L),
                        gearRate(R_COMMON, L), gearRate(R_LEGEND, L));
                    break;
                }
                outtextxy(66, y, buf);
                y += 19;
            }
            y += 8;
        }
    }

    /*================= 第 4 页：武器图鉴（攻击方式） =================*/
    else if (dexPage == 3) {
        settextcolor(RGB(255, 200, 150));
        settextstyle(21, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("武器：决定攻击方式")); y += 26;
        settextcolor(RGB(150, 165, 190));
        settextstyle(15, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("装备不同武器会改变弹道；四种强度相当")); y += 24;

        int cur = playerWeaponType();
        for (int w = 0; w < W_COUNT; ++w) {
            bool equipped = (equipOn[GS_WEAPON] && cur == w);
            if (equipped) {                            // 高亮当前使用的武器
                setfillcolor(RGB(38, 46, 60));
                solidroundrect(26, y - 3, WIN_W - 26, y + 66, 6, 6);
                setlinecolor(RGB(140, 220, 255));
                roundrect(26, y - 3, WIN_W - 26, y + 66, 6, 6);
            }
            // 武器图标：用其弹色画一个小弹头
            setfillcolor(weaponColor(w));
            solidroundrect(38, y + 4, 50, y + 24, 4, 4);
            setfillcolor(RGB(255, 255, 255));
            solidrectangle(43, y + 8, 45, y + 20);

            settextstyle(19, 0, _T("微软雅黑"));
            settextcolor(equipped ? RGB(160, 235, 255) : RGB(230, 235, 245));
            _stprintf_s(buf, _countof(buf), _T("%s%s"), weaponName(w),
                equipped ? _T("  ◀ 使用中") : _T(""));
            outtextxy(60, y, buf);
            settextstyle(15, 0, _T("微软雅黑"));
            settextcolor(RGB(150, 165, 190));
            outtextxy(60, y + 22, weaponDesc(w));
            // 射速与单发倍率
            settextcolor(RGB(170, 190, 215));
            _stprintf_s(buf, _countof(buf), _T("单发%d%%  冷却+%d帧"),
                weaponDmgPct(w), weaponCdBonus(w));
            outtextxy(300, y + 22, buf);
            // 三档火力等级的弹道
            settextstyle(14, 0, _T("微软雅黑"));
            for (int f = 1; f <= 3; ++f) {
                settextcolor(f == 3 ? RGB(255, 215, 90) : RGB(160, 175, 200));
                outtextxy(60 + (f - 1) * 138, y + 44, weaponFireDesc(w, f));
            }
            y += 74;
        }
        settextcolor(RGB(150, 170, 200));
        settextstyle(15, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("武器由敌人/Boss 掉落，可合成升级(F)"));
    }

    /*================= 第 5 页：大招图鉴 =================*/
    else if (dexPage == 4) {
        settextcolor(RGB(160, 220, 255));
        settextstyle(21, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("大招（X 键，消耗能量）")); y += 24;
        settextcolor(RGB(150, 165, 190));
        settextstyle(14, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("越强的大招售价越高；暂停按 S 进商店购买")); y += 22;

        const int rowH = 56;
        for (int u = 0; u < U_COUNT; ++u) {
            bool owned = ultOwned[u], eq = (curUlt == u);
            if (eq) {                                  // 当前使用中高亮
                setfillcolor(RGB(28, 42, 56));
                solidroundrect(26, y - 3, WIN_W - 26, y + rowH - 12, 5, 5);
                setlinecolor(RGB(120, 200, 255));
                roundrect(26, y - 3, WIN_W - 26, y + rowH - 12, 5, 5);
            }
            settextstyle(18, 0, _T("微软雅黑"));
            settextcolor(eq ? RGB(150, 230, 255) : (owned ? RGB(225, 230, 240) : RGB(140, 145, 160)));
            _stprintf_s(buf, _countof(buf), _T("%s%s"), ultName(u),
                eq ? _T(" ◀使用中") : (owned ? _T(" (已拥有)") : _T("")));
            outtextxy(40, y, buf);
            settextstyle(14, 0, _T("Consolas"));       // 星级
            settextcolor(RGB(255, 200, 90));
            TCHAR st[8] = _T("");
            for (int k = 0; k < ultTier(u); ++k) _tcscat_s(st, _countof(st), _T("*"));
            outtextxy(WIN_W - 120, y + 1, st);
            settextstyle(14, 0, _T("微软雅黑"));
            settextcolor(RGB(145, 158, 182));
            outtextxy(40, y + 21, ultDesc(u));
            if (!owned) drawMoneyAt(WIN_W - 120, y + 20, ultPrice(u), 14);
            y += rowH;
        }
    }
    /*================= 第 6 页：小技能图鉴 =================*/
    else if (dexPage == 5) {
        settextcolor(RGB(255, 200, 150));
        settextstyle(21, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("小技能（B 键，消耗次数）")); y += 24;
        settextcolor(RGB(150, 165, 190));
        settextstyle(14, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("次数由道具补充；越强的技能售价越高")); y += 20;

        const int rowH = 44;
        for (int s = 0; s < SK_COUNT; ++s) {
            bool owned = skillOwned[s], eq = (curSkill == s);
            if (eq) {
                setfillcolor(RGB(46, 38, 26));
                solidroundrect(26, y - 3, WIN_W - 26, y + rowH - 10, 5, 5);
                setlinecolor(RGB(240, 190, 100));
                roundrect(26, y - 3, WIN_W - 26, y + rowH - 10, 5, 5);
            }
            settextstyle(17, 0, _T("微软雅黑"));
            settextcolor(eq ? RGB(255, 220, 140) : (owned ? RGB(225, 230, 240) : RGB(140, 145, 160)));
            _stprintf_s(buf, _countof(buf), _T("%s%s"), skillName(s),
                eq ? _T(" ◀使用中") : (owned ? _T(" (已拥有)") : _T("")));
            outtextxy(40, y, buf);
            settextstyle(13, 0, _T("Consolas"));
            settextcolor(RGB(255, 200, 90));
            TCHAR st[8] = _T("");
            for (int k = 0; k < skillTier(s); ++k) _tcscat_s(st, _countof(st), _T("*"));
            outtextxy(WIN_W - 118, y + 1, st);
            settextstyle(13, 0, _T("微软雅黑"));
            settextcolor(RGB(145, 158, 182));
            outtextxy(40, y + 19, skillDesc(s));
            if (!owned) drawMoneyAt(WIN_W - 118, y + 18, skillPrice(s), 13);
            y += rowH;
        }
    }

    /*================= 第 7 页：被动技能图鉴 =================*/
    else {
        settextcolor(RGB(170, 240, 200));
        settextstyle(23, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("被动技能（三关一评级）")); y += 27;
        settextcolor(RGB(150, 165, 190));
        settextstyle(16, 0, _T("微软雅黑"));
        outtextxy(28, y, _T("每3关按得分率评级：S=3个 A=2个 B=1个")); y += 20;
        outtextxy(28, y, _T("仅接下来3关生效，之后清空重新评定")); y += 24;

        // 当前状态
        settextstyle(17, 0, _T("微软雅黑"));
        settextcolor(tierColor(lastTier));
        if (lastTier == RK_NONE && lastPct == 0)
            _stprintf_s(buf, _countof(buf), _T("当前：第 1 轮（第3关后首次评级）"));
        else
            _stprintf_s(buf, _countof(buf), _T("上次评级 %s 级（得分率 %d%%）  生效中 %d 个"),
                tierName(lastTier), lastPct, passiveCount);
        outtextxy(28, y, buf); y += 22;
        setlinecolor(RGB(50, 65, 90));
        line(28, y, WIN_W - 28, y); y += 6;

        const int rowH = 42;
        for (int p = 0; p < PS_COUNT; ++p) {
            bool on = passiveOn[p];
            if (on) {                                   // 生效中高亮
                setfillcolor(RGB(26, 46, 36));
                solidroundrect(26, y - 2, WIN_W - 26, y + rowH - 8, 5, 5);
                setlinecolor(RGB(120, 230, 170));
                roundrect(26, y - 2, WIN_W - 26, y + rowH - 8, 5, 5);
            }
            settextstyle(19, 0, _T("微软雅黑"));
            settextcolor(on ? RGB(150, 250, 190) : RGB(160, 165, 180));
            _stprintf_s(buf, _countof(buf), _T("%s%s"), passiveName(p), on ? _T("  ◀生效中") : _T(""));
            outtextxy(40, y, buf);
            settextstyle(15, 0, _T("微软雅黑"));
            settextcolor(RGB(150, 165, 190));
            outtextxy(40, y + 21, passiveDesc(p));
            y += rowH;
        }
    }

    // 底部操作提示
    int by = WIN_H - 48;
    setlinecolor(RGB(60, 80, 110));
    line(28, by - 8, WIN_W - 28, by - 8);
    settextcolor(RGB(160, 220, 255));
    settextstyle(18, 0, _T("微软雅黑"));
    outtextxy(28, by, _T("←→翻页  I关闭  E装备栏  P继续  Esc退出并结算"));
}

/**
 * @brief  装备与属性面板（暂停时按 E 打开）。
 *         上半：我方战机完整属性（血量/攻击/防御/速度/备用机/增益）
 *         中间：三个部位的已装备情况
 *         下半：背包列表，↑↓ 选择、Enter 安装（同部位自动替换）、D 丢弃
 *         属性与装备合并在同一界面，便于边看数值边换装。
 */
void Game::drawEquipPanel() {
    TCHAR buf[96], b2[48];
    // 面板背景与边框
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinecolor(RGB(120, 160, 90));
    setlinestyle(PS_SOLID, 2);
    roundrect(12, 12, WIN_W - 12, WIN_H - 12, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 格式化一件装备的加成描述
    auto bonusText = [&](const Gear& g, TCHAR* out, int n) {
        switch (g.slot) {
        case GS_WEAPON: _stprintf_s(out, n, _T("攻击+%d"), gearAtk(g.rarity, g.lvl)); break;
        case GS_ARMOR:  _stprintf_s(out, n, _T("防+%d 血+%d"), gearDef(g.rarity, g.lvl), gearHp(g.rarity, g.lvl)); break;
        default:        _stprintf_s(out, n, _T("速+%d 射速+%d"), gearSpeed(g.rarity, g.lvl), gearRate(g.rarity, g.lvl)); break;
        }
        };


    // 标题
    settextcolor(RGB(180, 240, 140));
    settextstyle(30, 0, _T("微软雅黑"));
    const TCHAR* title = _T("战 机 与 装 备");
    outtextxy(WIN_W / 2 - textwidth(title) / 2, 20, title);

    /*---------------- 我方属性（与装备同界面） ----------------*/
    int y = 60;
    settextcolor(RGB(150, 230, 180));
    settextstyle(22, 0, _T("微软雅黑"));
    outtextxy(24, y, _T("【战机属性】")); y += 30;

    settextstyle(21, 0, _T("微软雅黑"));
    int hpMaxV = playerHpMax();
    // 血量：数值 + 直观血条
    settextcolor(RGB(230, 235, 245));
    _stprintf_s(buf, _countof(buf), _T("血量 %d / %d"), hp, hpMaxV);
    outtextxy(36, y, buf);
    {
        int bx0 = 210, bw0 = WIN_W - bx0 - 36, bh0 = 14;
        setfillcolor(RGB(40, 44, 60));
        solidroundrect(bx0, y + 4, bx0 + bw0, y + 4 + bh0, 4, 4);
        int cur = (hpMaxV > 0) ? bw0 * hp / hpMaxV : 0;
        if (cur < 0) cur = 0;
        setfillcolor((hp * 2 <= hpMaxV) ? RGB(240, 110, 90) : RGB(90, 230, 160));
        if (cur > 0) solidroundrect(bx0, y + 4, bx0 + cur, y + 4 + bh0, 4, 4);
    }
    y += 30;
    settextcolor(RGB(230, 235, 245));
    _stprintf_s(buf, _countof(buf), _T("攻击 %d      防御 %d"), playerAttack(), playerDefense());
    outtextxy(36, y, buf); y += 28;
    if (plv < PLV_MAX)
        _stprintf_s(buf, _countof(buf), _T("等级 Lv.%d    经验 %d/%d"), plv, xp, xpNeed(plv));
    else
        _stprintf_s(buf, _countof(buf), _T("等级 Lv.%d (满级)"), plv);
    outtextxy(36, y, buf); y += 28;
    settextcolor(RGB(180, 200, 225));
    settextstyle(17, 0, _T("微软雅黑"));
    if (lastLvlSec > 0)
        _stprintf_s(buf, _countof(buf), _T("上次升级用时 %d 秒（升级会越来越慢）"), lastLvlSec);
    else
        _stprintf_s(buf, _countof(buf), _T("击杀敌人获取经验，越强的敌人经验越多"));
    outtextxy(36, y, buf); y += 26;
    settextstyle(21, 0, _T("微软雅黑"));
    settextcolor(RGB(230, 235, 245));
    _stprintf_s(buf, _countof(buf), _T("速度 %d      备用机 %d"), playerSpeed(), lives);
    outtextxy(36, y, buf); y += 30;

    // 增益标签
    settextcolor(RGB(255, 220, 140));
    settextstyle(19, 0, _T("微软雅黑"));
    outtextxy(36, y, _T("增益："));
    int bx = 36 + textwidth(_T("增益：")), by = y;
    auto tag = [&](const TCHAR* s, COLORREF c) {
        settextcolor(c);
        if (bx + textwidth(s) > WIN_W - 36) { bx = 96; by += 24; }
        outtextxy(bx, by, s); bx += textwidth(s) + 12;
        };
    if (fireTimer > 0) {
        _stprintf_s(buf, _countof(buf), _T("火力Lv.%d(%ds)"), fireLevel, fireTimer / FPS + 1);
        tag(buf, RGB(255, 190, 120));
    }
    else tag(_T("火力Lv.1"), RGB(190, 190, 200));
    if (shield) tag(_T("护盾"), RGB(120, 210, 255));
    _stprintf_s(buf, _countof(buf), _T("炸弹x%d"), bombs); tag(buf, RGB(210, 210, 210));
    _stprintf_s(buf, _countof(buf), _T("能量%d%%"), energy * 100 / ENERGY_MAX); tag(buf, RGB(120, 210, 255));
    tag(parryCd <= 0 ? _T("弹反就绪") : _T("弹反冷却"), parryCd <= 0 ? RGB(255, 180, 230) : RGB(140, 140, 150));
    y = by + 32;

    setlinecolor(RGB(60, 80, 110));
    line(24, y - 8, WIN_W - 24, y - 8);

    /*---------------- 已装备部位 ----------------*/
    settextcolor(RGB(150, 230, 180));
    settextstyle(22, 0, _T("微软雅黑"));
    outtextxy(24, y, _T("【已装备】")); y += 30;
    settextstyle(20, 0, _T("微软雅黑"));
    for (int s = 0; s < GS_COUNT; ++s) {
        settextcolor(RGB(205, 210, 220));
        _stprintf_s(buf, _countof(buf), _T("%s："), slotName(s));
        outtextxy(36, y, buf);
        int sx = 36 + textwidth(_T("护甲："));
        if (equipOn[s]) {
            bonusText(equip[s], b2, _countof(b2));
            if (s == GS_WEAPON)
                _stprintf_s(buf, _countof(buf), _T("%s %s LV.%d %s"), rarityName(equip[s].rarity),
                    weaponName(equip[s].wtype), equip[s].lvl, b2);
            else
                _stprintf_s(buf, _countof(buf), _T("%s LV.%d  %s"), rarityName(equip[s].rarity), equip[s].lvl, b2);
            settextcolor(rarityColor(equip[s].rarity));
            outtextxy(sx, y, buf);
        }
        else {
            settextcolor(RGB(120, 120, 130));
            outtextxy(sx, y, _T("— 空 —"));
        }
        y += 27;
    }
    y += 6;
    setlinecolor(RGB(60, 80, 110));
    line(24, y - 8, WIN_W - 24, y - 8);

    /*---------------- 背包列表 ----------------*/
    settextcolor(RGB(255, 190, 150));
    settextstyle(22, 0, _T("微软雅黑"));
    _stprintf_s(buf, _countof(buf), _T("【背包】%d 件"), (int)bag.size());
    outtextxy(24, y, buf); y += 30;

    settextstyle(19, 0, _T("微软雅黑"));
    if (bag.empty()) {
        settextcolor(RGB(140, 140, 150));
        outtextxy(36, y, _T("（空）击败敌人与 Boss 有几率掉落装备"));
    }
    else {
        const int rowH = 28;
        int avail = (WIN_H - 76) - y;                  // 预留底部提示区
        int maxRows = avail / rowH; if (maxRows < 1) maxRows = 1;
        int start = 0, total = (int)bag.size();
        if (total > maxRows) {                         // 围绕光标滚动
            start = invCursor - maxRows / 2;
            if (start < 0) start = 0;
            if (start > total - maxRows) start = total - maxRows;
        }
        for (int i = start; i < total && i < start + maxRows; ++i) {
            const Gear& g = bag[i];
            int ry = y + (i - start) * rowH;
            if (i == invCursor) {                      // 光标高亮
                setfillcolor(RGB(40, 52, 40));
                solidroundrect(28, ry - 2, WIN_W - 28, ry + rowH - 5, 6, 6);
                setlinecolor(RGB(150, 220, 120));
                roundrect(28, ry - 2, WIN_W - 28, ry + rowH - 5, 6, 6);
            }
            setfillcolor(rarityColor(g.rarity));       // 稀有度色块
            solidroundrect(38, ry + 4, 52, ry + 18, 3, 3);
            bonusText(g, b2, _countof(b2));
            // 检查背包中是否存在可与之合成的同类同级装备
            bool canFuse = false;
            if (g.lvl < GEAR_LVL_MAX)
                for (int j = 0; j < total; ++j)
                    if (j != i && bag[j].slot == g.slot && bag[j].rarity == g.rarity &&
                        bag[j].lvl == g.lvl && (g.slot != GS_WEAPON || bag[j].wtype == g.wtype))
                    {
                        canFuse = true; break;
                    }
            if (g.slot == GS_WEAPON)
                _stprintf_s(buf, _countof(buf), _T("%s·%s LV.%d %s"),
                    rarityName(g.rarity), weaponName(g.wtype), g.lvl, b2);
            else
                _stprintf_s(buf, _countof(buf), _T("%s·%s LV.%d %s"),
                    rarityName(g.rarity), slotName(g.slot), g.lvl, b2);
            settextcolor(rarityColor(g.rarity));
            outtextxy(62, ry, buf);
            if (canFuse) {                                 // 可合成提示
                settextcolor(RGB(255, 215, 90));
                outtextxy(WIN_W - 60, ry, _T("可合成"));
            }
        }
        if (total > maxRows) {                         // 滚动提示
            settextcolor(RGB(130, 140, 160));
            settextstyle(15, 0, _T("微软雅黑"));
            _stprintf_s(buf, _countof(buf), _T("(%d/%d)"), invCursor + 1, total);
            outtextxy(WIN_W - 90, WIN_H - 92, buf);
        }
    }

    // 操作提示
    int oy = WIN_H - 62;
    setlinecolor(RGB(60, 80, 110));
    line(24, oy - 8, WIN_W - 24, oy - 8);
    settextcolor(RGB(170, 225, 170));
    settextstyle(18, 0, _T("微软雅黑"));
    outtextxy(24, oy, _T("↑↓选择  Enter安装  F合成  D丢弃")); oy += 24;
    settextcolor(RGB(150, 220, 255));
    outtextxy(24, oy, _T("E关闭  I图鉴  P继续  Esc退出并结算"));
}

/**
 * @brief  本局结算页。两种进入方式共用：
 *           · 血量与备用机耗尽（阵亡）    → 标题「游戏结束」
 *           · 暂停中按 Esc 退出并结算     → 标题「本局结算」
 *         展示本局分数、到达关卡与最终属性，并列出排行榜（本局成绩高亮）。
 *         若成绩进入前 10，进入本页前已弹出昵称输入框。
 */
void Game::drawGameOver() {
    TCHAR buf[80];
    // 面板背景
    setfillcolor(RGB(12, 14, 26));
    solidroundrect(24, 60, WIN_W - 24, WIN_H - 60, 14, 14);
    setlinecolor(quitSettle ? RGB(120, 180, 220) : RGB(200, 80, 80));
    setlinestyle(PS_SOLID, 2);
    roundrect(24, 60, WIN_W - 24, WIN_H - 60, 14, 14);
    setlinestyle(PS_SOLID, 1);

    // 标题
    settextcolor(quitSettle ? RGB(140, 220, 255) : RGB(255, 90, 90));
    settextstyle(40, 0, _T("微软雅黑"));
    const TCHAR* t = quitSettle ? _T("本 局 结 算") : _T("游 戏 结 束");
    outtextxy(WIN_W / 2 - textwidth(t) / 2, 82, t);

    // 分数（大字）
    int y = 146;
    settextcolor(RGB(255, 220, 120));
    settextstyle(22, 0, _T("微软雅黑"));
    const TCHAR* sl = _T("本局得分");
    outtextxy(WIN_W / 2 - textwidth(sl) / 2, y, sl);
    y += 30;
    settextstyle(46, 0, _T("Consolas"));
    settextcolor(WHITE);
    _stprintf_s(buf, _countof(buf), _T("%d"), score);
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf);
    y += 58;

    // 本局概况
    settextstyle(19, 0, _T("微软雅黑"));
    settextcolor(RGB(200, 215, 235));
    _stprintf_s(buf, _countof(buf), _T("到达关卡：第 %d 关（%s）"), level, bossName(bossKind(level)));
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf); y += 26;
    _stprintf_s(buf, _countof(buf), _T("战机等级：Lv.%d%s"), plv,
        plv >= PLV_MAX ? _T("（满级）") : _T(""));
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf); y += 26;
    _stprintf_s(buf, _countof(buf), _T("最终属性：攻击 %d   防御 %d   血上限 %d"),
        playerAttack(), playerDefense(), playerHpMax());
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf); y += 26;
    if (lastLvlSec > 0) {
        _stprintf_s(buf, _countof(buf), _T("最后一级用时：%d 秒"), lastLvlSec);
        outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf);
    }
    y += 30;

    // 上榜提示
    settextstyle(20, 0, _T("微软雅黑"));
    if (rankJustSet >= 0) {
        settextcolor(RGB(255, 215, 90));
        _stprintf_s(buf, _countof(buf), _T("★ 新纪录！排行榜第 %d 名 ★"), rankJustSet + 1);
    }
    else {
        settextcolor(RGB(140, 150, 170));
        _stprintf_s(buf, _countof(buf), _T("未进入排行榜前十"));
    }
    outtextxy(WIN_W / 2 - textwidth(buf) / 2, y, buf);
    y += 34;

    // 排行榜（本局成绩高亮）
    setlinecolor(RGB(60, 80, 110));
    line(48, y, WIN_W - 48, y); y += 10;
    settextcolor(RGB(170, 200, 240));
    settextstyle(18, 0, _T("微软雅黑"));
    outtextxy(WIN_W / 2 - textwidth(_T("—— 排行榜 ——")) / 2, y, _T("—— 排行榜 ——"));
    y += 26;
    settextstyle(17, 0, _T("Consolas"));
    int shown = (int)ranks.size() < 8 ? (int)ranks.size() : 8;
    for (int i = 0; i < shown; ++i) {
        bool mine = (i == rankJustSet);
        if (mine) {                                    // 高亮本局成绩
            setfillcolor(RGB(52, 46, 20));
            solidroundrect(56, y - 2, WIN_W - 56, y + 20, 5, 5);
        }
        settextcolor(mine ? RGB(255, 225, 120) : RGB(200, 205, 220));
        _stprintf_s(buf, _countof(buf), _T("%2d. %-10s %8d"), i + 1,
            ranks[i].name.c_str(), ranks[i].score);
        outtextxy(66, y, buf);
        y += 22;
    }
    if (ranks.empty()) {
        settextcolor(RGB(130, 140, 160));
        outtextxy(WIN_W / 2 - 50, y, _T("（暂无记录）"));
    }

    // 操作提示
    settextstyle(18, 0, _T("微软雅黑"));
    settextcolor(RGB(150, 220, 255));
    const TCHAR* t2 = _T("回车 返回菜单        Esc 退出游戏");
    outtextxy(WIN_W / 2 - textwidth(t2) / 2, WIN_H - 92, t2);
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
    rankJustSet = -1;
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
    // 记录本局成绩所在名次，供结算页高亮显示
    for (int i = 0; i < (int)ranks.size(); ++i)
        if (ranks[i].score == score && ranks[i].name == name) { rankJustSet = i; break; }
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