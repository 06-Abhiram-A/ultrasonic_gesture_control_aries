#include <stdio.h>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

//HC-SR04 Sensor
extern "C"{
#include "gpio.h" // Ensure your platform's GPIO driver is included here
#include "rgb.h"
long get_time()
    {
        long clock_count;  

        // Directly reads the mcycle CSR. This replaces 'read_csr(mcycle)' 
        // so you don't get compiler errors if the macro headers aren't included.
        __asm__ volatile ("csrr %0, mcycle" : "=r"(clock_count)); 
      
        return clock_count;
    }
}


#define TRUE 1
#define FALSE 0

#define WIN_SAMPLES EI_CLASSIFIER_RAW_SAMPLE_COUNT      //10
#define WIN_AXES EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME    //2 

#define STRIDE_MS 100
#define SAMPLES_PER_STRIDE (STRIDE_MS / EI_CLASSIFIER_INTERVAL_MS)
#define STRIDE_AXES_SIZE (SAMPLES_PER_STRIDE * WIN_AXES)

#define STATE_PRIMING   0
#define STATE_DETECTING 1
#define STATE_EXECUTE   2

#define IDLE_CATEGORY_INDEX 0

#define LEFT_TRIG  0    // Connect Left Trigger to GPIO 0
#define LEFT_ECHO  1    // Connect Left Echo to GPIO 1

#define RIGHT_TRIG 3    // Connect Right Trigger to GPIO 3
#define RIGHT_ECHO 4    // Connect Right Echo to GPIO 4

int udelay(unsigned int count);

unsigned long millis(void);

// Callback function declaration
static int get_signal_data(size_t offset, size_t length, float *out_ptr);

// Raw features copied from test sample
static  float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

float read_distance_cm(int trig_pin, int echo_pin) {
    // 1. Send a clean 10-microsecond high pulse to trigger the sensor
    GPIO_write_pin(trig_pin, LOW);
    udelay(2);
    GPIO_write_pin(trig_pin, HIGH);
    udelay(10);
    GPIO_write_pin(trig_pin, LOW);

    // 2. Measure echo high pulse duration using native SDK function
    unsigned long total_time = pulse_duration(echo_pin, HIGH,3000); 
    
    float distance;

    // 3. Replicate the Arduino filtering and clamping logic exactly
    if (total_time == 0) {
        // Timeout occurred (no object within ~51cm limit) -> set to IDLE state
        distance = 99.0f;
    } 
    else {
        // Calculate distance using your exact Arduino speed-of-sound factor (0.034 cm/us)
        distance = (float)total_time / 29.0f / 2.0f;
        
        // Strict baseline ceiling: if the object is too far, clamp it to the 99cm IDLE baseline
        if (distance > 45.0f) {
            distance = 99.0f;
        }
    }

    /*// 3. Convert microseconds to centimeters using SDK formula
    // (Cast to float to preserve precision for the machine learning model)
    float distance = (float)total_time / 29.0f / 2.0f;*/
    
    return distance;
}

/**
 * @brief Safely reads the full 64-bit hardware cycle counter (mcycle + mcycleh).
 * @note Prevents rollover glitches when reading split 32-bit registers at 100 MHz.
 */
static inline unsigned long long read_mcycle64(void) {
    unsigned int high, low, check_high;
    do {
        asm volatile("csrr %0, mcycleh" : "=r"(high));
        asm volatile("csrr %0, mcycle"  : "=r"(low));
        asm volatile("csrr %0, mcycleh" : "=r"(check_high));
    } while (high != check_high);

    return (((unsigned long long)high) << 32) | low;
}

// Change these from the old "latched" flags to "pending" flags
bool push_left_pending = false;
bool push_right_pending = false;

int main(int argc, char **argv) {
    // Note: The system initialization for GPIO and UART happens automatically 
    // in your platform initialization before main executes.

    ei_printf("\n\r ***************************************************************************");
    ei_printf("\n\r INFO: Edge Impulse Gesture Inference Engine Active");    
    ei_printf("\n\r INFO: Left Sensor -> Trig: GPIO %d, Echo: GPIO %d", LEFT_TRIG, LEFT_ECHO);  
    ei_printf("\n\r INFO: Right Sensor -> Trig: GPIO %d, Echo: GPIO %d", RIGHT_TRIG, RIGHT_ECHO);   
    ei_printf("\n\r ***************************************************************************\n\r\n\r");

    //unsigned long target_us = EI_CLASSIFIER_INTERVAL_MS * 1000;

    int current_state = STATE_PRIMING;
    unsigned long last_sample_time = 0;
    int primed_samples = 0;
    int confirmed_gesture_idx = -1; 
    
    ei_printf("INFO: Priming sensor buffer with initial data...\n\r");

    while(1)
    {
        run_background_blink_worker(); // Paused for initial testing

        unsigned long current_time = millis();

        if (current_time - last_sample_time >= EI_CLASSIFIER_INTERVAL_MS) 
        {
            last_sample_time = current_time;

            int total_elements = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
            for (int i = 0; i < (total_elements - WIN_AXES); i++) {
                features[i] = features[i + WIN_AXES];
            }
            float left_cm  = read_distance_cm(LEFT_TRIG, LEFT_ECHO);
            udelay(10000);
            float right_cm = read_distance_cm(RIGHT_TRIG, RIGHT_ECHO);

            features[(WIN_SAMPLES - 1) * WIN_AXES + 0] = left_cm;
            features[(WIN_SAMPLES - 1) * WIN_AXES + 1] = right_cm;

            switch (current_state) 
            {
                case STATE_PRIMING:
                    primed_samples++;
                    if (primed_samples >= WIN_SAMPLES) {
                        ei_printf("▶ SYSTEM READY: Start gesturing!\n\r");
                        current_state = STATE_DETECTING;
                    }
                    break;
                case STATE_DETECTING:
                    {
                        signal_t signal;
                        signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
                        signal.get_data = &get_signal_data;
                        
                        ei_impulse_result_t result;
                        if (run_classifier(&signal, &result, false) != 0) {
                            break; // Skip if classification fails
                        }

                        int best = 0;
                        float bv = result.classification[0].value;
                        for (size_t ix = 1; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                            if (result.classification[ix].value > bv) {
                                bv = result.classification[ix].value;
                                best = ix;
                            }
                        }
                        #if EI_CLASSIFIER_HAS_ANOMALY == 1
                        if (result.anomaly > 1.50f) { break; }
                        #endif

                        // Temporarily lower the threshold to test low-confidence triggers
                        // if (bv >= 0.40f) { 
                        //     if (best != IDLE_CATEGORY_INDEX) {
                        //         // Print the score alongside the guess so you can see the raw confidence
                        //         ei_printf("DEBUG: Guess %d at %.2f confidence\n\r", best, (double)bv);
                                
                        //         confirmed_gesture_idx = best;
                        //         current_state = STATE_EXECUTE;
                        //     }
                        // }
                        if (bv >= 0.75f) {
                            if (best != IDLE_CATEGORY_INDEX) {
                                 confirmed_gesture_idx = best;
                                 current_state = STATE_EXECUTE;
                            }
                        }
                    }
                    break;

                case STATE_EXECUTE:
                    // Print matching your specific deployment ID array name
                    ei_printf("GESTURE CONFIRMED: %s\n\r", 
                              ei_classifier_inferencing_categories[confirmed_gesture_idx]);
                    if (confirmed_gesture_idx == GESTURE_PUSH_LEFT) {
                        push_left_pending = true;
                        ei_printf("DEBUG: Push-Left pending (waiting for hand release...)\n\r");
                    } 
                    else if (confirmed_gesture_idx == GESTURE_PUSH_RIGHT) {
                        push_right_pending = true;
                        ei_printf("DEBUG: Push-Right pending (waiting for hand release...)\n\r");
                    } 
                    else if (confirmed_gesture_idx == GESTURE_SWIPE) {
                        // A swipe executes instantly!
                        process_gesture_action(GESTURE_SWIPE);
                        
                        // CRITICAL CANCEL: Wipe out any accidental push triggers caught during the swipe prep
                        push_left_pending = false;
                        push_right_pending = false;
                        ei_printf("DEBUG: Swipe executed. Canceled pending push actions!\n\r");
                    }
                    // --------------------------------------------------------
                    // TODO: Trigger lighting flags here based on index values
                    // 1 = Push-Left, 2 = Push-Right, 3 = Swipe
                    // --------------------------------------------------------
                    
                    // Zero-out buffer to force a re-prime cooldown
                    for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
                        features[i] = 0.0f;
                    }
                    primed_samples = 0;
                    current_state = STATE_PRIMING;
                    break;
            }
             // RELEASE TRIGGER: Execute pending actions ONLY when hand is removed
                    // -------------------------------------------------------------
                    if (left_cm == 99.0f && right_cm == 99.0f) {
                        if (push_left_pending) {
                            process_gesture_action(GESTURE_PUSH_LEFT); // Execute toggle now!
                            push_left_pending = false;                 // Clear flag
                            ei_printf("DEBUG [Release]: Hand cleared! Push-Left action triggered.\n\r");
                        }
                        if (push_right_pending) {
                            process_gesture_action(GESTURE_PUSH_RIGHT); // Execute blink now!
                            push_right_pending = false;                 // Clear flag
                            ei_printf("DEBUG [Release]: Hand cleared! Push-Right action triggered.\n\r");
                        }
                    }
        }
    }

    return 0; // Standard C compliance, though bare metal never reaches here
}

/**
 * @brief Edge Impulse callback function to step through data chunks safely
 */
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (features + offset)[i];
    }
    return EIDSP_OK;
}
int udelay(unsigned int count)
{
	for(volatile unsigned int i=0; i<(count*35u); i++); //(actually microseconds)delay millisecond calculation with board mhz
    return 0;
}

/**
 * @brief Microsecond timestamp function calibrated for 100 MHz.
 * @return Elapsed microseconds since boot. Overflows safely every ~71.5 minutes.
 */
unsigned long millis(void) {
    // At 100 MHz, the CPU ticks exactly 100,000 times every 1 millisecond
    return (unsigned long)(read_mcycle64() / 100000u);
}



//     for (int i = 0; i < WIN_SAMPLES; i++)
//     {
//         unsigned long start_time = micros(); // 1. Note the exact start time

//         float left_cm  = read_distance_cm(LEFT_TRIG, LEFT_ECHO);
//         udelay(10000);
//         float right_cm = read_distance_cm(RIGHT_TRIG, RIGHT_ECHO);

//         features[i * WIN_AXES + 0] = left_cm;
//         features[i * WIN_AXES + 1] = right_cm;

//         unsigned long elapsed_us = micros() - start_time; // 2. Calculate how long the code took
        
//         if (elapsed_us < target_us) {
//             udelay(target_us - elapsed_us); // 3. Delay only for the remaining microsecond balance
//         }
//     }
//     ei_printf("INFO: Buffer primed. Continuous inference loop online.\n\r");
    
//     while(1)
//     {
//         custom_memmove(features, 
//                        features + STRIDE_AXES_SIZE, 
//                        (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - STRIDE_AXES_SIZE) * sizeof(float));

//         // Append data loop with dynamic time compensation
//         for (int i = WIN_SAMPLES - SAMPLES_PER_STRIDE; i < WIN_SAMPLES; i++)
//         {
//             unsigned long start_time = micros(); // 1. Note the exact start time

//             float left_cm  = read_distance_cm(LEFT_TRIG, LEFT_ECHO);
//             float right_cm = read_distance_cm(RIGHT_TRIG, RIGHT_ECHO);

//             features[i * WIN_AXES + 0] = left_cm;
//             features[i * WIN_AXES + 1] = right_cm;

//             unsigned long elapsed_us = micros() - start_time; // 2. Calculate elapsed time
            
//             if (elapsed_us < target_us) {
//                 udelay(target_us - elapsed_us); // 3. Subtract overhead from the total delay window
//             }
//         }
//         // Wrap the raw array for the Edge Impulse framework
//         signal_t signal;
//         signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
//         signal.get_data = &get_signal_data;
        
//         ei_impulse_result_t result;
        
//         // Execute neural network classification locally on the processor
//         if (run_classifier(&signal, &result, false) != 0) {
//             continue;
//         }

//         // Find the category with the highest prediction score
//         int best = 0;
//         float bv = result.classification[0].value;
//         for (size_t ix = 1; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
//             if (result.classification[ix].value > bv) {
//                 bv = result.classification[ix].value;
//                 best = ix;
//             }
//         }

//         // Output results to the serial port
//         /*ei_printf("Gesture Detected: %s (%d%%)\n\r", 
//                ei_classifier_inferencing_categories[best], 
//                (int)(bv * 100));*/
        
// #if EI_CLASSIFIER_HAS_ANOMALY == 1
//         // Gate A: V2 Anomaly Shield
//         if (result.anomaly > 1.50f){
//             // Ensure your conversion matches your 1.50f limit!
//             int anom_milli = (int)(result.anomaly * 1000);
//             ei_printf("Ignored: Environmental Noise / Anomaly Detected (Score x1000: %d)\n\r", anom_milli);
//         }
//         else
// #endif
//         // Gate B: Strict V2 Confidence Filter
//         if (bv < 0.75f) {
//             ei_printf("Ignored: Ambiguous Movement (%s at %d%% confidence)\n\r", 
//                    ei_classifier_inferencing_categories[best], (int)(bv * 100));
//         } 
//         // Gate C: Clean, verified production output
//         else {
//             ei_printf("▶ GESTURE CONFIRMED: %s (%d%%)\n\r", 
//                    ei_classifier_inferencing_categories[best], 
//                    (int)(bv * 100));
            
//             // TODO: Insert your lighting controller commands or hardware execution flags here
//         }
//     }

//     return 0;
// }
