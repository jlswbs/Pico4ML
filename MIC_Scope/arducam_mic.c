#include "arducam_mic.h"
#include "mic_i2s.pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include <string.h>

int16_t dmabuf_1[256];
uint8_t isAvailable = 0;
int capture_index = 0;

mic_i2s_config_t config= {
  .data_pin =  26,
  .LRclk_pin = 27,
  .clock_pin = 28,
  .pio = pio1,
  .data_buf = dmabuf_1,
  .data_buf_size = 256,
  .pio_sm = 0,
  .dma_channel = 0,
  .sample_freq = 192000,
};

void update_pio_frequency(mic_i2s_config_t* config) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / config->sample_freq;
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(config->pio, config->pio_sm, divider >> 8u, divider & 0xffu);
}

uint8_t mic_i2s_init(mic_i2s_config_t* config){
    gpio_set_function(config->LRclk_pin, GPIO_FUNC_PIO1);
    gpio_set_function(config->clock_pin, GPIO_FUNC_PIO1);
    gpio_set_function(config->data_pin, GPIO_FUNC_PIO1);

    uint offset = pio_add_program(config->pio, &mic_i2s_program);
    mic_i2s_program_init(config->pio, config->pio_sm, offset,
                         config->LRclk_pin, config->clock_pin, config->data_pin);
    update_pio_frequency(config);
    pio_sm_clear_fifos(config->pio, config->pio_sm);

    mic_dma_init(config);

    return 0;
}

void dma_handler() {

    dma_hw->ints0 = 1u << config.dma_channel;

    capture_index += 1024;
    capture_index = capture_index % 8192;

    dma_channel_set_write_addr(config.dma_channel, config.data_buf + capture_index, true);

    isAvailable  = 1;

    if (config.update) {
        config.update();
    }
}

void mic_dma_init(mic_i2s_config_t *config) {

    dma_channel_config c = dma_channel_get_default_config(config->dma_channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(config->pio, config->pio_sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);

    dma_channel_configure(
        config->dma_channel,
        &c,
        config->data_buf,
        &config->pio->rxf[config->pio_sm],
        config->data_buf_size,
        false
    );

    dma_channel_start(config->dma_channel);
    dma_channel_set_irq0_enabled(config->dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

}

void mic_safe_snapshot(int16_t* snapshot_buffer, size_t length) {

    dma_channel_abort(config.dma_channel);
    memcpy(snapshot_buffer, config.data_buf, length * sizeof(int16_t));
    dma_channel_set_read_addr(config.dma_channel, &config.pio->rxf[config.pio_sm], false);
    dma_channel_start(config.dma_channel);

}