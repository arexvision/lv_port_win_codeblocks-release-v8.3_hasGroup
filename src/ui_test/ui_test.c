#define UI_TEST_IMPLEMENTATION
#include "ui_test.h"

#include "ui_test_flags.h"
#include "lvgl/lvgl.h"
#include "ui/core/ui_defs.h"

#if UI_LVGL_PAGE_STRESS_TEST_ENABLED

typedef struct {
    lv_obj_t *page;
    lv_obj_t *value[UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE];
} ui_lvgl_stress_page_t;

static ui_lvgl_stress_page_t s_stress_pages[UI_LVGL_PAGE_STRESS_PAGE_COUNT];
static uint32_t s_stress_tick;
static uint8_t s_stress_active_page;

static void stress_set_active_page(uint8_t page_index)
{
    if (page_index >= UI_LVGL_PAGE_STRESS_PAGE_COUNT) {
        page_index = 0;
    }

    s_stress_active_page = page_index;
    for (uint8_t i = 0; i < UI_LVGL_PAGE_STRESS_PAGE_COUNT; i++) {
        if (i == s_stress_active_page) {
            lv_obj_clear_flag(s_stress_pages[i].page, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_stress_pages[i].page, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_obj_t *stress_create_label(lv_obj_t *parent,
                                     const char *text,
                                     lv_coord_t x,
                                     lv_coord_t y,
                                     const lv_font_t *font,
                                     lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_label_set_text(label, text);
    return label;
}

static void stress_update_one_page(uint8_t page_index, uint32_t tick)
{
    ui_lvgl_stress_page_t *page = &s_stress_pages[page_index];

    for (uint8_t row = 0; row < UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE; row++) {
        char value[32];
        uint32_t sample = tick + (uint32_t)page_index * 37U + (uint32_t)row * 11U;
        lv_snprintf(value, sizeof(value), "%03lu.%02lu",
                    (unsigned long)(sample % 400U),
                    (unsigned long)((sample * 7U) % 100U));
        lv_label_set_text(page->value[row], value);
    }
}

static void stress_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    s_stress_tick++;

#if UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES
    for (uint8_t i = 0; i < UI_LVGL_PAGE_STRESS_PAGE_COUNT; i++) {
        stress_update_one_page(i, s_stress_tick);
    }
#else
    stress_update_one_page(s_stress_active_page, s_stress_tick);
#endif

#if UI_LVGL_PAGE_STRESS_FORCE_FULL_INVALIDATE
    lv_obj_invalidate(lv_scr_act());
#endif
}

static void stress_switch_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    stress_set_active_page((uint8_t)((s_stress_active_page + 1U) % UI_LVGL_PAGE_STRESS_PAGE_COUNT));
}

static void create_lvgl_page_stress_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, w, h);
    lv_obj_set_style_bg_color(scr, BLACK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t page = 0; page < UI_LVGL_PAGE_STRESS_PAGE_COUNT; page++) {
        lv_obj_t *container = lv_obj_create(scr);
        s_stress_pages[page].page = container;
        lv_obj_set_size(container, w, h);
        lv_obj_set_pos(container, 0, 0);
        lv_obj_set_style_bg_color(container, BLACK, 0);
        lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_radius(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

        char title[32];
        lv_snprintf(title, sizeof(title), "LVGL STRESS P%u/%u",
                    (unsigned int)(page + 1U),
                    (unsigned int)UI_LVGL_PAGE_STRESS_PAGE_COUNT);
        stress_create_label(container, title, 18, 12, LV_FONT_DEFAULT, GREEN);

        for (uint8_t row = 0; row < UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE; row++) {
            lv_coord_t y = (lv_coord_t)(48 + row * 18);
            char name[24];
            lv_snprintf(name, sizeof(name), "W%02u", (unsigned int)(row + 1U));
            stress_create_label(container, name, 22, y, LV_FONT_DEFAULT, lv_color_hex(0x707070));
            s_stress_pages[page].value[row] =
                stress_create_label(container, "000.00", 88, y, LV_FONT_DEFAULT, lv_color_hex(0xFFFFFF));
        }
    }

    s_stress_tick = 0U;
    stress_set_active_page(0U);
    lv_scr_load(scr);

    lv_timer_create(stress_update_timer_cb,
                    UI_LVGL_PAGE_STRESS_UPDATE_PERIOD_MS,
                    NULL);
    lv_timer_create(stress_switch_timer_cb,
                    UI_LVGL_PAGE_STRESS_SWITCH_PERIOD_MS,
                    NULL);
}

#endif /* UI_LVGL_PAGE_STRESS_TEST_ENABLED */

#if UI_OPTICAL_GHOST_TEST_ENABLED

#define OPTICAL_TEST_MARK_X      24
#define OPTICAL_TEST_MARK_Y      24
#define OPTICAL_TEST_MARK_THICK  36
#define OPTICAL_TEST_MARK_W      160
#define OPTICAL_TEST_MARK_H      160

static lv_obj_t *create_solid_rect(lv_obj_t *parent,
                                   lv_coord_t x,
                                   lv_coord_t y,
                                   lv_coord_t w,
                                   lv_coord_t h,
                                   lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void create_optical_ghost_test_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t w = lv_disp_get_hor_res(disp);
    lv_coord_t h = lv_disp_get_ver_res(disp);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, w, h);
    lv_obj_set_style_bg_color(scr, BLACK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Single asymmetric geometric L:
     * no font, no animation, no other bright objects. This keeps ghost direction
     * diagnosis independent from font antialiasing and normal UI refresh. */
    create_solid_rect(scr,
                      OPTICAL_TEST_MARK_X,
                      OPTICAL_TEST_MARK_Y,
                      OPTICAL_TEST_MARK_THICK,
                      OPTICAL_TEST_MARK_H,
                      GREEN);
    create_solid_rect(scr,
                      OPTICAL_TEST_MARK_X,
                      OPTICAL_TEST_MARK_Y + OPTICAL_TEST_MARK_H - OPTICAL_TEST_MARK_THICK,
                      OPTICAL_TEST_MARK_W,
                      OPTICAL_TEST_MARK_THICK,
                      GREEN);

    lv_scr_load(scr);
}

#endif /* UI_OPTICAL_GHOST_TEST_ENABLED */

bool ui_test_try_start(void)
{
#if UI_LVGL_PAGE_STRESS_TEST_ENABLED
    create_lvgl_page_stress_screen();
    return true;
#elif UI_OPTICAL_GHOST_TEST_ENABLED
    create_optical_ghost_test_screen();
    return true;
#else
    return false;
#endif
}
