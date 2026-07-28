#include "rgb.h"
#include "gpio.h"

// ==========================================
// 1. SYSTEM STATE TRACKING VARIABLES
// ==========================================
// These are defined here so they persist in memory across function calls.
US system_power_on = 0;
US blink_mode_active = 0;
int current_color_index = 0;   // 0 = Red, 1 = Green, 2 = Blue

// Timing trackers for non-blocking blinking
unsigned long last_blink_toggle_cycle = 0;
US blink_led_state = 0;

// ==========================================
// 2. HARDWARE-LEVEL LED CONTROL FUNCTIONS
// ==========================================

/**
 * Shuts off all three RGB channels simultaneously
 */
/**
 * @brief Shuts off all three onboard RGB LED channels simultaneously.
 * @details Safely clears the state of the pins by driving them LOW to ensure
 * no color bleeding occurs before switching states.
 * @return void
 */
void turn_all_leds_off(void) {
    // Reuses your SDK's GPIO abstraction layer
    GPIO_write_pin(RGB_RED_PIN, LED_OFF);
    GPIO_write_pin(RGB_GREEN_PIN, LED_OFF);
    GPIO_write_pin(RGB_BLUE_PIN, LED_OFF);
}

/**
 * Illuminates a single primary color channel based on the current index
 */
/**
 * @brief Drives the physical GPIO pins to illuminate the active color index.
 * @details Validates the master system power status before activating any pins.
 * Maps the internal state index to the corresponding physical LED pin.
 * @note This function handles hardware isolation; it turns off all LEDs before 
 * turning on the target pin.
 * @return void
 */
void apply_current_color(void) {
    // Always clear the board first to prevent overlapping colors
    //turn_all_leds_off(); 
    
    // Safety guard: Don't light up if master power is cut
    if (!system_power_on) {
        return;
    }

    switch (current_color_index) {
        case 0: // Red
            GPIO_write_pin(RGB_RED_PIN, LED_ON);
            GPIO_write_pin(RGB_GREEN_PIN, LED_OFF);
            GPIO_write_pin(RGB_BLUE_PIN, LED_OFF);
            break;
        case 1: // Green
            GPIO_write_pin(RGB_GREEN_PIN, LED_ON);
            GPIO_write_pin(RGB_RED_PIN, LED_OFF);
            GPIO_write_pin(RGB_BLUE_PIN, LED_OFF);
            break;
        case 2: // Blue
            GPIO_write_pin(RGB_BLUE_PIN, LED_ON);
            GPIO_write_pin(RGB_GREEN_PIN, LED_OFF);
            GPIO_write_pin(RGB_RED_PIN, LED_OFF);
            break;
        default:
            // Fallback safety baseline
            turn_all_leds_off();
            break;
    }
}

// ==========================================
// 3. THE GESTURE STATE MACHINE ENGINE
// ==========================================

/**
 * Processes incoming predictions and alters the system states
 */
/**
 * @brief Main state machine engine that processes classified ML gestures.
 * @details Evaluates incoming gesture IDs to mutate system state variables.
 * Handles master power toggles, sequential color indexing, and 
 * asynchronous background effects.
 * @param predicted_gesture Integer representing the winning classification index 
 * from the Edge Impulse runner (0=IDLE, 1=LEFT, 2=RIGHT, 3=SWIPE).
 * @return void
 */
void process_gesture_action(int predicted_gesture) {
    
    // 1. Handle PUSH_LEFT (Master Power Toggle)
    if (predicted_gesture == GESTURE_PUSH_LEFT) {
        system_power_on = !system_power_on;
        
        if (!system_power_on) {
            // Clean shutdown: clear all sub-states and kill pins
            blink_mode_active = 0;
            turn_all_leds_off();
            return;
        } else {
            // Restore last active color state instantly on boot
            apply_current_color();
        }
    }

    // Guard Clause: If the system power is off, ignore all other inputs
    if (!system_power_on) {
        return;
    }

    // 2. Handle SWIPE (Color Cycle: Red -> Green -> Blue -> Red)
    if (predicted_gesture == GESTURE_SWIPE) {
        current_color_index = (current_color_index + 1) % 3;
        apply_current_color();
    }

    // 3. Handle PUSH_RIGHT (Blink Mode Toggle)
    if (predicted_gesture == GESTURE_PUSH_RIGHT) {
        blink_mode_active = !blink_mode_active;
        
        if (!blink_mode_active) {
            // If blinking turns off, return back to a steady, solid color
            apply_current_color();
        }
    }
}

// ==========================================
// 4. NON-BLOCKING BACKGROUND WORKER
// ==========================================

/**
 * Runs continuously in the main execution background thread.
 * Toggles the LED on/off asynchronously without delaying code execution.
 */
/**
 * @brief Non-blocking worker module that manages the asynchronous blinking effect.
 * @details Evaluates raw hardware clock cycles against a target budget time interval.
 * Toggles the physical LED on and off dynamically without holding execution,
 * allowing the main loop to keep sampling ultrasonic sensors continuously.
 * @note Must be called continuously within the main application loop execution thread.
 * @return void
 */
void run_background_blink_worker(void) {
    // Exit instantly if blinking isn't needed or power is off
    if (!system_power_on || !blink_mode_active) {
        return;
    }

    // Fetch raw hardware cycles using your native SDK clock evaluation function
    unsigned long current_cycle = get_time(); 
    
    // Placeholder cycle delta limit (We will tune this with your mentor)
    unsigned long BLINK_INTERVAL_CYCLES = 20000000; 

    // Check if the hardware execution cycle window has elapsed
    if ((current_cycle - last_blink_toggle_cycle) >= BLINK_INTERVAL_CYCLES) {
        
        blink_led_state = !blink_led_state; // Toggle tracking state
        
        if (blink_led_state) {
            apply_current_color(); // Light turns on to active color
        } else {
            turn_all_leds_off();   // Light turns off completely
        }

        // Reset the execution timestamp anchor
        last_blink_toggle_cycle = current_cycle; 
    }
}
