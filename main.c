#include "boot_config.h"
#include "nrf.h"
#include "urf_radio.h"
#include "urf_timer.h"
#include "urf_uart.h"
#include "urf_ble_peripheral.h"
#include "urf_ble_att_process.h"
#include "ble_const.h"
#include <stdio.h> 
//#include <stdarg.h>

#define BOOTLOADER_AREA_SIZE 0x4000
#define FLASH_CACHE_AREA 0x40000

int button_pin = 26;
uint8_t led_pin = 8;
 
sService GAP_service;
sService FW_service;

sCharacteristic gap_name_ch;
sCharacteristic gap_appearance_ch;
sCharacteristic fw_io_ch;

uint8_t gen_mac[6] = {0x0A, 0x1F, 0x23, 0x58, 0xE2, 0x50};

int spr(uint8_t *ptr, char *txt)
{
	int len = 0;
	while(txt[len])
	{
		ptr[len] = txt[len];
		len++;
	}
	return len;
}

void fill_fw_services() 
{
	uint8_t er_key[34];
	uint8_t ir_key[34];
	spr(er_key, "1AEF158C32FC4CBBAA47AB29AA391AA0");
	spr(ir_key, "52C234877ABF6CEB8A82A649C3542953");
	ble_peripheral_set_ER(er_key);
	ble_peripheral_set_IR(ir_key);

	ble_peripheral_generate_mac(gen_mac);
	ble_set_our_mac(gen_mac);
	
	GAP_service.handle = 0x01;
	GAP_service.type = 0x2800;
	GAP_service.group_end = 0x0F;
	GAP_service.uuid_16 = 0x1800;
	ble_add_service(&GAP_service);
	
	FW_service.handle = 0x20;
	FW_service.type = 0x2800;
	FW_service.group_end = 0xFFFF;
	FW_service.uuid_16 = 0;
	//03B80E5A EDE84B33 A7516CE3 4EC4C700   
	ble_uuid_from_text(FW_service.uuid_128, "035A265E-3CA8-26EA-39C0-3CA36AE2F200");
	ble_add_service(&FW_service);

	gap_name_ch.handle = 0x02;
	gap_name_ch.value_handle = 0x03;
	gap_name_ch.uuid_16 = 0x2A00;
	gap_name_ch.descriptor_count = 0;
	gap_name_ch.val_length = spr(gap_name_ch.value, BLE_NAME_STRING);
	gap_name_ch.val_type = VALUE_TYPE_UTF8;
	gap_name_ch.properties = CHARACTERISTIC_READ;
	ble_add_characteristic(&gap_name_ch);
	GAP_service.char_idx[0] = gap_name_ch.mem_idx;
	
	
	gap_appearance_ch.handle = 0x04;
	gap_appearance_ch.value_handle = 0x05;
	gap_appearance_ch.uuid_16 = 0x2A01;
	gap_appearance_ch.descriptor_count = 0;
	gap_appearance_ch.val_length = 2;
	gap_appearance_ch.val_type = VALUE_TYPE_UINT16;
	gap_appearance_ch.value[0] = 0xC0;
	gap_appearance_ch.value[1] = 0x03;
	gap_appearance_ch.properties = CHARACTERISTIC_READ;
	ble_add_characteristic(&gap_appearance_ch);
	GAP_service.char_idx[1] = gap_appearance_ch.mem_idx;
	GAP_service.char_count = 2;
		
	fw_io_ch.handle = 0x21;
	fw_io_ch.value_handle = 0x22;
	fw_io_ch.descriptor_count = 1;
	fw_io_ch.descriptor_uuids[0] = 0x2902;
	fw_io_ch.descriptor_handles[0] = 0x23;
	fw_io_ch.descriptor_values[0] = 0;
	fw_io_ch.uuid_16 = 0;
	//7772E5DB 38684112 A1A9F266 9D106BF3
	ble_uuid_from_text(fw_io_ch.uuid_128, "772ACE23-4284-A634-8AE0-44AB38E1AC00");
	fw_io_ch.val_type = VALUE_TYPE_UTF8; //variable length actually
	fw_io_ch.val_length = 22;
	fw_io_ch.properties = CHARACTERISTIC_READ | CHARACTERISTIC_WRITE_NO_RESP | CHARACTERISTIC_NOTIFY;
	ble_add_characteristic(&fw_io_ch);
	FW_service.char_idx[0] = fw_io_ch.mem_idx;
	FW_service.char_count = 1;
}

static inline void start_code_test(uint32_t start_addr, uint32_t *ret)
{
	uint32_t saddr = start_addr;
	uint32_t raddr;
    __asm volatile(
			"mov   lr, %[saddr]\t\n"
			"MOV   R5, %[saddr]\t\n"
			"MOV   %[raddr], R5\t\n"
			: [raddr] "=r" (raddr)
			: [saddr] "r" (saddr)
    );
	*ret = raddr;
}

static inline void start_code2(uint32_t start_addr)
{
	uint32_t saddr = start_addr;
    __asm volatile(
			"mov   lr, %[saddr]\t\n"
			"MOV   R5, %[saddr]\t\n"
			"BX    R5\t\n"
			:
			: [saddr] "r" (saddr)
    );
}

int binary_exec(uint32_t address)
{
    int i;
    __disable_irq();
// Disable IRQs
	for (i = 0; i < 8; i ++) NVIC->ICER[i] = 0xFFFFFFFF;
// Clear pending IRQs
	for (i = 0; i < 8; i ++) NVIC->ICPR[i] = 0xFFFFFFFF;

// -- Modify vector table location
// Barriars
	__DSB();
	__ISB();
// Change the vector table
//	uint32_t adr = address>>1;
//	adr = adr<<1;
	SCB->VTOR = (BOOTLOADER_AREA_SIZE & 0x1ffff80);
//	SCB->VTOR = (0x20001000 & 0x1ffff80);
// Barriars 
	__DSB();
	__ISB();

	__enable_irq();

// -- Load Stack & PC
//	start_code_fix();
	uint32_t tst;
	start_code_test(address, &tst);
//	uprintf("asm code test: addr %d res %d\n", address, tst);
//	delay_ms(200);
	start_code2(address);
//	binexec(address);
	return 0;
}

#define FLASH_READFLAG 0x00
#define FLASH_WRITEFLAG 0x01
#define FLASH_ERASEFLAG 0x02


void erase_page(uint32_t page)
{
    // Turn on flash write
    while (!NRF_NVMC->READY) ;
    NRF_NVMC->CONFIG = FLASH_ERASEFLAG;
    while (!NRF_NVMC->READY) ;

    // Erase page.
    NRF_NVMC->ERASEPAGE = page;
    while (!NRF_NVMC->READY) ;

    NRF_NVMC->CONFIG = FLASH_READFLAG;
    while (!NRF_NVMC->READY) ;
}

void write_start()
{
    // Enable write.
    while (!NRF_NVMC->READY) ;
	NRF_NVMC->CONFIG = FLASH_WRITEFLAG; 
    while (!NRF_NVMC->READY) ;	
}

void write_word(uint32_t address, uint32_t value)
{
    *(uint32_t*)address = value;
    while (!NRF_NVMC->READY) ;
}
void write_end()
{
	NRF_NVMC->CONFIG = FLASH_READFLAG; 
    while (!NRF_NVMC->READY) ;
}


uint8_t uart_in_packet[300];
uint8_t pack_resp[16];
uint8_t radio_resp[16];

uint8_t last_uart_pos = 0;
uint32_t uart_buf_length;

int pack_length = 0;
int pack_state = 0;
int pack_begin = 0;

int code_uploading = 0;
int upload_complete = 0;

uint8_t upload_start_code[8] = {0x10, 0xFC, 0xA3, 0x05, 0xC0, 0xDE, 0x11, 0x77};
uint8_t upload_start_code_ble[8] = {0x10, 0xFC, 0xA3, 0x05, 0xC0, 0xDE, 0x11, 0xAA};

uint8_t pack_prefix[2] = {223, 98};

uint8_t last_pack_id = 0;
int remaining_bytes = 0;
uint32_t written_words = 0;

enum
{
	err_code_ok = 11,
	err_code_toolong = 100,
	err_code_wrongcheck,
	err_code_wronglen,
	err_code_packmiss
};

uint32_t bytes_to_uint32(uint8_t *bytes)
{
	return (bytes[0]<<24) | (bytes[1]<<16) | (bytes[2]<<8) | bytes[3];
}

void uart_cond_send(uint8_t *buf, uint8_t len)
{
#ifdef CFG_BOOTLOADING_UART_RX
	uart_send(buf, len);
#endif
	return;
}

uint8_t check_sum(uint8_t *pack, int len)
{
	uint8_t check_odd = 0;
	uint8_t check_tri = 0;
	uint8_t check_all = 0;
	for(int x = 0; x < len-3; x++)
	{
		if(x%2) check_odd += pack[x];
		if(x%3) check_tri += pack[x];
		check_all += pack[x];
	}
	uint8_t check_ok = 1;
	if(check_odd != pack[len-3]) check_ok = 0;
	if(check_tri != pack[len-2]) check_ok = 0;
	if(check_all != pack[len-1]) check_ok = 0;
	pack_resp[4] = check_odd;
	pack_resp[5] = check_tri;
	pack_resp[6] = check_all;
	return check_ok;
}

void parse_in_packet(int len, uint8_t *pack)
{
//	uprintf(" lastpos %d upos %d len %d\n", last_uart_pos, get_rx_position(), len);
//	if(len != 36)
	if(len < 6) //too short to consider if valid
	{
		pack_resp[0] = pack_prefix[0];
		pack_resp[1] = pack_prefix[1];
		pack_resp[2] = last_pack_id;
		pack_resp[3] = err_code_wronglen;
		uart_cond_send(pack_resp, 4);
		return; //accept only fixed length packets
	}
	uint8_t check_ok = check_sum(pack, len);
	if(code_uploading == 0)
	{
		int ppos = 0;
		last_pack_id = pack[ppos++];
		for(int n = 0; n < 8; n++)
		{
			if(upload_start_code[n] != pack[ppos++])
			{
//				uprintf("upload start code not found!\n");
				return; //no start code - ignore
			}
		}
		remaining_bytes = bytes_to_uint32(pack+ppos);// pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
						
		if(remaining_bytes >= 0x3C000) //too long
		{
			pack_resp[0] = pack_prefix[0];
			pack_resp[1] = pack_prefix[1];
			pack_resp[2] = last_pack_id;
			pack_resp[3] = err_code_toolong;
			uart_cond_send(pack_resp, 4);
			return;
		}
		if(!check_ok)
		{
			pack_resp[0] = pack_prefix[0];
			pack_resp[1] = pack_prefix[1];
			pack_resp[2] = last_pack_id;
			pack_resp[3] = err_code_wrongcheck;
			pack_resp[4] = len;
			uart_cond_send(pack_resp, 4);
			return;
		}
		uint32_t page_size = NRF_FICR->CODEPAGESIZE;
		uint32_t start_page = BOOTLOADER_AREA_SIZE / page_size;
		uint32_t end_page = (BOOTLOADER_AREA_SIZE + remaining_bytes) / page_size + 1;
//		uprintf("page size %d, start page %d, end page %d\n", page_size, start_page, end_page);
		for(int p = start_page; p < end_page; p++)
			erase_page(p*page_size);
		
		code_uploading = 1;
		written_words = 0;
		pack_resp[0] = pack_prefix[0];
		pack_resp[1] = pack_prefix[1];
		pack_resp[2] = last_pack_id;
		pack_resp[3] = err_code_ok;
		pack_resp[4] = 102; //bootloader version
		uart_cond_send(pack_resp, 4);
	}
	else
	{
		if(!check_ok)
		{
			pack_resp[0] = pack_prefix[0];
			pack_resp[1] = pack_prefix[1];
			pack_resp[2] = last_pack_id;
			pack_resp[3] = err_code_wrongcheck;
			uart_cond_send(pack_resp, 4);
			return;
		}
		
		uint8_t exp_pack = last_pack_id+1;
		int ppos = 0;
		if(exp_pack != pack[ppos++])
		{
			pack_resp[0] = pack_prefix[0];
			pack_resp[1] = pack_prefix[1];
			pack_resp[2] = last_pack_id;
			pack_resp[3] = err_code_packmiss;
			pack_resp[4] = 102; //bootloader version
			uart_cond_send(pack_resp, 4);
//			delay_ms(5);
			return;
		}
		last_pack_id = exp_pack;
		
		uint32_t code_words[64];
		int words_count = (len-4)/4;
		for(int x = 0; x < words_count; x++)
		{
			code_words[x] = bytes_to_uint32(pack+ppos);
			ppos += 4;
		}

		write_start();
		for(int x = 0; x < words_count; x++)
		{
			write_word(BOOTLOADER_AREA_SIZE + 4*written_words, code_words[x]);

			written_words++;
			remaining_bytes -= 4;
			
			if(remaining_bytes <= 0) break;			
		}
		write_end();

		if(remaining_bytes <= 0)
		{
			code_uploading = 0;
			upload_complete = 1;
		}

		pack_resp[0] = pack_prefix[0];
		pack_resp[1] = pack_prefix[1];
		pack_resp[2] = last_pack_id;
		pack_resp[3] = err_code_ok;
		uart_cond_send(pack_resp, 4);
	}
}

uint32_t launch_address = 0;

void SystemInit(void)
{
//    SystemCoreClock = __SYSTEM_CLOCK_64M;
}

enum
{
	berr_ok = 11,
	berr_wrongcode = 100,
	berr_toolong,
	berr_notstarted,
	berr_wrongpack_ahead,
	berr_wrongpack_behind,
	berr_wrongpack_duplicate
};
uint8_t ble_boot_response[32];

void ble_boot_resp_err(int code)
{
	for(int x = 0; x < 16; x++)
		ble_boot_response[x] = 0;
	ble_boot_response[0] = code;
	ble_boot_response[1] = last_pack_id;
}

void process_ble_boot_pack(uint8_t *pack)
{
//	uprintf(" lastpos %d upos %d len %d\n", last_uart_pos, get_rx_position(), len);
	if(code_uploading == 0)
	{
		int ppos = 0;
		last_pack_id = pack[ppos++];
		for(int n = 0; n < 8; n++)
		{
			if(upload_start_code_ble[n] != pack[ppos++])
			{
				ble_boot_resp_err(berr_wrongcode);
//				uprintf("upload start code not found!\n");
				return; //no start code - ignore
			}
		}
		remaining_bytes = bytes_to_uint32(pack+ppos);
//		remaining_bytes = pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
//		remaining_bytes <<= 8;
//		remaining_bytes += pack[ppos++];
		
		if(remaining_bytes >= 0x3C000) //too long
		{
			ble_boot_resp_err(berr_toolong);
			return;
		}
		uint32_t page_size = NRF_FICR->CODEPAGESIZE;
		uint32_t start_page = BOOTLOADER_AREA_SIZE / page_size;
		uint32_t end_page = (BOOTLOADER_AREA_SIZE + remaining_bytes) / page_size + 1;
//		uprintf("page size %d, start page %d, end page %d\n", page_size, start_page, end_page);
		for(int p = start_page; p < end_page; p++)
			erase_page(p*page_size);
		
		code_uploading = 1;
		written_words = 0;
		ble_boot_resp_err(berr_ok);
	}
	else
	{
		uint8_t exp_pack = last_pack_id+1;
		int ppos = 0;
		int pack_id = pack[ppos++];
		if(exp_pack != pack_id)
		{
			for(int t = -10; t < 10; t++)
			{
				uint8_t tpack = last_pack_id + t;
				if(tpack == pack_id)
				{
					if(t < 0)
					{
						ble_boot_resp_err(berr_wrongpack_behind);
						return;						
					}
					else if(t == 0)
					{
						ble_boot_resp_err(berr_wrongpack_duplicate);
						return;						
					}
					else
					{
						ble_boot_resp_err(berr_wrongpack_ahead);
						return;
					}
				}
			}
			ble_boot_resp_err(berr_wrongcode);
			return;
		}
		last_pack_id = exp_pack;
		
		uint32_t code_words[5];
		for(int x = 0; x < 4; x++)
		{
			code_words[x] = pack[ppos++];
			code_words[x] |= pack[ppos++]<<8;
			code_words[x] |= pack[ppos++]<<16;
			code_words[x] |= pack[ppos++]<<24;
			
//			code_words[x] = pack[ppos++];
//			code_words[x] <<= 8;
//			code_words[x] += pack[ppos++];
//			code_words[x] <<= 8;
//			code_words[x] += pack[ppos++];
//			code_words[x] <<= 8;
//			code_words[x] += pack[ppos++];
		}

		write_start();
		for(int x = 0; x < 4; x++)
		{
			write_word(BOOTLOADER_AREA_SIZE + 4*written_words, code_words[x]);

			written_words++;
			remaining_bytes -= 4;
			
			if(remaining_bytes <= 0) break;
		}
		write_end();

		if(remaining_bytes <= 0)
		{
			code_uploading = 0;
			upload_complete = 1;
		}
		ble_boot_resp_err(berr_ok);
	}
}

uint32_t last_adv_event_fw = 0;
int next_rnd = 10;
uint32_t ble_last_rcvd = 0;

void process_ble_bootloader()
{
	uint32_t ms = millis();
	if(ble_get_conn_state() == 0) 
	{
		if(ms - last_adv_event_fw > 200 + next_rnd)
		{
			next_rnd = micros()%30;
			last_adv_event_fw = ms;
			uint8_t pdu[40];
			uint8_t payload[40];
			payload[0] = gen_mac[0];
			payload[1] = gen_mac[1];
			payload[2] = gen_mac[2];
			payload[3] = gen_mac[3];
			payload[4] = gen_mac[4];
			payload[5] = gen_mac[5];

			int pp = 6;
			payload[pp++] = 0x02;
			payload[pp++] = 0x01; //flags
			payload[pp++] = 0b0110; //general discovery, br/edr not supported
			payload[pp++] = 0x02;
			payload[pp++] = 0x0A; //power level
			payload[pp++] = 0x04; //4 dBm

			static int send_fw_uuid = 0;
			
			if(!send_fw_uuid)
			{
				payload[pp] = 0;
				payload[pp+1] = 0x08;
				int len;
				len = spr(payload + pp + 2, "uECG boot");
				payload[pp] = len + 1;
				pp += len + 2;
			}
			else
			{
				payload[pp++] = 17;
				payload[pp++] = 0x07;
				for(int n = 0; n < 16; n++)
					payload[pp++] = FW_service.uuid_128[n];
			}
			send_fw_uuid = !send_fw_uuid;
			
			payload[pp++] = 0;//37-2-6-3-6-1;
	//			payload[pp++] = 0xFF;
			
			static int adv_ch = 37;
			
			int len = ble_prepare_adv_pdu(pdu, 35, payload, BLE_ADV_IND_TYPE, 1, 1);
			for(int x = 0; x < 3; x++)
			{
				ble_LL_send_PDU(0x8E89BED6, len, pdu, adv_ch);
				adv_ch++;
				if(adv_ch > 39) adv_ch = 37;
				delay_ms(15);
				if(ble_get_conn_state() != 0) break;				
			}
	//			adv_disc_packet_send();
		}
	}
	else
	{ //connected state - transport MIDI messages to stm
	
		if(fw_io_ch.had_write)
		{
			ble_last_rcvd = millis();
			uint8_t val_buffer[32];
			__disable_irq();
			for(int x = 0; x < 32; x++)
				val_buffer[x] = fw_io_ch.value[x]; //could be changed in BLE interrupt, this way it won't happen
			__enable_irq();
			process_ble_boot_pack(val_buffer);
//			uprintf("midi write rcvd %d\n", midi_io_ch.val_length);
			fw_io_ch.had_write = 0;
			for(int x = 0; x < 17; x++)
				fw_io_ch.value[x] = ble_boot_response[x];
			fw_io_ch.changed = 1;
		}
	}
}

#ifdef CFG_BOOTLOADING_UART_RX

void check_uart_packet()
{
	uint8_t pos = last_uart_pos;
	uint8_t upos = uart_get_rx_position();
	if(pos == upos) return;
	int unproc_length = 0;
	unproc_length = upos - pos;
	last_uart_pos = upos;
	uart_buf_length = uart_get_rx_buf_length();
	if(unproc_length < 0) unproc_length += uart_buf_length;
	uint8_t *ubuf = uart_get_rx_buf();
	
	for(uint8_t pp = pos; pp != upos; pp++)
	{
//		if(pp >= uart_buf_length) pp = 0;
		if(pack_state == 0 && ubuf[pp] == pack_prefix[0])
		{
			pack_state = 1;
			continue;
		}
		if(pack_state == 1)
		{
			if(ubuf[pp] == pack_prefix[1]) pack_state = 2;
			else pack_state = 0;
			continue;
		}
		if(pack_state == 2)
		{
			pack_length = ubuf[pp];
			if(pack_length >= uart_buf_length-2) //something is wrong or too long packet
			{
				pack_length = 0;
				pack_state = 0;
				continue;
			}
			pack_begin = pp+1;
			if(pack_begin >= uart_buf_length) pack_begin = 0;
			pack_state = 3;
			continue;
		}
		if(pack_state == 3)
		{
			int len = pp - pack_begin;
			if(len < 0) len += uart_buf_length;
			if(len >= pack_length)
			{
				uint8_t bg = pack_begin;
				for(int x = 0; x < pack_length; x++)
				{
					uart_in_packet[x] = ubuf[bg++];
					//if(bg >= uart_buf_length) bg = 0;
				}
				parse_in_packet(pack_length, uart_in_packet);
				pack_state = 0;
				pack_length = 0;
			}
		}
	}
}

#endif

uint8_t btn_debounce_read()
{
	if(button_pin < 0) return 0;
	uint32_t ms = millis();
	static uint32_t prev_bt_change = 0;
//	return 1;
//	if(NRF_GPIO->IN & 1<<pin_button)
	uint8_t bt_state = ((NRF_GPIO->IN & 1<<button_pin)>0);
	if(CFG_BUTTON_PRESSED_LOW) bt_state = !bt_state;
	static uint8_t filtered_bt_state = 0;
	static uint8_t prev_sw_state = 0;
	if(bt_state != prev_sw_state)
	{
		prev_bt_change = ms;
		prev_sw_state = bt_state;
	}
	if(filtered_bt_state != prev_sw_state && ms - prev_bt_change > 20)
	{
		filtered_bt_state = prev_sw_state;
	}
//	if(filtered_bt_state) NRF_GPIO->OUTCLR = 1<<led_pin;
//	else NRF_GPIO->OUTSET = 1<<led_pin;
	return filtered_bt_state;
}

int main() 
{ 
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {}

	int sys_on_pin = 0xFF;

	sys_on_pin = CFG_SYSON_PIN;
	button_pin = CFG_BUTTON_PIN;
	led_pin = CFG_LED_PIN;
	if(sys_on_pin >= 0)
	{
		NRF_GPIO->DIRSET = 1<<sys_on_pin;
		NRF_GPIO->OUTSET = 1<<sys_on_pin;		
	}
	if(button_pin >= 0 && CFG_BUTTON_PULL > 0) NRF_GPIO->PIN_CNF[button_pin] = CFG_BUTTON_PULL<<2;
	if(button_pin >= 0 && CFG_BUTTON_PULL == 0) NRF_GPIO->PIN_CNF[button_pin] = 0;
	if(led_pin >= 0)
	{
		NRF_GPIO->DIRSET = 1<<led_pin;
		NRF_GPIO->OUTCLR = 1<<led_pin;
	}

#ifdef UECG_BUILD 
	uint8_t version_id_pin = 22;
	NRF_GPIO->PIN_CNF[version_id_pin] = 0b1100;
	uint8_t uecg_version;
	int cnt_up = 0;
	for(int x = 0; x < 10; x++)
		if(NRF_GPIO->IN & (1<<version_id_pin)) cnt_up++;
	if(cnt_up > 8) //high state -> old version
		uecg_version = 4;
	else
		uecg_version = 5; //only two possibilities as of now

	if(uecg_version == 4)
	{
		button_pin = 26;
		button_invert = 1;
	}
	if(uecg_version == 5)
	{ 
		button_pin = 27;
		sys_on_pin = 24;
		NRF_GPIO->DIRSET = 1<<sys_on_pin;
		NRF_GPIO->OUTSET = 1<<sys_on_pin;		
	}
	if(uecg_version == 4)
		NRF_GPIO->PIN_CNF[button_pin] = 0b1100;
	if(uecg_version == 5)
		NRF_GPIO->PIN_CNF[button_pin] = 0;
#endif		

//	binary_exec(launch_address);
	time_start();
	delay_ms(10);

#ifdef CFG_BOOTLOADING_UART_RX
	if(CFG_BOOTLOADING_UART_RX >= 0 && CFG_BOOTLOADING_UART_TX >= 0)
		uart_init(CFG_BOOTLOADING_UART_TX, CFG_BOOTLOADING_UART_RX, 115200);
#endif
/*
	rf_init(21, 250, 1); //compatibility with old version
//	fr_init(64);
	rf_listen();
*/	
	uint32_t boot_start = millis();
	int pack_st = 0;
	int boot_requested = 0;
	uint8_t btn_state = 0;
	uint32_t btn_up_time = 0;
	uint32_t btn_down_time = 0;
	uint32_t rf_last_rcvd = 0;
	uint8_t rf_boot_mode = 0;
	uint8_t ble_boot_mode = 0;
	btn_debounce_read();
	
	if(button_pin >= 0)
		while(millis() - boot_start < 300 || btn_debounce_read())
	{
		btn_debounce_read();
		if(millis() - boot_start > CFG_BOOT_REQUEST_TIME)
		{
			boot_requested = 1;
			break;
		}
	}
	else
	{
		while(millis() - boot_start < 300) ;
	}

	if(button_pin < 0)
			boot_requested = 1;
	if(button_pin >= 0)
	{
		btn_state = btn_debounce_read();
//		if(button_invert) btn_state = !btn_state;
	}
	
	if(boot_requested)
	{
		if(CFG_BOOTDEFAULT_BLE)
		{
			ble_boot_mode = 1;
			ble_init_radio();
			fill_fw_services();
		}
		else
		{
			rf_boot_mode = 1;
			schedule_event_stop();
			rf_disable();
			NRF_RADIO->POWER = 0;
			delay_ms(2);
			rf_init(21, 1000, 3); //not compatible with old versions
			delay_ms(2);
			rf_listen();			
		}
	}
		
	while(boot_requested && (millis() - boot_start < CFG_BOOT_REQUEST_TIMEOUT || code_uploading))
	{
		if(upload_complete) break;
		uint8_t had_btn_press = 0;
		uint8_t cur_btn_state = btn_debounce_read();// (NRF_GPIO->IN & (1<<button_pin))>0;
//		if(button_invert) cur_btn_state = !cur_btn_state;		
		if(button_pin >= 0)
		{
			if(cur_btn_state && !btn_state) //pressed
				btn_down_time = millis();
			if(!cur_btn_state && btn_state) //released
			{
				btn_up_time = millis();
				if(btn_up_time > btn_down_time + 50) //debounce
					had_btn_press = 1;
			}
		}
		btn_state = cur_btn_state;

		if(had_btn_press && btn_down_time > CFG_BOOT_REQUEST_TIME && CFG_BOOT_MODE_SELECT_ENABLE)
		{
			if(ble_boot_mode)
			{
				ble_boot_mode = 0;
				rf_boot_mode = 1;
				schedule_event_stop();
				rf_disable();
				NRF_RADIO->POWER = 0;
				delay_ms(2);
				rf_init(21, 1000, 3); //not compatible with old versions
				delay_ms(2);
	//			rf_init_ext(21, 250, 1, 0, 8, 40, 48); //compatibility with old version
	//			rf_init_ext(21, 250, 1, 0, 8, 64, 64);
				rf_listen();
			}
			else if(rf_boot_mode)
			{
				rf_boot_mode = 0;
				ble_boot_mode = 1;
				schedule_event_stop();
				rf_disable();
				NRF_RADIO->POWER = 0;
				delay_ms(2);
				ble_init_radio();
				fill_fw_services();
			}
		}
#ifdef CFG_BOOTLOADING_UART_RX
		if(CFG_BOOTLOADING_UART_RX >= 0 && CFG_BOOTLOADING_UART_TX >= 0)
			check_uart_packet();
#endif
		uint32_t ms = millis();
		uint32_t last_active_time = ble_last_rcvd;
		if(rf_boot_mode) last_active_time = rf_last_rcvd;
		if(ms - last_active_time < 1000)
		{
			if((ms%200) < 100)
				NRF_GPIO->OUTCLR = 1<<led_pin;
			else
				NRF_GPIO->OUTSET = 1<<led_pin;
		}
		else
		{
			if((ms%1000) < 500)
				NRF_GPIO->OUTCLR = 1<<led_pin;
			else
				NRF_GPIO->OUTSET = 1<<led_pin;
		}

		if(ms - boot_start > CFG_BOOTLOADING_TIMEOUT) break; //something went wrong with bootloader, breaking

		if(!rf_boot_mode)
		{
			process_ble_bootloader();
		}
		if(rf_boot_mode)
		{
			uint32_t ms = millis();
			if(rf_has_new_packet())
			{
				if(pack_st)
					NRF_GPIO->OUTCLR = 1<<led_pin;				
				else
					NRF_GPIO->OUTSET = 1<<led_pin;				
				pack_st = !pack_st;
				int rf_pack_len = rf_get_packet(uart_in_packet);
				parse_in_packet(rf_pack_len-2, uart_in_packet+2);
				radio_resp[0]++;
				radio_resp[1] = 10;
				for(int x = 0; x < 8; x++)
					radio_resp[2+x] = pack_resp[x];
				rf_send_and_listen(radio_resp, radio_resp[1]);
				rf_last_rcvd = ms;
			}
		}
	}
	
	NRF_GPIO->DIRCLR = 1<<led_pin;
	rf_disable();
	launch_address = *(uint32_t*)(BOOTLOADER_AREA_SIZE+4);
	delay_ms(100);
	
#ifdef CFG_BOOTLOADING_UART_RX
	uprintf("boot start at 0x%X\n", launch_address);
#endif		
//	process_fwupdate();
//	uprintf("resetting...\n");
	delay_ms(50);
	time_stop();
	binary_exec(launch_address);
}

