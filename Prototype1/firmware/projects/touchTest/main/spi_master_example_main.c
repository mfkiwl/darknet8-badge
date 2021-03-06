/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <esp_log.h>

#define PIN_NUM_MISO 35
#define PIN_NUM_MOSI 33
#define PIN_NUM_CLK  26
#define PIN_NUM_CS   27
#define PIN_NUM_IRQ  32

static bool check = false;

static void IRAM_ATTR touch_isr_handler(void* arg) {
	ets_printf("irq");
	check = true;
}

void app_main()
{
	const char *TAG = "tag";
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=128
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=1*1000*1000,           //Clock out at 2 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=3,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=0,
		  .post_cb=0,
		  .input_delay_ns = 0,
		  .cs_ena_pretrans = 0,
		  .cs_ena_posttrans=0,
		  .duty_cycle_pos = 0
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 2);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

	 gpio_install_isr_service(0);

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_NEGEDGE;
#define GPIO_MASK (1ULL << PIN_NUM_IRQ)
	io_conf.pin_bit_mask = GPIO_MASK;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	if(ESP_OK!=gpio_isr_handler_add( PIN_NUM_IRQ, touch_isr_handler, (void*) 0)) {
		ESP_LOGE(TAG,"Failed to install isr handler");
	}


	while(1) {
		if(check) {
			check = false;
			uint32_t xPos = 0, yPos = 0;
			spi_transaction_t t;
			memset(&t,0,sizeof(t));
			t.length=16;
			t.tx_data[0] = 0x90;
			t.tx_data[1] = 0x00;
			t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
			esp_err_t r = spi_device_transmit(spi, &t);
			ESP_ERROR_CHECK(r);


				t.tx_data[0] = 0x91;
				t.tx_data[1] = 0x00;
				r = spi_device_transmit(spi, &t);
				ESP_ERROR_CHECK(r);

				xPos = t.rx_data[0];
				xPos <<= 5;
				xPos |= ((uint16_t)t.rx_data[1])>>4;

				//xPos = t.rx_data[0]<<8|t.rx_data[1]>>8;
				ESP_LOGI(TAG,"xPos = %d",(int)xPos);
				t.tx_data[0] = 0xD1;
				t.tx_data[1] = 0x00;
				r = spi_device_transmit(spi, &t);
				ESP_ERROR_CHECK(r);
				//yPos = t.rx_data[0]<<5|t.rx_data[1]>>3;
				//yPos = t.rx_data[0]<<8|t.rx_data[1]>>8;
				ESP_LOGI(TAG,"yPos = %d",(int)yPos);

			vTaskDelay(100/portTICK_PERIOD_MS);
		}
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}
