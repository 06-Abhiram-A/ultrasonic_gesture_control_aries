#ifndef RGB_H
#define RGB_H

// ==========================================
// 1. PIN CONFIGURATIONS & HARDWARE CONSTANTS
// ==========================================
#define RGB_RED_PIN    24
#define RGB_GREEN_PIN  22
#define RGB_BLUE_PIN   23

#define LED_ON  LOW
#define LED_OFF HIGH

// ==========================================
// 2. EDGE IMPULSE GESTURE MAPPINGS
// ==========================================
#define GESTURE_IDLE       0
#define GESTURE_PUSH_LEFT  1  // Power Toggle
#define GESTURE_PUSH_RIGHT 2  // Blink Toggle
#define GESTURE_SWIPE      3  // Color Cycle

// ==========================================
// 3. FUNCTION PROTOTYPES (BLUEPRINTS)
// ==========================================
// These tell the compiler these functions exist, 
// allowing you to call them anywhere.

void turn_all_leds_off(void);
void apply_current_color(void);
void process_gesture_action(int predicted_gesture);
void run_background_blink_worker(void);

#endif // RGB_H
