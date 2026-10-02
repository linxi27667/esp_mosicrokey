#pragma once
#include "esp_err.h"
constexpr int SPI2_HOST = 2, SPI_DMA_CH_AUTO = 0;
struct spi_bus_config_t
{
    int mosi_io_num = -1, miso_io_num = -1, sclk_io_num = -1;
    int quadwp_io_num = -1, quadhd_io_num = -1, max_transfer_sz = 0;
};
esp_err_t spi_bus_initialize(int, const spi_bus_config_t*, int);
esp_err_t spi_bus_free(int);
