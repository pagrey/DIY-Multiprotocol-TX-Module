/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Multiprotocol is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Multiprotocol.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(MULTI_CONFIG_INO)

#ifdef CYRF6936_INSTALLED
#include "iface_cyrf6936.h"
#endif

void MULTI_write_ID(uint8_t *data, uint16_t addr, uint8_t n)
{
	for(uint8_t i=0; i<n; i++)
		eeprom_write_byte((EE_ADDR)addr+i,data[i]);
}
void MULTI_read_ID(uint8_t *packet, uint16_t addr, uint8_t start, uint8_t n)
{
	for(uint8_t i=0; i<n; i++)
		packet[start+i]=eeprom_read_byte((EE_ADDR)addr+i);

}
struct {
	uint8_t rxid_block; // eeprom memory block as defined in the Protocol_Offset array
	uint8_t number; // rx number
} active;
typedef struct rx_id_data {
	uint16_t addr; // eeprom start address of the rx id
	uint8_t len;  // length of the rx id
};
// Protocol_Offset contains the primary offset, secondary offset, bytes per model
const uint16_t Protocol_Offset[8][4] = {{AFHDS2A_EEPROM_OFFSET, AFHDS2A_EEPROM_OFFSET2, 4, PROTO_AFHDS2A},
					{BUGS_EEPROM_OFFSET, 0, 2, PROTO_BUGS},
					{BUGSMINI_EEPROM_OFFSET, 0, 2, PROTO_BUGSMINI},
					{TRAXXAS_EEPROM_OFFSET, 0, 3, PROTO_KYOSHO},
					{HOTT_EEPROM_OFFSET, 0, 5, PROTO_HOTT},
					{MOULDKG_EEPROM_OFFSET, 0, 3, PROTO_MOULDKG},
					{XK2_EEPROM_OFFSET, 0, 1, PROTO_XK2},
					{JIABAILE_EEPROM_OFFSET, 0, 3, PROTO_JIABAILE}};
void MULTI_get_RX_data(struct rx_id_data *rdata)
{
	rdata->len=Protocol_Offset[active.rxid_block][2];
	if(Protocol_Offset[active.rxid_block][1] > 0 && (active.number > 15))
		rdata->addr=Protocol_Offset[active.rxid_block][1]+(active.number-16)*Protocol_Offset[active.rxid_block][2];
	else
		rdata->addr=Protocol_Offset[active.rxid_block][0]+active.number*Protocol_Offset[active.rxid_block][2];
}
uint16_t CONFIG_callback()
{
	static uint8_t line=0, page=0;
	// [0] = page<<4|line number
	// [1..6] = max 6 bytes
	if(CONFIG_SerialRX)
	{
		debug("config");
		for(uint8_t i=0; i<7; i++)
			debug("%02X ",CONFIG_SerialRX_val[i]);
		debugln("");
		CONFIG_SerialRX = false;
		switch(CONFIG_SerialRX_val[0]&0x0F)
		{
			//case 0:
				// Page change
			//	break;
			case 1:
				debug("Update GID to ");
				for(uint8_t i=0; i<4; i++)
					debug("%02X ",CONFIG_SerialRX_val[1+i]);
				debugln("");
				MULTI_write_ID(&CONFIG_SerialRX_val[1], EEPROM_ID_OFFSET, 4);
				break;
			case 2:
				if(CONFIG_SerialRX_val[1]==0xAA)
				{
					uint8_t stm_data[4];
					#define STM32_UUID ((uint32_t *)0x1FFFF7E8)
					uint32_t id = STM32_UUID[0] ^ STM32_UUID[1] ^ STM32_UUID[2];
					memcpy(stm_data,&id,4);
					debugln("Reset GID to %lx", id);
					MULTI_write_ID(stm_data, EEPROM_ID_OFFSET, 4);
				}
				break;
#ifdef CYRF6936_INSTALLED
			case 3:
				debug("Update CID to ");
				for(uint8_t i=0; i<6; i++)
					debug("%02X ",CONFIG_SerialRX_val[1+i]);
				debugln("");
				MULTI_write_ID(&CONFIG_SerialRX_val[1], EEPROM_CID_OFFSET, 6);
				break;
			case 4:
				if(CONFIG_SerialRX_val[1]==0xAA)
				{
					uint8_t cyrf_data[6];
					CYRF_WriteRegister(CYRF_25_MFG_ID, 0xFF);	/* Fuses power on */
					CYRF_ReadRegisterMulti(CYRF_25_MFG_ID, cyrf_data, 6);
					CYRF_WriteRegister(CYRF_25_MFG_ID, 0x00);	/* Fuses power off */
					debug("Reset CID to ");
					for(uint8_t i=0; i<6; i++)
						debug("%02X ",cyrf_data[i]);
					debugln("");	
					MULTI_write_ID(cyrf_data, EEPROM_CID_OFFSET, 6);
				}
				break;
#endif
			case 5:
				active.rxid_block = CONFIG_SerialRX_val[1];
				if (active.rxid_block > (sizeof(Protocol_Offset) - 1))
					active.rxid_block = sizeof(Protocol_Offset) - 1;
				debug("Update offset to ");
				debug("%02X ",active.number);
				debugln("");
				active.number = CONFIG_SerialRX_val[2];
				if (active.number > 0x40)
					active.number = 0x40;
				debug("Update RX number to ");
				debug("%02X ",active.number);
				debugln("");
				break;
			case 6:
			 	struct rx_id_data active_data;
				MULTI_get_RX_data(&active_data);
				debug("Update RXID to ");
				for(uint8_t i=0; i<active_data.len; i++)
					debug("%02X ",CONFIG_SerialRX[1+i]);
				debugln("");
				MULTI_write_ID(&CONFIG_SerialRX_val[1], active_data.addr, active_data.len);
				break;
			case 7:
				if(CONFIG_SerialRX_val[1]==0xAA)
				{
					debugln("Format EE");
					#if defined(STM32_BOARD)
						EEPROM.format();
					#else
						for (uint16_t i = 0; i < 512; i++)
    							eeprom_write_byte((EE_ADDR)i, 0xFF);
					#endif
				}
				break;
		}
	}

	if(	telemetry_link )
		return 10000;
	// [0] = page<<4|line number
	// line=0: VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_PATCH_LEVEL, Channel order:RUDDER<<6|THROTTLE<<4|ELEVATOR<<2|AILERON
	// [1..21] = max 20 characters, any displayable chars followed by:
	//    0x00    : end of line
	//    0x80+len:  selectable text to follow
	//    0x90+len:  selectable text to follow with "Are you sure?"
	//    0xA0+len:  not editable dec value
	//    0xB0+len:  editable dec value
	//    0xC0+len:  not editable hex value
	//    0xD0+len:  editable hex value
	memset(&packet_in[1],0,20);
	do
	{
		packet_in[0] = (page<<4) | line;
		switch(line)
		{
			case 0:
				packet_in[1]=VERSION_MAJOR;
				packet_in[2]=VERSION_MINOR;
				packet_in[3]=VERSION_REVISION;
				packet_in[4]=VERSION_PATCH_LEVEL;
				packet_in[5]=RUDDER<<6|THROTTLE<<4|ELEVATOR<<2|AILERON;
				break;
			case 1:
				//Global ID
				memcpy(&packet_in[1],"GID",3);
				#ifndef FORCE_GLOBAL_ID
					packet_in[4] = 0xD0 + 4;
				#else
					packet_in[4] = 0xC0 + 4;
				#endif
				MProtocol_id_master = random_id(EEPROM_ID_OFFSET,false);
				set_rx_tx_addr(MProtocol_id_master);
				for(uint8_t i=0; i<4; i++)
					packet_in[8-i]=rx_tx_addr[i];
				break;
			#if defined(STM32_BOARD) && not defined(FORCE_GLOBAL_ID)
			case 2:
				//Reset global ID
				packet_in[1] = 0x90+9;
				memcpy(&packet_in[2],"Reset GID",9);
				break;
			#endif
#ifdef CYRF6936_INSTALLED
			case 3:
				//Cyrf ID
				memcpy(&packet_in[1],"CID",3);
				#ifndef FORCE_CYRF_ID
					packet_in[4] = 0xD0 + 6;
				#else
					packet_in[4] = 0xC0 + 6;
				#endif
				CYRF_GetMfgData(&packet_in[5]);
				break;
			#ifndef FORCE_CYRF_ID
			case 4:
				//Reset Cyrf ID
				packet_in[1] = 0x90+9;
				memcpy(&packet_in[2],"Reset CID",9);
				break;
			#endif
#endif
			case 5:
				//Protocol Number
				for (uint8_t i = 0; multi_protocols[i].protocol != 0xFF; i++) {
					if (Protocol_Offset[active.rxid_block][3] == multi_protocols[i].protocol) {
						memcpy(&packet_in[1],multi_protocols[i].ProtoString,strlen(multi_protocols[i].ProtoString));
						memcpy(&packet_in[strlen(multi_protocols[i].ProtoString)+1]," RX",3);
						packet_in[strlen(multi_protocols[i].ProtoString)+4] = 0xB0+2;
						packet_in[strlen(multi_protocols[i].ProtoString)+5] = active.rxid_block;
						//RX Number
						packet_in[strlen(multi_protocols[i].ProtoString)+6] = active.number;
						memcpy(&packet_in[strlen(multi_protocols[i].ProtoString)+7],"line",4);
						break;
					}
					else if (multi_protocols[i+1].protocol == 0xFF) {
						memcpy(&packet_in[1],"Unknown:RX",7);
						packet_in[8] = 0xB0+2;
						packet_in[9] = active.rxid_block;
						//RX Number
						packet_in[12] = active.number;
						break;
					}
				}
				break;
			case 6:
				//RX ID
			 	struct rx_id_data active_data;
				MULTI_get_RX_data(&active_data);
				memcpy(&packet_in[1],"ID",2);
				packet_in[3] = 0xD0 + active_data.len;
				MULTI_read_ID(packet_in, active_data.addr, 4, active_data.len);
				break;
			case 7:
				packet_in[1] = 0x90+13;
				memcpy(&packet_in[2],"Format EEPROM",13);
				break;
		}
		line++;
		line %= 8;
	}
	while(packet_in[1]==0);	// next line if empty
	telemetry_link = 1;
	return 10000;
}

void CONFIG_init()
{
	active.number = RX_num;
	active.rxid_block = 0;
}

#endif
