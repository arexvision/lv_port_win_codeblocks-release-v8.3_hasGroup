# arex_ui_state.h 结构体对比

## 文件路径
- **新架构**: `E:\UI\lv_port_win_codeblocks-release-v8.3_hasGroup\同时的新架构\arex_ui\arex_ui_state.h`
- **主工程**: `E:\UI\lv_port_win_codeblocks-release-v8.3_hasGroup\src\arex_ui\arex_ui_state.h`

---

## 结构体定义对比

### 新架构 `arex_ui_ctx_t` (105 行)

```c
typedef struct {
    arex_ui_state_t  state;

    uint8_t  dash_card;         /* 当前 DASH 所在 tile 位置：1~(CARD_POS_SETUP-1) */

    /* Menu cursors */
    uint8_t  menu_info_idx;
    uint8_t  menu_setup_idx;
    uint8_t  sub_menu_idx;
    uint8_t  gas_cursor;

    /* Wall-charge: consecutive scroll presses at boundary */
    uint8_t  wall_charge;       /* 0~3; reach 3 → cross boundary */
    int8_t   wall_dir;          /* +1 bottom  -1 top */

    /* Sub-menu stack */
    arex_sub_history_t sub_history[AREX_SUB_HISTORY_MAX];
    uint8_t            sub_history_depth;

    /* Inline value edit context */
    struct {
        float   value;
        float   min;
        float   max;
        float   step;
        float   original;
        uint8_t item_index;
        bool    active;
    } edit_ctx;

    /* Sub-menu content (current page) */
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;

    /* Parent state when sub-menu was opened */
    arex_ui_state_t sub_parent;

} arex_ui_ctx_t;
```

### 主工程 `arex_ui_ctx_t` (108 行)

```c
typedef struct {
    arex_ui_state_t  state;

    uint8_t  dash_card;         /* card_order[] 位置：0=INFO 1=COMPASS 2=DECO 3=GAS 4=PLAN 5=SETUP */

    /* Menu cursors */
    uint8_t  menu_info_idx;
    uint8_t  menu_setup_idx;
    uint8_t  sub_menu_idx;
    uint8_t  gas_cursor;

    /* Wall-charge: consecutive scroll presses at boundary */
    uint8_t  wall_charge;       /* 0~3; reach 3 → cross boundary */
    int8_t   wall_dir;          /* +1 bottom  -1 top */

    /* Sub-menu stack */
    arex_sub_history_t sub_history[AREX_SUB_HISTORY_MAX];
    uint8_t            sub_history_depth;

    /* Inline value edit context */
    struct {
        float   value;
        float   min;
        float   max;
        float   step;
        float   original;
        uint8_t item_index;
        bool    active;
    } edit_ctx;

    /* Sub-menu content (current page) */
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;

    /* Parent state when sub-menu was opened */
    arex_ui_state_t sub_parent;

    /* 告警清除标志：触发后必须先 click/rotate 一次才可清除 */
    bool alarm_pending_click;

} arex_ui_ctx_t;
```

---

## 差异汇总

| 字段 | 新架构 | 主工程 | 说明 |
|------|--------|--------|------|
| `dash_card` 注释 | `1~(CARD_POS_SETUP-1)` | `0=INFO 1=COMPASS 2=DECO 3=GAS 4=PLAN 5=SETUP` | 注释不同，实际含义一致 |
| `alarm_pending_click` | **不存在** | `bool alarm_pending_click` | **主工程新增字段** |

---

## 主工程新增字段详情

### `alarm_pending_click`
- **类型**: `bool`
- **位置**: 结构体末尾（第 77 行）
- **注释**: `/* 告警清除标志：触发后必须先 click/rotate 一次才可清除 */`
- **用途**: 控制告警清除逻辑，确保告警触发后需要用户操作才能清除

### 关联逻辑变化

主工程 `arex_ui_state.c` 中 `ui_handle_rotate()` 函数曾包含：

```c
void ui_handle_rotate(int8_t dir)
{
    /* 告警锁：触发后必须先 click/rotate 一次才可清除 */
    if (g_ui.alarm_pending_click) {
        g_ui.alarm_pending_click = false;
        arex_clear_all_alarm_styles();
    }
    // ... 其余逻辑
}
```

**注意**: 根据会话记录，该逻辑已从 `ui_handle_rotate()` 中移除（仅保留在头文件定义中）。

---

## API 接口对比

两者的 API 接口完全一致：

| 函数 | 两者 |
|------|------|
| `arex_ui_state_init()` | ✓ |
| `ui_handle_rotate(int8_t dir)` | ✓ |
| `ui_handle_click()` | ✓ |
| `ui_handle_back()` | ✓ |
| `arex_ui_refresh_all()` | ✓ |
| `arex_ui_go_to_card(uint8_t idx)` | ✓ |

---

## 状态枚举对比

两者的 `arex_ui_state_t` 枚举完全一致：

```c
typedef enum {
    UI_DASH         = 0,  /* scrolling dashboard cards */
    UI_INFO         = 1,  /* INFO menu list active */
    UI_SETUP        = 2,  /* SETUP menu list active */
    UI_EDIT_GAS     = 3,  /* gas cursor moving on 3F */
    UI_MODAL_GAS    = 4,  /* confirm-gas modal open */
    UI_MODAL_COMPASS= 5,  /* clear-compass-target modal */
    UI_SUB_MENU     = 6,  /* sub-menu layer visible */
    UI_MODAL_ACT    = 7,  /* generic action modal */
    UI_EDIT_VALUE   = 8,  /* inline value editor */
} arex_ui_state_t;
```

---

## 结论

- **新架构**: 纯状态机定义，无告警相关字段
- **主工程**: 新增 `alarm_pending_click` 字段，用于告警清除控制

如需同步到新架构，需要：
1. 添加 `alarm_pending_click` 字段到结构体
2. 在相应的 `.c` 文件中实现告警清除逻辑
