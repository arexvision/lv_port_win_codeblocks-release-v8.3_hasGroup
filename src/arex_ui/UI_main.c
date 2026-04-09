#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_screen.h"
#include "arex_input.h"
#include "../../lvgl/lvgl.h"

static lv_timer_t *s_sim_timer;

/* Simulate slowly changing depth and heading for demo */
static void sim_tick_cb(lv_timer_t *t)
{
    (void)t;
    g_arex.compass.heading = (float)((int)(g_arex.compass.heading + 1) % 360);
    g_arex.dive.dive_time_s++;
    arex_screen_refresh_left_panel();
    arex_ui_refresh_all();
}

void UI_main(void)
{
    arex_data_init();
    arex_ui_state_init();
    arex_screen_create();

    /* Get screen handle to pass to input init.
       arex_screen_create() calls lv_scr_load(), so active screen is ours. */
    lv_obj_t *scr = lv_scr_act();
    arex_input_init(scr);

    /* Populate left panel with initial values */
    arex_screen_refresh_left_panel();

    /* Set initial selection highlight on INFO card item 0 */
    arex_screen_set_info_selection(0);

    /* Simulation tick: 1 second interval */
    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);
}
