# AREX Pro Dive Computer UI 鈥?鏋舵瀯瑙ｈ鏂囨。

> 鍩轰簬 `UI_html/arex_ui_test_0.10.html` 鍘熷瀷锛岀Щ妞嶅埌 LVGL v8.3 (Windows/CodeBlocks)  
> 鍏ュ彛锛歚UI_main()` in `src/arex_ui/UI_main.c`

---

## 1. 鏁翠綋鐩綍缁撴瀯

```
src/arex_ui/
鈹溾攢鈹€ UI_main.c               # 鍏ュ彛锛屽垵濮嬪寲搴忓垪 + 浠跨湡 tick 瀹氭椂鍣?
鈹溾攢鈹€ arex_ui_engine.h/c      # 鍏ㄥ眬鐘舵€佹暟鎹ā鍨嬶紙g_sys_config + g_sensor_data锛?
鈹溾攢鈹€ arex_ui_state.h/c       # 鐘舵€佹満鏍稿績锛坓_ui锛夛紝涓変釜杈撳叆澶勭悊鍑芥暟
鈹溾攢鈹€ arex_screen.h/c         # LVGL 鎺т欢鏍戝垱寤?+ 鎵€鏈夊睆骞曟搷浣?API
鈹溾攢鈹€ arex_input.h/c          # 杈撳叆浜嬩欢鎹曡幏锛堥敭鐩?缂栫爜鍣?鈫?鐘舵€佹満锛?
鈹溾攢鈹€ arex_card_registry.h/c # 鍗＄墖娉ㄥ唽琛紙ID銆乼itle銆乧reate/update 鍥炶皟锛?
鈹溾攢鈹€ arex_data.h/c          # 鏁版嵁鎬荤嚎澶存枃浠跺瓨鏍癸紙arex_ui_engine.h 鍖呭惈涓€鍒囷級
鈹斺攢鈹€ cards/
    鈹溾攢鈹€ card_info.c          # 0F: INFO MENU锛? 鏉￠潤鎬佸垪琛級
    鈹溾攢鈹€ card_compass.c       # 1F: NAV COMPASS锛坈anvas 缁樺埗鑸悜鍗峰昂锛?
    鈹溾攢鈹€ card_deco.c          # 2F: TISSUES & DECO锛?6 闅斿鏌辩姸鍥?+ GF/CNS/OTU锛?
    鈹溾攢鈹€ card_gas.c           # 3F: GAS SWITCH锛? 绉嶆皵浣擄紝MOD 鏍￠獙锛?
    鈹溾攢鈹€ card_plan.c          # 4F: DIVE PLAN TRACK锛坈anvas 缁樺埗娼滄按鍓栭潰鍥撅級
    鈹斺攢鈹€ card_setup.c         # 5F: DIVE SETUP锛? 鏉￠潤鎬佸垪琛級
```

---

## 2. 鍚姩搴忓垪锛歚UI_main()`

```
UI_main()
  鈹?
  鈹溾攢 arex_ui_init()             鈫?鍔犺浇榛樿閰嶇疆鍒?g_sys_config + 鍒濆鍖?g_sensor_data 婕旂ず鏁版嵁
  鈹溾攢 arex_screen_create()        鈫?鍒涘缓鏁翠釜 LVGL 鎺т欢鏍戯紙宸﹂潰鏉?+ tileview + 寮圭獥 + 瀛愯彍鍗曞眰锛?
  鈹?   鈹溾攢鈹€ left_panel_create()
  鈹?   鈹溾攢鈹€ right_panel_create()  鈫?鍒涘缓 tileview锛屾寜 card_order 璋冪敤 card_*_create()
  鈹?   鈹溾攢鈹€ wall_create()
  鈹?   鈹溾攢鈹€ modal_create()
  鈹?   鈹斺攢鈹€ submenu_layer_create()
  鈹溾攢 arex_input_init(scr)        鈫?娉ㄥ唽閿洏/缂栫爜鍣ㄤ簨浠跺洖璋?
  鈹溾攢 arex_screen_refresh_left_panel()   鈫?宸︿晶闈㈡澘鍒濆鍊煎～鍏?
  鈹溾攢 arex_screen_scroll_to_card(0)      鈫?璺冲埌 tile 0锛圛NFO 鍗★級
  鈹溾攢 arex_screen_set_info_selection(0)  鈫?楂樹寒绗竴鏉?LAST DIVE
  鈹溾攢 arex_ui_state_init()       鈫?灏?g_ui 娓呴浂锛宻tate=UI_INFO锛宒ash_card=1
  鈹斺攢 lv_timer_create(sim_tick_cb, 1000ms)  鈫?姣忕浠跨湡 tick
```

**浠跨湡 tick (`sim_tick_cb`) 姣忕鍋氾細**
1. `g_sensor_data.heading += 1掳 % 360`锛堣埅鍚戠紦鎱㈡紓绉伙級+ `g_sensor_data.dive_time_s++`
2. `arex_screen_refresh_left_panel()` 鈫?鍒锋柊宸﹂潰鏉挎暟鍊?
3. `arex_ui_refresh_all()` 鈫?閬嶅巻娉ㄥ唽琛紝璋冪敤姣忎釜鍗＄墖鐨?`update_cb()`

---

## 3. 鏁版嵁妯″瀷锛歚arex_ui_engine.h/c`

### 鏍稿績鏁版嵁缁撴瀯浣擄紙涓ゆ€荤嚎鍒嗙锛?

璇﹁ Section 16 `arex_sys_config_t` 鍜?`arex_sensor_data_t` 瀹氫箟銆?
- 瀹炴椂鏁版嵁鎬荤嚎锛歚g_sensor_data` 鈥?UI 鎺т欢姣?tick 璇诲彇
- 閰嶇疆鎬荤嚎锛歚g_sys_config` 鈥?甯冨眬鍙傛暟 + 鐢ㄦ埛璁剧疆锛圓PP 鍙悓姝ワ級

### 姘斾綋琛紙闈欐€侊紝4 绉嶏級

| Index | name | MOD(m) |
|-------|------|--------|
| 0 | AIR | 56 |
| 1 | NX 32 | 34 |
| 2 | TX 18/45 | 68 |
| 3 | O2 100% | 6 |

### 鍏抽敭璁捐鐐?

- `g_sys_config.card_order[pos] = card_id`锛氭帶鍒?tileview 涓崱鐗囩殑鏄剧ず椤哄簭
- **浣嶇疆鏋氫妇 `arex_card_pos_t`**锛歚CARD_POS_INFO`=0锛堝浐瀹?tile 0锛夈€乣CARD_POS_1`~`CARD_POS_4`锛堜腑闂?4 涓彲閲嶆帓锛夈€乣CARD_POS_SETUP`=5锛堝浐瀹?tile 5锛?
- **鍗＄墖 ID 鏋氫妇 `arex_card_id_t`**锛歚CARD_ID_INFO` ~ `CARD_ID_SETUP`锛岃〃绀哄崱鐗囧浐鏈夎韩浠?
- `g_sys_card_order(pos)`锛氱粺涓€鍏ュ彛锛岄€氳繃 `card_order[pos]` 鏌ヨ鍗＄墖 ID
- 鐢ㄦ灇涓炬樉寮忚祴鍊硷細`cfg->card_order[CARD_POS_INFO] = CARD_ID_INFO`锛圛NFO/SETUP 鍥哄畾锛屼腑闂村彲閲嶆帓锛?
- 宸︿晶閿氱偣閫氳繃 `left_layout[]` 琛岄厤缃┍鍔紝浠绘剰涓ゆā鍧楀彲鑷敱鍙屾嫾
- 姘斾綋甯搁噺 `AREX_GAS_NAMES[]` / `AREX_GAS_MOD_M[]` 瀹氫箟浜?`arex_ui_engine.c`
- `g_sensor_data.tissue_pct[]` 鍘熷鍊肩敱鍑忓帇寮曟搸璁＄畻锛孶I 灞傛寜鐧惧垎姣旀覆鏌?

---

## 4. 鐘舵€佹満锛歚arex_ui_state.h/c`

### 鐘舵€佹灇涓?

```
UI_DASH         (0)  鈥?涓诲崱鐗囨粴鍔ㄦā寮?
UI_INFO         (1)  鈥?INFO 鑿滃崟鍒楄〃婵€娲?
UI_SETUP        (2)  鈥?SETUP 鑿滃崟鍒楄〃婵€娲?
UI_EDIT_GAS     (3)  鈥?3F 姘斾綋閫夋嫨鍏夋爣绉诲姩涓?
UI_MODAL_GAS    (4)  鈥?姘斾綋鍒囨崲纭寮圭獥宸叉墦寮€
UI_MODAL_COMPASS(5)  鈥?娓呴櫎缃楃洏鐩爣纭寮圭獥
UI_SUB_MENU     (6)  鈥?瀛愯彍鍗曞眰宸插脊鍑猴紙浠庡彸渚ф粦鍏ワ級
UI_MODAL_ACT    (7)  鈥?閫氱敤鍔ㄤ綔寮圭獥锛?绉掕嚜鍔ㄥ叧闂級
UI_EDIT_VALUE   (8)  鈥?鏁板€煎唴鑱旂紪杈戯紙渚嬪 MOD PO2锛?
```

### 鍏ㄥ眬 UI 涓婁笅鏂囷細`arex_ui_ctx_t g_ui`

```c
typedef struct {
    arex_ui_state_t  state;           // 褰撳墠鐘舵€侊紙鍒濆 UI_INFO锛?
    uint8_t  dash_card;               // 褰撳墠鍗＄墖绱㈠紩锛堝垵濮?1锛孋OMPASS锛?
    uint8_t  menu_info_idx;           // INFO 鑿滃崟鍏夋爣
    uint8_t  menu_setup_idx;          // SETUP 鑿滃崟鍏夋爣
    uint8_t  sub_menu_idx;            // 瀛愯彍鍗曞厜鏍?
    uint8_t  gas_cursor;              // 姘斾綋鍒楄〃鍏夋爣锛圲I_EDIT_GAS 鏈熼棿锛?
    uint8_t  wall_charge;             // 杈圭晫纰版挒璁℃暟锛?~3锛屽埌3鎵嶇┛瓒婏級
    int8_t   wall_dir;                // +1=搴曢儴  -1=椤堕儴
    arex_sub_history_t sub_history[4];// 瀛愯彍鍗曞鑸爤
    uint8_t  sub_history_depth;
    struct { float value,min,max,step,original; uint8_t item_index; bool active; } edit_ctx;
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;
    arex_ui_state_t sub_parent;       // 杩涘叆瀛愯彍鍗曟椂鐨勭埗鐘舵€?
} arex_ui_ctx_t;
```

> **鍚姩琛屼负锛?* `arex_ui_state_init()` 灏?`state=UI_INFO`锛宍dash_card=1`锛宍menu_info_idx=0`锛宍wall_charge=0`銆傚惎鍔ㄥ悗鐩存帴鏄剧ず INFO 鑿滃崟锛坱ile 0锛夛紝绛夊緟鐢ㄦ埛鎿嶄綔銆?

### 涓変釜鍏紑杈撳叆澶勭悊鍑芥暟

| 鍑芥暟 | 瑙﹀彂 | 浣滅敤 |
|------|------|------|
| `ui_handle_rotate(int8_t dir)` | 涓婁笅閿?缂栫爜鍣ㄦ棆杞?| 鍗＄墖婊氬姩銆佽彍鍗曠Щ鍔ㄣ€佹暟鍊艰皟鏁?|
| `ui_handle_click()` | Enter/缂栫爜鍣ㄦ寜涓?| 纭閫夋嫨銆佽繘鍏ュ瓙鑿滃崟銆佹爣璁拌埅鍚?|
| `ui_handle_back()` | ESC/Backspace | 鍙栨秷/閫€鍑?鍏抽棴寮圭獥 |

---

## 5. 鏍稿績浜や簰娴佺▼

### 5.1 Wall-Charge 杈圭晫绌胯秺鏈哄埗

**`dash_card` 璇箟锛堜笌 HTML 涓€鑷达級锛?*
- `dash_card` = card 鍦?`card_order[]` 涓殑浣嶇疆锛?~5锛?
- `dash_card=0` 鈫?INFO锛堜粎 wall-charge 鍙繘锛夛紝`dash_card=1` 鈫?COMPASS锛宍dash_card=2` 鈫?DECO锛宍dash_card=3` 鈫?GAS锛宍dash_card=4` 鈫?PLAN锛宍dash_card=5` 鈫?SETUP锛堜粎 wall-charge 鍙繘锛?

```
card_order 甯冨眬锛歔0]=INFO  [1]=COMPASS  [2]=DECO  [3]=GAS  [4]=PLAN  [5]=SETUP
                  鈫?wall-charge 鎵嶈兘杩?                           鈫?wall-charge 鎵嶈兘杩?

DASH 鍙粴鍔ㄨ寖鍥达細dash_card 鈭?[1, AREX_CARD_COUNT-2]锛堝彲閰嶇疆锛?

鍦?UI_DASH 涓嬶細
  dash_card==1 涓旂户缁悜涓?鈫?wall_charge++锛屾樉绀洪《閮ㄥ "[#][ ][ ]"
                            鈫?tileview 鍚戜笅鍋忕Щ charge脳20px 鐒跺悗寮瑰洖锛堟鐨瓔鎰燂級
  杩炵画3娆?鈫?绌胯秺鍒?UI_INFO锛堟粴鍔ㄥ埌 tile_pos=0锛夛紝wall_charge 娓呴浂

  dash_card==AREX_CARD_COUNT-2 涓旂户缁悜涓?鈫?wall_charge++锛屾樉绀哄簳閮ㄥ
                            鈫?tileview 鍚戜笂鍋忕Щ charge脳20px 鐒跺悗寮瑰洖
  杩炵画3娆?鈫?绌胯秺鍒?UI_SETUP锛堟粴鍔ㄥ埌 tile_pos=AREX_CARD_COUNT-1锛?

  浠讳綍涓€旀敼鍙樻柟鍚?鈫?wall_charge = 0锛屽UI闅愯棌锛宼ileview 绔嬪嵆褰掍綅
```

**姗＄毊绛嬪姩鐢诲疄鐜帮紙`wall_nudge_tileview`锛夛細**
瀵?`s_tileview` 鍋?`lv_obj_set_y` 鍔ㄧ敾锛?50ms ease-out 骞虫粦鎺ㄥ埌 `charge脳20px`锛屽仠鍦ㄩ偅閲岀洿鍒?wall 娓呴浂銆?
`arex_screen_hide_walls` 鏃剁珛鍗?`set_y(0)` 褰掍綅銆?
瀵瑰簲 HTML 鐨?`transition: 0.35s cubic-bezier(0.2,0.8,0.2,1)` + `updateElevator(wallCharge * 20)`锛屾棤鍥炲脊銆?

UI_INFO 閫€鍑猴紙wall-charge 鎴?ESC锛夆啋 杩斿洖 DASH锛宒ash_card=1锛圕OMPASS锛?
UI_SETUP 閫€鍑猴紙wall-charge 鎴?ESC锛夆啋 杩斿洖 DASH锛宒ash_card=AREX_CARD_COUNT-2锛圥LAN锛?

> **鍚姩琛屼负璇存槑锛?* 鍚姩鐩存帴杩涘叆 `UI_INFO` 鐘舵€侊紝鏄剧ず INFO 鍗★紙tile 0锛夛紝鍏夋爣鑱氱劍绗竴鏉?LAST DIVE銆?
> 鍦?INFO 鑿滃崟搴曢儴 wall-charge锛堣繛缁?娆″悜涓嬶級鈫?杩涘叆 `UI_DASH`锛屼粠 COMPASS锛坱ile 1锛夊紑濮嬨€?
> 鍦?DASH 椤堕儴 wall-charge锛圕OMPASS 澶勮繛缁?娆″悜涓婏級鈫?杩斿洖 `UI_INFO`銆?

### 5.2 姘斾綋鍒囨崲娴佺▼锛?F 鍗＄墖锛?

> **銆愰噸鐐?路 CONFIG GAS 閫€鍑鸿鍒欍€?*  
> **姘斾綋鍦ㄥ綋鍓嶆繁搴︿笉鍙敤**锛坄dive.depth >` 璇ユ皵浣撶殑 **MOD**锛屼笉閫傜敤娣卞害锛夋椂锛?*涓嶈兘**鐢ㄧ‘璁ら敭瀹屾垚鍒囨崲锛氬脊绐楀唴 **Enter/鐐瑰嚮** 浠呰Е鍙?`arex_screen_pulse_modal()` 闇囧姩锛?*涓嶄細**鏀?`active_idx`銆?*涓嶄細**鍥炲埌浠〃鐩樸€? 
> 姝ゆ椂**蹇呴』**閫氳繃 **杩斿洖閿紙Back / ESC锛?* 閫€鍑?CONFIG GAS锛歚UI_MODAL_GAS` 鍏堝洖鍒?`UI_EDIT_GAS`锛屽啀鎸変竴娆¤繑鍥炴墠鍥炲埌 `UI_DASH`銆? 
> **涓嶅彲鐢ㄦ皵浣撴椂锛屼笉寰椾互銆岀‘璁ゅ垏鎹€嶇殑鏂瑰紡绂诲紑姘斾綋閰嶇疆娴佺▼銆?*

```
UI_DASH锛堝綋鍓嶅崱鐗囦负 GAS锛宑ard_order index=3锛?
  鈹?
  CLICK 鈫?UI_EDIT_GAS锛実as_cursor = active_idx
  鈹?       card_gas_update() 楂樹寒褰撳墠鍏夋爣琛?
  鈹?
  ROTATE 鈫?gas_cursor 寰幆绉诲姩锛?鈫?鈫?鈫?鈫?鈥︼級
  鈹?       card_gas_update() 閲嶇粯
  鈹?
  CLICK 鈫?UI_MODAL_GAS锛宎rex_screen_show_modal_gas()
  鈹?
  鈹溾攢 娣卞害 鈮?MOD 鈫?CLICK 纭锛歛ctive_idx = gas_cursor锛岃繑鍥?UI_DASH
  鈹斺攢 娣卞害 > MOD锛堟皵浣撲笉鍙敤锛夆啋 CLICK 鏃犳晥锛歮odal 闇囧姩锛涗粎鑳介€氳繃 BACK 閫€鍑猴紙瑙佷笂鏂囬噸鐐癸級
  
  BACK / ESC:
    UI_MODAL_GAS 鈫?UI_EDIT_GAS锛堝叧寮圭獥锛岀暀鍦ㄦ皵浣撶紪杈戯級
    UI_EDIT_GAS  鈫?UI_DASH锛堥€€鍑?CONFIG GAS锛?
```

### 5.3 缃楃洏鏍囪娴佺▼锛?F 鍗＄墖锛?

```
UI_DASH锛堝綋鍓嶅崱鐗囦负 COMPASS锛?
  鈹?
  CLICK锛堟湭鏍囪锛夆啋 compass.marked=true锛宼arget=heading锛宑anvas 鐢婚粍鑹茬珫绾?
  鈹?
  CLICK锛堝凡鏍囪锛夆啋 UI_MODAL_COMPASS 寮圭獥
  鈹?
  CLICK 纭 鈫?compass.marked=false锛屾竻闄ゆ爣璁帮紝杩斿洖 UI_DASH
  ESC      鈫?鍙栨秷锛岃繑鍥?UI_DASH
```

### 5.4 瀛愯彍鍗曟祦绋嬶紙INFO/SETUP 鑿滃崟锛?

```
UI_INFO / UI_SETUP
  鈹?
  CLICK 鈫?arex_screen_open_info/setup_submenu(item_idx)
           - 鐢ㄥ唴缃瓧绗︿覆琛ㄥ～鍏?sub_items[]
           - submenu_slide_in()锛氫粠鍙充晶婊戝叆锛坙v_anim 250ms ease-out锛?
           - state 鈫?UI_SUB_MENU锛宻ub_parent = UI_INFO/UI_SETUP
  鈹?
  UI_SUB_MENU
    ROTATE 鈫?sub_menu_idx 绉诲姩
    CLICK 鈫?arex_screen_handle_submenu_select()
             "< BACK" 鈫?arex_screen_close_submenu()锛坰lide out锛屾仮澶?sub_parent 鐘舵€侊級
             鍏朵粬     鈫?锛堟墿灞曞疄鐜颁腑锛?
    ESC 鈫?arex_screen_close_submenu()
```

### 5.5 鏁板€煎唴鑱旂紪杈戯紙MOD PO2锛?

```
UI_SUB_MENU锛圖IVE SETUP 瀛愯彍鍗曪紝"MOD PO2: X.X" 琛岄珮浜級
  鈹?
  CLICK "MOD PO2: X.X" 鈫?arex_screen_begin_edit_value()
    - edit_ctx = {value=褰撳墠鍊? min=1.0, max=1.6, step=0.1, original=鏃у€紏
    - UI 鍙樺寲锛氳浠庣豢搴曢粦瀛楁仮澶嶄负榛戝簳缁垮瓧 + 缁胯竟妗?
    - 甯冨眬锛氫笌 HTML `.menu-item` 涓€鑷?鈥?`display:flex; justify-content:space-between; align-items:center`锛圠VGL锛歚LV_LAYOUT_FLEX` + `LV_FLEX_ALIGN_SPACE_BETWEEN`锛夛紝涓夊垪锛歚MOD PO2: ` | 缁垮簳鏁板€?| `^ v`
    - value badge锛氱豢搴?AREX_GREEN) + 榛戝瓧(AREX_BLACK)
    - 鈻测柤 绠ご锛欰REX_LIGHT 鐏拌壊锛岃创鍙?
    - 鍚姩 600ms 瀹氭椂鍣?toggle 闂儊
  鈹?
  ROTATE 鈫?edit_ctx.value 卤 step锛坈lamp 鍒?min/max锛?
            arex_screen_refresh_edit_value() 鏇存柊 badge 鍐呮暟鍊?
            闂儊涓嶄腑鏂紙瀹氭椂鍣ㄧ户缁級
  鈹?
  CLICK  鈫?鎻愪氦锛歡_sys_config.mod_ppo2 = edit_ctx.value
            鍋滄闂儊锛屾竻鐞?badge/arrows锛屾仮澶嶅畬鏁存爣绛?
            杩斿洖 UI_SUB_MENU锛堣琛屾仮澶嶉€変腑鎬侊級
  ESC    鈫?鍙栨秷锛氭仮澶?edit_ctx.original
            鍋滄闂儊锛屾竻鐞?badge/arrows锛屾仮澶嶆棫鍊?
            杩斿洖 UI_SUB_MENU
```

> **瀹炵幇缁嗚妭锛?* `s_edit_flash_timer`锛坄lv_timer`锛?00ms锛夋寔缁垏鎹?badge 鑳屾櫙鑹诧紙缁库啍榛戯級涓庢暟鍊?label 鏂囧瓧鑹诧紙榛戔啍缁匡級锛宍edit_flash_start()` 涓嶆竻绌?`s_edit_flash_badge`/`s_edit_flash_val_lbl`锛岀‘淇濆畾鏃跺櫒鍥炶皟鏈夋晥銆?

---

## 6. 灞忓箷甯冨眬锛歚arex_screen.h/c`

### 鏁翠綋甯冨眬锛?40脳480px锛?

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
鈹? Left Panel 180px  鈹?     Right Canvas 460px         鈹?
鈹?                   鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?  鈹?
鈹? DEPTH  45.2       鈹? 鈹? Tileview锛堝瀭鐩?鍗＄墖锛?   鈹?  鈹?
鈹? NDL 0  TTS 24'    鈹? 鈹? card_order[0] 鈫?tile 0  鈹?  鈹?
鈹? NEXT STOP 21m 3'  鈹? 鈹? card_order[1] 鈫?tile 1  鈹?  鈹?
鈹? POD1 210  POD2195 鈹? 鈹? ...                     鈹?  鈹?
鈹? GAS TX18/45       鈹? 鈹? card_order[5] 鈫?tile 5  鈹?  鈹?
鈹? PO2 1.2|1.2|1.3   鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?  鈹?
鈹? TIME 38:14        鈹? [澧橴I top/bottom 闅愯棌鍙犲姞]     鈹?
鈹?                   鈹? [瀛愯彍鍗曞眰 浠庡彸渚ф帹鍏          鈹?
鈹?                   鈹? [寮圭獥閬僵灞?hidden]            鈹?
鈹?                   鈹? [scroll dots 鍙充晶 6涓偣]       鈹?
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?
```

### 鍐呴儴闈欐€佹帶浠跺彞鏌勶紙鍏ㄩ儴 `static` 鍦?arex_screen.c锛?

| 鍙ユ焺 | 璇存槑 |
|------|------|
| `s_scr` | 鏍?screen 瀵硅薄 |
| `s_left_panel` | 宸﹂潰鏉垮鍣?|
| `s_tileview` | 鍙充晶鍗＄墖 tileview |
| `s_lbl_depth/ndl/tts/鈥 | 宸﹂潰鏉垮悇鏁板€兼爣绛?|
| `s_wall_top/bottom` | 杈圭晫鎸囩ず澧欙紙榛樿 HIDDEN锛?|
| `s_scroll_dots[6]` | 鍙充晶婊氬姩鐐?|
| `s_modal / s_modal_box` | 寮圭獥閬僵 + 鍐呭妗?|
| `s_submenu_layer` | 瀛愯彍鍗曞叏灞忓眰锛堥粯璁?x=460 闅愯棌鍦ㄥ彸渚у睆澶栵級|
| `s_info_list / s_setup_list` | 鐢?card_info.c/card_setup.c 娉ㄥ唽 |

### Tileview 宸ヤ綔鏂瑰紡

- 鍒涘缓鏃舵寜 `card_order[]` 椤哄簭閫愪釜璋冪敤 `lv_tileview_add_tile()`
- 绂佺敤鑷韩 touch/scroll锛坄LV_OBJ_FLAG_SCROLLABLE` 宸叉竻闄わ級
- 鍒囨崲鐢?`arex_screen_scroll_to_card()` 璋冪敤 `lv_obj_set_tile()` 甯﹀姩鐢?

---

## 7. 鍗＄墖绯荤粺锛歚arex_card_registry.h/c`

### 鍗＄墖鎻忚堪绗?

```c
typedef struct {
    arex_card_id_t  id;             // 绋冲畾ID锛?~5锛?
    const char     *title;          // 鍗＄墖鏍囬锛岃嫳鏂?
    lv_obj_t       *tile_obj;       // create 鍚庡～鍏ワ紙NULL 鐩村埌 create 鍥炶皟鎵ц锛?
    void (*create_cb)(lv_obj_t *parent);   // 涓€娆℃€у缓鎺т欢
    void (*update_cb)(void);               // 姣?tick 鍒锋柊鏁版嵁
    void (*on_enter_cb)(void);             // 婊氬姩鍒版鍗℃椂锛堝彲閫夛紝NULL 琛ㄧず涓嶅鐞嗭級
} arex_card_reg_t;
```

### 鏁伴噺甯搁噺

```c
AREX_CARD_COUNT        = 6   // INFO+COMPASS+DECO+GAS+PLAN+SETUP
AREX_DASH_CARD_COUNT   = 4   // DASH 鍙粦鍔ㄨ寖鍥达紙鎺掗櫎棣栧熬 INFO/SETUP锛?
```

### 6寮犲崱鐗囦竴瑙?

| ID | 鏂囦欢 | 鏍囬 | 鏍稿績瀹炵幇 |
|----|------|------|----------|
| 0 CARD_ID_INFO | card_info.c | INFO MENU | `arex_render_dynamic_menu()` 娓叉煋锛宍arex_menu_item_cfg_t[]` 閰嶇疆鏁版嵁 |
| 1 CARD_ID_COMPASS | card_compass.c | NAV COMPASS | 420脳380 canvas锛宍draw_tape()` 姣忓抚閲嶇粯锛岀洰鏍囪埅鍚戦粍绾?|
| 2 CARD_ID_DECO | card_deco.c | TISSUES & DECO | 16 绔栨潯璐村崱鐗囧簳锛堝榻?HTML `margin-top:auto`锛夛紱鏉℃Ы `AREX_DARK` 鍗婇€忔槑锛涘～鍏呯豢 / `>70%` 娴呯豢 / `鈮?0%` 鍙嶈壊闂儊锛汼urfGF>100 缁垮簳榛戝瓧 |
| 3 CARD_ID_GAS | card_gas.c | GAS SWITCH | 4琛?`lv_obj` 瀹瑰櫒锛屽厜鏍?婵€娲?瓒匨OD涓夋€侀鑹诧紝瀹炴椂 PPO2 璁＄畻 |
| 4 CARD_ID_PLAN | card_plan.c | DIVE PLAN TRACK | 380脳280 canvas锛岀綉鏍?鎶樼嚎+褰撳墠浣嶇疆榛勭偣 |
| 5 CARD_ID_SETUP | card_setup.c | DIVE SETUP | `arex_render_dynamic_menu()` 娓叉煋锛宍arex_menu_item_cfg_t[]` 鍚?badge 寰界珷 |

### `arex_card_get(pos)` vs `arex_card_get_by_id(id)`

```c
// 閫氳繃鏄剧ず浣嶇疆鍙栵紙璧?card_order[] 闂存帴灞傦級
arex_card_get(CARD_POS_1)  鈫? s_registry[ card_order[CARD_POS_1] ] = s_registry[CARD_ID_COMPASS]

// 閫氳繃绋冲畾ID鍙栵紙涓嶈蛋闂存帴灞傦級
arex_card_get_by_id(CARD_ID_GAS)  鈫? s_registry[CARD_ID_GAS]

// 鍗＄墖鏁伴噺锛堢粺涓€鍏ュ彛锛?
arex_card_count()  鈫? AREX_CARD_COUNT (=6)
```

> **淇敼鍗＄墖椤哄簭**锛堜粎闄愪腑闂?4 涓紝INFO/SETUP 鍥哄畾锛夛細
> ```c
> cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;     // 鍥哄畾 tile 0
> cfg->card_order[CARD_POS_1]     = CARD_ID_COMPASS; // 鍙噸鎺?
> cfg->card_order[CARD_POS_2]     = CARD_ID_DECO;    // 鍙噸鎺?
> cfg->card_order[CARD_POS_3]     = CARD_ID_GAS;     // 鍙噸鎺?
> cfg->card_order[CARD_POS_4]     = CARD_ID_PLAN;    // 鍙噸鎺?
> cfg->card_order[CARD_POS_SETUP] = CARD_ID_SETUP;    // 鍥哄畾 tile 5
> ```
> `card_order[pos] = card_id`锛宲os 鐢?`CARD_POS_*`锛宑ard_id 鐢?`CARD_ID_*`锛屽惈涔夋竻鏅般€佷笉鏄撳～鍙嶃€?

---

## 8. 杈撳叆澶勭悊锛歚arex_input.c`

```
arex_input_init()
  鈹?
  鈹溾攢 鍒涘缓 1脳1px 閫忔槑 btn锛坈atcher锛夛紝鎸傚湪 scr 涓?
  鈹溾攢 group_kbd锛歬eypad 鈫?KEY 浜嬩欢 鈫?key_event_cb()
  鈹斺攢 group_enc锛歟ncoder锛屽浐瀹?editing=true 鈫?enc_diff 鈫?LEFT/RIGHT 鈫?key_event_cb()
                encoder 鎸変笅 鈫?enc_click_cb() 鈫?ui_handle_click()

key_event_cb():
  LV_KEY_UP / LV_KEY_LEFT    鈫?ui_handle_rotate(-1)
  LV_KEY_DOWN / LV_KEY_RIGHT 鈫?ui_handle_rotate(+1)
  LV_KEY_ENTER               鈫?ui_handle_click()
  LV_KEY_ESC / LV_KEY_BACKSPACE 鈫?ui_handle_back()
```

**娉ㄦ剰锛?* 缂栫爜鍣ㄧ粍寮哄埗 `editing=true`锛岃繖鎰忓懗鐫€缂栫爜鍣ㄦ棆杞湪 LVGL 鍐呴儴浼氬彂鍑?`LV_KEY_LEFT/RIGHT`锛堣€岄潪 `LV_KEY_UP/DOWN`锛夛紝涓庨敭鐩樼粍鍏辩敤鍚屼竴涓?`key_event_cb`銆?

---

## 9. 鍚勫崱鐗囪缁嗗疄鐜?

### 9.1 card_compass.c锛?F锛?

- 涓庤鑼冨榻愶細Canvas **420脳140**px锛屾爣棰樹笅 y=50锛?
  - 婊戝姩妗?Tape)锛氶珮 **60px**锛?px `AREX_DARK` 杈规
  - 鑸悜鏁板瓧锛氭渶鎺ヨ繎瑙勮寖 46.4px锛屽瓧搴撶敤 `AREX_FONT_HUGE`(48px)锛屽眳涓?
  - 涓績娓告爣锛?*瀹?4px**锛岄珮 60px锛宍AREX_GREEN`
- `draw_tape(heading)` 姣忔瀹屾暣娓呭睆閲嶇粯锛?
  - 浠?heading 涓轰腑蹇冿紝卤60掳 鑼冨洿鐢诲埢搴︾嚎锛屾瘡搴?3px
  - major锛堟瘡45掳锛夌敾楂樺埢搴?鏂逛綅瀛楁瘝锛圢/NE/E/SE/S/SW/W/NW锛?
  - minor锛堟瘡15掳锛夌敾涓埢搴︼紱鍏朵綑鐢荤煭鍒诲害
  - **涓嶈緭鍑?`掳` 瀛楃** 鈥?`lv_font_courier_*` 浠呭惈 ASCII `0x20-0x7E`锛宍U+00B0` 浼氭樉绀轰负鏂规
  - 鑻?`compass.marked`锛氬湪鐩爣鑸悜瀵瑰簲浣嶇疆鐢婚粍鑹茬珫绾匡紝涓嬫柟鏄剧ず `TARGET 265`锛堝悓涓婃棤搴︾鍙凤級
- 姣忕閫氳繃 `card_compass_update()` 瑙﹀彂閲嶇粯

### 9.2 card_deco.c锛?F锛?

- 甯冨眬锛氫笌 `arex_ui_test_0.10.html` 涓€鑷?鈥?`.card` 涓哄垪 flex锛宍.tissue-section-title` 浣跨敤 `margin-top:auto`锛岄殧瀹ゅ浘鍦ㄥ崱鐗?*鏈€涓嬫柟**锛汱VGL 鐢?`BARS_Y` 浠?`TILE_H` 鍚戜笂鍙嶇畻锛涜鑼?Section Title Y鈮?50锛宍BOTTOM_PAD=36`銆?
- 椤堕儴涓夎 `.deco-grid`锛坹=60/107/154锛夛細鏂囨 `SurfGF`銆乣GF LOW / HIGH` 绛変笌 HTML 涓€鑷淬€?
- **SurfGF**锛歚surf_gf > 100` 鏃朵负 `.highlight-invert`锛坄AREX_GREEN` 搴?+ `AREX_BLACK` 瀛?+ 姘村钩 padding 4锛夛紝鏃犵孩瀛楅棯鐑併€?
- 16 涓?`lv_bar`锛堣鑼冿細闂磋窛 4px锛屾Ы `AREX_DARK`+`LV_OPA_50`锛屽～鍏呪墹70%缁?>70%娴呯豢/鈮?0%鍗遍櫓闂儊锛夛細
  - 妲介亾锛歚.t-bar` 瀵瑰簲 `AREX_DARK` + `LV_OPA_50`銆?
  - 濉厖锛歚鈮?0%` 鈫?`AREX_GREEN`锛沗>70%` 涓?`<90%` 鈫?`AREX_LIGHT`锛坄.t-fill.high`锛夛紱`鈮?0%` 鈫?`.t-fill.danger` 寮忓弽鑹查棯鐑侊紙**300ms** 瀹氭椂鍣紝缁?榛戝垏鎹級銆?
- M 鍊艰櫄绾匡細瀹瑰櫒楂樺害 80px 鐨?**top 20%**锛宍LV_OPA_50`锛涘彸渚?`M-VALUE` 灏忓瓧鏍囩銆?

### 9.3 card_gas.c锛?F锛?

> **銆愪笌 搂5.2 涓€鑷淬€?* 褰撳墠娣卞害瓒呰繃鏌愯 MOD銆佽姘斾綋涓嶅彲鐢ㄦ椂锛岀‘璁ゅ垏鎹㈡棤鏁堬紝**椤荤敤杩斿洖閿€€鍑?CONFIG GAS**锛堣 搂5.2 閲嶇偣璇存槑锛夈€?

- 瑙勮寖锛氳楂?**49px**锛岄棿璺?8锛宲adding 涓婁笅 **12px**锛屽乏鍙?**15px**锛涘瓧浣?`AREX_FONT_TITLE`(20px)銆?
- 涓?HTML `.menu-list` / `.menu-item` / `.static-active` / `.active` / `.hint-text` / `#gas-card-status` 瀵归綈锛?
  - 姘斾綋鍚嶏細`AREX_GREEN` 宸﹀榻愶紱MOD/PO2锛歚AREX_LIGHT`锛屽彸涓?鍙充笅瀵归綈銆?
  - 鍏夋爣琛岋紙`UI_EDIT_GAS && gas_cursor==i`锛夛細`.active` 鈥?缁垮簳銆侀粦瀛椼€佺豢杈规銆?
  - 褰撳墠鍛煎惛姘旓紙`active_idx==i`锛岄潪鍏夋爣锛夛細`.static-active` 鈥?**榛戝簳**銆佺豢杈规銆佹皵浣撳悕 `AREX_GREEN`銆?
  - 鏅€氳锛氶粦搴曘€佺豢瀛椼€乣AREX_DARK` 杈规銆?
  - `#gas-card-status`锛氬彸涓婅 `[EDIT MODE]`锛坄AREX_FONT_SMALL`锛宍AREX_GREEN`锛夈€?
  - `.hint-text`锛氬簳閮ㄦ彁绀猴紝`AREX_LIGHT` + `LV_OPA_60`锛涚紪杈?绌洪棽鏂囨涓?HTML 涓€鑷淬€?
- **瓒?MOD**锛坄depth > MOD`锛夛細绾㈣竟妗嗭紱鑻ヨ琛屽悓鏃朵负缂栬緫鍏夋爣锛岃竟妗嗕繚鎸佺豢鑹层€?
- PPO2 绠€鍖栬绠楋細`depth/10 * 0.21`锛屾枃妗?`PO2 %.2f`銆?

### 9.4 card_plan.c锛?F锛?

- 瑙勮寖鍙傛暟锛堝凡瀵归綈锛夛細
  - 澶栧３锛?px 瀹炵嚎 `AREX_DARK`锛孭adding 10px
  - 鐢诲竷锛?*400脳320px**锛坄CHART_W`/`CHART_H`锛?
  - 鑳屾櫙缃戞牸锛氭í闂磋窛鎸?CHART_W/53px 姝ヨ繘锛岀旱闂磋窛鎸?CHART_H/64px 姝ヨ繘锛涚嚎瀹?1px锛宍LV_OPA_51`锛堥€忔槑搴?20%锛?
  - 鍧愭爣杞存枃瀛楋細`AREX_FONT_SMALL`(14px)锛宍LV_OPA_191`锛堥€忔槑搴?75%锛?
  - 璧板娍绾匡細瀹炵嚎锛岀矖缁?**4px**锛宍AREX_GREEN`
  - 鍋滅暀鐐癸細鍗婂緞 **6px**锛屽～鍏呴粦锛屾弿杈?2px `AREX_GREEN`锛堜粎姘村钩娈典笖鍋滅暀鈮?min 鏃剁粯鍒讹級
- 褰撳墠浣嶇疆锛氶粍鑹插渾鐐癸紝鍔ㄦ€佺敱 `g_sensor_data.dive_time_s` 鍜?`g_sensor_data.depth` 璁＄畻锛屽甫 "NOW" 鏍囩

---

## 10. 棰滆壊甯搁噺涓庡瓧浣?

```c
/* 棰滆壊 */
AREX_GREEN  = #00FF00   // 涓昏壊锛堟枃瀛椼€佹寚閽堛€佹縺娲绘€侊級
AREX_LIGHT  = #55FF55   // 鍓壊锛堟爣绛俱€佽緟鍔╂枃瀛椼€佹瑕佹暟鍊硷級
AREX_DARK   = #003300   // 杈规銆佸埢搴︾嚎銆侀潪婵€娲昏儗鏅?
AREX_BLACK  = #000000   // 鍗＄墖鑳屾櫙
AREX_BG     = #050505   // 灞忓箷鏍硅儗鏅?
```

> **瀛椾綋绯荤粺宸插叏闈㈤噸鏋勶紝璇峰弬瑙?Section 17 瀛椾綋 ID 鏄犲皠寮曟搸銆?*
> 鏃х増 `AREX_FONT_*` 瀹忓畾涔変粎鍦?`arex_screen.h` 涓繚鐣欏吋瀹瑰眰锛?*绂佹鍦ㄦ柊浠ｇ爜涓娇鐢?*銆?

## 10.1 寮圭獥鍙傛暟锛堣鑼冨€硷級

```c
modal_overlay:  bg #000000, opacity 95% (LVGL opa=242)
modal_box:      400脳? px, bg #000000, border 4px #00FF00, padding 30px
```

## 10.2 婊氬姩鎸囩ず鍣紙瑙勮寖鍊硷級

```c
浣嶇疆: 鍙充晶 8px锛屽瀭鐩村眳涓紙LV_ALIGN_RIGHT_MID锛?
澶у皬: 6脳6px锛坆order-radius:0 鈫?姝ｆ柟褰級
闂磋窛: 绾靛悜 gap 8px
榛樿: #003300 (AREX_DARK)
婵€娲? #00FF00 (AREX_GREEN) + shadow_width=8, shadow_color=#00FF00
```

## 10.3 宸︿晶闈㈡澘 PO2锛堣鑼冨€硷級

```c
PO2 鏍囩: AREX_FONT_SMALL(14px), AREX_LIGHT
涓変釜鍊兼 + 涓や釜 | 鍒嗛殧绗︼紝x=30/66/102, 闂磋窛鈮?8px
| 鍒嗛殧绗? 閫忔槑搴?30% (LV_OPA_30)
DEPTH 澶ф暟瀛? AREX_FONT_HUGE(48px), AREX_GREEN, 瀛楅棿璺?-2px, y=24
DEPTH 鏍囩:  AREX_FONT_SMALL(14px), AREX_LIGHT, y=10
```

---

## 11. HTML 鍘熷瀷 鈫?LVGL 瀵瑰簲鍏崇郴

| HTML 鍏冪礌 | LVGL 瀹炵幇 |
|-----------|-----------|
| `#left-anchor` | `s_left_panel`锛?80px 缁濆瀹氫綅瀹瑰櫒锛?|
| `#elevator-track`锛坱ranslateY 鍔ㄧ敾锛?| `s_tileview`锛坄lv_obj_set_tile` 甯﹀姩鐢伙級 |
| `.card`锛?涓?div锛?| 6涓?tileview tile + card_*_create |
| `#top-wall-ui / #bottom-wall-ui` | `s_wall_top / s_wall_bottom`锛圚IDDENFlag 鍒囨崲锛?|
| `#canvas-modal` | `s_modal + s_modal_box` |
| `#sub-menu-layer`锛坱ranslateX 鍔ㄧ敾锛墊 `s_submenu_layer`锛坙v_anim 姘村钩婊戝叆/婊戝嚭锛?|
| `#scroll-indicator` | `s_scroll_dots[6]`锛堟縺娲绘椂 shadow_width=8, shadow_color=AREX_GREEN锛?|
| `.menu-list`/`.menu-item`/`.static-active`/`.active` | `card_gas.c`锛堣 428脳49銆侀棿璺?銆乸adding 12/15px銆佷笁鎬侀鑹诧級 |
| JS `gasData[]` | `AREX_GAS_TABLE[]` |
| JS `setInterval 150ms`锛堢綏鐩橈級 | `sim_tick_cb` 1000ms + `card_compass_update` |
| JS `flashInvert`锛坄.t-fill.danger`锛?| `tissue_danger_flash_cb` 300ms锛坈ard_deco.c锛屼粎 `pct鈮?0` 鐨勬潯锛?|
| JS `renderModal('GAS')` MOD 璀﹀憡 | `arex_screen_show_modal_gas()` hint 瀛楃涓插垏鎹?|

---

## 12. 鏁版嵁娴佹€昏

```
main.c (WinMain)
  鈹斺攢 UI_main()
       鈹溾攢 鍒濆鍖栧簭鍒楋紙瑙佺2鑺傦級
       鈹斺攢 lv_timer(sim_tick_cb, 1000ms)
            鈹?
            鈹溾攢 鏇存柊 g_sensor_data锛坔eading++, dive_time_s++, depth娴姩锛?
            鈹溾攢 arex_screen_refresh_left_panel()   [璇?g_sensor_data 鈫?鍐?s_lbl_*]
            鈹斺攢 arex_ui_refresh_all()
                 for i in 0..arex_card_count()-1:
                   card = arex_card_get(i)        // 閫氳繃 g_sys_card_order(i) 鏌?ID
                   if (card->update_cb) card->update_cb()

鐢ㄦ埛杈撳叆锛堥敭鐩?缂栫爜鍣級
  鈹斺攢 key_event_cb() / enc_click_cb()
       鈹斺攢 ui_handle_rotate / click / back
            鈹溾攢 淇敼 g_ui.state / g_ui.dash_card / g_ui.gas_cursor / 鈥?
            鈹溾攢 淇敼 g_sensor_data / g_sys_config / 鈥?
            鈹斺攢 璋冪敤 arex_screen_* 鍑芥暟鏇存柊鎺т欢澶栬
```

---

## 13. 閲嶈璁捐绾﹀畾

1. **鍗＄墖涓嶇洿鎺ュ啓鐘舵€?*锛歝ard_*.c 鍙 `g_sensor_data` 鍜?`g_ui`锛屼笉淇敼瀹冧滑銆傜姸鎬佷慨鏀圭粺涓€鍦?`arex_ui_state.c` 涓繘琛屻€?

2. **screen 灞傛槸鍝戠殑**锛歚arex_screen.c` 鐨勫嚱鏁板彧璐熻矗鎿嶄綔鎺т欢锛屼笟鍔″垽鏂紙濡傛皵浣撴繁搴︽牎楠岋級鍦ㄧ姸鎬佹満閲屽畬鎴愩€?

3. **card_order 闂存帴灞?*锛歵ileview 鐨勭墿鐞嗛『搴忓湪鍒涘缓鏃跺浐瀹氾紝浣嗙敤鎴峰彲浠ラ€氳繃淇敼 `g_sys_config.card_order[]` 鏀瑰彉鍚勫崱鐗囩殑閫昏緫浣嶇疆锛宍arex_card_get(pos)` 閫氳繃 `g_sys_card_order(pos)` 璐熻矗瑙ｅ紩鐢ㄣ€?

4. **娉ㄥ唽鍥炶皟**锛歝ard_info.c 鍜?card_setup.c 閫氳繃 `arex_screen_register_info_list()` / `arex_screen_register_setup_list()` 鎶婂畠浠唴閮ㄥ垱寤虹殑鍒楄〃瀵硅薄鍛婄煡 screen 灞傦紝閬垮厤鍦?arex_screen.c 涓噸澶嶅垱寤烘帶浠躲€?

5. **Wall-charge 闃茶瑙?*锛氳繛缁?娆℃墠绌胯秺杈圭晫锛岄槻姝㈠崟娆℃姈鍔ㄨ瑙﹁繘鍏ヨ彍鍗曘€?

6. **Modal 闇囧姩鍙嶉**锛氭皵浣撳垏鎹㈣秴 MOD 鏃讹紝`arex_screen_pulse_modal()` 鐢?lv_anim 鍋氬乏鍙?卤6px 鎶栧姩锛?娆￠噸澶嶏紝80ms锛夛紝瀵瑰簲 HTML 鐨?`scale(1.05)` 寮硅烦銆?

7. **鏁版嵁鎬荤嚎褰掍竴鍖?*锛歚arex_data.h` 浠呬綔瀛樻牴锛坄#include "arex_ui_engine.h"`锛夛紝鎵€鏈夋暟鎹€荤嚎锛坄g_sys_config`銆乣g_sensor_data`锛夈€佹灇涓俱€佸畯鍧囩粺涓€鍦?`arex_ui_engine.h` 涓畾涔夛紝娑堥櫎璺ㄦ枃浠朵緷璧栥€?

---

## 14. 瀛愯彍鍗曞姩浣滆矾鐢憋紙v0.10 鏂板锛?

`arex_screen_handle_submenu_select()` 鍦?`arex_screen.c` 涓叏闈㈠疄鐜帮紝鎸?`cur_title` 鍒嗘敮璺敱锛?

### 14.1 鍔ㄤ綔璺敱琛?

| 褰撳墠瀛愯彍鍗曟爣棰?| 閫変腑椤硅鍒?| 鎵ц鍔ㄤ綔 |
|---|---|---|
| `GAS SWITCH` | `SELECT XXX` | 鏇存柊 `g_sensor_data.gas_active_idx`锛屽埛鏂?gas 鍗″拰宸﹂潰鏉匡紝鍏抽棴瀛愯彍鍗?|
| `CONSERVATISM` | `LOW/MED/HIGH` 寮€澶?| 鏇存柊 `g_sys_config.conservatism`锛屾洿鏂?SETUP 鑿滃崟 badge锛屽叧闂瓙鑿滃崟 |
| `BRIGHTNESS` | 绮剧‘鍖归厤 `LOW/MED/HIGH/MAX` | 鏇存柊 `g_sys_config.brightness`锛屾洿鏂?SETUP 鑿滃崟 badge锛屽叧闂瓙鑿滃崟 |
| `DIVE SETUP`锛堝祵濂楋級 | `MOD PO2:` 寮€澶?| 璋冪敤 `arex_screen_begin_edit_value()` 鈫?`UI_EDIT_VALUE` |
| `DIVE SETUP`锛堝祵濂楋級 | 鍏朵粬椤?| `arex_screen_show_modal_act(text)` 閫氱敤鍔ㄤ綔寮圭獥 |
| 浠绘剰鏍囬 | 鏈熬鍚?`>` | 瑙ｆ瀽鏍囬锛岃皟鐢?`arex_screen_open_nested_submenu()` 杩涘叆涓嬩竴绾?|
| 浠绘剰鏍囬 | `< BACK` | `arex_screen_close_submenu()` 閫€鍑?鍥炰笂绾?|
| 鍏朵粬鎵€鏈?| 浠绘剰 | `arex_screen_show_modal_act(text)` 閫氱敤鍔ㄤ綔寮圭獥锛?绉掕嚜鍔ㄥ叧闂級 |

### 14.2 宓屽瀛愯彍鍗曪紙涓夌骇锛?

```
SETUP 鈫?SYSTEM SETUP锛堜簩绾э級
           鈹溾攢 MODE SETUP >   鈫?[AIR / NITROX / 3 GAS NX / GAUGE]
           鈹溾攢 DIVE SETUP >   鈫?[SALINITY / MOD PO2 / SAFETY STOP / ALTITUDE]
           鈹?                     鈹斺攢 MOD PO2 鈫?UI_EDIT_VALUE锛堝唴鑱旀暟鍊肩紪杈戯級
           鈹溾攢 AI SETUP >     鈫?[PAIR T1 / PAIR T2 / GTR MODE: ON]
           鈹溾攢 ALERTS SETUP > 鈫?[DEPTH ALARM / TIME ALARM / LOW NDL / TEST VIB]
           鈹斺攢 DISPLAY / SYS >鈫?[UNITS / DATE & CLOCK / LOG RATE / BLUETOOTH / RESET]
```

**瀵艰埅鏍?* (`g_ui.sub_history[]`, 鏈€娣?4 灞?锛?
- 杩涘叆宓屽鏃?`submenu_history_push()` 淇濆瓨褰撳墠鏍囬鍜屽厜鏍囦綅缃?
- `arex_screen_close_submenu()` 妫€娴?`sub_history_depth > 0` 鏃舵墽琛?pop锛岄噸鏂?populate 涓婁竴绾у唴瀹?
- `sub_history_depth == 0` 鏃舵墽琛?`submenu_slide_out()`锛岃繑鍥?`sub_parent` 鐘舵€?

### 14.3 SETUP badge 鏇存柊

`card_setup.c` 姣忎釜鑿滃崟椤规湁涓や釜瀛?label锛?
- child 0锛氭爣棰樻枃瀛楋紙`> GAS SWITCH` 绛夛級
- child 1锛氬彸渚?badge锛坄MED` / `HIGH` / 绌猴級

`arex_screen_update_setup_badge(item_idx, value)` 閫氳繃 `s_setup_list` 鐩存帴鍐?child 1銆? 
`card_setup_update()` 姣?tick 浠?`g_sys_config` 鍚屾 CONSERVATISM / BRIGHTNESS 鐨?badge 鏂囧瓧銆?

### 14.4 INFO 瀛愯彍鍗曞姩鎬佹暟鎹?

`arex_screen_open_info_submenu()` 鍦ㄦ墦寮€鍓嶈皟鐢?`build_info_submenu(idx)` 浠?`g_sensor_data` 鍔ㄦ€佹瀯寤哄瓧绗︿覆锛?

| 瀛愯彍鍗?| 鍔ㄦ€佸瓧娈垫潵婧?|
|--------|-------------|
| LAST DIVE | `g_sensor_data.depth`锛宍g_sensor_data.dive_time_s` |
| TISSUE & TOX | `g_sensor_data.cns_pct`锛宍g_sensor_data.otu` |
| GAS & CALC | `AREX_GAS_TABLE[g_sensor_data.gas_active_idx].name` |
| SENSOR & DEVICE | `g_sensor_data.pod1_bar`锛宍g_sensor_data.pod2_bar` |

### 14.5 DIVE SETUP 宓屽鑿滃崟涓?MOD PO2 瀹炴椂鍊?

`build_nested_dive_setup()` 鍦ㄦ瘡娆℃墦寮€璇ュ瓙鑿滃崟鍓嶈皟鐢紝灏?`g_sys_config.mod_ppo2` 鏍煎紡鍖栬繘 `s_modppo2_str[]`锛屼繚璇佹樉绀烘渶鏂板€笺€傜紪杈戞彁浜ゅ悗 `arex_screen_commit_edit_value()` 鐩存帴鏇存柊鍚屼竴 label銆?

### 14.6 鏂板 arex_screen.h 鍏紑 API

| 鍑芥暟绛惧悕 | 浣滅敤 |
|----------|------|
| `arex_screen_open_nested_submenu(title, items, count)` | 鎶婂綋鍓嶇姸鎬佸帇鏍堬紝鍘熷湴鏇挎崲瀛愯彍鍗曞唴瀹癸紙鏃犳粦鍔ㄥ姩鐢伙級 |
| `arex_screen_update_setup_badge(item_idx, value)` | 鏇存柊 SETUP 鑿滃崟琛岀殑鍙充晶 badge label |
| `arex_screen_show_modal_act(action_text)` | 鏄剧ず閫氱敤鍔ㄤ綔寮圭獥锛?绉掑悗鑷姩鍏抽棴锛岀姸鎬佸洖 `UI_SUB_MENU` |
| `arex_screen_begin_edit_value(item_idx, value, min, max, step)` | 鍒濆鍖?`edit_ctx`锛岃繘鍏?`UI_EDIT_VALUE` 鐘舵€侊紱UI锛氳榛戝簳+缁胯竟妗嗭紱鏁磋 flex `space-between`锛堝榻?HTML 绗?137 琛岋級锛涙暟鍊?`X.X` 鍦ㄧ豢搴?badge 鍐呭眳涓紱绠ご `^ v`锛沗s_edit_flash_timer` 600ms 鍒囨崲 badge 鑳屾櫙锛堢豢鈫旈粦锛変笌鏁板€兼枃瀛楅鑹诧紙榛戔啍缁匡級 |
## 15. LVGL 缁濆鍧愭爣鎺掔増鏍囧噯锛坴2026-04-22锛?

> 璇︾粏瑙勮寖瑙?`UI_html_DOC/LVGL_LAYOUT_GUIDE.md`銆傛湰鑺備负蹇€熺储寮曘€?

### 15.1 鐗╃悊鍙傛暟涓庡畨鍏ㄥ尯

| 鍙傛暟 | 鍊?| 璇存槑 |
|------|-----|------|
| `AREX_BASE_U` | 10px | 1U = 10px锛屾墍鏈夊昂瀵稿繀椤绘槸 10 鐨勫€嶆暟 |
| `AREX_PHYSICAL_W/H` | 640x480 | 鐗╃悊灞忓箷閿佹锛屼笉鍙€捐秺 |
| `SAFE_ZONE` | 580x400 | 瀹夊叏鐢诲竷锛宱ffset_x/y 椹卞姩鐗╃悊浣嶇Щ |
| `AREX_LEFT_ANCHOR_W` | 160px | 宸︿晶閿氱偣鍥哄畾瀹藉害 |
| `GLOBAL_GAP` | 20px | 宸?鍙冲垎鍖洪殧绂婚棿璺?|

### 15.2 Tech 妯″紡甯冨眬鎺ㄧ畻

```
uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;  // 20px
uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;  // 400px
```

### 15.3 娓叉煋閾佸緥锛堣嚜鏌ユ竻鍗曪級

| # | 瑙勫垯 | 閿欒浠ｇ爜 |
|---|------|----------|
| 1 | `pad_all = 0` 鎵€鏈夊鍣ㄩ浂杈硅窛 | `pad_all = 8` |
| 2 | `border_width = 0` 鎵€鏈夊鍣ㄩ浂杈规 | `border_width = 2` |
| 3 | 绂佹 Flex 甯冨眬 | `lv_obj_set_layout(obj, LV_LAYOUT_FLEX)` |
| 4 | label 灏哄閿佹 | `lv_obj_set_size(lbl, LV_SIZE_CONTENT, ...)` |
| 5 | `LV_LABEL_LONG_DOT` 闃叉埅鏂?| 鏃?`long_mode` |
| 6 | 鑿滃崟椤?x=0 涓?INFO MENU 瀵归綈 | `x=16`锛堟孩鍑猴級 |
| 7 | `clip_corner=true` 闃叉瀛愬厓绱犳孩鍑?| 鏃犺鍓?|

### 15.4 姘斾綋閫夐」瀹藉害淇璁板綍

**闂**锛歚card_gas.c` 姘斾綋閫夐」瀹藉害婧㈠嚭锛屽彸杈圭紭瓒呭嚭 tile 杈圭晫銆?

**鏍瑰洜**锛氳 x=16 璧峰锛宍row_w = right_canvas_w - 15`锛屽鑷村彸杈圭紭鍒拌揪 `16 + (right_canvas_w - 15) = right_canvas_w + 1`锛岃秴鍑?tile銆?

**淇**锛氬皢 `lv_obj_set_pos(row, 16, row_y)` 鏀逛负 `lv_obj_set_pos(row, 0, row_y)`锛屼笌 `card_info.c` 鐨?x=0 瀵归綈銆?

```
// 淇鍓?(card_gas.c:32)
lv_obj_set_pos(row, 16, row_y);  // 婧㈠嚭

// 淇鍚?
lv_obj_set_pos(row, 0, row_y);   // 涓?INFO MENU 瀵归綈
```

### 15.5 鍏抽敭浠ｇ爜鍙樻洿鏃ュ織

| 鏃ユ湡 | 鏂囦欢 | 鍙樻洿 |
|------|------|------|
| 2026-04-22 | `card_gas.c` | 姘斾綋閫夐」 x=16 鍒?x=0锛屼笌 INFO MENU 瀵归綈 |
| 2026-04-22 | `UI_html_DOC/LVGL_LAYOUT_GUIDE.md` | 鏂板缓鎺掔増钀藉湴鎸囧崡 |
| 2026-04-23 | `arex_ui_engine.h` | 鍒犻櫎 `left_order[]` 鍜?`left_mod_*[]`锛屾浛鎹负 `arex_left_row_cfg_t left_layout[8]` 琛岄厤缃粨鏋勪綋 |
| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `AREX_MODULE_EMPTY=0`锛堟浛鎹㈡棫 NONE锛夛紝鏂板 `AREX_MAX_LEFT_ROWS=8`銆乣AREX_ROW_MAX_SLOTS=2`銆乣ANCHOR_COMP_COUNT=16` |
| 2026-04-23 | `arex_ui_engine.c` | `arex_calc_anchor_layout()` 瀹屽叏閲嶅啓锛氶亶鍘?`left_layout[]` 鑰岄潪 `left_order[]`锛屽崟鏍忕嫭鍗犲叏瀹?160px)锛屽弻鎷煎悇鍗婂(80px)锛岄浂鍙屾嫾纭紪鐮?|
| 2026-04-23 | `arex_ui_engine.c` | `arex_sys_config_defaults()` 濉厖 `left_layout[]` 榛樿琛岄厤缃紙DEPTH/GA + NDL+TTS + POD1+POD2 + BATT+WTM + GAS + TIME锛?|
| 2026-04-23 | `arex_screen.c` | `left_anchor_create()` 寰幆鏀逛负 `for (i < comp_count)`锛屾爣棰樺拰鏁板€兼枃瀛楁敼涓?`switch (c->module)` 鏋氫妇椹卞姩 |
| 2026-04-23 | `arex_screen.c` | `left_anchor_rebuild()` 鍚屾鏇存柊涓虹┖妯″潡妫€鏌ュ拰瀵归綈澶勭悊 |
| 2026-04-23 | `AREX_ARCH.md` | Section 16 鍏ㄩ潰鍗囩骇锛氭柊澧?16.3 `arex_left_row_cfg_t` 缁撴瀯浣撱€?6.4 榛樿琛屽竷灞€銆?6.9 娓叉煋娴佺▼鍥?|
| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `arex_font_id_t` 鏋氫妇瀛楀吀銆乣arex_get_font()` 澹版槑锛涘垹闄ゅ簾寮冪殑 `align_title/huge/med` 瀛楁 |
| 2026-04-23 | `arex_ui_engine.c` | 瀹炵幇 `arex_get_font()` 瀛椾綋鏄犲皠鍣紱鍒犻櫎搴熷純鐨?`font_sz_*` 榛樿鍊硷紱鏇存柊 `def_layout[]` 浣跨敤 `AREX_FONT_ID_*` 鏋氫妇 |
| 2026-04-23 | `arex_screen.c` | 鍏ㄩ儴 `AREX_FONT_*` 瀹忔浛鎹负 `arex_get_font(id)`锛涘垹闄?`font_cat[]` 涓棿灞傛暟缁勶紙涓ゅ锛夛紱`left_anchor_rebuild()` 澧炲姞 `title_font`/`title_align` 濉厖 |
| 2026-04-23 | `cards/*.c` | 鍏ㄩ儴 `AREX_FONT_*` 瀹忔浛鎹负 `arex_get_font(id)`锛坈ompass/setup/gas/deco/info/plan锛?|
| 2026-04-23 | `arex_screen.h` | 鏃?`AREX_FONT_*` 瀹忔爣璁颁负搴熷純锛岄檮姝ｇ‘鐢ㄦ硶娉ㄩ噴 |
| 2026-04-23 | `AREX_ARCH.md` | 鏂板 Section 17 瀛椾綋鏄犲皠寮曟搸鏂囨。锛涙洿鏂?Section 10/16 寮曠敤 |
| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `arex_menu_item_cfg_t` 鍙?`arex_render_dynamic_menu()` 澹版槑 |
| 2026-04-23 | `arex_ui_engine.c` | 瀹炵幇宸ュ巶鍑芥暟锛宧eight/gap 鍏ㄧ▼浠?`g_sys_config` 鎺ㄧ畻 |
| 2026-04-23 | `card_info.c` | 瀹屽叏閲嶆瀯锛氶厤缃暟缁?+ 1 琛屽伐鍘傝皟鐢?|
| 2026-04-23 | `card_setup.c` | 瀹屽叏閲嶆瀯锛歜adge 鍒锋柊閫昏緫淇濈暀锛屽彞鏌勬暟缁勬敼涓哄伐鍘傝緭鍑?|
| 2026-04-23 | `card_gas.c` | `GAS_ROW_GAP` 瀹忓垹闄わ紝`gap_y` 鏀逛负閰嶇疆鎺ㄧ畻 |
| 2026-04-23 | `AREX_ARCH.md` | 鏂板 Section 19/20 鍙充晶鍗＄墖鍔ㄦ€佽彍鍗曞紩鎿庢枃妗?|
| 2026-04-23 | `arex_card_registry.c` | 閲嶅啓锛氭寚瀹氬垵濮嬪寲鍣ㄣ€乣tile_obj=NULL` 鍒濆鍖栥€乣g_sys_card_order()` 闂存帴鏌ヨ銆乣arex_card_count()` API |
| 2026-04-23 | `arex_card_registry.h` | 鏂板 `AREX_CARD_COUNT`銆乣AREX_DASH_CARD_COUNT`锛沗arex_card_reg_t` 鏂板 `on_enter_cb` |
| 2026-04-23 | `arex_ui_engine.c` | 鏂板 `g_sys_card_order(pos)` 鍑芥暟灏佽锛沗arex_sys_config_defaults()` 濉厖榛樿 `card_order[]` |
| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `g_sys_card_order()` 澹版槑锛涙柊澧?`arex_ui_update_data()` 绌洪挬瀛?|
| 2026-04-23 | `arex_ui_state.c` | `arex_ui_refresh_all()` 鏀逛负 `arex_card_count()` 寰幆锛沗AREX_CARD_COUNT - 2` 鏇挎崲涓?`AREX_DASH_CARD_COUNT` |
| 2026-04-23 | `arex_data.h/c` | 鏂板缓鏁版嵁鎬荤嚎澶存枃浠跺瓨鏍癸紙`#include "arex_ui_engine.h"`锛夛紝鎵€鏈夊畾涔変繚鐣欏湪 engine |
| 2026-04-23 | `card_info.c` | 鏀逛负 `arex_get_font()` + `arex_data.h`锛涚┖ update 鍥炶皟 |
| 2026-04-23 | `card_setup.c` | 鏀逛负 `arex_get_font()` + dirty check badge 鏇存柊锛沚adge 瀛?label 绱㈠紩淇 |
| 2026-04-23 | `UI_main.c` | 绉婚櫎 `lv_timer_create`锛堝凡绉昏嚦 `arex_screen_create`锛夛紱鍚姩鐩存帴杩涘叆 INFO 鍗?|
| 2026-04-23 | `AREX_ARCH.md` | 鏂板 Section 18 閲嶆瀯鍙樻洿鏃ュ織锛涙洿鏂?Section 1/3/4/7/12/13 |
|| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `AREX_DEBUG_BORDER` 瀹?0=鍏抽棴/1=寮€鍚?锛岀粺涓€鎺у埗 title_zone/val_zone 璋冭瘯杈规锛沗RENDER_FIXES.md` Section 7 鍚屾鏇存柊 |

---

## 16. APP 鍚屾灏辩华锛氬叏鍔ㄦ€佸竷灞€寮曟搸锛坴2026-04-23锛?

### 16.1 涓夊ぇ鏍稿績寮曟搸鐘舵€?

| 寮曟搸 | 鐘舵€?| 璇存槑 |
|------|------|------|
| 宸︿晶閿氱偣鑷敱鍙屾嫾 | 鉁?宸插疄鐜?| `left_layout[]` 琛岄厤缃紝浠绘剰涓ゆā鍧楀彲鍙屾嫾鎴栫嫭鍗犲叏瀹?|
| 鍙充晶鍗＄墖椤哄簭 | 鉁?宸插疄鐜?| `card_order[]` + `arex_card_get()` 鍙屽皠鏄犲皠 |
| U 鍗曚綅闆剁‖缂栫爜 | 鉁?宸插疄鐜?| 鎵€鏈夊潗鏍囧熀浜?`脳 AREX_BASE_U`锛屾棤娈嬬暀鍍忕礌甯告暟 |

### 16.2 宸︿晶閿氱偣妯″潡鏋氫妇

```c
typedef enum {
    AREX_MODULE_EMPTY  = 0,  /* 绌烘Ы浣嶏細涓嶆覆鏌撲换浣曟ā鍧?*/
    AREX_MODULE_DEPTH  = 1,  /* DEPTH 澶ф暟瀛楋紙鐙珛涓€琛岋紝鍏ㄥ锛?*/
    AREX_MODULE_NDL    = 2,  /* NDL 鍏嶅噺鍘嬫椂闂?*/
    AREX_MODULE_TTS    = 3,  /* TTS 鍥炲埌姘撮潰鏃堕棿 */
    AREX_MODULE_POD1  = 4,  /* POD1 姘旂摱1鍘嬪姏 */
    AREX_MODULE_POD2  = 5,  /* POD2 姘旂摱2鍘嬪姏 */
    AREX_MODULE_BATT  = 6,  /* BATT 鐢垫睜 */
    AREX_MODULE_WTM   = 7,  /* W.TIME 娼滄按鎬绘椂闂?*/
    AREX_MODULE_GAS   = 8,  /* GAS 褰撳墠姘斾綋 */
    AREX_MODULE_TIME  = 9,  /* TIME 鐙珛璁℃椂 */
} arex_left_module_t;
```

### 16.3 宸︿晶琛岄厤缃粨鏋勪綋锛圓PP 鍚屾鏍稿績锛?

```c
#define AREX_MAX_LEFT_ROWS  8   /* 鏈€澶ц鏁?*/
#define AREX_ROW_MAX_SLOTS  2   /* 姣忚鏈€澶?2 涓ā鍧楁Ы */

typedef struct {
    uint8_t left_module;   /* 宸︿晶妯″潡鏋氫妇 (AREX_MODULE_*) */
    uint8_t right_module;  /* 鍙充晶妯″潡鏋氫妇 (AREX_MODULE_EMPTY=鐙崰鍏ㄥ) */
    uint8_t h_u;           /* 璇ヨ鎬婚珮搴︼紙鍗曚綅 U锛岄粯璁?0=鏌ユā鍧楅粯璁ゅ€硷級 */
    uint8_t title_h_u;     /* 鏍囬鍖洪珮搴︼紙榛樿 0=鐢ㄥ叏灞€ title_h_u锛?*/
    uint8_t title_font;    /* 鏍囬瀛楀彿: arex_font_id_t (0~3) */
    uint8_t val_font;      /* 鏁板€煎瓧鍙? arex_font_id_t (0~3) */
    uint8_t val_align;     /* 鏁板€煎榻? 0=LEFT 1=CENTER 2=RIGHT */
    uint8_t sep_style;     /* 鍒嗗壊绾挎牱寮? 0=NONE 1=SOLID 2=DASHED 3=DOTTED */
    uint8_t sep_thick;     /* 鍒嗗壊绾跨矖缁?px锛?=鐢ㄥ叏灞€ sep_thick锛?*/
} arex_left_row_cfg_t;

// g_sys_config.left_layout[] 鈥?APP 瀹為檯涓嬪彂姝ゆ暟缁?
arex_left_row_cfg_t left_layout[AREX_MAX_LEFT_ROWS];
```

### 16.4 榛樿琛屽竷灞€锛堝垵濮嬪€硷級

> **瀛楀彿 arex_font_id_t**: `0=SMALL(14px)` `1=TITLE(20px)` `2=MEDIUM(28px)` `3=HUGE(48px)`

```c
/* row 0: DEPTH 鍗曟爮鍏ㄥ */
{ AREX_MODULE_DEPTH, AREX_MODULE_EMPTY, 8, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 1: NDL + TTS 鍙屾嫾 */
{ AREX_MODULE_NDL,  AREX_MODULE_TTS,  6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 2: POD1 + POD2 鍙屾嫾 */
{ AREX_MODULE_POD1, AREX_MODULE_POD2, 6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 3: BATT + WTM 鍙屾嫾 */
{ AREX_MODULE_BATT, AREX_MODULE_WTM,  5, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 4: GAS 鍗曟爮鍏ㄥ */
{ AREX_MODULE_GAS,  AREX_MODULE_EMPTY, 6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 5: TIME 鍗曟爮鍏ㄥ */
{ AREX_MODULE_TIME, AREX_MODULE_EMPTY, 5, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 6-7: EMPTY */
{ AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0,
  0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE, 0 },
{ AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0,
  0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE, 0 },
```

> **銆愰浂鍙烽搧寰嬨€慙VGL 8.3 鍒嗗壊绾垮疄鐜拌鑼?*
>
> LVGL 8.3 鐨勫師鐢?`border`锛堣竟妗嗭級灞炴€?*涓嶆敮鎸佽櫄绾?*锛佺粷瀵圭姝娇鐢?`lv_obj_set_style_border_*` 鏉ョ敾鍒嗗壊绾裤€?
>
> 鍒嗗壊绾挎覆鏌撹鍒欙紙`arex_screen.c` 宸ュ巶鍑芥暟锛夛細
>
> | `sep_style` | 娓叉煋瀵硅薄 | 瀹炵幇鏂瑰紡 |
> |---|---|---|
> | `AREX_SEP_NONE` (0) | 鏃?| 涓嶅垱寤轰换浣曞璞?|
> | `AREX_SEP_SOLID` (1) | `lv_obj` 瀹炵嚎鑹插潡 | 绾壊鐭╁舰锛岃儗鏅壊 + bg_opa |
> | `AREX_SEP_DASHED` (2) | `lv_line` + 鍘熺敓铏氱嚎寮曟搸 | `line_dash_width=6` + `line_dash_gap=4` |
> | `AREX_SEP_DOTTED` (3) | `lv_line` + 鍘熺敓鐐圭嚎寮曟搸 | `line_dash_width=thick` + `line_dash_gap=thick*2` |
>
> **鍐呭瓨绠＄悊閾佸緥**锛歚lv_line_set_points()` 浼犲叆鐨?`lv_point_t[]` 鎸囬拡鐢辫皟鐢ㄦ柟鎵樼锛孡VGL 涓嶄細鑷姩閲婃斁銆傛瘡娆￠噸寤哄垎鍓茬嚎鏃讹細
> 1. 鍏堥€氳繃 `lv_obj_get_user_data()` 鍙栧嚭鏃?pts锛岃皟鐢?`lv_mem_free()` 閲婃斁
> 2. `lv_line_create()` 鏃堕€氳繃 `lv_obj_add_event_cb(sep, line_delete_cb, LV_EVENT_DELETE, pts)` 灏?pts 缁戝畾鍒颁簨浠跺洖璋?
> 3. `line_delete_cb` 鍦ㄥ璞￠攢姣佹椂鑷姩閲婃斁 pts锛岄槻姝㈠唴瀛樻硠婕?
>
> **鍏抽敭鏁版嵁缁撴瀯**锛歚arex_anchor_comp_t` 鏂板 `sep_style`/`sep_thick` 瀛楁锛岀敱 `arex_calc_anchor_layout()` 浠?`left_layout[]` 濉厖銆?

**APP 鑷敱鍙屾嫾绀轰緥**锛氬皢 BATT 鍜?GAS 鎷煎湪鍚屼竴琛岋細
```json
{
  "left_layout": [
    { "left_module": 1, "right_module": 0, "h_u": 8, ... },  // DEPTH
    { "left_module": 2, "right_module": 3, "h_u": 6, ... },  // NDL+TTS
    { "left_module": 6, "right_module": 8, "h_u": 5, ... },  // BATT+GAS 鈫?鑷敱鍙屾嫾锛?
    ...
  ]
}
```
鍗曠墖鏈烘敹鍒板悗璋冪敤 `arex_ui_apply_config()`锛屾暣涓乏渚ч潰鏉胯嚜鍔ㄩ噸鎺掞紝鏃犻渶鏀逛换浣?C 浠ｇ爜銆?

### 16.5 甯冨眬寮曟搸鍑芥暟涓€瑙?

| 鍑芥暟 | 鏂囦欢 | 浣滅敤 |
|------|------|------|
| `arex_calc_anchor_layout()` | arex_ui_engine.c | 閬嶅巻 `left_layout[]`锛屽～ `arex_anchor_comp_t[]`锛堝崟鏍?鍏ュ彛锛屽弻鎷?鍏ュ彛锛夛紝杩斿洖瀹為檯 count |
| `arex_calc_tech_layout()` | arex_ui_engine.c | Tech 妯″紡宸﹀彸鍒嗗尯鍧愭爣锛氬乏閿氱偣鍥哄畾 160px锛屽彸鍖哄煙 `= safe_zone_w - 160 - gap` |
| `arex_calc_classic_layout()` | arex_ui_engine.c | Classic 妯″紡涓婁笅鍒嗗尯锛屾渶灏忛珮搴?`AREX_MIN_CLASSIC_TOP_H=200px` |
| `arex_calc_widget_cell()` | arex_ui_engine.c | 5x6 缃戞牸鍗曞厓鏍煎潗鏍囷紝`unit_w = parent_w/5`锛宍unit_h = parent_h/6` |
| `arex_calc_tissue_bars()` | arex_ui_engine.c | 16 鏌辩粍缁囧浘 X 鍧愭爣锛宍col_w = total_w/16` |
| `left_anchor_create()` | arex_screen.c | 棣栨鍒涘缓锛屾寜 `c->module` 鏋氫妇椹卞姩锛屾棤绱㈠紩纭紪鐮?|
| `left_anchor_rebuild()` | arex_screen.c | 閰嶇疆鍙樻洿鍚庨噸寤猴紝鏁版嵁椹卞姩瀛椾綋/瀵归綈鍒锋柊 |
| `right_panel_create()` | arex_screen.c | 鍒涘缓 tileview锛屾寜 `card_order[]` 椤哄簭鎸傝浇鍗＄墖 |

### 16.6 鍏抽敭鍛藉悕甯搁噺锛堥槻姝㈢‖缂栫爜锛?

| 甯搁噺 | 鍊?| 鐢ㄩ€?|
|------|-----|------|
| `AREX_BASE_U` | 10px | 鎵€鏈?U 鍗曚綅涔樻暟 |
| `AREX_MIN_CLASSIC_TOP_H` | 200px | Classic 妯″紡鏈€灏忎笂鍖洪珮搴?|
| `AREX_MASK_EDGE_GUARD` | 80px | 闈㈤暅鐩插尯鎺╄啘搴曢儴璀︽垝闃堝€?|
| `AREX_LEFT_ANCHOR_W` | 160px | 宸︿晶閿氱偣鍥哄畾瀹藉害 |
| `ANCHOR_COMP_COUNT` | 16 | 宸︿晶缁勪欢鏈€澶у彞鏌勬暟锛堝竷灞€杈撳嚭缂撳啿锛?|
| `AREX_MAX_LEFT_ROWS` | 8 | 宸︿晶鏈€澶ц閰嶇疆鏁?|

### 16.7 right_w Fallback 鍏紡

鎵€鏈夊彸渚у搴?fallback 浣跨敤浠ヤ笅鍏紡锛屼笉鍐嶄娇鐢ㄧ‖缂栫爜 `420`锛?

```c
uint16_t right_w_fallback = g_sys_config.safe_zone_w
                          - AREX_LEFT_ANCHOR_W          // 160px
                          - g_sys_config.gap_u * AREX_BASE_U;  // gap
// 渚? 580 - 160 - 10 = 410px
```

### 16.8 APP 瀹屾暣鍚屾鍗忚

1. **APP 涓嬪彂** JSON 閰嶇疆锛堜粎鍖呭惈 `g_sys_config` 瀛楁瀛愰泦锛?
2. **鍗曠墖鏈鸿В鏋?* 鈫?瑕嗙洊 `g_sys_config` 瀵瑰簲瀛楁
3. **璋冪敤** `arex_ui_apply_config()` 鈫?`left_anchor_rebuild()` + `arex_screen_rebuild_tileview()`
4. **缁撴灉**锛氭暣涓?UI 鎸夋柊閰嶇疆閲嶆帓锛屾棤闇€閲嶅惎锛屾棤闇€鏀?C 浠ｇ爜

### 16.9 娓叉煋娴佺▼锛堣嚜鐢卞弻鎷肩増锛?

```
arex_calc_anchor_layout()
  for each row in left_layout[]:
    left_mod  = left_layout[row].left_module
    right_mod = left_layout[row].right_module
    if left_mod == EMPTY: continue
    if right_mod == EMPTY:
      鈫?鍗曟爮: 濉厖 comps[out_idx++] (w=160px, split=0)
    else:
      鈫?鍙屾嫾: 濉厖 comps[out_idx++] (w=80px, split=1)  // 宸﹀潡
              濉厖 comps[out_idx++] (w=80px, split=2)  // 鍙冲潡
    cur_y += h_px + gap

left_anchor_create() / left_anchor_rebuild()
  for i in 0..count-1:
    c = comps[i]
    switch (c->module):
      case DEPTH:  render "DEPTH", "45.2" ...
      case NDL:    render "NDL", "5" ...
      case TTS:    render "TTS", "24'" ...
      ...
      // 闆跺弻鎷肩‖缂栫爜锛氫换鎰忔ā鍧楀潎鍙嚭鐜板湪浠绘剰琛岋紒
```

---

## 17. 瀛椾綋绯荤粺锛欼D 鏄犲皠寮曟搸锛坴2026-04-23锛?

### 17.1 鏍稿績閾佸緥

> **闆跺彿閾佸緥**锛氭墍鏈夐厤缃粨鏋勪綋锛坄arex_left_row_cfg_t`銆乣arex_anchor_comp_t` 绛夛級涓彧鍏佽淇濆瓨瀛椾綋 ID锛堟灇涓惧€硷級锛岀姝繚瀛?`lv_font_t*` 鎸囬拡锛丄PP 鍙兘涓嬪彂鏁板瓧 ID锛屾覆鏌撳紩鎿庨€氳繃 `arex_get_font(id)` 缁熶竴鏄犲皠銆?

### 17.2 瀛椾綋 ID 鏋氫妇瀛楀吀

```c
typedef enum {
    AREX_FONT_ID_SMALL  = 0,  /* 14px  鏍囩/鍗曚綅/Badge */
    AREX_FONT_ID_TITLE,       /* 20px  鑿滃崟椤?鍗＄墖鏍囬 */
    AREX_FONT_ID_MEDIUM,      /* 28px  鏁版嵁鍊?*/
    AREX_FONT_ID_HUGE,        /* 48px  娣卞害澶ф暟瀛?*/
} arex_font_id_t;
```

### 17.3 瀛椾綋鏄犲皠琛?

| ID 鏋氫妇 | 鍍忕礌 | 鐢ㄩ€?|
|---------|------|------|
| `AREX_FONT_ID_SMALL`  (0) | 14px | 鏍囩/鍗曚綅/Status Badge |
| `AREX_FONT_ID_TITLE`  (1) | 20px | 鑿滃崟椤?鍗＄墖鏍囬 |
| `AREX_FONT_ID_MEDIUM` (2) | 28px | 鏁版嵁鍊?|
| `AREX_FONT_ID_HUGE`   (3) | 48px | 娣卞害澶ф暟瀛楋紙瑙勮寖 58px 鏈€杩戯級 |

### 17.4 `arex_get_font()` 鏄犲皠鍣?

瀛椾綋鏄犲皠鍣ㄦ槸鍏ㄧ郴缁熶腑**鍞竴**鍏佽灏嗗瓧浣?ID 杞崲涓虹湡瀹?`lvgl` 瀛椾綋鎸囬拡鐨勫湴鏂癸紝浣嶄簬 `arex_ui_engine.c`锛?

```c
/* 澹版槑瀛椾綋璧勬簮锛堢敱 arex_fonts.h 涓殑 LV_FONT_DECLARE 鎻愪緵锛?*/
#include "fonts/arex_fonts.h"

const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id) {
        case AREX_FONT_ID_SMALL:  return AREX_FONT_SMALL;   /* 14px */
        case AREX_FONT_ID_TITLE:  return AREX_FONT_TITLE;   /* 20px */
        case AREX_FONT_ID_MEDIUM: return AREX_FONT_MEDIUM;  /* 28px */
        case AREX_FONT_ID_HUGE:   return AREX_FONT_HUGE;   /* 48px */
        default:                   return AREX_FONT_SMALL;   /* 姘镐笉涓?NULL */
    }
}
```

### 17.5 姝ｇ‘鐢ㄦ硶 vs 閿欒鐢ㄦ硶

```c
/* 姝ｇ‘锛氫紶 ID锛岃繍琛屾椂鏄犲皠 */
lv_obj_set_style_text_font(obj, arex_get_font(AREX_FONT_ID_HUGE), 0);
lv_obj_set_style_text_font(obj, arex_get_font(row->val_font), 0);

/* 閿欒锛氱洿鎺ヤ紶鎸囬拡锛堟棤娉曡 APP 鍚屾锛?*/
lv_obj_set_style_text_font(obj, &lv_font_courier_48, 0);
lv_obj_set_style_text_font(obj, AREX_FONT_HUGE, 0);
```

### 17.6 榛樿琛岄厤缃腑鐨勫瓧浣?ID

```c
/* row 0: DEPTH 鈥?鏍囬 SMALL(0)锛屾暟鍊?HUGE(3) */
{ AREX_MODULE_DEPTH, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   ... }
/* row 1: NDL+TTS 鈥?鏍囬 SMALL(0)锛屾暟鍊?MEDIUM(2) */
{ AREX_MODULE_NDL,  ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, ... }
/* row 2: POD1+POD2 鈥?鏍囬 SMALL(0)锛屾暟鍊?TITLE(1) */
{ AREX_MODULE_POD1, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE,  ... }
/* row 3: BATT+WTM 鈥?鏍囬 SMALL(0)锛屾暟鍊?SMALL(0) */
{ AREX_MODULE_BATT, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  ... }
```

### 17.7 瀛椾綋澹版槑鏉ユ簮

鎵€鏈夊瓧浣撹祫婧愬０鏄庣粺涓€鍦?`fonts/arex_fonts.h` 涓細

```c
#include "lvgl/lvgl.h"

#ifndef LV_FONT_COURIER_14
#define LV_FONT_COURIER_14 1
#endif
/* LV_FONT_COURIER_20/28/48 鍚岀悊 */

LV_FONT_DECLARE(lv_font_courier_14)
LV_FONT_DECLARE(lv_font_courier_20)
LV_FONT_DECLARE(lv_font_courier_28)
LV_FONT_DECLARE(lv_font_courier_48)

/* 瑙掕壊鍒悕锛堜粎鍦?arex_ui_engine.c 鍐呴儴鏄犲皠鍣ㄥ紩鐢級 */
#define AREX_FONT_SMALL    (&lv_font_courier_14)
#define AREX_FONT_TITLE    (&lv_font_courier_20)
#define AREX_FONT_MEDIUM   (&lv_font_courier_28)
#define AREX_FONT_HUGE     (&lv_font_courier_48)
```

### 17.8 搴熷純瀹忥紙浠呭吋瀹规棫浠ｇ爜锛?

`arex_screen.h` 涓繚鐣欎簡 4 涓棫鐨?`AREX_FONT_*` 瀹忎綔涓哄吋瀹瑰眰锛?*鏂颁唬鐮佺姝娇鐢?*锛?

```c
/* arex_screen.h 鈥?宸插簾寮冿紝浠呭吋瀹规棫浠ｇ爜 */
#define AREX_FONT_HUGE    (&lv_font_courier_48)
#define AREX_FONT_MEDIUM  (&lv_font_courier_28)
#define AREX_FONT_SMALL   (&lv_font_courier_14)
#define AREX_FONT_TITLE   (&lv_font_courier_20)
```

### 17.9 瀛椾綋绯荤粺鍙樻洿鏃ュ織

| 鏃ユ湡 | 鏂囦欢 | 鍙樻洿 |
|------|------|------|
| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `arex_font_id_t` 鏋氫妇瀛楀吀锛沗arex_get_font()` 澹版槑 |
| 2026-04-23 | `arex_ui_engine.c` | 瀹炵幇 `arex_get_font()` 鏄犲皠鍣紱寮曞叆 `fonts/arex_fonts.h`锛涘垹闄ゅ簾寮冪殑 `font_sz_*` 瀛楁鍜岄粯璁ゅ€?|
| 2026-04-23 | `arex_ui_engine.h` | 鍒犻櫎 `arex_sys_config_t` 涓簾寮冪殑 `align_title/huge/med` 瀛楁 |
| 2026-04-23 | `arex_screen.c` | 鎵€鏈?`AREX_FONT_*` 瀹忔浛鎹负 `arex_get_font(id)`锛涘垹闄?`font_cat[]` 涓棿灞傛暟缁勶紙涓ゅ锛?|
| 2026-04-23 | `cards/*.c` | 鎵€鏈夊崱鐗囦腑 `AREX_FONT_*` 瀹忔浛鎹负 `arex_get_font(id)` |
| 2026-04-23 | `arex_screen.h` | 鏃у畯鏍囪涓哄簾寮冿紝闄勬纭敤娉曟敞閲?|
| 2026-04-23 | `AREX_ARCH.md` | 鏂板 Section 17 瀛椾綋鏄犲皠寮曟搸鏂囨。 |

---

## 18. 閲嶆瀯鍙樻洿鏃ュ織锛坴2026-04-23 绗簩闃舵锛?

### 18.1 鏂囦欢鍙樻洿鎬昏

| 鏂囦欢 | 鎿嶄綔 | 璇存槑 |
|------|------|------|
| `arex_card_registry.c` | 閲嶅啓 | 鍗＄墖娉ㄥ唽琛ㄩ噸鏋勶細鏂板 `arex_card_count()`銆乣g_sys_card_order()` 闂存帴鏌ヨ銆乣tile_obj` 鍒濆鍖栨祦绋?|
| `arex_card_registry.h` | 閲嶅啓 | 鏂板 `arex_card_pos_t` 浣嶇疆鏋氫妇锛圛NFO/SETUP 鍥哄畾锛屼腑闂?4 涓彲閲嶆帓锛夛紱`arex_card_reg_t` 鏂板 `on_enter_cb`锛沗tile_obj` 鍒濆涓?NULL |
| `arex_ui_engine.c` | 閲嶅啓 | `arex_sys_config_defaults()` 鐢?`CARD_POS_*` / `CARD_ID_*` 鏋氫妇鏄惧紡璧嬪€?`card_order[]`锛屾浛浠ｆ棫鐨?`for` 寰幆璧嬪€?|
| `arex_ui_engine.h` | 鏂板 | 鏂板 `g_sys_card_order()` 澹版槑锛涙柊澧?`arex_ui_update_data()` 绌洪挬瀛?|
| `arex_ui_state.c` | 閲嶅啓 | `arex_ui_refresh_all()` 鏀逛负 `arex_card_count()` 寰幆锛沗ui_handle_rotate()` 涓?`AREX_CARD_COUNT - 2` 鏀逛负 `AREX_DASH_CARD_COUNT` |
| `arex_data.h/c` | 鏂板缓 | 鏁版嵁鎬荤嚎澶存枃浠跺瓨鏍癸紝鎵€鏈夊畾涔変繚鐣欏湪 `arex_ui_engine.h` |
| `card_info.c` | 閲嶅啓 | 鏀逛负 `arex_get_font()` + `arex_data.h`锛涘紩鍏?`arex_render_dynamic_menu()` 鍔ㄦ€佽彍鍗曞伐鍘?|
| `card_setup.c` | 閲嶅啓 | badge 鍒锋柊閫昏緫淇濈暀锛屽彞鏌勬暟缁勬敼涓哄伐鍘傝緭鍑?|
| `card_gas.c` | 娓呯悊 | `GAS_ROW_GAP` 鍒犻櫎锛宍gap_y` 鏀逛负 `gap_menu` 閰嶇疆鎺ㄧ畻 |
| `UI_main.c` | 閲嶅啓 | 绉婚櫎 `lv_timer_create`锛堝凡鍦?`arex_screen_create` 涓垱寤猴級锛涘惎鍔ㄧ洿鎺ヨ繘鍏?INFO 鍗?|
| `AREX_ARCH.md` | 鏇存柊 | 鏈閲嶆瀯鍐欏叆鏂囨。 |

### 18.2 `arex_card_registry.c` 閲嶅啓瑕佺偣

#### 闈欐€佹敞鍐岃〃浣跨敤鎸囧畾鍒濆鍖栧櫒

```c
static arex_card_reg_t s_registry[AREX_CARD_COUNT] = {
    [CARD_ID_INFO] = {
        .id          = CARD_ID_INFO,
        .title       = "INFO MENU",
        .tile_obj    = NULL,          // create 鍚庢墠濉叆
        .create_cb   = card_info_create,
        .update_cb   = card_info_update,
        .on_enter_cb = NULL,          // 鍙€夛紝鏆傛棤瀹炵幇
    },
    [CARD_ID_COMPASS] = { ... },
    // ...
};
```

#### `arex_card_get()` 閫氳繃 `g_sys_card_order()` 闂存帴鏄犲皠

```c
arex_card_reg_t *arex_card_get(uint8_t order_pos)
{
    if (order_pos >= AREX_CARD_COUNT) return NULL;
    uint8_t id = g_sys_card_order(order_pos);   // 鏌?card_order[]
    if (id >= AREX_CARD_COUNT) return NULL;
    return &s_registry[id];
}
```

#### `arex_ui_refresh_all()` 鐢?`arex_card_count()` 鏇夸唬纭紪鐮?

```c
void arex_ui_refresh_all(void)
{
    for (uint8_t i = 0; i < arex_card_count(); i++) {
        arex_card_reg_t *c = arex_card_get(i);
        if (c && c->update_cb) c->update_cb();
    }
}
```

### 18.3 鍚姩娴佺▼鍙樻洿

**閲嶆瀯鍓?*锛堝師鐗?`arex_ui_state.c`锛夛細
```
arex_ui_state_init() -> state=UI_DASH, dash_card=0
UI_main() -> lv_timer_create(sim_tick_cb, 1000ms)
```
鍚姩鐩存帴杩涘叆 DASH锛坱ile 0 = INFO锛寃all-charge 杩涘叆锛夈€?

**閲嶆瀯鍚?*锛堟柊鐗堬級锛?
```
arex_ui_state_init() -> state=UI_INFO, dash_card=1, menu_info_idx=0
UI_main() -> arex_screen_scroll_to_card(0), arex_screen_set_info_selection(0)
```
鍚姩鐩存帴杩涘叆 INFO 鑿滃崟锛坱ile 0锛夛紝绛夊緟鐢ㄦ埛鎿嶄綔銆傛ā鎷熷畾鏃跺櫒鍦?`arex_screen_create()` 涓垱寤恒€?

### 18.4 `tile_obj` 鐢熷懡鍛ㄦ湡

```
create 闃舵:
  arex_card_registry.c: s_registry[i].tile_obj = NULL锛堝垵濮嬪寲锛?
  card_*.c: create_cb() 鍒涘缓 tile 鎺т欢 -> 杩斿洖 parent
  right_panel_create(): 鎹曡幏 tile 瀵硅薄 -> 濉叆 registry
    -> registry[i].tile_obj = tile_obj;

update 闃舵:
  浠绘剰妯″潡閫氳繃 arex_card_get_by_id(id)->tile_obj 璁块棶
```


### 18.5 鏂板 API / 鏋氫妇閫熸煡

| 鏋氫妇 / 瀹?| 鏂囦欢 | 璇存槑 |
|------|------|------|
| `arex_card_pos_t` | arex_card_registry.h | 浣嶇疆鏋氫妇锛欳ARD_POS_INFO=0(鍥哄畾), CARD_POS_1~4(鍙噸鎺?, CARD_POS_SETUP=5(鍥哄畾) |
| `arex_card_id_t` | arex_card_registry.h | 鍗＄墖鍥烘湁韬唤鏋氫妇锛欳ARD_ID_INFO ~ CARD_ID_SETUP |
| `AREX_CARD_COUNT` | arex_card_registry.h | 鍗＄墖鎬绘暟 = 6 |
| `AREX_DASH_CARD_COUNT` | arex_card_registry.h | DASH 鍙粦鍔ㄦ暟 = 4 |

| 鍑芥暟 | 鏂囦欢 | 浣滅敤 |
|------|------|------|
| `arex_card_count()` | arex_card_registry.c | 杩斿洖鍗＄墖鎬绘暟 |
| `arex_card_get(pos)` | arex_card_registry.c | 鎸変綅缃彇鍗＄墖锛堣蛋 card_order[] 闂存帴灞傦級 |
| `arex_card_get_by_id(id)` | arex_card_registry.c | 鎸?ID 鍙栧崱鐗囷紙涓嶈蛋闂存帴灞傦級 |
| `g_sys_card_order(pos)` | arex_ui_engine.c | 閫氳繃 card_order[] 鏌ヨ鍗＄墖 ID |
| `arex_ui_refresh_all()` | arex_ui_state.c | 閬嶅巻鎵€鏈夊崱鐗囨墽琛?update 鍥炶皟 |
| `arex_ui_update_data()` | arex_ui_engine.c | 绌洪挬瀛愶紝渚涙湭鏉ユ墿灞曟暟鎹洿鏂伴€昏緫 |
| `arex_ui_state_init()` | arex_ui_state.c | 鍒濆鍖?UI 涓婁笅鏂囷紝鍚姩 state=UI_INFO |
| `arex_screen_register_info_list()` | arex_screen.c | INFO 鍒楄〃娉ㄥ唽锛堢敱 card_info.c 璋冪敤锛?|
| `arex_screen_register_setup_list()` | arex_screen.c | SETUP 鍒楄〃娉ㄥ唽锛堢敱 card_setup.c 璋冪敤锛?|

---

## 19. 鍙充晶鍗＄墖鍔ㄦ€佽彍鍗曞紩鎿庯紙v2026-04-23锛?

### 19.1 鏍稿績鐩爣

褰诲簳娑堢伃 `card_info.c` / `card_setup.c` 涓啓姝荤殑 `lv_obj_create` 寰幆銆傛墍鏈夎彍鍗曠殑閫夐」鏁伴噺銆佹枃鏈€佸瓧浣撱€佽竟妗嗗叏閮ㄨВ鑰﹁嚦閰嶇疆缁撴瀯浣撴暟缁勶紝鐢?APP 閫氳繃 JSON/缁撴瀯浣撳姩鎬佷笅鍙戙€?

### 19.2 閰嶇疆缁撴瀯浣?

**`arex_ui_engine.h`** 涓柊澧烇細

```c
typedef struct {
    const char *title_text;      /* 宸︿晶涓绘枃鏈?(鍙负绌? */
    const char *value_badge;     /* 鍙充晶鏁板€?鐘舵€佸窘绔?(鍙负绌? */
    uint8_t     title_font_id;   /* 鏍囬瀛椾綋 ID: arex_font_id_t */
    uint8_t     value_font_id;   /* 寰界珷瀛椾綋 ID: arex_font_id_t */
    uint8_t     border_width;    /* 杈规绮楃粏 px锛?=鏃犺竟妗?*/
    uint8_t     height_u;        /* 楂樺害 (鍗曚綅 U锛?=鐢?h_menu_item 榛樿鍊? */
} arex_menu_item_cfg_t;

void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);
```

### 19.3 宸ュ巶鍑芥暟灏哄鎺ㄧ畻瑙勫垯

`arex_render_dynamic_menu()` 鍐呴儴鎵€鏈夊昂瀵稿叏閮ㄤ粠 `g_sys_config` 鎺ㄧ畻锛屾棤纭紪鐮佸儚绱狅細

| 鍙傛暟 | 鏉ユ簮 |
|------|------|
| `item_h` | `height_u > 0 ? height_u : h_menu_item` 脳 `AREX_BASE_U` |
| `gap_y` | `g_sys_config.gap_menu 脳 AREX_BASE_U` |
| `item_w` | `safe_zone_w - LEFT_ANCHOR_W - gap_u脳AREX_BASE_U - 15` |

### 19.4 涓氬姟浠ｇ爜閲嶆瀯瀵圭収

#### card_info.c

**閲嶆瀯鍓?*锛?6 琛岀‖缂栫爜寰幆锛宍item_h=48` 鍐欐銆?

**閲嶆瀯鍚?*锛氶厤缃暟缁?+ 1 琛屽伐鍘傝皟鐢細

```c
static const arex_menu_item_cfg_t s_info_items[] = {
    { "> LAST DIVE",       NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> DIVE PLAN",       NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> TISSUE & TOX",   NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> GAS & CALC",      NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> SENSOR & DEVICE", NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
};
#define INFO_ITEM_COUNT (sizeof(s_info_items) / sizeof(s_info_items[0]))

/* 鍒楄〃鎬婚珮搴︿粠 h_menu_item 鍜?gap_menu 鎺ㄧ畻 */
uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * AREX_BASE_U;
uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * AREX_BASE_U;
uint16_t list_h = INFO_ITEM_COUNT * item_h_px
                 + (INFO_ITEM_COUNT - 1) * gap_y_px;

arex_render_dynamic_menu(s_list, s_info_items, INFO_ITEM_COUNT, 0, NULL);
```

#### card_setup.c

badge 鍒锋柊閫昏緫瀹屾暣淇濈暀锛屽彞鏌勬暟缁勬敼涓哄伐鍘傝緭鍑猴細

```c
arex_render_dynamic_menu(s_list, s_setup_items, SETUP_ITEM_COUNT, 0, s_setup_item_objs);

for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
    s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
}
```

### 19.5 card_gas.c 鐗规畩澶勭悊

GAS 鍗＄墖姣忚鍖呭惈涓変釜 label锛堟皵浣撳悕 + MOD + PPO2锛夛紝瓒呭嚭閫氱敤鑿滃崟宸ュ巶鑳藉姏鑼冨洿锛屼繚鐣欏師鏈夊鏍囩琛屾覆鏌撻€昏緫銆備粎灏?`GAS_ROW_GAP` 浠庣‖缂栫爜 `8` 鏀逛负 `g_sys_config.gap_menu 脳 AREX_BASE_U`銆?

### 19.6 APP 鍚屾鍗忚鎵╁睍

APP 鍔ㄦ€佷笅鍙戣彍鍗曢厤缃ず渚嬶細

```json
{
  "menus": {
    "info": {
      "items": [
        { "title": "> LAST DIVE", "badge": null, "title_font": 1, "value_font": 0, "border": 2, "height_u": 0 }
      ]
    }
  }
}
```

---

## 20. Section 19 鍙樻洿鏃ュ織锛坴2026-04-23锛?

|| 鏃ユ湡 | 鏂囦欢 | 鍙樻洿 |
||------|------|------|
|| 2026-04-23 | `arex_ui_engine.h` | 鏂板 `arex_menu_item_cfg_t` 鍙?`arex_render_dynamic_menu()` 澹版槑 |
|| 2026-04-23 | `arex_ui_engine.c` | 瀹炵幇宸ュ巶鍑芥暟锛宧eight/gap 鍏ㄧ▼浠?`g_sys_config` 鎺ㄧ畻 |
|| 2026-04-23 | `card_info.c` | 瀹屽叏閲嶆瀯锛氶厤缃暟缁?+ 1 琛屽伐鍘傝皟鐢?|
|| 2026-04-23 | `card_setup.c` | 瀹屽叏閲嶆瀯锛歜adge 鍒锋柊閫昏緫淇濈暀锛屽彞鏌勬暟缁勬敼涓哄伐鍘傝緭鍑?|
|| 2026-04-23 | `card_gas.c` | `GAS_ROW_GAP` 瀹忓垹闄わ紝`gap_y` 鏀逛负閰嶇疆鎺ㄧ畻 |
|| 2026-04-23 | `AREX_ARCH.md` | 鏂板 Section 19/20 |
||| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 19/20 |
||| 2026-04-23 | `arex_ui_engine.h` | 新增 arex_widget_id_t、arex_alarm_level_t 枚举；widget_r[]/widget_c[] 字段 |
||| 2026-04-23 | `arex_ui_engine.h` | 新增 Section 10 5F 引擎 API 声明 |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 arex_render_5f_custom_grid() 总线渲染器（纯数学行×列→绝对坐标） |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 arex_calc_widget_grid() 绝对坐标推算（WIDGET_GAP=2px） |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 create_custom_widget() 组件工厂（span→font自适应，user_data靶向烙印） |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 arex_widget_set_value()（遍历定位label，仅更新文字） |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 arex_trigger_alarm() / arex_clear_all_alarm_styles() 靶向告警同步 |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 arex_show/hide_alarm_banner() 纯英文横幅；s_widget_meta[]元数据字典 |
||| 2026-04-23 | `arex_ui_engine.c` | 新增 span_to_font() 字号自适应 |
||| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 21 |

---

## 21. 5F 自定义网格引擎 (v2026-04-23)

### 21.1 核心设计原则

**零 lv_grid**：MCU 资源受限，动态重构 lv_grid 极耗内存和性能。5F 渲染完全采用"行×列相乘→绝对物理坐标"纯数学映射。

**零硬编码字号**：字号由组件跨度自动决定，渲染器只查元数据表，不做 `if(w_id==DEPTH)` 判断。

**靶向告警烙印**：每个组件创建时 `lv_obj_set_user_data(obj, (void*)(uintptr_t)widget_id)` 打上全系统唯一身份烙印，告警引擎靠此烙印实现"左侧锚点 + 5F 组件同时反色闪烁"。

### 21.2 配置数据（来自 g_sys_config）

```
uint8_t  widget_count;    /* 当前装填的组件数量 (最多30) */
uint8_t  widget_ids[30];  /* 组件类型: arex_widget_id_t */
uint8_t  widget_r[30];    /* 起始行 0~5 */
uint8_t  widget_c[30];    /* 起始列 0~4 */
uint8_t  widget_w[30];    /* 列跨度 1~2 */
uint8_t  widget_h[30];    /* 行跨度 1~2 */
```

APP 下发示例：

```json
{
  "widget_count": 6,
  "widget_ids": [1, 2, 3, 4, 5, 6],
  "widget_r":  [0, 0, 0, 1, 1, 2],
  "widget_c":  [0, 2, 3, 0, 2, 2],
  "widget_w":  [2, 1, 2, 2, 2, 2],
  "widget_h":  [2, 1, 1, 1, 1, 1]
}
```

### 21.3 组件 ID 枚举

| ID | 组件 | 数据源 |
|----|------|--------|
| 0 | EMPTY | - |
| 1 | DEPTH | `g_sensor_data.depth` |
| 2 | TEMP | `g_sensor_data.temp` |
| 3 | HEADING | `g_sensor_data.heading` |
| 4 | SAC_RATE | `g_sensor_data.sac_rate` |
| 5 | BATTERY | `g_sensor_data.battery_pct` |
| 6 | NDL | `g_sensor_data.ndl` |
| 7 | TTS | `g_sensor_data.tts` |
| 8 | PPO2 | `g_sensor_data.ppo2[active_gas]` |
| 9 | CNS | `g_sensor_data.cns_pct` |
| 10 | POD1 | `g_sensor_data.pod1_bar` |
| 11 | POD2 | `g_sensor_data.pod2_bar` |
| 12 | W.TIME | `g_sensor_data.dive_time_s` |

### 21.4 纯数学绝对坐标映射（含 40px 标题避让 + 锁定 80x60 基准）

> **重要**：`AREX_CARD_TITLE_H = 40px`（4U），即常规卡片绿色大标题+分割线的占用高度。60px 的区域是**告警横幅悬浮覆盖层**，不属于常规标题。

```
排版矩阵严格锁定 80x60 基准（完美整数）：
  cell_w = 80px  (tile_w=400 / 5列)
  cell_h = 60px  ((tile_h=400 - AREX_CARD_TITLE_H=40) / 6行)

abs_x  = col * 80 + WIDGET_GAP              (WIDGET_GAP=2px 缝隙)
abs_y  = AREX_CARD_TITLE_H + row * 60 + WIDGET_GAP
abs_w  = span_w * 80 - WIDGET_GAP * 2       (减4px制造四周2px物理留白)
abs_h  = span_h * 60 - WIDGET_GAP * 2
```

**物理尺寸对照**：

| 跨度 | 逻辑尺寸 | 物理尺寸(含留白) |
|------|----------|-----------------|
| 1x1  | 80x60    | 76x56           |
| 2x1  | 160x60   | 156x56          |
| 1x2  | 80x120   | 76x116          |
| 2x2  | 160x120  | 156x116         |

> **关键**：row=0 时 abs_y = 40 + 0 + 2 = **42px**，确保第一排组件落在标题区（~40px）下方的黑色内容区内。

### 21.5 字号自适应引擎

| 跨度条件 | 选用字体 | 字号 |
|----------|----------|------|
| `span_w>=2 && span_h>=2` | `AREX_FONT_ID_HUGE` | 48px |
| `span_w>=2 \|\| span_h>=2` | `AREX_FONT_ID_MEDIUM` | 28px |
| `span_w==1 && span_h==1` | `AREX_FONT_ID_SMALL` | 14px |

### 21.6 靶向告警同步引擎

当 `arex_trigger_alarm(level, text, target_id)` 调用时：

1. 弹出横幅（纯英文，永不显示图案）
2. 遍历所有容器子节点，`lv_obj_get_user_data()` 匹配 `target_id`
3. 同步闪烁：`CRIT` → 2Hz(500ms) / `WARN` → 1Hz(1000ms) / `INFO` → 仅横幅
4. 消失时调用 `arex_clear_all_alarm_styles()`

### 21.7 核心 API

| 函数 | 说明 |
|------|------|
| `arex_render_5f_custom_grid()` | 总线渲染器，遍历配置数组渲染所有组件 |
| `arex_widget_set_value()` | 按 ID 更新数值 label，绝不触发重绘 |
| `arex_trigger_alarm()` | 靶向告警触发 |
| `arex_clear_all_alarm_styles()` | 清除所有告警样式 |
| `arex_get_widget_name()` | 按 ID 获取显示名称 |
| `arex_calc_widget_grid()` | 网格→绝对坐标（含TITLE_ZONE_H=40px避让偏移，锁定80x60基准） |
| `arex_show_alarm_banner()` | 纯英文告警横幅 |

---

## 22. 卡片注册系统 (Card Registry)

### 22.1 设计目标

单张表管理所有卡片信息，新增卡片只需在 `g_cards[]` 加一条，无需修改 `arex_screen.c`。

### 22.2 核心数据结构

```c
/* arex_card_registry.h */

typedef enum {
    CARD_ENGINE_MENU   = 0,   /* create_cb() 完整创建（含 list 注册） */
    CARD_ENGINE_GRID   = 1,   /* arex_render_5f_custom_grid()         */
    CARD_ENGINE_CHART  = 2,   /* 预留                                  */
    CARD_ENGINE_CUSTOM = 3,   /* create_cb() 完整创建                  */
} arex_card_engine_t;

typedef struct {
    arex_card_id_t      id;
    const char         *title;
    arex_card_engine_t  engine_type;
    const void         *config_data;   /* arex_menu_list_cfg_t* for MENU engine */
    lv_obj_t           *tile_obj;      /* filled at runtime by right_panel_create() */
    void (*create_cb)(lv_obj_t *parent);
    void (*update_cb)(void);
    void (*on_enter_cb)(void);
} arex_card_t;
```

```c
/* arex_ui_engine.h */

typedef struct {
    const arex_menu_item_cfg_t *items;
    uint8_t                     count;
} arex_menu_list_cfg_t;
```

### 22.3 引擎分发流程

```
right_panel_create()
  └─ for each card in g_cards[]
       ├─ CARD_ENGINE_GRID → make_title + render_5f_custom_grid()
       └─ 其余              → card->create_cb(tile)
```

### 22.4 文件职责

| 文件 | 职责 |
|------|------|
| `arex_card_registry.h` | `arex_card_engine_t` + `arex_card_t` 类型定义 + API 声明 |
| `arex_card_registry.c` | `g_cards[]` 单张统一表（ROM 字段 + 运行时 tile_obj） |
| `arex_ui_engine.h` | `arex_menu_item_cfg_t` + `arex_menu_list_cfg_t` 配置结构体 |
| `card_info.c` | 暴露 `info_menu_cfg`，`create_cb` 内注册 `s_info_list` |
| `card_setup.c` | 暴露 `setup_menu_cfg`，`create_cb` 内注册 `s_setup_list` |
| `arex_screen.c` | `right_panel_create()` 引擎分发循环 |

### 22.5 当前卡片引擎映射

| 卡片 | engine_type | 说明 |
|------|-------------|------|
| `CARD_ID_INFO` | `CARD_ENGINE_MENU` | `card_info_create()` 完整创建，含 list 注册 |
| `CARD_ID_COMPASS` | `CARD_ENGINE_CUSTOM` | `card_compass_create()` canvas 绘制 |
| `CARD_ID_DECO` | `CARD_ENGINE_CUSTOM` | `card_deco_create()` 柱状图 + GF/CNS/OTU |
| `CARD_ID_GAS` | `CARD_ENGINE_CUSTOM` | `card_gas_create()` 4 行气体行，含静态句柄 |
| `CARD_ID_PLAN` | `CARD_ENGINE_CUSTOM` | `card_plan_create()` canvas 潜水剖面图 |
| `CARD_ID_SETUP` | `CARD_ENGINE_MENU` | `card_setup_create()` 完整创建，含 list 注册 |

> `CARD_ENGINE_GRID`（`arex_render_5f_custom_grid()`）目前由 5F 自定义卡片使用，详见 Section 23。

### 22.6 新增卡片步骤

1. 在 `arex_card_id_t` 枚举中添加新 ID
2. 在 `g_cards[]` 中添加一条 `arex_card_t` 条目
3. 实现 `card_xxx_create()` / `card_xxx_update()`
4. 无需修改 `arex_screen.c`

---

## 23. 5F 标题区保护与坐标避让修复 (v2026-04-24)

### 23.1 问题描述

**现象**：`CARD_ENGINE_GRID` 卡片渲染时，网格组件（DEPTH、TEMP 等）直接顶到 Y=0，顶部绿色标题文字和分割线完全被黑色网格背景覆盖。

**根因**：`arex_calc_widget_grid()` 中 `out_y = parent_y + row * cell_h + WIDGET_GAP`，当 row=0 时 out_y=2。而 `arex_screen_make_card_title()` 渲染的标题 label 在 y=12、标题线在 y=38，整个黑色网格组件（黑色背景）完全覆盖了绿色标题。

### 23.2 修复方案

**增加标题区常量**：

```c
// arex_ui_engine.h
#define AREX_CARD_TITLE_H   40  /* 卡片顶部标题区高度(4U)，包含绿色标题文字+分割线 */
```

> **注意**：视觉规范中 60px 的区域是**告警横幅悬浮覆盖层**，不属于常规卡片标题。常规卡片绿色大标题+分割线严格占用 **40px**。

**重构 `arex_calc_widget_grid()` 签名与逻辑**：

- 移除 `parent_x/parent_y` 参数（不再需要）
- 锁定 80x60 基准网格：`cell_w=80px (400/5)`, `cell_h=60px ((400-40)/6)`
- Y 坐标增加 `AREX_CARD_TITLE_H=40` 偏移量：`out_y = AREX_CARD_TITLE_H + row * 60 + WIDGET_GAP`
- row=0 时 out_y = 40 + 0 + 2 = **42px**，恰好落在标题区下方

**数学验证**（tile 高 400px）：

```
cell_w = 400 / 5 = 80px
cell_h = (400 - 40) / 6 = 60px
row=0:  out_y = 40 + 0*60 + 2 = 42  ← 超过标题线(y=38)，落入黑色区域 ✓
row=5:  out_y = 40 + 5*60 + 2 = 342, out_h = 60-4 = 56, 底线 = 342+56 = 398 < 400 ✓
```

### 23.3 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | 新增 `AREX_CARD_TITLE_H` 常量(40px)；新增 `arex_calc_widget_grid()` 公开声明 |
| 2026-04-24 | `arex_ui_engine.c` | 重构 `arex_calc_widget_grid()`：移除 parent_x/y，锁定 80x60 基准，增加40px标题避让偏移 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 23 记录本次修复 |

### 23.4 API 签名变更

**旧签名**（已废弃）：
```c
void arex_calc_widget_grid(int16_t parent_x, int16_t parent_y,
                            uint16_t parent_w, uint16_t parent_h,
                            uint8_t row, uint8_t col,
                            uint8_t span_w, uint8_t span_h,
                            int16_t *out_x, int16_t *out_y,
                            uint16_t *out_w, uint16_t *out_h);
```

**新签名**（当前）：
```c
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);
```

### 23.5 渲染层级保证

路由中 `CARD_ENGINE_GRID` 分支的调用顺序：
```c
arex_screen_make_card_title(tile, card->title);  // 先画标题（label y=12, 线 y=38）
arex_render_5f_custom_grid(tile, ...);            // 后画网格（row=0 的组件从 y=52 开始）
```

网格组件 Z-Order 在标题之后创建，自然覆盖标题区下方的黑色区域，不会覆盖标题文字和分割线。

---

## 24. 左侧面板数据修复 (v2026-04-24)

### 24.1 问题描述

**问题 1：W.TIME 始终显示 `00:00`**
- `AREX_MODULE_WTM` 的数值 label 读取 `g_sensor_data.surface_time_s`，但 `sim_tick_cb` 中从未递增此字段
- 根因：`UI_main.c` 的 `sim_tick_cb` 只更新了 `dive_time_s`，漏掉了 `surface_time_s`

**问题 2：TIME 标签创建时硬编码 `00:00`**
- `AREX_MODULE_TIME` 的 label 在 `left_anchor_create()` 中创建时直接设置 `"00:00"`
- 虽然 `arex_screen_refresh_left_panel()` 有正确的刷新逻辑，但初始值应为数据总线的当前值

**问题 3：POD1/POD2 初始值显示为 `210` / `195` 而非 `"--"`**
- `arex_ui_engine.c` 的 `arex_ui_init()` 中 `pod1_bar=210.0f`，`pod2_bar=195.0f` 为模拟值
- 下水前 POD 未连接时应显示 `"--"`
- 渲染层直接 `snprintf("%.0f")` 输出数字，无 0 值判断

**问题 4：PO2 1 和 PO2 2 未显示 `"--"`**
- 初始文本已在 `left_anchor_create()` 中改为 `"--"`，刷新函数中也改为固定 `"--"`
- 需确认重编译后生效

### 24.2 修复方案

**修复 1：`UI_main.c` 的 `sim_tick_cb` 中增加 `surface_time_s` 递增**

```c
g_sensor_data.dive_time_s += 1;
g_sensor_data.surface_time_s += 1;  // 新增：水面休息计时同步递增
```

**修复 2：`arex_screen.c` 的 `left_anchor_create()` 中 `AREX_MODULE_TIME` 改用数据总线**

```c
case AREX_MODULE_TIME:
    snprintf(buf, sizeof(buf), "%02d:%02d",
             g_sensor_data.dive_time_s / 60,
             g_sensor_data.dive_time_s % 60);
    lv_label_set_text(lbl_val, buf);
    s_lbl_time = lbl_val;
    break;
```

**修复 3：POD1/POD2 初始值归零，渲染层按 0 = `"--"` 处理**

统一约定：气压传感器 `pod1_bar / pod2_bar` 为 `0.0f` 代表"未连接"，渲染时显示 `"--"`。

```c
// arex_ui_engine.c:513-514 初始值归零
g_sensor_data.pod1_bar = 0.0f;
g_sensor_data.pod2_bar = 0.0f;

// arex_screen.c left_anchor_create() / arex_screen_refresh_left_panel()
// 渲染时判断 0 显示 "--"
if (g_sensor_data.pod1_bar <= 0.0f)
    snprintf(buf, sizeof(buf), "--");
else
    snprintf(buf, sizeof(buf), "%.0f", g_sensor_data.pod1_bar);
```

### 24.3 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 增加 `g_sensor_data.surface_time_s += 1` |
| 2026-04-24 | `arex_screen.c` | `AREX_MODULE_TIME` 创建时从硬编码 `"00:00"` 改为 `g_sensor_data.dive_time_s` |
| 2026-04-24 | `arex_ui_engine.c` | `pod1_bar / pod2_bar` 初始值从 `210.0f/195.0f` 改为 `0.0f` |
| 2026-04-24 | `arex_screen.c` | POD1/POD2 渲染（创建+刷新+INFO菜单）加 `0.0f = "--"` 判断 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 24 记录本次修复 |
| 2026-04-24 | `arex_ui_engine.h` | `arex_dive_pt_t.time_min` → `time_s`（统一秒级单位） |
| 2026-04-24 | `card_plan.c` | 重写 `plan_chart_draw_cb` 为秒级坐标引擎；`init_test_data()` 清零 `g_dive_log_count`；新增 `arex_dive_log_append()` |
| 2026-04-24 | `arex_data.h` | 新增 `arex_dive_log_append()` 声明 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 改用 `arex_dive_log_append()` 推流 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 25 |

---

## 25. 4F 曲线时间单位统一与历史轨迹推流 (v2026-04-24)

### 25.1 问题描述

**问题 1：X 轴单位错位**
- 原代码底层以"分钟"为单位，但 X 轴网格标签在 `<120s` 时标 `"%ds"`，导致 20 分钟坐标系里 4 秒只占 0.33%，曲线像悬崖

**问题 2：历史轨迹污染未来预测**
- `g_dive_log[]` 的 `time_min` 字段被旧代码预填充未来大时间值，`g_dive_log_count` 初始化不为 0，导致预测虚线被历史数据实线覆盖

**问题 3：无统一推流接口**
- `UI_main.c` 直接操作 `g_dive_log[]` 数组，不符合数据总线统一管理原则

### 25.2 修复方案

**统一时间单位**：全系统以"秒"为唯一时间基准，消除分钟/秒混用

```c
// arex_ui_engine.h
typedef struct { float time_s; float depth_m; } arex_dive_pt_t;  // 原 time_min → time_s
```

**历史轨迹推流接口**：

```c
// arex_data.h 声明
void arex_dive_log_append(float current_time_s, float current_depth_m);

// card_plan.c 实现
void arex_dive_log_append(float current_time_s, float current_depth_m)
{
    if (g_dive_log_count < MAX_DIVE_LOG) {
        g_dive_log[g_dive_log_count].time_s   = current_time_s;
        g_dive_log[g_dive_log_count].depth_m  = current_depth_m;
        g_dive_log_count++;
    }
}
```

**清零启动**：`init_test_data()` 中 `g_dive_log_count = 0`，轨迹从零生长

**秒级坐标引擎**：`plan_chart_draw_cb` 核心改为：
- 当前时间 `current_t_sec = g_sensor_data.dive_time_s`（秒）
- 升水速度 `6.0f` 秒/米（对应 10m/min）
- X 轴最小锁定 20 秒视口，`fmaxf` 动态扩展
- 映射宏 `MAP_X(t_sec) = pad_x + (t_sec / max_t_axis_sec) * w`

### 25.3 X 轴秒级步长表

| 时间范围 | X 轴最大刻度 | 步长 |
|---------|-------------|------|
| 0~20s | 20s | 10s |
| 20~60s | ceil/10*10 | 10s |
| 60~120s | ceil/60*60 | 15s |
| 2~5min | ceil/60*60 | 30s |
| 5~10min | ceil/60*60 | 60s |
| 10~20min | ceil/60*60 | 120s |
| 20~60min | ceil/60*60 | 300s |

### 25.4 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | `arex_dive_pt_t.time_min` → `time_s` |
| 2026-04-24 | `card_plan.c` | 完全重写 `plan_chart_draw_cb` 为秒级引擎；清零 `g_dive_log_count` |
| 2026-04-24 | `card_plan.c` | 新增 `arex_dive_log_append()` |
| 2026-04-24 | `arex_data.h` | 新增 `arex_dive_log_append()` 声明 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 改用 `arex_dive_log_append()` |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 25 |
| 2026-04-24 | `arex_ui_engine.h` | `arex_sensor_data_t` 增加 `dirty_mask` 字段；新增 `arex_dirty_bit_t` 脏标记枚举；声明 Data Bus Setter + UI Consumer |
| 2026-04-24 | `arex_data.h` | 彻底重构为 Data Bus 硬件写入接口层；`arex_bus_set_*()` 系列声明 |
| 2026-04-24 | `arex_data.c` | 新建文件；实现全部 `arex_bus_set_*()` Setter，含防抖阈值 |
| 2026-04-24 | `arex_ui_engine.c` | 实现 `arex_ui_update_task()` 集中消费任务；`arex_screen.h` include |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 全部改用 `arex_bus_set_*()`；撤销直接 `g_sensor_data` 写入；撤销 `arex_ui_refresh_all()` |
| 2026-04-24 | `UI_main.c` | `arex_ui_update_task(50ms)` 驱动 UI 渲染；`sim_tick_cb(1Hz)` 驱动数据写入 |
| 2026-04-24 | `arex_card_registry.h` | 新增所有卡片 update forward 声明 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 26 |

---

## 26. Data Bus 架构：硬件写入接口与 UI 消费任务 (v2026-04-24)

### 26.1 架构铁律

```
硬件工程师 ──arex_bus_set_*()──▶ g_sensor_data (dirty_mask)
                                          │
                              arex_ui_update_task() (50ms lv_timer)
                                          │
                                按脏标记按需刷新 UI
```

- **硬件工程师**：只能调用 `arex_bus_set_*()` 系列函数。禁止直接写 `g_sensor_data`，禁止包含任何 LVGL 代码。
- **UI 工程师**：只能修改 `arex_ui_update_task()` 消费者函数。禁止绕过消费任务直接操作 LVGL。
- **两者通过 `g_sensor_data.dirty_mask` 完全解耦**。

### 26.2 脏标记位枚举

| 位 | 宏 | 含义 |
|----|----|------|
| 0 | `DIRTY_DEPTH` | 深度数据 |
| 1 | `DIRTY_NDL` | 免减压时间 |
| 2 | `DIRTY_TTS` | 回到水面时间 |
| 3 | `DIRTY_POD` | 气瓶压力（pod1/pod2） |
| 4 | `DIRTY_BATT` | 电池电量 |
| 5 | `DIRTY_HEADING` | 罗盘航向 |
| 6 | `DIRTY_TIME` | 潜水时间 / W.TIME |
| 7 | `DIRTY_PPO2` | PO2 值 |
| 8 | `DIRTY_GAS` | 气体切换 |
| 9 | `DIRTY_DECO` | 减压数据 |
| 10 | `DIRTY_CHART` | 4F 曲线图刷新 |
| 11 | `DIRTY_ALARM` | 告警状态 |

### 26.3 Data Bus Setter 接口（`arex_data.h / arex_data.c`）

```c
void arex_bus_set_depth(float depth_m);         // 防抖阈值 0.05m
void arex_bus_set_ndl(int16_t ndl_min);
void arex_bus_set_tts(uint16_t tts_min);
void arex_bus_set_pod(uint8_t pod_idx, float bar); // pod_idx: 0=pod1, 1=pod2
void arex_bus_set_battery(float pct);
void arex_bus_set_heading(uint16_t heading_deg);
void arex_bus_set_dive_time(uint32_t dive_s);   // 同时触发 DIRTY_TIME | DIRTY_CHART
void arex_bus_set_surface_time(uint32_t surface_s);
void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val);
void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name);
void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min);
void arex_bus_set_cns(uint8_t cns_pct);
void arex_bus_set_otu(uint16_t otu_val);
void arex_bus_set_chart_refresh(void);            // 仅打 DIRTY_CHART
void arex_bus_clear_all_dirty(void);
```

### 26.4 UI 消费任务（`arex_ui_engine.c`）

```c
// 由 lv_timer 驱动，50ms 周期（20 FPS）
void arex_ui_update_task(lv_timer_t *timer)
{
    uint32_t mask = g_sensor_data.dirty_mask;
    if (mask == DIRTY_NONE) return;

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_DECO)) {
        arex_screen_refresh_left_panel();
        card_deco_update();
    }
    if (mask & DIRTY_POD)    { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_BATT)   { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_HEADING){ arex_screen_refresh_compass_target(); }
    if (mask & DIRTY_TIME)   { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_PPO2)   { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_GAS)    { arex_screen_refresh_gas_menu(); arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_CHART)  { card_plan_update(); }

    arex_bus_clear_all_dirty();
}
```

### 26.5 定时器分层

| 定时器 | 周期 | 职责 |
|-------|------|------|
| `sim_tick_cb` | 1Hz | 硬件数据写入，通过 `arex_bus_set_*()` 打脏标记 |
| `arex_ui_update_task` | 50ms | UI 消费任务，按脏标记按需刷新 LVGL |

### 26.6 防抖策略

- 深度 `arex_bus_set_depth`：变化超过 0.05m 才打脏标记
- 电池 `arex_bus_set_battery`：变化超过 0.1 才打脏标记
- 其余字段：任何变化均打脏标记

### 26.7 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | `arex_sensor_data_t` 增加 `dirty_mask`；新增 `arex_dirty_bit_t` 枚举；声明 Setter + Consumer |
| 2026-04-24 | `arex_data.h` | 彻底重构为 Data Bus 接口层头文件 |
| 2026-04-24 | `arex_data.c` | 新建；全部 Setter 实现，含防抖逻辑 |
| 2026-04-24 | `arex_ui_engine.c` | 实现 `arex_ui_update_task()` 集中消费任务 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 全部改用 Setter；撤销直接写入；分离两定时器 |
| 2026-04-24 | `arex_card_registry.h` | 卡片 update forward 声明 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 26 |

