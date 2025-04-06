#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <math.h>

#define CUSTOM_SPLASH_WIDTH 62
#define CUSTOM_SPLASH_HEIGHT 25
#define SPLASH_HEIGHT CUSTOM_SPLASH_HEIGHT
#define SPLASH_WIDTH CUSTOM_SPLASH_WIDTH

#define M_PI 3.14159265358979323846

typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_NONE
} Direction;

typedef struct {
    char* name;
    Direction sequence[10];
    uint8_t length;
} Stratagem;

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAY,
    GAME_STATE_GAME_OVER,
    GAME_STATE_STRATAGEM_SUCCESS
} GameState;

#define ARROW_HEAD_SIZE 4
#define ARROW_TAIL_SIZE 4

#define MAX_STARS 30
#define MAX_PLANETS 3

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t brightness;
    uint8_t blink_rate;
} Star;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t size;
    uint8_t ship_x;
    uint8_t ship_y;
} Planet;

typedef struct {
    int16_t shake_offset_x;
    int16_t shake_offset_y;
    uint8_t shake_duration;
} ScreenShake;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t depth;
    uint8_t animation_stage;
    uint8_t duration;
} StratagemSuccess;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    
    NotificationApp* notifications;
    
    bool exit_requested;
    
    GameState state;
    
    Stratagem* stratagems;
    uint8_t stratagem_count;
    uint8_t current_stratagem_index;
    uint8_t current_input_index;
    uint8_t lives;
    uint32_t score;
    uint32_t high_score;
    
    FuriTimer* timer;
    uint32_t time_remaining;
    
    uint8_t animation_frame;
    FuriTimer* animation_timer;
    
    bool last_input_success;
    
    bool current_input_correct;
    uint32_t feedback_timer;
    
    int8_t scroll_offset;
    
    Star stars[MAX_STARS];
    Planet planets[MAX_PLANETS];
    
    ScreenShake screen_shake;
    StratagemSuccess success_anim;
    
    uint8_t custom_splash[((CUSTOM_SPLASH_WIDTH + 7) / 8) * CUSTOM_SPLASH_HEIGHT];
} StratagemHeroApp;

const Stratagem STRATAGEMS[] = {
    // Основные стратагемы
    {
        "Resupply", 
        {DIRECTION_DOWN, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT}, 
        4
    },
    {
        "Reinforce", 
        {DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT, DIRECTION_LEFT, DIRECTION_UP}, 
        5
    },
    {
        "SOS Beacon", 
        {DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT, DIRECTION_LEFT}, 
        4
    },
    
    // Орбитальные удары
    {
        "Orbital Strike", 
        {DIRECTION_RIGHT, DIRECTION_RIGHT, DIRECTION_RIGHT}, 
        3
    },
    {
        "Orbital Precision Strike", 
        {DIRECTION_RIGHT, DIRECTION_RIGHT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT}, 
        5
    },
    {
        "Orbital Gatling Barrage", 
        {DIRECTION_RIGHT, DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_UP, DIRECTION_RIGHT}, 
        5
    },
    
    // Оружие поддержки
    {
        "Machine Gun", 
        {DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_RIGHT, DIRECTION_UP}, 
        4
    },
    {
        "Anti-Materiel Rifle", 
        {DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_RIGHT, DIRECTION_UP, DIRECTION_DOWN}, 
        5
    },
    {
        "Stalwart", 
        {DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT}, 
        5
    },
    
    // Оборона
    {
        "Shield Generator", 
        {DIRECTION_DOWN, DIRECTION_UP, DIRECTION_LEFT, DIRECTION_RIGHT, DIRECTION_LEFT}, 
        5
    },
    {
        "Tesla Tower", 
        {DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT, DIRECTION_DOWN, DIRECTION_LEFT}, 
        5
    },
    
    // Специальные
    {
        "Jump Pack", 
        {DIRECTION_DOWN, DIRECTION_UP, DIRECTION_UP, DIRECTION_DOWN}, 
        4
    },
    {
        "HMG Emplacement", 
        {DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_UP, DIRECTION_RIGHT, DIRECTION_DOWN}, 
        5
    },
    {
        "Eagle Strafing Run", 
        {DIRECTION_UP, DIRECTION_RIGHT, DIRECTION_UP}, 
        3
    },
    {
        "Eagle Airstrike", 
        {DIRECTION_UP, DIRECTION_RIGHT, DIRECTION_DOWN, DIRECTION_RIGHT}, 
        4
    }
};

#define STRATAGEM_COUNT (sizeof(STRATAGEMS) / sizeof(STRATAGEMS[0]))

#define INITIAL_TIME 10000
#define TIME_DECREASE 2000
#define MIN_TIME 3000
#define TIME_BONUS 2000
#define INITIAL_LIVES 3
#define MAX_LEVEL 50

static const uint8_t custom_splash[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xA8, 0xBB, 0x8D, 0x51, 0x1C, 0x93, 0xFC,
    0xFF, 0xAB, 0xBB, 0xB5, 0x57, 0x6B, 0xD7, 0xFC,
    0xFF, 0x88, 0xBB, 0xB5, 0x51, 0x68, 0xD7, 0xFC,
    0xFF, 0xAB, 0xBB, 0xB5, 0xB7, 0x1E, 0xD7, 0xFC,
    0xFF, 0xA8, 0x88, 0x8D, 0xB1, 0x69, 0x93, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C,
    0xF8, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x7C,
    0xFE, 0x00, 0x3F, 0xFF, 0xFF, 0xF8, 0x00, 0xFC,
    0xFF, 0xFE, 0x00, 0x03, 0x80, 0x00, 0xFF, 0xFC,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xF8, 0x3F, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFA, 0xBF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xF8, 0x3F, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFC, 0x7F, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};

static const NotificationSequence sequence_correct = {
    &message_vibro_on,
    &message_blue_255,
    &message_note_c5,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

static const NotificationSequence sequence_wrong = {
    &message_vibro_on,
    &message_red_255,
    &message_note_g4,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_delay_50,
    &message_vibro_on,
    &message_red_255,
    &message_note_e4,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

static const NotificationSequence sequence_game_over = {
    &message_vibro_on,
    &message_red_255,
    &message_note_c4,
    &message_delay_100,
    &message_note_b3,
    &message_delay_100,
    &message_note_a3,
    &message_delay_100,
    &message_note_g3,
    &message_delay_500,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

static const NotificationSequence sequence_navigate = {
    &message_blue_255,
    &message_note_e5,
    &message_delay_10,
    &message_blue_0,
    NULL,
};

static const NotificationSequence sequence_welcome_midi = {
    // Паттерн 2-5
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5 
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    &message_note_a5,  // Нота 5
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5
    &message_delay_50,
    
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    // Паттерн 2-4
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_g5,  // Нота 4
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    &message_note_g5,  // Нота 4
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_g5,  // Нота 4
    &message_delay_50,
    
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_g5,  // Нота 4
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    // Возврат к паттерну 2-5
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    &message_note_a5,  // Нота 5
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5
    &message_delay_50,
    
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_a5,  // Нота 5
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    // Паттерн 2-7
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_b5,  // Нота 7
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    &message_note_b5,  // Нота 7
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_b5,  // Нота 7
    &message_delay_50,
    
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_b5,  // Нота 7
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    // Паттерн 2-9
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_d6,  // Нота 9
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    &message_note_d6,  // Нота 9
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_d6,  // Нота 9
    &message_delay_50,
    
    &message_note_d5,  // Нота 2
    &message_delay_50,
    &message_note_d6,  // Нота 9
    &message_delay_50,
    &message_note_d5,  // Нота 2
    &message_delay_50,
    
    // Переход к более высоким нотам (A-J-M)
    &message_note_e6,  // Нота A (14)
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    
    // Финальная часть MIDI
    &message_note_e6,  // Нота A
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    
    &message_note_e6,  // Нота @
    &message_delay_50,
    &message_note_e6,  // Нота @
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    &message_note_e6,  // Нота A
    &message_delay_50,
    
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_f6,  // Нота C
    &message_delay_50,
    
    &message_note_f6,  // Нота C
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    &message_note_c6,  // Нота >
    &message_delay_50,
    
    // Завершающие ноты (J-M-J-L-J-M...)
    &message_note_f6, // Нота J (вместо f#6)
    &message_delay_50,
    &message_note_g6,  // Нота M
    &message_delay_50,
    &message_note_f6, // Нота J (вместо f#6)
    &message_delay_50,
    
    &message_note_g6,  // Нота L
    &message_delay_50,
    &message_note_f6, // Нота J (вместо f#6)
    &message_delay_50,
    &message_note_g6,  // Нота M
    &message_delay_50,
    
    &message_note_f6, // Нота J (вместо f#6)
    &message_delay_50,
    &message_note_g6,  // Нота Q
    &message_delay_50,
    &message_note_f6,  // Нота F
    &message_delay_50,
    
    // Продолжительные ноты из конца MIDI
    &message_sound_off, // Выключаем предыдущий звук чтобы избежать наложения
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    
    // A#5 (ближайшая доступная - a5 или b5)
    &message_note_b5, // Используем b5 как ближайшую к A#5
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    
    // F5
    &message_note_f5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    
    // E5
    &message_note_e5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    
    // D5
    &message_note_d5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    
    // A4
    &message_note_a4,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_100,
    &message_delay_100,
    
    // C5
    &message_note_c5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,
    
    // D5 (финальная нота)
    &message_note_d5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100, // Увеличиваем длительность финальной ноты
    
    &message_sound_off, // Важно выключить звук в конце последовательности
    NULL,
};

static const NotificationSequence sequence_level_complete = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_100,
    NULL,
};

static void init_stars(StratagemHeroApp* app) {
    if (!app) return;
    
    for (int i = 0; i < MAX_STARS; i++) {
        app->stars[i].x = rand() % 128;
        app->stars[i].y = rand() % 64;
        app->stars[i].brightness = rand() % 3;
        app->stars[i].blink_rate = rand() % 5 + 1;
    }
}

static void init_planets(StratagemHeroApp* app) {
    if (!app) return;
    
    for (int i = 0; i < MAX_PLANETS; i++) {
        app->planets[i].x = 20 + (rand() % 88);
        app->planets[i].y = 15 + (rand() % 25);
        app->planets[i].size = 4 + (rand() % 5);
        // Ensure ship coordinates are valid
        int range = 5;
        app->planets[i].ship_x = app->planets[i].x + (rand() % (2*range + 1)) - range;
        app->planets[i].ship_y = app->planets[i].y + (rand() % (2*range + 1)) - range;
        
        // Ensure they're within screen boundaries
        if (app->planets[i].ship_x >= 128) app->planets[i].ship_x = 127;
        if (app->planets[i].ship_y >= 64) app->planets[i].ship_y = 63;
    }
}

static void timer_callback(void* context) {
    StratagemHeroApp* app = (StratagemHeroApp*)context;
    
    if(app->state != GAME_STATE_PLAY && app->state != GAME_STATE_STRATAGEM_SUCCESS) {
        return;
    }
    
    if(app->time_remaining <= 100) {
        app->lives--;
        notification_message(app->notifications, &sequence_wrong);
        
        if(app->lives <= 0) {
            app->state = GAME_STATE_GAME_OVER;
            notification_message(app->notifications, &sequence_game_over);
        } else {
            app->current_input_index = 0;
            app->current_stratagem_index = rand() % STRATAGEM_COUNT;
            app->time_remaining = INITIAL_TIME;
            app->current_input_correct = true;
        }
    } else {
        app->time_remaining -= 100;
    }
}

static void animation_timer_callback(void* context) {
    StratagemHeroApp* app = (StratagemHeroApp*)context;
    app->animation_frame = (app->animation_frame + 1) % 4;
    
    if(app->state == GAME_STATE_MENU) {
        app->scroll_offset = (app->scroll_offset + 1) % 16;
    }
    
    if(app->feedback_timer > 0) {
        app->feedback_timer -= 50;
    }
    
    if(app->screen_shake.shake_duration > 0) {
        app->screen_shake.shake_duration--;
        app->screen_shake.shake_offset_x = (rand() % 5) - 2;
        app->screen_shake.shake_offset_y = (rand() % 5) - 2;
    } else {
        app->screen_shake.shake_offset_x = 0;
        app->screen_shake.shake_offset_y = 0;
    }
    
    if(app->state == GAME_STATE_STRATAGEM_SUCCESS) {
        if(app->success_anim.duration > 0) {
            app->success_anim.duration--;
            if(app->success_anim.animation_stage < 5) {
                app->success_anim.animation_stage++;
            }
        } else {
            app->state = GAME_STATE_PLAY;
            app->success_anim.animation_stage = 0;
        }
    }
}

static void draw_star(Canvas* canvas, Star* star, uint8_t anim_frame, int8_t offset_x, int8_t offset_y) {
    if (!canvas || !star) return;
    
    bool should_draw = true;
    
    if (star->blink_rate > 0) {
        should_draw = ((anim_frame / star->blink_rate) % 2 == 0);
    }
    
    if (should_draw) {
        // Make sure coordinates are within screen bounds
        uint8_t x = star->x + offset_x;
        uint8_t y = star->y + offset_y;
        
        if (x >= 128 || y >= 64) return;
        
        if (star->brightness == 0) {
            canvas_draw_dot(canvas, x, y);
        } else if (star->brightness == 1) {
            canvas_draw_dot(canvas, x, y);
            if (x + 1 < 128) canvas_draw_dot(canvas, x + 1, y);
            if (y + 1 < 64) canvas_draw_dot(canvas, x, y + 1);
        } else {
            if (x > 0) canvas_draw_dot(canvas, x - 1, y);
            if (x + 1 < 128) canvas_draw_dot(canvas, x + 1, y);
            if (y > 0) canvas_draw_dot(canvas, x, y - 1);
            if (y + 1 < 64) canvas_draw_dot(canvas, x, y + 1);
            canvas_draw_dot(canvas, x, y);
        }
    }
}

static void draw_planet(Canvas* canvas, Planet* planet, int8_t offset_x, int8_t offset_y) {
    if (!canvas || !planet) return;
    
    // Ensure coordinates are within bounds
    uint8_t x = planet->x + offset_x;
    uint8_t y = planet->y + offset_y;
    
    if (x >= 128 || y >= 64) return;
    
    canvas_draw_circle(canvas, x, y, planet->size);
    
    uint8_t ring_size = planet->size + 2;
    if (rand() % 3 == 0) {
        canvas_draw_circle(canvas, x, y, ring_size);
    }
    
    uint8_t ship_x = planet->ship_x + offset_x;
    uint8_t ship_y = planet->ship_y + offset_y;
    
    if (ship_x >= 128 || ship_y >= 64) return;
    
    if (ship_x >= 2 && ship_x + 2 < 128) {
        canvas_draw_line(canvas, 
                        ship_x - 2, ship_y, 
                        ship_x + 2, ship_y);
    }
    
    if (ship_y >= 1 && ship_y + 1 < 64) {
        canvas_draw_line(canvas, 
                        ship_x, ship_y - 1, 
                        ship_x, ship_y + 1);
    }
}

static void draw_space_background(Canvas* canvas, StratagemHeroApp* app) {
    int8_t offset_x = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_x : 0;
    int8_t offset_y = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_y : 0;
    
    for (int i = 0; i < MAX_STARS; i++) {
        draw_star(canvas, &app->stars[i], app->animation_frame, offset_x, offset_y);
    }
    
    for (int i = 0; i < MAX_PLANETS; i++) {
        draw_planet(canvas, &app->planets[i], offset_x, offset_y);
    }
}

static void draw_arrow_bitmap(Canvas* canvas, Direction dir, uint8_t x, uint8_t y, bool filled, int8_t offset_x, int8_t offset_y) {
    uint8_t center_x = x + offset_x;
    uint8_t center_y = y + offset_y;
    
    switch(dir) {
        case DIRECTION_UP:
            canvas_draw_line(canvas, center_x, center_y - ARROW_HEAD_SIZE, center_x - ARROW_HEAD_SIZE, center_y);
            canvas_draw_line(canvas, center_x, center_y - ARROW_HEAD_SIZE, center_x + ARROW_HEAD_SIZE, center_y);
            
            if(filled) {
                for(int j = 1; j < ARROW_HEAD_SIZE; j++) {
                    canvas_draw_line(canvas, center_x - j, center_y - ARROW_HEAD_SIZE + j, center_x + j, center_y - ARROW_HEAD_SIZE + j);
                }
            }
            
            canvas_draw_line(canvas, center_x, center_y, center_x, center_y + ARROW_TAIL_SIZE);
            break;
            
        case DIRECTION_DOWN:
            canvas_draw_line(canvas, center_x, center_y + ARROW_HEAD_SIZE, center_x - ARROW_HEAD_SIZE, center_y);
            canvas_draw_line(canvas, center_x, center_y + ARROW_HEAD_SIZE, center_x + ARROW_HEAD_SIZE, center_y);
            
            if(filled) {
                for(int j = 1; j < ARROW_HEAD_SIZE; j++) {
                    canvas_draw_line(canvas, center_x - j, center_y + ARROW_HEAD_SIZE - j, center_x + j, center_y + ARROW_HEAD_SIZE - j);
                }
            }
            
            canvas_draw_line(canvas, center_x, center_y, center_x, center_y - ARROW_TAIL_SIZE);
            break;
            
        case DIRECTION_LEFT:
            canvas_draw_line(canvas, center_x - ARROW_HEAD_SIZE, center_y, center_x, center_y - ARROW_HEAD_SIZE);
            canvas_draw_line(canvas, center_x - ARROW_HEAD_SIZE, center_y, center_x, center_y + ARROW_HEAD_SIZE);
            
            if(filled) {
                for(int j = 1; j < ARROW_HEAD_SIZE; j++) {
                    canvas_draw_line(canvas, center_x - ARROW_HEAD_SIZE + j, center_y - j, center_x - ARROW_HEAD_SIZE + j, center_y + j);
                }
            }
            
            canvas_draw_line(canvas, center_x, center_y, center_x + ARROW_TAIL_SIZE, center_y);
            break;
            
        case DIRECTION_RIGHT:
            canvas_draw_line(canvas, center_x + ARROW_HEAD_SIZE, center_y, center_x, center_y - ARROW_HEAD_SIZE);
            canvas_draw_line(canvas, center_x + ARROW_HEAD_SIZE, center_y, center_x, center_y + ARROW_HEAD_SIZE);
            
            if(filled) {
                for(int j = 1; j < ARROW_HEAD_SIZE; j++) {
                    canvas_draw_line(canvas, center_x + ARROW_HEAD_SIZE - j, center_y - j, center_x + ARROW_HEAD_SIZE - j, center_y + j);
                }
            }
            
            canvas_draw_line(canvas, center_x, center_y, center_x - ARROW_TAIL_SIZE, center_y);
            break;
            
        default:
            break;
    }
}

static void draw_stratagem_success_animation(Canvas* canvas, StratagemHeroApp* app) {
    int8_t offset_x = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_x : 0;
    int8_t offset_y = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_y : 0;
    
    uint8_t x = app->success_anim.x + offset_x;
    uint8_t y = app->success_anim.y + offset_y;
    uint8_t stage = app->success_anim.animation_stage;
    
    canvas_set_color(canvas, ColorBlack);
    
    // Always draw ground/platform
    canvas_draw_line(canvas, x - 20, y + 20, x + 20, y + 20);
    
    // Only draw capsule and effects during animation
    if(app->state == GAME_STATE_STRATAGEM_SUCCESS) {
        if (stage >= 1) {
            uint8_t capsule_length = 10;
            uint8_t capsule_width = 5;
            uint8_t impact_y = y + 10 + stage * 2;
            
            canvas_draw_circle(canvas, x, impact_y - capsule_length/2, capsule_width/2);
            canvas_draw_box(canvas, x - capsule_width/2, impact_y - capsule_length/2, capsule_width, capsule_length);
            canvas_draw_circle(canvas, x, impact_y + capsule_length/2, capsule_width/2);
        }
        
        if (stage >= 2) {
            canvas_draw_line(canvas, x - 10, y + 20, x - 5, y + 20 + 3);
            canvas_draw_line(canvas, x + 10, y + 20, x + 5, y + 20 + 3);
        }
        
        if (stage >= 3) {
            canvas_draw_line(canvas, x - 15, y + 20, x - 12, y + 20 + 5);
            canvas_draw_line(canvas, x + 15, y + 20, x + 12, y + 20 + 5);
        }
        
        if (stage >= 4) {
            canvas_draw_line(canvas, x - 18, y + 20, x - 17, y + 20 + 7);
            canvas_draw_line(canvas, x + 18, y + 20, x + 17, y + 20 + 7);
        }
    }
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    StratagemHeroApp* app = (StratagemHeroApp*)ctx;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    if(app->state == GAME_STATE_MENU) {
        draw_space_background(canvas, app);
        
        int8_t offset_x = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_x : 0;
        int8_t offset_y = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_y : 0;
        
        uint8_t centered_x = (128 - SPLASH_WIDTH) / 2;
        uint8_t centered_y = (64 - SPLASH_HEIGHT) / 2;
        
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 
                      centered_x - 5 + offset_x, 
                      centered_y - 5 + offset_y, 
                      SPLASH_WIDTH + 10, 
                      SPLASH_HEIGHT + 10);
        
        canvas_set_color(canvas, ColorWhite);
        for(uint16_t y = 0; y < SPLASH_HEIGHT; y++) {
            for(uint16_t x = 0; x < SPLASH_WIDTH; x++) {
                uint16_t byte_index = (y * ((SPLASH_WIDTH + 7) / 8)) + (x / 8);
                uint8_t bit_position = 7 - (x % 8);
                
                if(app->custom_splash[byte_index] & (1 << bit_position)) {
                    canvas_draw_dot(canvas, centered_x + x + offset_x, centered_y + y + offset_y);
                }
            }
        }
        
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 
                        centered_x - 5 + offset_x, 
                        centered_y - 5 + offset_y, 
                        SPLASH_WIDTH + 10, 
                        SPLASH_HEIGHT + 10);
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 
                              64 + offset_x, 
                              centered_y + SPLASH_HEIGHT + 15 + offset_y, 
                              AlignCenter, 
                              AlignCenter, 
                              "PRESS OK TO DEPLOY");
        
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 52, 128, 12);
        canvas_set_color(canvas, ColorBlack);
        
        if(app->high_score > 0) {
            char score_str[32];
            snprintf(score_str, sizeof(score_str), "HIGH SCORE: %lu", app->high_score);
            
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 0, 0, 128, 10);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, score_str);
        }
        
    } else if(app->state == GAME_STATE_PLAY || app->state == GAME_STATE_STRATAGEM_SUCCESS) {
        int8_t offset_x = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_x : 0;
        int8_t offset_y = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_y : 0;
        
        Stratagem current = STRATAGEMS[app->current_stratagem_index];
        
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2 + offset_x, 12 + offset_y, current.name);
        
        canvas_draw_frame(canvas, 0 + offset_x, 0 + offset_y, 128, 64);
        
        uint8_t progress_width = (120 * app->time_remaining) / INITIAL_TIME;
        if(progress_width > 120) progress_width = 120;
        
        canvas_draw_frame(canvas, 4 + offset_x, 16 + offset_y, 120, 6);
        canvas_draw_box(canvas, 4 + offset_x, 16 + offset_y, progress_width, 6);
        
        for(uint8_t i = 0; i < app->lives; i++) {
            uint8_t heart_x = 104 + (i * 8);
            uint8_t heart_y = 8;
            
            canvas_draw_box(canvas, heart_x + offset_x, heart_y + offset_y, 6, 6);
            canvas_draw_line(canvas, 
                           heart_x + 2 + offset_x, 
                           heart_y + 1 + offset_y, 
                           heart_x + 2 + offset_x, 
                           heart_y + 1 + offset_y);
        }
        
        draw_stratagem_success_animation(canvas, app);
        
        uint8_t direction_size = 16;
        uint8_t total_width = current.length * (direction_size + 2);
        uint8_t start_x = (128 - total_width) / 2;
        
        for(uint8_t i = 0; i < current.length; i++) {
            uint8_t x = start_x + i * (direction_size + 2) + direction_size/2;
            uint8_t y = 24 + direction_size/2;
            bool filled = false;
            
            if(app->state == GAME_STATE_PLAY || app->state == GAME_STATE_STRATAGEM_SUCCESS) {
                filled = i < app->current_input_index;
                if(i == app->current_input_index) {
                    canvas_draw_frame(canvas, x - 10 + offset_x, y - 10 + offset_y, 20, 20);
                }
            }
            
            draw_arrow_bitmap(canvas, current.sequence[i], x, y, filled, offset_x, offset_y);
        }
        
    } else if(app->state == GAME_STATE_GAME_OVER) {
        int8_t offset_x = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_x : 0;
        int8_t offset_y = app->screen_shake.shake_duration > 0 ? app->screen_shake.shake_offset_y : 0;
        
        draw_space_background(canvas, app);
        
        // Score display
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0 + offset_x, 0 + offset_y, 128, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 0 + offset_x, 0 + offset_y, 128, 32);
        
        char score_str[32];
        snprintf(score_str, sizeof(score_str), "SCORE: %lu", app->score);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64 + offset_x, 10 + offset_y, AlignCenter, AlignCenter, score_str);
        
        uint32_t display_high_score = app->high_score;
        if(app->score > app->high_score) {
            display_high_score = app->score;
        }
        
        char high_str[32];
        snprintf(high_str, sizeof(high_str), "BEST: %lu", display_high_score);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64 + offset_x, 24 + offset_y, AlignCenter, AlignCenter, high_str);
        
        // Hill drawing
        uint8_t hill_center_x = 64;
        uint8_t hill_height = 8;
        uint8_t hill_base_y = 63;
        uint8_t hill_width = 100;
        
        for(int16_t x = hill_center_x - hill_width/2; x <= hill_center_x + hill_width/2; x++) {
            float a = (float)hill_height / ((hill_width/2) * (hill_width/2));
            uint8_t y = hill_base_y - hill_height + (a * (x - hill_center_x) * (x - hill_center_x));
            
            if(x >= 0 && x < 128 && y < 64) {
                canvas_draw_line(canvas, x + offset_x, y + offset_y, x + offset_x, hill_base_y + offset_y);
            }
        }
        
        // Flagpole (leaning left)
        uint8_t pole_start_x = hill_center_x - 5;
    uint8_t pole_start_y = hill_base_y - hill_height;
    uint8_t pole_height = 25;
    uint8_t pole_end_x = pole_start_x - 3;
    uint8_t pole_end_y = pole_start_y - pole_height;
    
    canvas_draw_line(canvas, 
                   pole_start_x + offset_x, 
                   pole_start_y + offset_y, 
                   pole_end_x + offset_x, 
                   pole_end_y + offset_y);

    // Improved flag animation with proper edges
    uint8_t flag_width = 24;
    uint8_t flag_height = 10;
    
    // Animation control - make sure app->animation_frame increments elsewhere
    float phase = (float)(app->animation_frame % 64) / 64.0f * 2 * M_PI;
    
    // Initialize previous points at pole position
    int8_t prev_top_x = pole_end_x;
    int8_t prev_top_y = pole_end_y;
    int8_t prev_bottom_x = pole_end_x;
    int8_t prev_bottom_y = pole_end_y + flag_height;
    
    // Draw flag body
    for(uint8_t i = 0; i <= flag_width; i++) {
        float pos = (float)i / (float)flag_width * 2 * M_PI; // Two full waves
        
        // Calculate current points with animation
        int8_t current_x = pole_end_x + i;
        int8_t current_top_y = pole_end_y + sin(pos + phase) * 3;
        int8_t current_bottom_y = pole_end_y + flag_height + sin(pos + phase + 0.5f) * 2;
        
        // Draw vertical connections at the pole
        if(i < 3) {
            canvas_draw_line(canvas,
                          current_x + offset_x,
                          current_top_y + offset_y,
                          current_x + offset_x,
                          current_bottom_y + offset_y);
        }
        
        // Draw top wave
        if(i > 0) {
            canvas_draw_line(canvas,
                          prev_top_x + offset_x,
                          prev_top_y + offset_y,
                          current_x + offset_x,
                          current_top_y + offset_y);
        }
        
        // Draw bottom wave
        if(i > 0) {
            canvas_draw_line(canvas,
                          prev_bottom_x + offset_x,
                          prev_bottom_y + offset_y,
                          current_x + offset_x,
                          current_bottom_y + offset_y);
        }
        
        // Draw vertical stripes
        if(i > 2 && i % 4 == 0 && i < flag_width) {
            canvas_draw_line(canvas,
                          current_x + offset_x,
                          current_top_y + offset_y,
                          current_x + offset_x,
                          current_bottom_y + offset_y);
        }
        
        // Close the flag end
        if(i == flag_width) {
            canvas_draw_line(canvas,
                          current_x + offset_x,
                          current_top_y + offset_y,
                          current_x + offset_x,
                          current_bottom_y + offset_y);
        }
        
        prev_top_x = current_x;
        prev_top_y = current_top_y;
        prev_bottom_x = current_x;
        prev_bottom_y = current_bottom_y;
    }
}
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    StratagemHeroApp* app = ctx;
    
    if(input_event->type == InputTypeShort || input_event->type == InputTypeLong) {
        if(app->state == GAME_STATE_MENU) {
            if(input_event->key == InputKeyOk) {
                app->state = GAME_STATE_PLAY;
                app->lives = INITIAL_LIVES;
                app->score = 0;
                app->current_input_index = 0;
                app->current_stratagem_index = rand() % STRATAGEM_COUNT;
                app->time_remaining = INITIAL_TIME;
                app->current_input_correct = true;
                
                app->success_anim.x = 64;
                app->success_anim.y = 30;
                
            } else if(input_event->key == InputKeyBack) {
                app->exit_requested = true;
            }
        } else if(app->state == GAME_STATE_PLAY || app->state == GAME_STATE_STRATAGEM_SUCCESS) {
            Direction input_dir = DIRECTION_NONE;
            
            if(input_event->key == InputKeyUp) {
                input_dir = DIRECTION_UP;
            } else if(input_event->key == InputKeyDown) {
                input_dir = DIRECTION_DOWN;
            } else if(input_event->key == InputKeyLeft) {
                input_dir = DIRECTION_LEFT;
            } else if(input_event->key == InputKeyRight) {
                input_dir = DIRECTION_RIGHT;
            } else if(input_event->key == InputKeyBack) {
                app->state = GAME_STATE_MENU;
                notification_message(app->notifications, &sequence_navigate);
                
                notification_message(app->notifications, &sequence_welcome_midi);
                return;
            }
            
            if(input_dir != DIRECTION_NONE) {
                Stratagem current = STRATAGEMS[app->current_stratagem_index];
                
                if(app->current_input_index < current.length) {
                    if(input_dir == current.sequence[app->current_input_index]) {
                        app->current_input_index++;
                        app->current_input_correct = true;
                        notification_message(app->notifications, &sequence_correct);
                        
                        if(app->current_input_index >= current.length) {
                            app->score += current.length * 100;
                            app->time_remaining += TIME_BONUS;
                            app->last_input_success = true;
                            
                            notification_message(app->notifications, &sequence_level_complete);
                            
                            if(app->state != GAME_STATE_STRATAGEM_SUCCESS) {
                                app->state = GAME_STATE_STRATAGEM_SUCCESS;
                                app->success_anim.x = 64;
                                app->success_anim.y = 30;
                                app->success_anim.depth = 0;
                                app->success_anim.animation_stage = 0;
                                app->success_anim.duration = 20;
                            }
                            
                            app->current_input_index = 0;
                            app->current_stratagem_index = rand() % STRATAGEM_COUNT;
                        }
                    } else {
                        app->current_input_correct = false;
                        notification_message(app->notifications, &sequence_wrong);
                        
                        if(app->time_remaining > 2000) {
                            app->time_remaining -= 2000;
                        } else {
                            app->time_remaining = 100;
                        }
                        
                        app->screen_shake.shake_duration = 10;
                        
                        app->current_input_index = 0;
                    }
                }
            }
        } else if(app->state == GAME_STATE_GAME_OVER) {
            if(input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                if(app->score > app->high_score) {
                    app->high_score = app->score;
                }
                
                app->state = GAME_STATE_MENU;
                notification_message(app->notifications, &sequence_navigate);
                
                notification_message(app->notifications, &sequence_welcome_midi);
            }
        }
    }
}

int32_t stratagem_hero_app(void* p) {
    UNUSED(p);
    
    StratagemHeroApp* app = malloc(sizeof(StratagemHeroApp));
    if (!app) return -1;
    
    memset(app, 0, sizeof(StratagemHeroApp));
    
    app->exit_requested = false;
    app->state = GAME_STATE_MENU;
    app->score = 0;
    app->high_score = 0;
    app->animation_frame = 0;
    app->feedback_timer = 0;
    app->scroll_offset = 0;
    
    memcpy(app->custom_splash, custom_splash, sizeof(custom_splash));
    
    app->gui = furi_record_open(RECORD_GUI);
    if (!app->gui) {
        free(app);
        return -2;
    }
        
    app->view_port = view_port_alloc();
    if (!app->view_port) {
        furi_record_close(RECORD_GUI);
        free(app);
        return -4;
    }
    
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    if (!app->timer) {
        gui_remove_view_port(app->gui, app->view_port);
        view_port_free(app->view_port);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return -5;
    }
    
    app->animation_timer = furi_timer_alloc(animation_timer_callback, FuriTimerTypePeriodic, app);
    if (!app->animation_timer) {
        furi_timer_free(app->timer);
        gui_remove_view_port(app->gui, app->view_port);
        view_port_free(app->view_port);
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return -6;
    }
    
    furi_timer_start(app->timer, 100);
    furi_timer_start(app->animation_timer, 50);
    
    srand(furi_get_tick() ^ (uint32_t)app);
    
    init_stars(app);
    init_planets(app);
    
    notification_message(app->notifications, &sequence_welcome_midi);
    
    while(!app->exit_requested) {
        furi_delay_ms(50);
    }
    
    if(app->score > app->high_score) {
        app->high_score = app->score;
    }
    
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    
    furi_timer_stop(app->animation_timer);
    furi_timer_free(app->animation_timer);
    
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    
    furi_record_close(RECORD_NOTIFICATION);
    
    free(app);
    
    return 0;
}