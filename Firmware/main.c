/*
This file is part of ppm2usb.

ppm2usb is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ppm2usb is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ppm2usb.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "STM32F1/Inc/stm32f1xx_hal_conf.h"
#include "STM32F1/Inc/usbd_custom_hid_if.h"

#ifdef DEBUG_PRINT
#include "xprintf.h"
#endif

extern int8_t USBD_CUSTOM_HID_SendReport_FS ( uint8_t *report,uint16_t len);
extern TIM_HandleTypeDef htim1;

#define TX_CHANNELS 8

uint32_t ppm_data[TX_CHANNELS];
uint32_t last_ppm_time = 0;

uint8_t curr_channel = 0;

#define PPM_MIN 950
#define PPM_MAX 1950

#define USB_MIN -127
#define USB_MAX 127

int8_t mapPPMtoUSB(uint32_t val){
	//(val - A)*(b-a)/(B-A) + a
	return (val - PPM_MIN) * (USB_MAX-USB_MIN)/(PPM_MAX-PPM_MIN) + USB_MIN;
}

static uint8_t* USBD_HID_GetPos (void)
{
	static int8_t HID_Buffer[5] = {0};
	HID_Buffer[0] = 0x00;


	for(int i= 0; i< 4; ++i){
		int8_t out_val = mapPPMtoUSB(ppm_data[i]);
		HID_Buffer[i+1] = out_val;
	}

	return (uint8_t*)HID_Buffer;
}


#ifdef DEBUG_PRINT
void send_command(int command, const void *message)
{
   asm("mov r0, %[cmd];"
       "mov r1, %[msg];"
       "bkpt #0xAB"
         :
         : [cmd] "r" (command), [msg] "r" (message)
         : "r0", "r1", "memory");
}

char buf[512];
uint16_t buffPos = 0;
// dirty hack but its faster and works
void serial_putc( uint8_t c ){

	buf[buffPos++] = c;

	if(c == '\n'){
		buf[buffPos++] = 0;
		send_command(0x04, buf);
		buffPos = 0;
	}

}
#endif

int isSyncFrame(uint32_t time){
	return time > 5000;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim){
	uint32_t now = __HAL_TIM_GET_COUNTER(htim);
	uint32_t ppm = 0;
	if(now < last_ppm_time){ //timer overflowed
		ppm = now - last_ppm_time + 65536;
	}
	else {
		ppm = now - last_ppm_time;
	}

	last_ppm_time = now;

	if(isSyncFrame(ppm)){
		curr_channel = 0;
	}
	else {
		ppm_data[curr_channel] = ppm;
		++curr_channel;
		if(curr_channel == TX_CHANNELS){
			curr_channel = 0;
		}
	}
}

int main(){

	init_stm32cube();

	HAL_TIM_Base_Start_IT(&htim1);
	HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);

#ifdef DEBUG_PRINT
	xdev_out(serial_putc);
#endif


	while(42){
		USBD_CUSTOM_HID_SendReport_FS(USBD_HID_GetPos(), 5);
	}
}
