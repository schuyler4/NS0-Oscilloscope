#ifndef MAIN_H
#define MAIN_H

#define WORD_SIZE 32

#define TRIGGER_PIN_COUNT 9
#define FORCE_TRIGGER_PIN_COUNT 8
#define FIFO_REGISTER_WIDTH 32
#define PIN_BASE 0
#define SAMPLE_BIT_COUNT 8
#define SAMPLE_BIT_MASK 0xFF
#define SAMPLE_COUNT 16384

#define CAL_PIN 14

#define PWM_HIGH_COUNT 32770
#define PWM_CLK_DIV 2

#define CHARACTER_TIMEOUT 100
#define MAX_STRING_LENGTH 100

#include <stdint.h>

#include "pico/stdlib.h"

typedef enum
{
    FALLING_EDGE,
    RISING_EDGE
} TriggerType;

typedef struct
{
    uint8_t created;
    uint dma_channel;
    uint second_dma_channel;
    uint8_t* capture_buffer;
    uint clock_div;
    TriggerType trigger_type;
} Sampler;

void setup_IO(void);
void setup_SPI(void);
void sampler_init(Sampler* sampler, uint8_t sampler_number);
void update_clock(Sampler force_sampler, Sampler normal_sampler);
uint8_t sampler_pio_init(Sampler sampler, uint8_t pin_base);
uint16_t get_dma_last_index(Sampler sampler);
void arm_sampler(void);
void trigger(Sampler* force_sampler, Sampler* normal_sampler, uint8_t forced);
void trigger_callback(uint gpio, uint32_t event_mask);
void transmit_vector(uint16_t* vector, uint16_t point_count);
void get_string(char* str);
void setup_cal_pin(void);
void run_trigger(void);
void initialize_peripherals(void);
void set_cal_command(void);
void read_cal_command(void);
void stop_capture(void);
bool record_callback(struct repeating_timer *t);
void stop_trigger(void);

#endif
