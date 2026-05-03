# arex_sys_config_t 卡片配置结构体对比

## 文件路径
- **新架构**: `E:\UI\lv_port_win_codeblocks-release-v8.3_hasGroup\同时的新架构\arex_ui\arex_ui_engine.h`
- **主工程**: `E:\UI\lv_port_win_codeblocks-release-v8.3_hasGroup\src\arex_ui\arex_ui_engine.h`

---

## 卡片区配置对比（完全不同的架构）

### 新架构：多卡片嵌套结构

```c
/* 独立卡片配置结构体 */
typedef struct {
    uint8_t            widget_count;                 /* 该卡片包含的组件数量 */
    arex_grid_widget_t widgets[AREX_5F_MAX_WIDGETS]; /* 该卡片的组件数组 */
} arex_custom_card_cfg_t;

/* arex_sys_config_t 中的卡片配置 */
typedef struct {
    // ...
    /* --- 右侧多张自定义网格卡片配置 --- */
    uint8_t                custom_card_count;          /* 自定义卡片数量 */
    arex_custom_card_cfg_t custom_cards[AREX_MAX_CUSTOM_CARDS]; /* 每张卡片的完整配置 */
    uint8_t                custom_card_slot[AREX_CARD_COUNT];   /* 每个槽位对应哪张卡片 */
    // ...
} arex_sys_config_t;
```

**架构特点**：
- 每张卡片有独立的组件数组
- 支持多张不同配置的卡片
- `custom_card_slot[]` 管理槽位映射

---

### 主工程：扁平数组结构

```c
/* arex_sys_config_t 中的卡片配置 */
typedef struct {
    // ...
    /* --- 右侧 5F 自定义网格配置 --- */
    uint8_t            custom_5f_count;              /* 组件数量 */
    arex_grid_widget_t custom_5f_widgets[AREX_5F_MAX_WIDGETS]; /* 扁平组件数组 */
    // ...
} arex_sys_config_t;
```

**架构特点**：
- 所有组件平铺在一个大数组里
- 只支持单张 5F 卡片
- 无槽位映射机制

---

## 结构差异总结

| 特性 | 新架构 | 主工程 |
|------|--------|--------|
| **卡片数量** | `custom_card_count` (多张) | `custom_5f_count` (单卡) |
| **组件存储** | 每张卡片独立 `widgets[]` | 所有组件扁平 `custom_5f_widgets[]` |
| **槽位管理** | `custom_card_slot[]` 映射 | 无 |
| **结构类型** | 嵌套 `arex_custom_card_cfg_t` | 直接 `arex_grid_widget_t[]` |
| **灵活性** | 高（支持多卡配置） | 低（单卡限制） |
| **内存占用** | 较大（预留多卡空间） | 较小 |

---

## 新架构的 AREX_MAX_CUSTOM_CARDS 定义

```c
#define AREX_MAX_CUSTOM_CARDS AREX_MAX_DYNAMIC_SLOTS
```

由 `arex_card_registry.h` 中的 `CARD_POS_COUNT` 或动态槽数量决定。

---

## 结论

两个版本的卡片配置**架构完全不同**：
- **新架构**：多卡片 + 槽位映射（复杂但灵活）
- **主工程**：单卡扁平数组（简单但受限）

如果要同步，需要决定：
1. 主工程是否升级为多卡片架构？
2. 还是保持当前单卡结构？
