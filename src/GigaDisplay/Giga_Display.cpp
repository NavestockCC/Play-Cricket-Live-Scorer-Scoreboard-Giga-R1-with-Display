#include <Arduino.h>
#include <Arduino_H7_Video.h>
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#include "GigaDisplay/Giga_Display.h"
#include "scoreboard_config/pins_and_constants.h"
#include "scoreboard/shift_register.h"
#include "scoreboard/display_controller.h"

/* -------------------------------------------------------------
 * HARDWARE CONFIGURATION & GLOBAL LABELS
 * ------------------------------------------------------------- */
// Primary hardware display wrapper object instance
Arduino_H7_Video Display(800, 480, GigaDisplayShield); 

// Global object hooks for updating UI elements from BLE or main program files
lv_obj_t *lbl_batA_val;
lv_obj_t *lbl_score_val;
lv_obj_t *lbl_batB_val;
lv_obj_t *lbl_wickets_val;
lv_obj_t *lbl_overs_val;
lv_obj_t *lbl_target_val;
lv_obj_t *lbl_sidebar_log; // Global handle targeting the rolling list view

/* -------------------------------------------------------------
 * ROLLING LOG TRACKING MEMORY ARRAYS
 * ------------------------------------------------------------- */
#define MAX_HISTORY_ITEMS 15
#define ITEM_BUFFER_SIZE  32 

static char history_log[MAX_HISTORY_ITEMS][ITEM_BUFFER_SIZE];
static int history_count = 0;

// Assembly staging buffer sized to safely house strings, middle dots, and line breaks
static char combined_text_buffer[MAX_HISTORY_ITEMS * (ITEM_BUFFER_SIZE + 6)];

/* -------------------------------------------------------------
 * SCOREBOARD CONFIG PICKER: lets the operator choose which physical rig
 * (SCOREBOARD_CONFIGS entry) is active, from the touchscreen.
 * ------------------------------------------------------------- */
static lv_obj_t *config_dropdown = NULL;
static char config_options_buffer[128];

// Builds the '\n'-separated dropdown options string from SCOREBOARD_CONFIGS
// names (LVGL dropdowns expect one option per line).
static void build_config_options_text() {
    config_options_buffer[0] = '\0';
    for (int i = 0; i < SCOREBOARD_CONFIG_COUNT; i++) {
        strncat(config_options_buffer, SCOREBOARD_CONFIGS[i].name,
                sizeof(config_options_buffer) - strlen(config_options_buffer) - 1);
        if (i < SCOREBOARD_CONFIG_COUNT - 1) {
            strncat(config_options_buffer, "\n",
                    sizeof(config_options_buffer) - strlen(config_options_buffer) - 1);
        }
    }
}

// Fired when the operator picks a different rig from the dropdown: switches
// the active config, re-configures that rig's pins, and repaints the
// physical displays with the current scores through the new hardware.
static void config_dropdown_event_cb(lv_event_t * e) {
    (void)e;
    if (config_dropdown == NULL) return;

    uint16_t selectedIndex = lv_dropdown_get_selected(config_dropdown);
    if (selectedIndex >= (uint16_t)SCOREBOARD_CONFIG_COUNT) return;

    activeScoreboardConfigIndex = selectedIndex;
    const ScoreboardChainsConfig& newConfig = SCOREBOARD_CONFIGS[selectedIndex];

    setupShiftRegisterChain(newConfig.top);
    setupShiftRegisterChain(newConfig.bottom);
    update_Scoreboard();

    char message[40];
    snprintf(message, sizeof(message), "Config: %s", newConfig.name);
    update_display_card(lbl_sidebar_log, message);
}

/* -------------------------------------------------------------
 * HELPER FUNCTION: CREATE A STYLIZED WHITE SCORE CARD
 * ------------------------------------------------------------- */
lv_obj_t* create_card(lv_obj_t *parent, const char *heading, int col, int row) {
    // 1. Instantiate layout cell backing container box 
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    
    // 2. Visual styling properties (White card with rounded corners)
    lv_obj_set_style_bg_color(card, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);          
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);     
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);         
    lv_obj_set_style_clip_corner(card, true, LV_PART_MAIN);

    // 3. Setup Category Heading Label (Small, Dark Gray text centered at top)
    lv_obj_t *lbl_head = lv_label_create(card);
    lv_label_set_text(lbl_head, heading);
    lv_obj_set_style_text_color(lbl_head, lv_color_hex(0x555555), LV_PART_MAIN);
    
    const lv_font_t * base_font = lv_font_get_default();
    lv_obj_set_style_text_font(lbl_head, base_font, LV_PART_MAIN); 
    lv_obj_align(lbl_head, LV_ALIGN_TOP_MID, 0, 0);

    // 4. Setup Dynamic Output Value Label (Large, Bold Black text centered)
    lv_obj_t *lbl_val = lv_label_create(card);
    lv_label_set_text(lbl_val, "-");
    lv_obj_set_style_text_color(lbl_val, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_val, base_font, LV_PART_MAIN); 
    lv_obj_set_style_text_letter_space(lbl_val, 2, LV_PART_MAIN);
    lv_obj_align(lbl_val, LV_ALIGN_CENTER, 0, 10);

    return lbl_val; 
}

/* -------------------------------------------------------------
 * CORE SCOREBOARD UI GENERATOR
 * ------------------------------------------------------------- */
void create_scoreboard_grid() {
    // 1. Initialize screen layer backing properties
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x03989e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    // 2. Establish main base responsive bounding canvas box
    lv_obj_t * main_container = lv_obj_create(scr);
    lv_obj_set_size(main_container, 800, 480); 
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x03989e), LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 15, LV_PART_MAIN);     
    lv_obj_set_style_pad_gap(main_container, 15, LV_PART_MAIN);     

    // Slices screen space horizontally using fractional allocations (50% left vs 50% right)
    // Row 0 is a fixed-height header (title + config picker); row 1 holds the main content.
    static int32_t main_col_dsc[] = {LV_GRID_FR(50), LV_GRID_FR(50), LV_GRID_TEMPLATE_LAST};
    static int32_t main_row_dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(main_container, main_col_dsc, main_row_dsc);
    lv_obj_set_layout(main_container, LV_LAYOUT_GRID);

    /* -------------------------------------------------------------
     * HEADER: Title + scoreboard rig config picker (spans both columns)
     * ------------------------------------------------------------- */
    lv_obj_t * header = lv_obj_create(main_container);
    lv_obj_set_grid_cell(header, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Cricket Scoreboard");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, lv_font_get_default(), LV_PART_MAIN);

    build_config_options_text();
    config_dropdown = lv_dropdown_create(header);
    lv_dropdown_set_options(config_dropdown, config_options_buffer);
    lv_dropdown_set_selected(config_dropdown, (uint16_t)activeScoreboardConfigIndex);
    lv_obj_add_event_cb(config_dropdown, config_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* -------------------------------------------------------------
     * LEFT PANEL SUB-GRID: 3 Columns x 2 Rows (50% Width)
     * ------------------------------------------------------------- */
    lv_obj_t * left_panel = lv_obj_create(main_container);
    lv_obj_set_grid_cell(left_panel, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(left_panel, 0, LV_PART_MAIN);
    
    // Inject pixel gaps separating the cards so elements do not touch
    lv_obj_set_style_pad_row(left_panel, 15, LV_PART_MAIN);
    lv_obj_set_style_pad_column(left_panel, 15, LV_PART_MAIN);

    static int32_t left_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t left_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(left_panel, left_col_dsc, left_row_dsc);
    lv_obj_set_layout(left_panel, LV_LAYOUT_GRID);

    // Build upper row metric blocks (Row index 0)
    lbl_batA_val  = create_card(left_panel, "Bat A",  0, 0);
    lbl_score_val = create_card(left_panel, "Score",  1, 0);
    lbl_batB_val  = create_card(left_panel, "Bat B",  2, 0);

    // Build lower row metric blocks (Row index 1)
    lbl_wickets_val = create_card(left_panel, "Wickets", 0, 1);
    lbl_overs_val   = create_card(left_panel, "Overs",   1, 1);
    lbl_target_val  = create_card(left_panel, "Target",  2, 1);

    /* -------------------------------------------------------------
     * RIGHT PANEL SUB-GRID: Sidebar Module (50% Width)
     * ------------------------------------------------------------- */
    lv_obj_t * right_panel = lv_obj_create(main_container);
    lv_obj_set_grid_cell(right_panel, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_panel, 0, LV_PART_MAIN);

    static int32_t right_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t right_row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(right_panel, right_col_dsc, right_row_dsc);
    lv_obj_set_layout(right_panel, LV_LAYOUT_GRID);

    /* -------------------------------------------------------------
     * SIDEBAR COMPONENT: ROUNDED BLACK BOX LIST CONSOLE
     * ------------------------------------------------------------- */
    lv_obj_t * black_box = lv_obj_create(right_panel);
    lv_obj_set_grid_cell(black_box, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
    // Formatting configurations (Deep black fill block with uniform structural rounded edges)
    lv_obj_set_style_bg_color(black_box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(black_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(black_box, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(black_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(black_box, 15, LV_PART_MAIN); 

    // Setup rolling text ledger wrapper target element
    lbl_sidebar_log = lv_label_create(black_box);
    lv_label_set_text(lbl_sidebar_log, "• No Updates"); 
    lv_obj_set_style_text_color(lbl_sidebar_log, lv_color_white(), LV_PART_MAIN);
    
    const lv_font_t * base_font = lv_font_get_default();
    lv_obj_set_style_text_font(lbl_sidebar_log, base_font, LV_PART_MAIN);
    
    // FIXED / RECOMMENDED ADJUSTMENT: Left-align list blocks so bullets line up flush
    lv_obj_set_style_text_align(lbl_sidebar_log, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(lbl_sidebar_log, LV_ALIGN_TOP_LEFT, 5, 5);
}

/* -------------------------------------------------------------
 * DYNAMIC ENGINE: ROLLING UPDATE BULLETED LIST CONTROLLER
 * ------------------------------------------------------------- */
void update_display_card(lv_obj_t *label, const char *new_value) {
    if (label == NULL || new_value == NULL) return;

    // 1. Shift memory array metrics when max list index constraints are reached
    if (history_count >= MAX_HISTORY_ITEMS) {
        for (int i = 0; i < MAX_HISTORY_ITEMS - 1; i++) {
            strncpy(history_log[i], history_log[i + 1], ITEM_BUFFER_SIZE - 1);
            history_log[i][ITEM_BUFFER_SIZE - 1] = '\0'; 
        }
        strncpy(history_log[MAX_HISTORY_ITEMS - 1], new_value, ITEM_BUFFER_SIZE - 1);
        history_log[MAX_HISTORY_ITEMS - 1][ITEM_BUFFER_SIZE - 1] = '\0';
    } 
    else {
        // Log has open slots remaining, simply append item to active counter tail index
        strncpy(history_log[history_count], new_value, ITEM_BUFFER_SIZE - 1);
        history_log[history_count][ITEM_BUFFER_SIZE - 1] = '\0';
        history_count++;
    }

    // 2. Reset character string array index pointer tracking state cleanly
    combined_text_buffer[0] = '\0';

    // 3. Construct block mapping from newest records down to historical rows
    for (int i = history_count - 1; i >= 0; i--) {
        // Appends the clean, standard middle-dot character configuration to row starts
        strcat(combined_text_buffer, "• ");
        strcat(combined_text_buffer, history_log[i]);
        
        // Append carriage break parameters for all elements except the final index string
        if (i > 0) {
            strcat(combined_text_buffer, "\n");
        }
    }

    // 4. Force repaint payload message block onto the layout layer
    lv_label_set_text(label, combined_text_buffer);
}

/* -------------------------------------------------------------
 * SIMPLE SETTER: SINGLE CURRENT VALUE (SCORE CARDS)
 * ------------------------------------------------------------- */
void set_card_value(lv_obj_t *label, const char *new_value) {
    if (label == NULL || new_value == NULL) return;
    lv_label_set_text(label, new_value);
}

/* -------------------------------------------------------------
MAIN INITIALIZATION ENTRY POINT
------------------------------------------------------------- */
void display_setup() {
    Display.begin();
    create_scoreboard_grid();
}

