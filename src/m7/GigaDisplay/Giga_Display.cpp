#include <Arduino.h>
#include <Arduino_H7_Video.h>
#include "dsi.h" // Arduino_H7_Video's display-controller helpers, for display_flush()
#include <Arduino_GigaDisplayTouch.h>
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#include "GigaDisplay/Giga_Display.h"
#include "shared/scoreboard_configs.h"
#include "shared/score_fields.h"
#include "shared/core_link.h"

/* -------------------------------------------------------------
 * HARDWARE CONFIGURATION & GLOBAL LABELS
 * ------------------------------------------------------------- */
// Primary hardware display wrapper object instance
static Arduino_H7_Video Display(800, 480, GigaDisplayShield);

// GT911 touch controller. begin() registers it with LVGL as the pointer
// input device -- without it the touchscreen (e.g. the config picker) is inert.
static Arduino_GigaDisplayTouch TouchDetector;

// Value label of each field's score card, indexed by ScoreFieldId.
static lv_obj_t *score_cards[FIELD_COUNT];
static lv_obj_t *lbl_sidebar_log; // the rolling log list

/* -------------------------------------------------------------
 * ROLLING LOG TRACKING MEMORY ARRAYS
 * ------------------------------------------------------------- */
#define MAX_HISTORY_ITEMS 12 // as many lines as fit above the BLE signal chart
#define ITEM_BUFFER_SIZE  32

static char history_log[MAX_HISTORY_ITEMS][ITEM_BUFFER_SIZE];
static int history_count = 0;

// Assembly staging buffer sized to safely house strings, middle dots, and line breaks
static char combined_text_buffer[MAX_HISTORY_ITEMS * (ITEM_BUFFER_SIZE + 6)];

/* -------------------------------------------------------------
 * SCOREBOARD CONFIG PICKER: lets the operator choose which physical rig
 * (SCOREBOARD_CONFIGS entry) is active, from the touchscreen. The M4 owns
 * the active rig: the picker only sends it a request, and the selection is
 * confirmed (or snapped back) by the M4's reply -- see show_active_config().
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

// Fired when the operator picks a different rig from the dropdown: asks the
// M4 to switch the physical scoreboard to it. The M4 replies with the rig
// now active, which the picker then shows (see show_active_config()).
static void config_dropdown_event_cb(lv_event_t * e) {
    (void)e;
    if (config_dropdown == NULL) return;

    uint16_t selectedIndex = lv_dropdown_get_selected(config_dropdown);
    coreLinkSendInt(LINK_MSG_CONFIG, selectedIndex);
}

void show_active_config(int index) {
    if (config_dropdown == NULL) return;
    if (index < 0 || index >= SCOREBOARD_CONFIG_COUNT) return;
    lv_dropdown_set_selected(config_dropdown, (uint32_t)index); // doesn't fire VALUE_CHANGED
}

/* -------------------------------------------------------------
 * BLE SIGNAL CHART: the connected phone's signal strength (RSSI) over the
 * last minute, one point a second from the BLE thread (see show_ble_signal()).
 * ------------------------------------------------------------- */
#define SIGNAL_CARD_HEIGHT  140  // px, below the log in the right panel
#define SIGNAL_CHART_POINTS  60  // one a second: the last minute
#define SIGNAL_MIN_DBM     -100  // chart range; readings outside it are
#define SIGNAL_MAX_DBM      -40  //   drawn at the edge
#define SIGNAL_GRID_LINES     4  // at -100, -80, -60 and -40 dBm
#define SIGNAL_SCALE_WIDTH   44  // px, fits the "-100" label
#define SIGNAL_PLOT_PAD       8  // px above and below the plot: room for the end labels

static lv_obj_t *signal_chart = NULL;
static lv_chart_series_t *signal_series = NULL;
static lv_obj_t *lbl_signal_value = NULL;
static lv_obj_t *signal_offline_block = NULL; // red "BLE not connected" over the chart

// Shows the latest reading above the chart, with how good it is in words
// and colour -- or, with nothing connected, the red "BLE not connected"
// block over the chart instead.
static void show_signal_reading(int rssi) {
    bool connected = (rssi != BLE_RSSI_NONE);
    lv_obj_set_hidden(signal_offline_block, connected);
    if (!connected) {
        lv_label_set_text(lbl_signal_value, ""); // the block says it
        return;
    }

    // Tested with a phone: the connection held down to about -95 dBm and
    // dropped below it, so Weak means "may drop".
    const char *level = "Weak";  uint32_t color = 0xF44336;      // red
    if (rssi >= -60)      { level = "Strong"; color = 0x4CAF50; } // green
    else if (rssi >= -70) { level = "Good";   color = 0x8BC34A; } // light green
    else if (rssi >= -95) { level = "Fair";   color = 0xFFC107; } // amber

    lv_label_set_text_fmt(lbl_signal_value, "%d dBm  %s", rssi, level);
    lv_obj_set_style_text_color(lbl_signal_value, lv_color_hex(color), LV_PART_MAIN);
}

/* -------------------------------------------------------------
 * HELPER FUNCTION: CREATE A STYLIZED WHITE SCORE CARD
 * ------------------------------------------------------------- */
static lv_obj_t* create_card(lv_obj_t *parent, const char *heading, int col, int row) {
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
    lv_obj_set_style_text_font(lbl_head, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_head, LV_ALIGN_TOP_MID, 0, 0);

    // 4. Setup Dynamic Output Value Label (Large Black text centered).
    // 36px keeps the widest value (e.g. overs "12.3") inside a ~95px-wide card.
    lv_obj_t *lbl_val = lv_label_create(card);
    lv_label_set_text(lbl_val, "-");
    lv_obj_set_style_text_color(lbl_val, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_val, 2, LV_PART_MAIN);
    lv_obj_align(lbl_val, LV_ALIGN_CENTER, 0, 10);

    return lbl_val;
}

/* -------------------------------------------------------------
 * HELPER FUNCTION: CREATE THE BLE SIGNAL CARD
 * Heading and latest reading on top; below, the chart with a dBm scale.
 * ------------------------------------------------------------- */
static void create_signal_card(lv_obj_t *parent, int col, int row) {
    // 1. Black card, matching the log box above it
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(card, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(card, 0, LV_PART_MAIN);

    // Row 0: heading + reading. Row 1: scale + chart.
    static int32_t col_dsc[] = {SIGNAL_SCALE_WIDTH, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(card, col_dsc, row_dsc);
    lv_obj_set_layout(card, LV_LAYOUT_GRID);

    // 2. Heading (left) and latest reading (right), sharing the top row
    lv_obj_t *heading = lv_label_create(card);
    lv_label_set_text(heading, "BLE signal, last minute");
    lv_obj_set_style_text_color(heading, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_grid_cell(heading, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);

    lbl_signal_value = lv_label_create(card);
    lv_obj_set_grid_cell(lbl_signal_value, LV_GRID_ALIGN_END, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);

    // 3. dBm scale. Same vertical padding as the chart, so its ticks line up
    // with the chart's grid lines and the end labels fit inside.
    lv_obj_t *scale = lv_scale_create(card);
    lv_obj_set_grid_cell(scale, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_scale_set_mode(scale, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_scale_set_range(scale, SIGNAL_MIN_DBM, SIGNAL_MAX_DBM);
    lv_scale_set_total_tick_count(scale, SIGNAL_GRID_LINES);
    lv_scale_set_major_tick_every(scale, 1);
    lv_obj_set_style_pad_ver(scale, SIGNAL_PLOT_PAD, LV_PART_MAIN);
    lv_obj_set_style_line_color(scale, lv_color_hex(0x444444), LV_PART_MAIN);      // axis line
    lv_obj_set_style_line_color(scale, lv_color_hex(0x444444), LV_PART_INDICATOR); // ticks
    lv_obj_set_style_length(scale, 4, LV_PART_INDICATOR);
    lv_obj_set_style_pad_left(scale, 4, LV_PART_INDICATOR); // tick to label gap
    lv_obj_set_style_text_color(scale, lv_color_hex(0xAAAAAA), LV_PART_INDICATOR);

    // 4. The chart. The theme draws charts as white bordered cards; make it a
    // bare plot on the black card instead.
    signal_chart = lv_chart_create(card);
    lv_obj_set_grid_cell(signal_chart, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_opa(signal_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(signal_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(signal_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(signal_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(signal_chart, SIGNAL_PLOT_PAD, LV_PART_MAIN);
    lv_obj_set_style_line_color(signal_chart, lv_color_hex(0x444444), LV_PART_MAIN); // grid lines
    lv_obj_set_style_line_width(signal_chart, 2, LV_PART_ITEMS);  // the signal line
    lv_obj_set_style_size(signal_chart, 0, 0, LV_PART_INDICATOR);  // no dot on each point

    lv_chart_set_type(signal_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(signal_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(signal_chart, SIGNAL_CHART_POINTS);
    lv_chart_set_axis_range(signal_chart, LV_CHART_AXIS_PRIMARY_Y, SIGNAL_MIN_DBM, SIGNAL_MAX_DBM);
    lv_chart_set_div_line_count(signal_chart, SIGNAL_GRID_LINES, 0);
    // Starts with every point empty (LV_CHART_POINT_NONE): no line yet.
    signal_series = lv_chart_add_series(signal_chart, lv_color_hex(0x03989e), LV_CHART_AXIS_PRIMARY_Y);

    // 5. Red "BLE not connected" block over the scale and chart while nothing
    // is connected (see show_signal_reading()). Same grid cells, created
    // last, so it's drawn on top.
    signal_offline_block = lv_obj_create(card);
    lv_obj_set_grid_cell(signal_offline_block, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_color(signal_offline_block, lv_color_hex(0xD32F2F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(signal_offline_block, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(signal_offline_block, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(signal_offline_block, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(signal_offline_block, 0, LV_PART_MAIN);

    lv_obj_t *offline_text = lv_label_create(signal_offline_block);
    lv_label_set_text(offline_text, "BLE not connected");
    lv_obj_set_style_text_color(offline_text, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(offline_text, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(offline_text);

    show_signal_reading(BLE_RSSI_NONE); // nothing is connected yet
}

/* -------------------------------------------------------------
 * CORE SCOREBOARD UI GENERATOR
 * ------------------------------------------------------------- */
static void create_scoreboard_grid() {
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);

    build_config_options_text();
    config_dropdown = lv_dropdown_create(header);
    lv_dropdown_set_options(config_dropdown, config_options_buffer);
    // Shows the boot default until the M4 reports the actual active rig.
    lv_dropdown_set_selected(config_dropdown, (uint32_t)defaultScoreboardConfigIndex());
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

    // Card positions: {field, column, row}
    static const struct { ScoreFieldId field; int col; int row; } card_cells[FIELD_COUNT] = {
        {FIELD_BAT_A,   0, 0}, {FIELD_SCORE, 1, 0}, {FIELD_BAT_B,  2, 0},
        {FIELD_WICKETS, 0, 1}, {FIELD_OVERS, 1, 1}, {FIELD_TARGET, 2, 1},
    };
    for (int i = 0; i < FIELD_COUNT; i++) {
        ScoreFieldId field = card_cells[i].field;
        score_cards[field] = create_card(left_panel, SCORE_FIELDS[field].label,
                                         card_cells[i].col, card_cells[i].row);
    }

    /* -------------------------------------------------------------
     * RIGHT PANEL SUB-GRID: Sidebar Module (50% Width)
     * Rolling log on top, BLE signal chart below it.
     * ------------------------------------------------------------- */
    lv_obj_t * right_panel = lv_obj_create(main_container);
    lv_obj_set_grid_cell(right_panel, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(right_panel, 15, LV_PART_MAIN);

    static int32_t right_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t right_row_dsc[] = {LV_GRID_FR(1), SIGNAL_CARD_HEIGHT, LV_GRID_TEMPLATE_LAST};

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

    /* -------------------------------------------------------------
     * BLE SIGNAL CHART (below the log)
     * ------------------------------------------------------------- */
    create_signal_card(right_panel, 0, 1);
}

/* -------------------------------------------------------------
 * DYNAMIC ENGINE: ROLLING UPDATE BULLETED LIST CONTROLLER
 * ------------------------------------------------------------- */
void append_log_line(const char *new_value) {
    if (lbl_sidebar_log == NULL || new_value == NULL) return;

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
    lv_label_set_text(lbl_sidebar_log, combined_text_buffer);
}

/* -------------------------------------------------------------
 * SCORE MESSAGES FROM THE M4
 * ------------------------------------------------------------- */
void show_score_message(const char *message) {
    ScoreFieldId field = findScoreField(message);
    if (field == FIELD_COUNT || score_cards[field] == NULL) return;

    // Full value text, e.g. overs "12.3" (the physical board shows only "12").
    lv_label_set_text(score_cards[field], message + SCORE_CODE_LENGTH);
    // Log the raw message, e.g. "• BTR142".
    append_log_line(message);
}

/* -------------------------------------------------------------
 * BLE SIGNAL READINGS FROM THE BLE THREAD
 * ------------------------------------------------------------- */
void show_ble_signal(int rssi) {
    if (signal_chart == NULL) return;

    // A gap in the line while nothing is connected. Readings outside the
    // chart's range are drawn at its edge.
    int32_t point = (rssi == BLE_RSSI_NONE) ? LV_CHART_POINT_NONE
                                            : constrain(rssi, SIGNAL_MIN_DBM, SIGNAL_MAX_DBM);
    lv_chart_set_next_value(signal_chart, signal_series, point);
    show_signal_reading(rssi);
}

/* -------------------------------------------------------------
 * DISPLAY FLUSH: copies each chunk LVGL renders onto the screen
 *
 * Replaces Arduino_H7_Video's LVGL 9 flush callback, which reallocates its
 * rotation buffer on every flush (at twice the size it needs) without
 * checking the result. Once the heap is fragmented enough for that realloc
 * to fail, it leaks the old buffer and rotates into address 0: the screen
 * stops updating, and each later failure leaks another buffer until the
 * heap is full. (Seen a minute into scoring with a phone connected.) This
 * one rotates into a single fixed buffer instead.
 * ------------------------------------------------------------- */
// LVGL renders at most one draw buffer's worth per flush, and
// Arduino_H7_Video sizes that at width x height / 10 bytes.
static uint8_t rotated_buf[800 * 480 / 10];

static void display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    lv_color_format_t cf = lv_display_get_color_format(disp);
    uint32_t rotated_stride = lv_draw_buf_width_to_stride(h, cf);

    // Can't happen (see above), but never write past the buffer.
    if (rotated_stride * w > sizeof(rotated_buf)) {
        lv_display_flush_ready(disp);
        return;
    }

    // The panel is portrait (480 x 800) and the UI landscape, so every chunk
    // is rotated (Display.begin() sets LV_DISPLAY_ROTATION_270) and placed
    // where it lands on the panel -- the same mapping as Arduino_H7_Video.
    lv_draw_rotate(px_map, rotated_buf, w, h, lv_draw_buf_width_to_stride(w, cf), rotated_stride,
                   lv_display_get_rotation(disp), cf);
    int32_t panel_x = lv_display_get_vertical_resolution(disp) - area->y2 - 1;
    int32_t panel_y = area->x1;
    uint32_t offset = (panel_x + dsi_getDisplayXSize() * panel_y) * sizeof(uint16_t);

    // Copies with DMA2D and waits for it (cleaning the data cache first).
    dsi_lcdDrawImage(rotated_buf, (void *)(dsi_getActiveFrameBuffer() + offset), h, w, DMA2D_INPUT_RGB565);
    lv_display_flush_ready(disp);
}

/* -------------------------------------------------------------
MAIN INITIALIZATION ENTRY POINT
------------------------------------------------------------- */
void display_setup() {
    Display.begin();
    lv_display_set_flush_cb(lv_display_get_default(), display_flush); // see display_flush()
    TouchDetector.begin(); // must follow Display.begin() (which calls lv_init())
    create_scoreboard_grid();
}

