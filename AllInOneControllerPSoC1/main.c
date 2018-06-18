//----------------------------------------------------------------------------
// C main line
//----------------------------------------------------------------------------

#include <m8c.h>        // part specific constants and macros
#include "PSoCAPI.h"    // PSoC API definitions for all User Modules
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int commandId = 0;
int commandGuid = 0;
BOOL commandReady = FALSE;
BYTE sh;
BOOL bDeviceEnumerated;
unsigned char bCtr;
//int dataIndex = -1;

/* Array of Keycode information to send to PC */ 
static BYTE Keyboard_Data[8] = {0, 0, 0, 0, 0, 0, 0, 0};  

/*Mouse Data to send to PC */
static BYTE Mouse_Data[3] = {0, 0, 0}; /* [0] = Buttons, [1] = X-Axis, [2] = Y-Axis */

/*Gamepad Data to send to PC */
static BYTE Gamepad_Data[3] = {0, 0, 0}; /*[0] = X-Axis, [1] = Y-Axis, [2] = Buttons */

#define GAMEPAD_DATA_LENGTH     3
#define MOUSE_DATA_LENGTH       3
#define KEYBOARD_DATA_LENGTH    8

//#define DEBUG
//#define DEBUG_INFO

//void ProcessByte(BYTE b);
void EnumerateDevice(void);
void CheckCommand(void);
void CheckKeyboardCommand(void);
void CheckMouseCommand(void);
void CheckGamepadCommand(void);
void CheckRenameCommand(void);
void WriteCommandResponse(void);
int hex2data(char *data, char *hexstring);

#ifdef DEBUG	

void DebugWriteKeyboard(void);
void DebugWriteMouse(void);
void DebugWriteGamepad(void);
void DebugWriteUNKNOWN(void);
void DebugWriteRx(BYTE b);
void DebugWriteParam(char* s);

#endif 

void main(void)
{
	UART_Start(UART_PARITY_NONE);
    UART_EnableInt();
	UART_IntCntl(UART_ENABLE_RX_INT);

	M8C_EnableGInt ;
	INT_MSK2 |= 0x02;				//Enable SOF interrupts and Sleep interrupts
	INT_MSK0 |= 0x40;	

    USBFS_Start(0, USB_5V_OPERATION);
	bDeviceEnumerated = FALSE;
	EnumerateDevice();

	UART_CmdReset();
	
	//Set device name on start
	UART_CPutString("AT+NAMEMyTeletouch\r\n");
	
	UART_CmdReset();
	
	while (1)
	{
		EnumerateDevice();
		
		//ProcessByte(UART_cReadChar());
		CheckCommand();
		
		if (commandReady)
		{
			commandReady = FALSE;
			
			switch (commandId)
			{
				case 1://Keyboard
#ifdef DEBUG	
					DebugWriteKeyboard();
#endif 
					commandId = 0;

					//while(!USBFS_bGetEPAckState(1)); /* Wait for ACK before loading data */
					while(USBFS_bGetEPState(1) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
		            USBFS_LoadInEP(1, Keyboard_Data, KEYBOARD_DATA_LENGTH, USB_TOGGLE);     

					WriteCommandResponse();					
					break;
				case 2://Mouse
#ifdef DEBUG	
					DebugWriteMouse();
#endif
					commandId = 0;
		
					//while(!USBFS_bGetEPAckState(2)); /* Wait for ACK before loading data */
					while(USBFS_bGetEPState(2) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
		            USBFS_LoadInEP(2, Mouse_Data, MOUSE_DATA_LENGTH, USB_TOGGLE); 

					WriteCommandResponse();					
					break;

				case 3://Gamepad
#ifdef DEBUG	
					DebugWriteGamepad();
#endif
					commandId = 0;
		
					//while(!USBFS_bGetEPAckState(3)); /* Wait for ACK before loading data */
					while(USBFS_bGetEPState(3) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
		            USBFS_LoadInEP(3, Gamepad_Data, GAMEPAD_DATA_LENGTH, USB_TOGGLE);     

					WriteCommandResponse();					
					break;
				case 13://Rename
					commandId = 0;
					UART_CPutString("AT+NAMEMyTeletouch\r\n");
					break;
				default:
					commandId = 0;
#ifdef DEBUG	
					DebugWriteUNKNOWN();
#endif
					break;
			}
		}
	}
}

#pragma interrupt_handler Interrupt
void Interrupt(void)
{
	bCtr++;
	
	if(bCtr >2)
	{
		bDeviceEnumerated = FALSE;
	}
}

#pragma interrupt_handler SOF_ISR
void SOF_ISR(void)
{
	bCtr = 0; //clear the monitoring counter on every SOF packet

}

void EnumerateDevice(void)
{
	if (!bDeviceEnumerated)
	{
		bDeviceEnumerated = TRUE;

		while(!USBFS_bGetConfiguration());

		while(USBFS_bGetEPState(1) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
	    /*Begins Keyboard USB Traffic*/ 
	    USBFS_LoadInEP(1, Keyboard_Data, KEYBOARD_DATA_LENGTH, USB_NO_TOGGLE);     

		while(USBFS_bGetEPState(2) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
	    /*Begins Mouse USB Traffic*/ 
	    USBFS_LoadInEP(2, Mouse_Data, MOUSE_DATA_LENGTH, USB_NO_TOGGLE); 
	    
		while(USBFS_bGetEPState(3) != IN_BUFFER_EMPTY); /* Wait for ACK before loading data */
	    /*Begins Gamepad USB Traffic*/ 
	    USBFS_LoadInEP(3, Gamepad_Data, GAMEPAD_DATA_LENGTH, USB_NO_TOGGLE);
		
		bDeviceEnumerated = TRUE;
	}
}

void WriteCommandResponse(void)
{
	char str[16];
	int len = csprintf(str, "4|%d|1]", commandGuid);
	UART_PutChar(len);
	UART_PutString(str);
}

void CheckCommand(void)
{
	char *pCmd;
	
	if (UART_bCmdCheck() != 0)
	{	
		pCmd = UART_szGetParam();
		commandId = 0;

#ifdef DEBUG_INFO
		DebugWriteParam(pCmd);
#endif
		
		if (cstrcmp("1", pCmd) == 0)
		{
			CheckKeyboardCommand();
		}
		else if (cstrcmp("2", pCmd) == 0)
		{
			CheckMouseCommand();
		}
		else if (cstrcmp("3", pCmd) == 0)
		{	
			CheckGamepadCommand();
		}
		else if (cstrcmp("13", pCmd) == 0)
		{	
			CheckRenameCommand();
		}
		
		UART_CmdReset();
	}
}

void CheckRenameCommand(void)
{
	char *pArg;
	int i = 0;
	int endCommandGuid = 0;

	commandId = 13;
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    commandGuid = atoi(pArg);
	else 
		commandGuid = -1;
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    endCommandGuid = atoi(pArg);
	else 
		endCommandGuid = -1;

	commandReady = commandGuid == endCommandGuid && commandGuid >= 0;
}

void CheckKeyboardCommand(void)
{
	char *pPrevArg;
	char *pArg;
	int i = 0;
	int endCommandGuid = 0;
	BOOL isEndCommandSet = FALSE;
	
	commandId = 1;
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO	
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    commandGuid = atoi(pArg);
	else 
		commandGuid = -1;
	
	for (i = 0; i < KEYBOARD_DATA_LENGTH; i++)
	{
		pArg = UART_szGetParam();
#ifdef DEBUG_INFO
		DebugWriteParam(pArg);
#endif
		sh = 0;
		if (pArg != NULL)
		    hex2data(&sh, pArg);
		else if (pPrevArg != NULL)
		{
			Keyboard_Data[i - 1] = 0;
			isEndCommandSet = TRUE;
		    endCommandGuid = atoi(pPrevArg);
		}

		Keyboard_Data[i] = sh;
#ifdef DEBUG_INFO
		DebugWriteRx(sh);
#endif
		pPrevArg = pArg;
	}
	
	if (!isEndCommandSet)
	{
		pArg = UART_szGetParam();
#ifdef DEBUG_INFO
		DebugWriteParam(pArg);
#endif
		if (pArg != NULL)
		    endCommandGuid = atoi(pArg);
		else 
			endCommandGuid = -1;
	}
	
	commandReady = commandGuid == endCommandGuid && commandGuid >= 0;
}

void CheckMouseCommand(void)
{
	char *pArg;
	int i = 0;
	int endCommandGuid = 0;

	commandId = 2;
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    commandGuid = atoi(pArg);
	else 
		commandGuid = -1;
	
	for (i = 0; i < MOUSE_DATA_LENGTH; i++)
	{
		pArg = UART_szGetParam();
#ifdef DEBUG_INFO
		DebugWriteParam(pArg);
#endif
		sh = 0;
		if (pArg != NULL)
		    hex2data(&sh, pArg);
		Mouse_Data[i] = sh;
#ifdef DEBUG_INFO
		DebugWriteRx(sh);
#endif
	}
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    endCommandGuid = atoi(pArg);
	else 
		endCommandGuid = -1;

	commandReady = commandGuid == endCommandGuid && commandGuid >= 0;
}

void CheckGamepadCommand(void)
{
	char *pArg;
	int i = 0;
	int endCommandGuid = 0;

	commandId = 3;
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    commandGuid = atoi(pArg);
	else 
		commandGuid = -1;
	
	for (i = 0; i < GAMEPAD_DATA_LENGTH; i++)
	{
		pArg = UART_szGetParam();
#ifdef DEBUG_INFO
		DebugWriteParam(pArg);
#endif
		sh = 0;
		if (pArg != NULL)
		    hex2data(&sh, pArg);
		Gamepad_Data[i] = sh;
#ifdef DEBUG_INFO
		DebugWriteRx(sh);
#endif
	}
	
	pArg = UART_szGetParam();
#ifdef DEBUG_INFO
	DebugWriteParam(pArg);
#endif
	if (pArg != NULL)
	    endCommandGuid = atoi(pArg);
	else 
		endCommandGuid = -1;

	commandReady = commandGuid == endCommandGuid && commandGuid >= 0;
}

//convert hexstring to len bytes of data
//returns 0 on success, -1 on error
//data is a buffer of two bytes
//hexstring is upper or lower case hexadecimal, NOT prepended with "0x"
int hex2data(char *data, char *hexstring)
{
    char *endptr;
	char buf[5];

    if ((hexstring[0] == '\0') || (strlen(hexstring) % 2)) {
        //hexstring contains no data
        //or hexstring has an odd length
        return -1;
    }

    if ((hexstring[0] == '0') && (hexstring[1] == '0')) {
		*data = '0';
	    return 0;
	}
	
    buf[0] = '0';
	buf[1] = 'x';
	buf[2] = hexstring[0];
	buf[3] = hexstring[1];
	buf[4] = 0;
    data[0] = strtol(buf, &endptr, 0);

    if (endptr[0] != '\0') {
        //non-hexadecimal character encountered
        return -1;
    }

    return 0;
}

//void uartRxISR(void)
//{
//    while (UART_bReadRxStatus() & UART_RX_REG_FULL)
//		ProcessByte(UART_bReadRxData());
//}

//void ProcessByte(BYTE b)
//{
//
//#ifdef DEBUG	
//	//DebugWriteRx(b);
//#endif
//
//	if (dataIndex < 0)
//	{
//		switch ((int)b)
//		{
//			case  1:
//				commandId = 1;
//				dataIndex = 0;
//				break;
//			case  2:
//				commandId = 2;
//				dataIndex = 0;
//				break;
//			case  3:
//				commandId = 3;
//				dataIndex = 0;
//				break;
//		}
//	}
//	else 
//	{
//		switch (commandId)
//		{
//			case  1:
//				Keyboard_Data[dataIndex] = b;
//				dataIndex ++;
//				if (dataIndex >= KEYBOARD_DATA_LENGTH)
//				{
//					commandReady = TRUE;
//					dataIndex = -1;
//				}
//				break;
//			case  2:
//				Mouse_Data[dataIndex] = b;
//				dataIndex ++;
//				if (dataIndex >= MOUSE_DATA_LENGTH)
//				{
//					commandReady = TRUE;
//					dataIndex = -1;
//				}
//				break;
//			case  3:
//				Gamepad_Data[dataIndex] = b;
//				dataIndex ++;
//				if (dataIndex >= GAMEPAD_DATA_LENGTH)
//				{
//					commandReady = TRUE;
//					dataIndex = -1;
//				}
//				break;
//		}
//	}
//}

#ifdef DEBUG	

void DebugWriteKeyboard(void)
{
	char str[64];
	int len = csprintf(str, "K|%x|%x|%x|%x|%x|%x|%x|%x", 
	Keyboard_Data[0],
	Keyboard_Data[1],
	Keyboard_Data[2],
	Keyboard_Data[3],
	Keyboard_Data[4],
	Keyboard_Data[5],
	Keyboard_Data[6],
	Keyboard_Data[7]);
	UART_PutChar(len);
	UART_PutString(str);
}

void DebugWriteMouse(void)
{
	char str[16];
	int len = csprintf(str, "M|%x|%x|%x", 
	Mouse_Data[0],
	Mouse_Data[1],
	Mouse_Data[2]);
	UART_PutChar(len);
	UART_PutString(str);
}

void DebugWriteGamepad(void)
{
	char str[16];
	int len = csprintf(str, "J|%x|%x|%x", 
	Gamepad_Data[0],
	Gamepad_Data[1],
	Gamepad_Data[2]);
	UART_PutChar(len);
	UART_PutString(str);
}

void DebugWriteUNKNOWN(void)
{
	char str[16] = "UNKNOWN";
	UART_PutChar(7);
	UART_PutString(str);
}

void DebugWriteRx(BYTE b)
{
	char str[16];
	int len = csprintf(str, "BC[%x]", b);
	UART_PutChar(len);
	UART_PutString(str);
}

void DebugWriteParam(char* s)
{
	char str[16];
	int len = csprintf(str, "[%s]", s);
	UART_PutChar(len);
	UART_PutString(str);
}

#endif
