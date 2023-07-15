/*
Copyright 2022 Mentako_ya
Copyright 2022 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H
#include "quantum.h"
#include "keycode.h"

bool keybord_initialized = false;

enum custom_keycodes {
    KC_TO_MOUSE_LAYER = KEYBALL_SAFE_RANGE, // 0x5DA6
    KC_TO_CLICKABLE_INC,                    // 0x5DA7
    KC_TO_CLICKABLE_DEC,                    // 0x5DB2
    KC_F15_HYPHEN,                          // 0x5DB3
    KC_F16_US,
};

const uint16_t ignore_disable_mouse_layer_keys[] = {KC_LGUI,    KC_LCTL,     KC_MS_BTN1, KC_MS_BTN2, KC_MS_BTN3, KC_MS_BTN4, KC_MS_BTN5, KC_MS_UP, KC_MS_DOWN,
                                                    KC_MS_LEFT, KC_MS_RIGHT, KC_HOME,    KC_END,     KC_PGUP,    KC_PGDN,    KC_LEFT,    KC_RIGHT

}; // この配列で指定されたキーはマウスレイヤー中に押下してもマウスレイヤーを解除しない

#ifdef AUTO_MOUSE_LAYER_ENABLE

enum click_state {
    NONE = 0,
    WAITING,   // マウスレイヤーが有効になるのを待つ。 Wait for mouse layer to activate.
    CLICKABLE, // マウスレイヤー有効になりクリック入力が取れる。 Mouse layer is enabled to take click input.
};

typedef union {
    uint64_t raw;
    struct {                
        bool    tg_clickable_enabled;
        int16_t to_clickable_movement;
        int16_t to_reset_time;
    };
} user_config_t;

user_config_t user_config;

bool disable_layer = false;

enum click_state clstate;                    // 現在のクリック入力受付の状態 Current click input reception status
uint16_t         click_timer;                // タイマー。状態に応じて時間で判定する。 Timer. Time to determine the clstate of the system.
const uint16_t   normal_layer          = 0;  // 初期レイヤー　初期レイヤー以外でマウス操作した場合、レイヤーの自動切り替えをしない。
const uint16_t   click_layer           = 2;  // マウス入力が可能になった際に有効になるレイヤー。Layers enabled when mouse input is enabled
const int        to_clickable_movement = 60; // マウスレイヤーに移行するためのマウス移動量。 Mouse movement amount to switch to mouse layer.
int16_t          mouse_movement;

void eeconfig_init_user(void) {
    user_config.raw                   = 0;
    user_config.tg_clickable_enabled  = true;
    user_config.to_clickable_movement = 200;
    user_config.to_reset_time         = 20;
    eeconfig_update_user(user_config.raw);
}

void keyboard_post_init_user(void) {
    user_config.raw     = eeconfig_read_user();
    keybord_initialized = true;
}

// クリック用のレイヤーを有効にする。　Enable layers for clicks
void enable_click_layer(void) {
    layer_on(click_layer);
    click_timer = timer_read();
    clstate     = CLICKABLE;
}

// クリック用のレイヤーを無効にする。 Disable layers for clicks.
void disable_click_layer(void) {
    clstate = NONE;
    layer_off(click_layer);
}

// 自前の絶対数を返す関数。 Functions that return absolute numbers.
int16_t my_abs(int16_t num) {
    if (num < 0) {
        num = -num;
    }

    return num;
}

// 自前の符号を返す関数。 Function to return the sign.
int16_t mmouse_move_y_sign(int16_t num) {
    if (num < 0) {
        return -1;
    }

    return 1;
}

// 現在クリックが可能な状態か。 Is it currently clickable?
bool is_clickable_mode(void) {
    return clstate == CLICKABLE;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (disable_layer == true) {
        disable_click_layer();
        disable_layer = false;
        return true;
    }
    switch (keycode) {
        // static bool lshift = false;
        // static bool rshift = false;     
        case KC_TO_MOUSE_LAYER:
            if (record->event.pressed) {
                enable_click_layer();
            }
            return false;
    

        default:

            switch (clstate) {
                case CLICKABLE: {
                    bool is_mouse_key = false;
                    for (int i = 0; i < sizeof(ignore_disable_mouse_layer_keys) / sizeof(ignore_disable_mouse_layer_keys[0]); i++) {
                        if (keycode == ignore_disable_mouse_layer_keys[i]) {
                            is_mouse_key = true;
                            break;
                        }
                    }
                    click_timer = timer_read();
                    if (!is_mouse_key) {
                        // disable_click_layer();
                        disable_layer = true;
                    }
                    return true;
                    break;

                    default:
                        return true;
                }

                    return true;
            }
    }
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        switch (clstate) {
            case CLICKABLE:
                click_timer = timer_read();
                break;

            case WAITING:
                /*
                if (timer_elapsed(click_timer) > user_config.to_clickable_time) {
                    enable_click_layer();
                }
                */
                if (user_config.tg_clickable_enabled == false) {
                    // CLICKABLE無効
                    break;
                }

                if (normal_layer != get_highest_layer(layer_state)) {
                    // CLICKABLEになる前から初期レイヤー以外になっている場合、CLICKABLEにしない
                    break;
                }

                mouse_movement += my_abs(mouse_report.x) + my_abs(mouse_report.y);

                if (mouse_movement >= to_clickable_movement) {
                    mouse_movement = 0;
                    enable_click_layer();
                }
                break;

            default:
                click_timer    = timer_read();
                clstate        = WAITING;
                mouse_movement = 0;
        }
    } else {
        switch (clstate) {
            case CLICKABLE:
                if (timer_elapsed(click_timer) > (user_config.to_reset_time * 100)) {
                    disable_click_layer();
                }
                break;

            case WAITING:
                if (timer_elapsed(click_timer) > 50) {
                    mouse_movement = 0;
                    clstate        = NONE;
                }
                break;

            default:
                mouse_movement = 0;
                clstate        = NONE;
        }
    }

    return mouse_report;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);

    // CLICKABLEな状態でレイヤー切替が発生した場合
    if (clstate == CLICKABLE) {
        // clstateをNONEにしておくことで、意図せずマウスレイヤーが解除されないようにする
        clstate = NONE;
        if ((state & ~(0b0001 << click_layer)) != 0b0000) {
            // マウスレイヤー以外が有効な場合、マウスレイヤーを無効化
            state &= ~(0b0001 << click_layer);
        }
    }

    return state;
}

#endif

#ifndef AUTO_MOUSE_LAYER_ENABLE
void keyboard_post_init_user(void) {
    keybord_initialized = true;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);
    return state;
}
#endif

#ifdef OLED_ENABLE

#    include "lib/oledkit/oledkit.h"

// static const char PROGMEM raw_logo[] = {
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0, 0, 0, 0, 0,  0,   0,   0,   0,   0,  0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0,  0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0, 0, 0, 0, 0,   0,   0, 0, 0, 0, 0, 0, 0, 0,   0,   0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,   0, 0, 0, 0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  128, 192, 192, 192, 192, 128, 0,  0,   0,   0,   128, 128, 0,   0,   0,   0,   0,   0,   0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 30, 112, 192, 0, 0, 0, 0, 0, 0, 0, 0, 192, 112, 28, 7, 255, 0, 0, 0, 0, 0, 0, 128, 128, 192, 64, 64, 64, 64, 192, 128, 0, 0, 0, 0, 0, 0, 192, 192, 0,  0,  0, 0, 0, 0, 0,  192, 192, 0, 0, 0, 0, 0, 128, 192, 64, 64, 64, 64, 64, 192, 192, 0, 0, 0, 0, 0, 128, 192, 192, 64, 64, 64, 192, 192, 128, 0, 0, 0, 0, 0, 0, 0, 207, 255, 249, 248, 249, 125, 127, 255, 254, 255, 231, 195, 199, 255, 255, 126, 0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 1, 7, 60, 240, 192, 192, 112, 28, 7, 1, 0, 0, 0, 255, 0, 0, 0, 0, 48, 254, 3, 1, 0, 0, 0, 0, 0, 0, 1, 207, 254, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 2, 15, 24, 24, 16, 48, 48, 32, 96, 192, 0, 0, 0, 48, 254, 19, 16, 16, 16, 16, 16, 16, 17, 23, 30, 0, 0, 0, 0, 0, 31, 31, 31,  31,  239, 127, 126, 126, 63, 255, 255, 255, 255, 255, 255, 255, 254, 252, 240, 192, 192, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15,  15,  0,  0,   0,   0, 0, 0, 1, 1, 0, 0, 0, 0,   0,   0,  0, 15,  0, 0, 0, 0, 0, 1, 7,   14,  12,  8,  8,  8,  8,  12,  6,   3, 1, 0, 0, 0, 0, 1,   7,   14, 12, 8, 8, 8, 8, 12, 7,   15,  0, 0, 0, 0, 4, 12,  12,  8,  8,  8,  8,  8,  12,  7,   1, 0, 0, 0, 1, 7,   6,   12,  8,  8,  8,  8,   8,   12,  4, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   22,  23,  55,  33,  44,  126, 110, 111, 103, 103, 51,  51, 49, 24, 31, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
// };

// void oledkit_render_info_user(void) {
//     if (get_highest_layer(layer_state) == 2) {
//         oled_write_raw_P(raw_logo, sizeof(raw_logo));
//     } else {
//         oled_clear();
//     }
// }
#endif
// clang-format off





// combo
enum combos { 
  D_F_COMMA,
  J_K_DOT,
  Q_W_TAB,
  K_L_BSPC,
  C_V_UNDS,
  M_COMMA_MINUS,
  
};

const uint16_t PROGMEM d_f_comma[] = { KC_D, KC_F, COMBO_END};
const uint16_t PROGMEM j_k_dot[] = { KC_J, KC_K, COMBO_END};
const uint16_t PROGMEM q_w_tab[] = { KC_Q, KC_W, COMBO_END};
const uint16_t PROGMEM k_l_bspc[] = { KC_K, KC_L, COMBO_END};
const uint16_t PROGMEM c_v_unds[] = { KC_C, KC_V, COMBO_END};
const uint16_t PROGMEM m_comma_minus[] = { KC_M, KC_COMMA, COMBO_END};

combo_t key_combos[COMBO_COUNT] = {
  [D_F_COMMA] = COMBO(d_f_comma, KC_COMMA),
  [J_K_DOT] = COMBO(j_k_dot, KC_DOT),
  [Q_W_TAB] = COMBO(q_w_tab, KC_TAB),
  [K_L_BSPC] = COMBO(k_l_bspc, KC_BSPC),
  [C_V_UNDS] = COMBO(c_v_unds, KC_UNDS),
  [M_COMMA_MINUS] = COMBO(m_comma_minus, KC_MINUS),
  
};
// tap dance

enum TapDanceDeclarations { 
    TD_LBRC = 0,
    TD_RBRC,
    TD_LPRN,
    TD_RPRN,
    TD_F2,
    TD_F12,
    TD_ESC,
    TD_ONE,
    TD_TWO,
    TD_THREE,
    TD_FOUR,
    TD_FIVE,

 };

qk_tap_dance_action_t tap_dance_actions[] = {[TD_LBRC] = ACTION_TAP_DANCE_DOUBLE(KC_LBRC, KC_LCBR),
                                            [TD_RBRC] = ACTION_TAP_DANCE_DOUBLE(KC_RBRC, KC_RCBR),
                                            [TD_LPRN] = ACTION_TAP_DANCE_DOUBLE(KC_LEFT_PAREN, KC_LEFT_ANGLE_BRACKET),
                                            [TD_RPRN] = ACTION_TAP_DANCE_DOUBLE(KC_RIGHT_PAREN, KC_RIGHT_ANGLE_BRACKET),
                                            [TD_F2] = ACTION_TAP_DANCE_DOUBLE(KC_F2, KC_F5),
                                            [TD_F12] = ACTION_TAP_DANCE_DOUBLE(KC_F12, KC_F4),
                                            [TD_ESC] = ACTION_TAP_DANCE_DOUBLE(KC_ESC, LCAG(KC_ESC)),
                                            [TD_ONE] = ACTION_TAP_DANCE_DOUBLE(KC_1, LCAG(KC_F1)),
                                            [TD_TWO] = ACTION_TAP_DANCE_DOUBLE(KC_2, LCAG(KC_F2)),
                                            [TD_THREE] = ACTION_TAP_DANCE_DOUBLE(KC_3, LCAG(KC_F3)),
                                            [TD_FOUR] = ACTION_TAP_DANCE_DOUBLE(KC_4, LCAG(KC_F4)),
                                            [TD_FIVE] = ACTION_TAP_DANCE_DOUBLE(KC_5, LCAG(KC_F5)),
                                           };



const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  LAYOUT_universal(
    TD(TD_ESC)   , TD(TD_ONE)   , TD(TD_TWO)   , TD(TD_THREE) , TD(TD_FOUR)  , TD(TD_FIVE)  ,                                  KC_6     , KC_7     , KC_8     , KC_9     , KC_0     , KC_MINS  ,
	KC_TAB   , KC_Q     , KC_W     , KC_E     , KC_R     , KC_T     ,                                  KC_Y     , KC_U     , KC_I     , KC_O     , KC_P     , KC_LBRC  ,
	KC_LCTL  , KC_A     , KC_S     , KC_D     , KC_F     , KC_G     ,                                  KC_H     , KC_J     , KC_K     , KC_L     , KC_SCLN  , KC_QUOTE ,
    KC_LEFT_SHIFT, KC_Z     , KC_X     , KC_C     , KC_V     , KC_B     , KC_RBRC  ,              KC_NUHS, KC_N     , KC_M     , KC_COMM  , KC_TO_MOUSE_LAYER   , KC_SLSH  , KC_RO    ,
	KC_LALT  , KC_LGUI  ,                  LT(1,KC_LANG2),LT(2,KC_SPC),                                KC_BSPC  , KC_ENT   ,                       KC_EQL   , KC_JYEN  ,
                          TG(2)    ,            KC_ENT   ,       LT(3,KC_LANG1),             KC_LGUI ,            KC_ESC   ,            KC_ENT   ,
					 KC_LEFT , KC_RGHT ,      KC_UP,KC_DOWN,           G(KC_C) ,             G(KC_V) ,     SGUI(KC_Z),LGUI(KC_Z),  S(KC_TAB),KC_TAB
  ),

  LAYOUT_universal(
    S(KC_ESC), S(KC_1)  , KC_LBRC  , S(KC_3)  , S(KC_4)  , S(KC_5)  ,                                  S(KC_6)  , S(KC_7)  , S(KC_8)  , S(KC_9)  , S(KC_0)  ,S(KC_INT1),
    S(KC_TAB), S(KC_Q)  , S(KC_W)  , S(KC_E)  , S(KC_R)  , S(KC_T)  ,                                  S(KC_Y)  , S(KC_U)  , S(KC_I)  , S(KC_O)  , S(KC_P)  ,S(KC_LBRC),
    S(KC_LCTL),S(KC_A)  , S(KC_S)  , S(KC_D)  , S(KC_F)  , TD(TD_LPRN)  ,                                  TD(TD_RPRN)  , S(KC_J)  , S(KC_K)  , S(KC_L)  ,S(KC_SCLN),S(KC_QUOTE),
    _______  , S(KC_Z)  , S(KC_X)  , S(KC_C)  , S(KC_V)  , S(KC_B)  ,S(KC_RBRC),           S(KC_NUHS), S(KC_N)  , S(KC_M)  ,S(KC_COMM), S(KC_DOT),S(KC_SLSH),S(KC_RO)  ,
    S(KC_LALT),S(KC_LGUI),                      _______  , _______  ,                                  _______  , _______  ,                      S(KC_EQL) ,S(KC_JYEN),
                          _______  ,            _______  ,             _______,              _______ ,            _______  ,             _______ ,
                      _______,_______,      _______,_______,           KC_F16_US,              _______ ,        _______,_______,       _______,_______
  ),

  LAYOUT_universal(
    _______  , KC_F1    , KC_F2    , KC_F3    , KC_F4    , KC_F5    ,                                  KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
    _______  , _______  , KC_7     , KC_8     , KC_9     , _______  ,                                  _______  , KC_PGUP  , KC_UP    , KC_PGDN  , _______  , KC_F12   ,
    _______  , _______  , KC_4     , KC_5     , KC_6     ,S(KC_SCLN),                                  KC_HOME  , KC_LEFT  , KC_DOWN  , KC_RGHT  , KC_END  , _______  ,
    _______  , _______  , KC_1     , KC_2     , KC_3     ,S(KC_MINS), S(KC_8)  ,            KC_BTN4  , KC_BTN5  , KC_BTN1  , KC_BTN2  , KC_BTN3  , _______  , _______  ,
    _______  , _______  ,                       _______  , _______  ,                                  _______  , _______  ,                       _______  , _______  ,
	                      _______  ,            _______  ,             _______,              _______ ,            _______  ,             _______ ,
				    S(KC_TAB),KC_TAB ,      _______,_______,           TD(TD_F2),              TD(TD_F12) ,        _______,_______,       _______,_______
  ),

  LAYOUT_universal(
    RGB_TOG  , _______  , _______  , _______  , _______  , _______  ,                                  RGB_M_P  , RGB_M_B  , RGB_M_R  , RGB_M_SW , RGB_M_SN , RGB_M_K  ,
    RGB_MOD  , RGB_HUI  , RGB_SAI  , RGB_VAI  , _______  , _______  ,                                  RGB_M_X  , RGB_M_G  , RGB_M_T  , RGB_M_TW , _______  , _______  ,
    RGB_RMOD , RGB_HUD  , RGB_SAD  , RGB_VAD  , SCRL_MO  , SCRL_TO  ,                                  CPI_D1K  , CPI_D100 , CPI_I100 , CPI_I1K  , KBC_SAVE , KBC_RST  ,
	_______ , _______ ,
	_______     , _______     , SCRL_DVD , SCRL_DVI , EEP_RST ,              _______ , _______  , _______  , _______  , _______  , _______  , _______  ,
	RESET    , KC_TO_MOUSE_LAYER ,                _______  , _______  ,                                  _______  , _______  ,                       _______  , _______  ,
                          _______  ,            _______  ,             _______,              _______ ,            _______  ,             _______ ,
                      _______,_______,      _______,_______,           _______,              _______ ,        _______,_______,       _______,_______
  ),

  
  LAYOUT_universal(
    RGB_TOG  , _______  , _______  , _______  , _______  , _______  ,                                  RGB_M_P  , RGB_M_B  , RGB_M_R  , RGB_M_SW , RGB_M_SN , RGB_M_K  ,
    RGB_MOD  , RGB_HUI  , RGB_SAI  , RGB_VAI  , _______  , _______  ,                                  RGB_M_X  , RGB_M_G  , RGB_M_T  , RGB_M_TW , _______  , _______  ,
    RGB_RMOD , RGB_HUD  , RGB_SAD  , RGB_VAD  , SCRL_MO  , SCRL_TO  ,                                  CPI_D1K  , CPI_D100 , CPI_I100 , CPI_I1K  , KBC_SAVE , KBC_RST  ,
	_______ , _______ ,
	_______     , _______     , SCRL_DVD , SCRL_DVI , EEP_RST ,              EEP_RST , _______  , _______  , _______  , _______  , _______  , _______  ,
	RESET    , _______ ,                _______  , _______  ,                                  _______  , _______  ,                       _______  , _______  ,
                          _______  ,            _______  ,             _______,              _______ ,            _______  ,             _______ ,
                      _______,_______,      _______,KC_TO_MOUSE_LAYER,           TD(TD_LBRC),          TD(TD_RBRC) ,        _______,_______,       _______,_______
  ),

};
// clang-format on

// encoder logic
#ifdef ENCODER_ENABLE

enum encoder_number {
    _1ST_ENC = 0,
    _2ND_ENC,
    _3RD_ENC,
    _4TH_ENC,
};

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (keybord_initialized != true) {
        return true;
    }

    keypos_t key;

    switch (index) {
        case _1ST_ENC:
            if (clockwise) {
                key.row = 5;
                key.col = 2;
            } else {
                key.row = 5;
                key.col = 3;
            }
            break;
        case _2ND_ENC:
            if (clockwise) {
                key.row = 5;
                key.col = 0;
            } else {
                key.row = 5;
                key.col = 1;
            }
            break;
        case _3RD_ENC:
            if (clockwise) {
                key.row = 11;
                key.col = 2;
            } else {
                key.row = 11;
                key.col = 3;
            }
            break;
        case _4TH_ENC:
            if (clockwise) {
                key.row = 11;
                key.col = 0;
            } else {
                key.row = 11;
                key.col = 1;
            }
            break;
    }
    uint8_t  layer   = layer_switch_get_layer(key);
    uint16_t keycode = keymap_key_to_keycode(layer, key);

    tap_code16(keycode);

#    ifdef CONSOLE_ENABLE
    uprintf("encoder_update_user: index: %u, clockwise: %u \n", index, clockwise);
#    endif

    return true;
}
#endif
