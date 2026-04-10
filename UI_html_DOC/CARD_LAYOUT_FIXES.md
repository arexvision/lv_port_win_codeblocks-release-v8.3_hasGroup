# Card Layout Fixes: HTML → LVGL

## Reference: HTML dimensions

| Element | HTML value | LVGL equivalent |
|---------|-----------|-----------------|
| Card size | 460×480px, `padding: 25px` | 460×480, tile, no padding |
| Content area | x=25..435, y=25..455 (410×430) | x=16..444, y=50..464 |
| `.card-title` font | `28*0.75 = 21px` Courier New bold | `AREX_FONT_TITLE` (20px) ✓ |
| `.card-title` color | `#55FF55` (AREX_LIGHT) | `AREX_LIGHT` ✓ |
| `.card-title` border | `2px solid #003300` at bottom | 2px AREX_DARK line ✓ |
| Title text top | y=25 (card padding) | y=12 (LVGL) |
| Title border bottom edge | y≈54 (25+~21+8) | y=40 (38+2) |
| Content top (after title) | y≈69 (54+2+15 margin) | y=50 (40+10 gap) |
| `.menu-list` gap | `8px` between items | flex gap or set_style_pad_column |
| `.menu-item` padding | `12px 15px` | pad_ver=12, pad_hor=15 |
| `.menu-item` height | ~45px (12+21+12) | 48px (12*2 + 20px font) |
| `.menu-item` font | `28*0.75=21px` | `AREX_FONT_TITLE` (20px) ✓ |
| `.menu-item` border | `2px solid #003300` | AREX_DARK 2px ✓ |

---

## Per-card fix list (priority order)

### INFO MENU (`card_info.c`) — DONE ✓
- `lv_obj_set_pos(s_list, 16, 50)` — fixed in prev session

### DIVE SETUP (`card_setup.c`) — NEEDS FIX
- Same structure as INFO MENU
- Change `lv_obj_align(LV_ALIGN_CENTER, 0, 20)` → `lv_obj_set_pos(s_list, 16, 50)`
- Add `gap: 8px` via `lv_obj_set_style_pad_row(s_list, 8, 0)`

### INFO MENU + DIVE SETUP list items — NEEDS FIX
- Both: item `pad_hor` is 12 everywhere (should be `pad_hor=15`, `pad_ver=12`)
- Item height: 52px → reduce to 48px (matches ~45px HTML + 2px border)
- Add `gap: 8` between items in list container

### 1F: NAV COMPASS (`card_compass.c`) — NEEDS FIX
- Canvas currently placed with `lv_obj_align(LV_ALIGN_BOTTOM_MID, 0, -20)`
- HTML: target-info-box (20px high) at y=69, compass wrapper height=140, heading text below
- Fix: set canvas pos to `(0, 50)`, fit canvas to remaining height

### 2F: TISSUES & DECO (`card_deco.c`) — NEEDS FIX
- HTML: three `deco-grid` rows each ~30px, total ~90px before tissue bars
- `deco-grid`: `border-bottom: 1px dashed #003300`, `padding-bottom: 5px`, `margin-bottom: 5px`
- Tissue container: `height: 80px`, starts after "TISSUE SATURATION" label
- LVGL currently: top row at y=36, bars at y=60+56=116 — too far down
- Fix: compact top stats to y=50..~140, tissue bar area at y=145 (height=80)

### 3F: GAS SWITCH (`card_gas.c`) — NEEDS FIX
- HTML: menu-list starts at y≈69, gap=8 between rows
- LVGL: rows at `lv_obj_set_pos(row, 30, 48 + i * 86)` — x=30 should be x=16, 86px pitch too large
- Item height 72 → reduce to 48, pitch = 48+8 = 56
- MOD/PPO2 labels inside row need repositioning (currently at x=200, y=8/28)

### 4F: DIVE PLAN TRACK (`card_plan.c`) — NEEDS FIX
- HTML: `plan-chart-shell` has `border: 2px solid #003300`, `padding: 10px`
- SVG chart: 400×320 in a shell starting at y≈69
- LVGL: canvas starts at (30, 60) → change to (16, 50)
- Canvas: CHART_W=380, CHART_H=280 → can expand to ~400×310 to use more space

---

## Global issues (all cards)

| Issue | HTML | Fix |
|-------|------|-----|
| Content x-offset | `padding: 25px` → x starts at 25 | Use x=16 in LVGL (card border area) |
| Content y-start | y≈69 after card-title | y=50 in LVGL |
| Flex gap in lists | `gap: 8px` | `lv_obj_set_style_pad_row(list, 8, 0)` |
| Item padding | `12px 15px` (ver/hor) | `pad_ver=12, pad_hor=15` |
| Item size | ~45px + 2×2px border = 49px | 48px |
