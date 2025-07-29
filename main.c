#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "tusb.h"

#include "main.h"
#include "serial_protocol.h"
#include "ring_buffer.h"

static uint8_t trigger_flag = 0;
uint8_t force_trigger = 0;

static volatile uint8_t trigger_vector_available = 0;

static Sampler normal_sampler;
static Sampler force_sampler;

static uint8_t aligned_memory[SAMPLE_COUNT] __attribute__((aligned(SAMPLE_COUNT)));

uint clk_div;
uint16_t last_index;

int main(void)
{
    stdio_init_all();
    tusb_init();
    uint8_t peripherals_initialized = 0;

    while(1)
    {
        if(tud_mounted())
        {
            if(!peripherals_initialized)
            {
                initialize_peripherals();
                peripherals_initialized = 1;
            }

            char command = (char)getchar_timeout_us(CHARACTER_TIMEOUT);
            switch(command)
            {
                case RISING_EDGE_TRIGGER_COMMAND:
                    normal_sampler.trigger_type = RISING_EDGE;
                    break;
                case FALLING_EDGE_TRIGGER_COMMAND:
                    normal_sampler.trigger_type = FALLING_EDGE;
                    break;
                case TRIGGER_COMMAND:
                    {
                        trigger_vector_available = 0;
                        force_trigger = 0;
                        trigger(&force_sampler, &normal_sampler, force_trigger);
                        break;
                    }
                case FORCE_TRIGGER_COMMAND:
                    {
                        trigger_vector_available = 0;
                        force_trigger = 1;
                        trigger(&force_sampler, &normal_sampler, force_trigger);
                        dma_hw->ints0 = 1 << force_sampler.dma_channel;
                        break;
                    }
                case CLOCK_DIV_COMMAND:
                    update_clock(force_sampler, normal_sampler); 
                    break;
                default:
                    // Do nothing
                    break;
            }

            if(trigger_vector_available)
            {
                if(force_trigger)
                {
                    write(1, (char*)aligned_memory, SAMPLE_COUNT*sizeof(char));
                }
                else
                {
                    flatten_ring_buffer(normal_sampler.capture_buffer, last_index, SAMPLE_COUNT);
                    write(1, (char*)normal_sampler.capture_buffer, SAMPLE_COUNT*sizeof(char));
                }
                trigger_vector_available = 0;
            }
        }
    }

    // The program should never return. 
    return 1;
}

void initialize_peripherals(void)
{
    setup_IO();
    setup_cal_pin();

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    //sampler_init(&normal_sampler, 0, pio0);
    sampler_init(&force_sampler, 0);
    clk_div = 1;
}

void sampler_init(Sampler* sampler, uint8_t sampler_number)
{
    sampler->created = 0;
    sampler->dma_channel = sampler_number;
    sampler->second_dma_channel = sampler_number + 1;
}

void get_string(char* str)
{
    uint8_t i;
    for(i = 0; i < MAX_STRING_LENGTH; i++)
    {
        *(str + i) = (char)getchar();
        if(*(str + i) == '\0') return;
    }
}

void setup_IO(void)
{
    gpio_set_function(CAL_PIN, GPIO_FUNC_PWM);
}

void setup_cal_pin(void)
{
    uint8_t slice_number = pwm_gpio_to_slice_num(CAL_PIN);
    pwm_set_chan_level(slice_number, PWM_CHAN_A, PWM_HIGH_COUNT);
    pwm_set_clkdiv(slice_number, PWM_CLK_DIV);
    pwm_set_enabled(slice_number, 1);
}

void dma_complete_handler(void)
{
    adc_run(false);
    if(force_trigger)
    {
        dma_hw->ints0 = 1 << force_sampler.dma_channel;
        dma_channel_abort(force_sampler.dma_channel);
        irq_set_enabled(DMA_IRQ_0, false);
        irq_remove_handler(DMA_IRQ_0, dma_complete_handler);
    }
    else
    {
        stop_trigger();
    }
    trigger_vector_available = 1; 
}

void stop_trigger(void)
{
    // Not implemented
}

void arm_sampler(void)
{
    if(!force_trigger)
    {
        // Not implemented
    }
    else
    {
        adc_init();
        adc_gpio_init(26);
        adc_select_input(0);

        adc_fifo_setup(true, true, 1, false, true);
        if(clk_div == 1)
            adc_set_clkdiv(0);
        else
            adc_set_clkdiv((clk_div*96)-1);
        adc_fifo_drain();

        dma_channel_config cfg = dma_channel_get_default_config(force_sampler.dma_channel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);
        
        dma_channel_configure(force_sampler.dma_channel, &cfg, aligned_memory, &adc_hw->fifo, SAMPLE_COUNT, false);
        adc_run(true);
        dma_channel_set_irq0_enabled(force_sampler.dma_channel, true);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
        irq_set_enabled(DMA_IRQ_0, true);
        dma_channel_start(force_sampler.dma_channel);
    }
}

uint16_t get_dma_last_index(Sampler normal_sampler)
{
    if(dma_channel_is_busy(normal_sampler.second_dma_channel)) 
        return SAMPLE_COUNT - (dma_channel_hw_addr(normal_sampler.second_dma_channel)->transfer_count*4) - 1;
    if(dma_channel_is_busy(normal_sampler.dma_channel))
        return SAMPLE_COUNT - (dma_channel_hw_addr(normal_sampler.dma_channel)->transfer_count*4) - (SAMPLE_COUNT/2) - 1;
    return 0;
}

void update_clock(Sampler force_sampler, Sampler normal_sampler)
{
    if(normal_sampler.created)
    {
        stop_trigger();
    }
    char code_string[MAX_STRING_LENGTH];
    get_string(code_string);
    uint16_t commanded_clock_div = atoi(code_string);   
    if(commanded_clock_div > 0)
    {
        clk_div = commanded_clock_div;
        if(force_sampler.created)
        {
            if(clk_div == 1)
                adc_set_clkdiv(0);
            else
                adc_set_clkdiv((clk_div*96)-1);
        }
    }
}

void trigger(Sampler* force_sampler, Sampler* normal_sampler, uint8_t forced)
{
    if(forced)
    {
        arm_sampler();
    }
    else
    {
        // Not implemented
    }
}