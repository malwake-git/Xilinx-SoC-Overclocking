
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_clocking.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xil_printf.h"
#include <math.h>
#include "platform.h"
#include "xuartps.h"
#include "sleep.h"
#include "time.h"
#include "AES.h"
#include "xcpu_cortexa9.h"
#include "Zynq.s"

#define APLL_CTRL (u32) 0XF8000100
#define APLL_CTRL_RATIO (u32) 0XF8000120
#define APLL_CFG  (u32) 0XF8000110
#define PLL_STATUS (u32) 0XF800010C
#define LOCK_KEY (u32) 0XF8000004
#define UNLOCK_KEY (u32) 0XF8000008
#define UART_CLK_CTRL (u32) 0XF8000154
#define IO_PLL_CTRL (u32) 0XF8000108
#define IO_PLL_CFG (u32) 0XF8000118
#define FPGA0_CLK_CTRL (u32) 0XF8000170
#define FPGA_RST_CTRL (u32) 0XF8000240

#define CLOCK_DEVICE_ID              (0)
#define XCLOCK_ACPU_REF_RATE1        (649999000)
#define XCLOCK_ACPU_REF_RATE2        (649999000)
#define PLL_MUL						 (1)
#define XCLOCK_ACPU_REF_START        (649999000)

unsigned char key[16] = {0x2B ,0x7E ,0x15 ,0x16 ,0x28 ,0xAE ,0xD2 ,0xA6 ,0xAB ,0xF7 ,0x15 ,0x88 ,0x09 ,0xCF ,0x4F ,0x3C};

#define RECV_BUFFER_SIZE 100
#define SEND_BUFFER_SIZE (RECV_BUFFER_SIZE/2)


static inline XStatus XClock_ReadReg(u32 RegAddr, u32 *Value)
{
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
	XSmc_OutVar RegValue;

	RegValue = Xil_Smc(MMIO_READ_SMC_FID, (u64)(RegAddr), 0, 0, 0, 0, 0, 0);
	if (0x00 == (RegValue.Arg0 & 0xFFFFFFFF)) {
		*Value = RegValue.Arg0 >> 32;
		return XST_SUCCESS;
	}

	return XST_FAILURE;
#else
	*Value = Xil_In32((u32)RegAddr);
	return XST_SUCCESS;
#endif
}

static inline XStatus XClock_WriteReg(u32 RegAddr, u32 Value)
{
#if defined (__aarch64__) && (EL1_NONSECURE == 1)
	XSmc_OutVar RegValue;
	RegValue = Xil_Smc(MMIO_WRITE_SMC_FID,
			(u64)(RegAddr) | ((u64)(0xFFFFFFFF) << 32),
						(u64)Value, 0, 0, 0, 0, 0);
	if (0x00 == (RegValue.Arg0 & 0xFFFFFFFF)) {
		return XST_SUCCESS;
	}

	return XST_FAILURE;
#else
	Xil_Out32((u32)RegAddr, (u32)Value);
	return XST_SUCCESS;
#endif
}

XStatus XClock_PLLRate( int * Rate){
	u32 		 Value_PLL;
	XStatus      Status;
	u32 		 Value;

	Status = XClock_ReadReg(APLL_CTRL_RATIO, &Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	Status = XClock_ReadReg(APLL_CTRL, &Value);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}


	*Rate = (int)((Value) >> 12)*50000000 / ((Value_PLL>>8)&0x3f);
	return XST_SUCCESS;

}

XStatus XClock_PllDiv(u32 Value){
	u32 		 Value_PLL;
	XStatus      Status;
	int Rate;

	/*
	Status = XClock_ReadReg(APLL_CTRL_RATIO, &Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	*/

	XClock_WriteReg(UNLOCK_KEY, 0x0000DF0DU);

	Status = XClock_WriteReg(APLL_CTRL_RATIO,Value);
	   if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	XClock_WriteReg(LOCK_KEY, 0x0000767BU);

	//XClock_PLLRate(&Rate);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	//printf("Rate: %d\n",Rate);

	return XST_SUCCESS;

}

XStatus XClock_ReadPll(u32 Value){

	u32 		 Value_CFG;
	u32 		 Value_PLL;
	XStatus      Status;
	int   Rate;

	if(Value >= 0x0001A008 && Value < 0x0001B008){
		Value_CFG = 0x1772C0;
	}
	else if(Value == 0x00014008){
			Value_CFG = 0x1F42C0;
		}
	else if(Value >= 0x0001B008 && Value <= 0x0001C008){
		Value_CFG = 0x15E2C0;
	}
	else if(Value >= 0x0001D008 && Value <= 0x0001E008){
			Value_CFG = 0x1452C0;
		}
	else if(Value >= 0x0001F008 && Value <= 0x00021008){
			Value_CFG = 0x12C220;
		}
	else if(Value >= 0x00022008 && Value <= 0x00024008){
			Value_CFG = 0x113220;
		}
	/////
	else if(Value >= 0x00025008 && Value <= 0x00028008){
			Value_CFG = 0x0FA220;
		}
	else if(Value >= 0x00029008 && Value <= 0x0002F008){
			Value_CFG = 0x0FA3C0;
		}
	else if(Value >= 0x00030008 && Value <= 0x00042008){
			Value_CFG = 0x0FA240;
		}

	Status = XClock_ReadReg(APLL_CTRL,&Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	XClock_WriteReg(UNLOCK_KEY, 0x0000DF0DU);

	XClock_WriteReg(APLL_CTRL, (u32) (Value));
	XClock_WriteReg(APLL_CFG, Value_CFG);
	XClock_WriteReg(APLL_CTRL, (u32) (Value+(0x10)));
	XClock_WriteReg(APLL_CTRL, (u32) (Value+(0x11)));
	XClock_WriteReg(APLL_CTRL, (u32) (Value+(0x10)));
	XClock_WriteReg(PLL_STATUS, (u32) 0x0000001U);
	XClock_WriteReg(APLL_CTRL, (u32) (Value));
	//XClock_WriteReg(APLL_CTRL_RATIO, 0x1F000200U);

	XClock_WriteReg(LOCK_KEY, 0x0000767BU);

	XClock_ReadReg(APLL_CTRL,&Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	//printf("PLL After Register: %u\n",Value_PLL);

	XClock_PLLRate(&Rate);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	//printf("Rate: %d\n",Rate);

	//XClock_ReadReg(APLL_CFG, &Value_CFG);

	return XST_SUCCESS;

}

XStatus XClock_PllDiv_Reg(u32 Value, u32 CTRL_reg){
	u32 		 Value_PLL;
	XStatus      Status;
	int Rate;

	XClock_WriteReg(UNLOCK_KEY, 0x0000DF0DU);

	Status = XClock_WriteReg(CTRL_reg,Value);
	   if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	XClock_WriteReg(LOCK_KEY, 0x0000767BU);

	//XClock_PLLRate(Value_PLL,&Rate);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	//printf("Rate: %d\n",Rate);

	return XST_SUCCESS;

}


XStatus XClock_ReadPll_reg(u32 Value, u32 ctrl_reg, u32 cfg_reg){

	u32 		 Value_CFG;
	u32 		 Value_PLL;
	XStatus      Status;
	int   Rate;

	if(Value >= 0x0001A008 && Value < 0x0001B008){
		Value_CFG = 0x1772C0;
	}
	else if(Value == 0x00014008){
			Value_CFG = 0x1F42C0;
		}
	else if(Value >= 0x0001B008 && Value <= 0x0001C008){
		Value_CFG = 0x15E2C0;
	}
	else if(Value >= 0x0001D008 && Value <= 0x0001E008){
			Value_CFG = 0x1452C0;
		}
	else if(Value >= 0x0001F008 && Value <= 0x00021008){
			Value_CFG = 0x12C220;
		}
	else if(Value >= 0x00022008 && Value <= 0x00024008){
			Value_CFG = 0x113220;
		}
	/////
	else if(Value >= 0x00025008 && Value <= 0x00028008){
			Value_CFG = 0x0FA220;
		}
	else if(Value >= 0x00029008 && Value <= 0x0002F008){
			Value_CFG = 0x0FA3C0;
		}
	else if(Value >= 0x00030008 && Value <= 0x00042008){
			Value_CFG = 0x0FA240;
		}

	Status = XClock_ReadReg(ctrl_reg,&Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	XClock_WriteReg(UNLOCK_KEY, 0x0000DF0DU);

	XClock_WriteReg(ctrl_reg, (u32) (Value));
	XClock_WriteReg(cfg_reg, Value_CFG);
	XClock_WriteReg(ctrl_reg, (u32) (Value+(0x10)));
	XClock_WriteReg(ctrl_reg, (u32) (Value+(0x11)));
	XClock_WriteReg(ctrl_reg, (u32) (Value+(0x10)));
	XClock_WriteReg(PLL_STATUS, (u32) 0x0000001U);
	XClock_WriteReg(ctrl_reg, (u32) (Value));
	//XClock_WriteReg(APLL_CTRL_RATIO, 0x1F000200U);

	XClock_WriteReg(LOCK_KEY, 0x0000767BU);

	XClock_ReadReg(ctrl_reg,&Value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}
	//printf("PLL After Register: %u\n",Value_PLL);

//	XClock_PLLRate(&Rate);
//	if (Status != XST_SUCCESS) {
//		xil_printf("Failed: Reading Register Value\r\n");
//		return Status;
//	}

	return XST_SUCCESS;

}


XStatus UartPsSendRec(u16 DeviceId, u32 Value)
{
	XStatus Status;
	XUartPs Uart_Ps;
	XUartPs_Config *Config;
	u32 value_PLL;
	int Rate;
	u64 a = 0;
	u64 b = 0;
	u64 c = 0;
	u64 d = 0;
	u32 Value2;
	u32 inpu = 1+(1<<1)+(0x5678<<2)+(0x001234<<18);

	//u64 SendBuffer_32_mul[SEND_BUFFER_SIZE];	/* Buffer for Transmitting Data */
	//u64 SendBuffer_32_sum[SEND_BUFFER_SIZE];
	//u32 RecvBuffer_32[RECV_BUFFER_SIZE];	/* Buffer for Receiving Data */

	u8 SendBuffer[8];	/* Buffer for Transmitting Data */
	u8 RecvBuffer[4];	/* Buffer for Receiving Data */

	u64 * SendBuffer_32_mul = (u64*) malloc(SEND_BUFFER_SIZE * sizeof(u64));
//	u64 * SendBuffer_32_sum = (u64*) malloc(SEND_BUFFER_SIZE * sizeof(u64));
	u32 * RecvBuffer_32 = (u32*) malloc(RECV_BUFFER_SIZE * sizeof(u32));

	Config = XUartPs_LookupConfig(DeviceId);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XUartPs_SetBaudRate(&Uart_Ps, 115200);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XUartPs_SelfTest(&Uart_Ps);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	unsigned int SentCount;
	unsigned int ReceivedCount;
	int Index;

	XUartPs_SetOperMode(&Uart_Ps, XUARTPS_OPER_MODE_NORMAL);

	for (Index = 0; Index < 4; Index++) {
		RecvBuffer[Index] = 0;

	}
	for (Index = 0; Index < 8; Index++) {
		SendBuffer[Index] = 0;
	}

	for (Index = 0; Index < RECV_BUFFER_SIZE; Index++) {
			RecvBuffer_32[Index] = 0;
	}
	for (Index = 0; Index < SEND_BUFFER_SIZE; Index++) {
				SendBuffer_32_mul[Index] = 0;
//				SendBuffer_32_sum[Index] = 0;
	}



	int totalRecCnt = 0;
	while (totalRecCnt < RECV_BUFFER_SIZE){
		ReceivedCount = 0;
		while (ReceivedCount < 4) {
			ReceivedCount += XUartPs_Recv(&Uart_Ps, &RecvBuffer[ReceivedCount],1);
		}
		RecvBuffer_32[totalRecCnt] = (RecvBuffer[0] | (RecvBuffer[1] << 8) | (RecvBuffer[2] << 16) | (RecvBuffer[3] << 24));

		totalRecCnt++; //
	}
	int cnt = 0;

	//usleep(10000);
	//FPGA underclocking to 1MHZ
	Status = XClock_ReadReg(FPGA0_CLK_CTRL, &value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	Status = XClock_ReadPll_reg(value_PLL + ((35<<8)+(23<<20)), FPGA0_CLK_CTRL, IO_PLL_CFG);
	if (Status != XST_SUCCESS) {
			xil_printf("Failed: Reading Register Value\r\n");
			return Status;
	}

	Status = XClock_ReadReg(FPGA0_CLK_CTRL, &value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}


	//Overclocking PS
	Status = XClock_ReadPll( (u32)(Value+(0<<12)) ); // Frequancy Swap
		if (XST_SUCCESS != Status) {
			xil_printf("Failed: Setting ReadPll Func\r\n");
			return Status;
		}

	Status = XClock_ReadReg(APLL_CTRL_RATIO, &value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	Status = XClock_PllDiv(value_PLL-(1<<8));
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}


	//usleep(10000);

	for (int Index = 0; Index < SEND_BUFFER_SIZE; Index++) {

			SendBuffer_32_mul[Index] = ((u64)RecvBuffer_32[cnt] * (u64)RecvBuffer_32[cnt+1]);
//			SendBuffer_32_sum[Index] = ((u64)RecvBuffer_32[cnt] + (u64)RecvBuffer_32[cnt+1]);
			cnt += 2;

		}

	//usleep(10000);
	Status = XClock_PllDiv(value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	Status = XClock_ReadPll( (u32)(Value) );
	if (XST_SUCCESS != Status) {
		xil_printf("Failed: Setting ReadPll Func\r\n");
		return Status;
	}


	//// overclocking UART PLL
//	Status = XClock_PllDiv_Reg(Value-(2<<8), UART_CLK_CTRL);
//	Status = XClock_ReadReg(IO_PLL_CTRL, &value_PLL);
//	if (Status != XST_SUCCESS) {
//		xil_printf("Failed: Reading Register Value\r\n");
//		return Status;
//	}
//
//	Status = XClock_ReadPll_reg((u32)value_PLL+(0<<12), IO_PLL_CTRL, IO_PLL_CFG);
//	if (XST_SUCCESS != Status) {
//		xil_printf("Failed: Setting ReadPll Func\r\n");
//		return Status;
//	}

	//Status = XClock_PLLRate(&Rate);
//	if (Status != XST_SUCCESS) {
//		return XST_FAILURE;
//	}
//
//	SendBuffer[0] = (u32)Rate >> 24;
//	SendBuffer[1] = (u32)Rate >> 16;
//	SendBuffer[2] = (u32)Rate >>  8;
//	SendBuffer[3] = (u32)Rate;

	for (int j=0;j < SEND_BUFFER_SIZE;j++){

//		SendBuffer[0] = SendBuffer_32_sum[j] >> 56;
//		SendBuffer[1] = SendBuffer_32_sum[j] >> 48;
//		SendBuffer[2] = SendBuffer_32_sum[j] >> 40;
//		SendBuffer[3] = SendBuffer_32_sum[j] >> 32;
//		SendBuffer[4] = SendBuffer_32_sum[j] >> 24;
//		SendBuffer[5] = SendBuffer_32_sum[j] >> 16;
//		SendBuffer[6] = SendBuffer_32_sum[j] >>  8;
//		SendBuffer[7] = SendBuffer_32_sum[j];
//
//		SentCount = XUartPs_Send(&Uart_Ps, SendBuffer, 8);

		SendBuffer[0] = SendBuffer_32_mul[j] >> 56;
		SendBuffer[1] = SendBuffer_32_mul[j] >> 48;
		SendBuffer[2] = SendBuffer_32_mul[j] >> 40;
		SendBuffer[3] = SendBuffer_32_mul[j] >> 32;
		SendBuffer[4] = SendBuffer_32_mul[j] >> 24;
		SendBuffer[5] = SendBuffer_32_mul[j] >> 16;
		SendBuffer[6] = SendBuffer_32_mul[j] >>  8;
		SendBuffer[7] = SendBuffer_32_mul[j];

		SentCount = XUartPs_Send(&Uart_Ps, SendBuffer, 8);
		usleep(10000);
//		usleep(10000);
	}

	free(RecvBuffer_32);
	free(SendBuffer_32_mul);
//	free(SendBuffer_32_sum);

	return XST_SUCCESS;
}

XStatus UartAES(u16 DeviceId, u32 Value, 	unsigned char * expandedKey)
{
	XStatus Status;
	XUartPs Uart_Ps;
	XUartPs_Config *Config;
	u32 value_PLL;
	int Rate;
	unsigned char plainText[16];	/* Buffer for Transmitting Data */
	unsigned char plainText0[16];
	u8 RecvBuffer[16];	/* Buffer for Receiving Data */
	u8 SendBuffer[16];

	unsigned char * ciper1 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * ciper2 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * ciper3 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * ciper4 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * ciper5 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * ciperAr[5];
	ciperAr[0] = ciper1;
	ciperAr[1] = ciper2;
	ciperAr[2] = ciper3;
	ciperAr[3] = ciper4;
	ciperAr[4] = ciper5;

	unsigned char * plainText1 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * plainText2 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * plainText3 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * plainText4 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * plainText5 = (unsigned char *) malloc(16* sizeof(unsigned char));
	unsigned char * plainTextAr[5];
	plainTextAr[0] = plainText1;
	plainTextAr[1] = plainText2;
	plainTextAr[2] = plainText3;
	plainTextAr[3] = plainText4;
	plainTextAr[4] = plainText5;

	u64 * RecvBuffer_64 = (u64*) malloc(RECV_BUFFER_SIZE* 2 * sizeof(u64));

	Config = XUartPs_LookupConfig(DeviceId);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XUartPs_SetBaudRate(&Uart_Ps, 115200);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XUartPs_SelfTest(&Uart_Ps);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	unsigned int SentCount;
	unsigned int ReceivedCount;
	u8 Index;

	XUartPs_SetOperMode(&Uart_Ps, XUARTPS_OPER_MODE_NORMAL);


	for (Index = 0; Index < 16; Index++) {
		RecvBuffer[Index] = 0;
		SendBuffer[Index] = 0;
		plainText[Index] = 0;

	}

	int totalRecCnt = 0;
	while (totalRecCnt < RECV_BUFFER_SIZE*2){
		ReceivedCount = 0;
		while (ReceivedCount < 16) {
			ReceivedCount += XUartPs_Recv(&Uart_Ps, &RecvBuffer[ReceivedCount],1);
		}
		RecvBuffer_64[totalRecCnt] = ((u64)RecvBuffer[0] | ((u64)RecvBuffer[1] << 8) | ((u64)RecvBuffer[2] << 16) | ((u64)RecvBuffer[3] << 24)| ((u64)RecvBuffer[4] << 32) | ((u64)RecvBuffer[5] << 40) | ((u64)RecvBuffer[6] << 48) | ((u64)RecvBuffer[7] << 56));

		RecvBuffer_64[totalRecCnt+1] = ((u64)RecvBuffer[8] | ((u64)RecvBuffer[9] << 8) | (RecvBuffer[10] << 16) | ((u64)RecvBuffer[11] << 24) | ((u64)RecvBuffer[12] << 32) | ((u64)RecvBuffer[13] << 40) | ((u64)RecvBuffer[14] << 48) | ((u64)RecvBuffer[15] << 56));


		totalRecCnt++; //
		totalRecCnt++;
	}

	Status = XClock_PLLRate(&Rate);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	SendBuffer[0] = (u32)Rate >> 24;
	SendBuffer[1] = (u32)Rate >> 16;
	SendBuffer[2] = (u32)Rate >>  8;
	SendBuffer[3] = (u32)Rate;
	//SentCount = XUartPs_Send(&Uart_Ps, SendBuffer, 4);
	//printf("Rate: %d",Rate);

	int RecvCnt = 0;
	for(int x=0;x<RECV_BUFFER_SIZE/10;x++){

		for(int u=0; u<5; u++){
			plainText[7] = RecvBuffer_64[RecvCnt] >> 56;
			plainText[6] = RecvBuffer_64[RecvCnt] >> 48;
			plainText[5] = RecvBuffer_64[RecvCnt] >> 40;
			plainText[4] = RecvBuffer_64[RecvCnt] >> 32;
			plainText[3] = RecvBuffer_64[RecvCnt] >> 24;
			plainText[2] = RecvBuffer_64[RecvCnt] >> 16;
			plainText[1] = RecvBuffer_64[RecvCnt] >>  8;
			plainText[0] = RecvBuffer_64[RecvCnt];

			plainText[15] = RecvBuffer_64[RecvCnt+1] >> 56;
			plainText[14] = RecvBuffer_64[RecvCnt+1] >> 48;
			plainText[13] = RecvBuffer_64[RecvCnt+1] >> 40;
			plainText[12] = RecvBuffer_64[RecvCnt+1] >> 32;
			plainText[11] = RecvBuffer_64[RecvCnt+1] >> 24;
			plainText[10] = RecvBuffer_64[RecvCnt+1] >> 16;
			plainText[9] = RecvBuffer_64[RecvCnt+1] >>  8;
			plainText[8] = RecvBuffer_64[RecvCnt+1];

			plainText0[7] =  RecvBuffer_64[RecvCnt+2] >> 56;
			plainText0[6] = RecvBuffer_64[RecvCnt+2] >> 48;
			plainText0[5] = RecvBuffer_64[RecvCnt+2] >> 40;
			plainText0[4] = RecvBuffer_64[RecvCnt+2] >> 32;
			plainText0[3] = RecvBuffer_64[RecvCnt+2] >> 24;
			plainText0[2] = RecvBuffer_64[RecvCnt+2] >> 16;
			plainText0[1] = RecvBuffer_64[RecvCnt+2] >>  8;
			plainText0[0] = RecvBuffer_64[RecvCnt+2];

			plainText0[15] = RecvBuffer_64[RecvCnt+3] >> 56;
			plainText0[14] = RecvBuffer_64[RecvCnt+3] >> 48;
			plainText0[13] = RecvBuffer_64[RecvCnt+3] >> 40;
			plainText0[12] = RecvBuffer_64[RecvCnt+3] >> 32;
			plainText0[11] = RecvBuffer_64[RecvCnt+3] >> 24;
			plainText0[10] = RecvBuffer_64[RecvCnt+3] >> 16;
			plainText0[9] = RecvBuffer_64[RecvCnt+3] >>  8;
			plainText0[8] = RecvBuffer_64[RecvCnt+3];

			Status = XClock_ReadPll( (u32)(Value + (0<<12)) ); // frequancy swap
			if (XST_SUCCESS != Status) {
				xil_printf("Failed: Setting ReadPll Func\r\n");
				return Status;
			}
			//usleep(10000);
			Status = XClock_ReadReg(APLL_CTRL_RATIO, &value_PLL);
			if (Status != XST_SUCCESS) {
				xil_printf("Failed: Reading Register Value\r\n");
				return Status;
			}

			Status = XClock_PllDiv(value_PLL+(0<<8));
			if (Status != XST_SUCCESS) {
				xil_printf("Failed: Reading Register Value\r\n");
				return Status;
			}

			AESEncryption(&plainText, expandedKey, ciperAr[u]);

			AESDecryption(&plainText0, expandedKey, plainTextAr[u]);

			//usleep(10000);
			Status = XClock_PllDiv(value_PLL);
			if (Status != XST_SUCCESS) {
				xil_printf("Failed: Reading Register Value\r\n");
				return Status;
			}
			Status = XClock_ReadPll( (u32)(Value) );
			if (XST_SUCCESS != Status) {
				xil_printf("Failed: Setting ReadPll Func\r\n");
				return Status;
			}

			RecvCnt = RecvCnt+4;

		}

		SentCount = XUartPs_Send(&Uart_Ps, ciper1, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, ciper2, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, ciper3, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, ciper4, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, ciper5, 16);
		usleep(100000);

		SentCount = XUartPs_Send(&Uart_Ps, plainText1, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, plainText2, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, plainText3, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, plainText4, 16);
		usleep(10000);
		SentCount = XUartPs_Send(&Uart_Ps, plainText5, 16);
		usleep(100000);
	}

	free(ciper1);
	free(ciper2);
	free(ciper3);
	free(ciper4);
	free(ciper5);
	free(plainText1);
	free(plainText2);
	free(plainText3);
	free(plainText4);
	free(plainText5);
	free(RecvBuffer_64);

	return XST_SUCCESS;
}

XStatus ClockPs( u16 ClockDevId){
	XStatus    Status;
	u32 		 Value;
	u32			 Value_PLL;

	//ConfigPtr = XClock_LookupConfig(ClockDevId);

	Status = XClock_ReadReg(APLL_CTRL,&Value);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	unsigned char * expandedKey;
	expandedKey = keyExpansion(key);


	for(int l = 0; l<PLL_MUL; l++){
		Status = XClock_ReadPll( (u32)(Value+(0<<12)) );
		if (XST_SUCCESS != Status) {
			xil_printf("Failed: Setting ReadPll Func\r\n");
			return Status;
		}

		Status = XClock_ReadReg(APLL_CTRL,&Value_PLL);
		if (Status != XST_SUCCESS) {
			xil_printf("Failed: Reading Register Value\r\n");
			return Status;
		}

		// Algorithum Select
		//Status = UartAES(XPAR_XUARTPS_0_DEVICE_ID, Value_PLL, expandedKey);
		Status = UartPsSendRec(XPAR_XUARTPS_0_DEVICE_ID, Value_PLL);
		if (Status != XST_SUCCESS) {
			xil_printf("Failed: Reading Register Value\r\n");
			return Status;
		}

	}

	return XST_SUCCESS;

}


int main()
{
    init_platform();
    u32 Value;
    XStatus Status;


    Status = ClockPs(CLOCK_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Clock Test Failed\r\n");
		return XST_FAILURE;
	}


    /*
    u32 value_PLL;
    Status = XClock_ReadReg(APLL_CTRL, &Value);
    if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}


	int cnt = 0;
    for(int k=0;k<2;k++){
		for(int i=0; i<30000000; i++){
			cnt++;
		}
		printf("Time: %d\n\r", cnt);
	}
	printf("\n\n");


	Status = XClock_ReadPll(Value+(1<<12));
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

   Status = XClock_ReadReg(APLL_CTRL_RATIO, &value_PLL);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

	Status = XClock_PllDiv(value_PLL-(1<<8));
	if (Status != XST_SUCCESS) {
		xil_printf("Failed: Reading Register Value\r\n");
		return Status;
	}

    cnt = 0;
	for(int s=0;s<2;s++){
		for(int l=0; l<30000000; l++){
			cnt++;
		}
		printf("Time: %d\n\r", cnt);
	}
	printf("\n\n");

	int Rate;
	Status = XClock_PLLRate(&Rate);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	printf("Rate: %d\n",Rate);
	*/

    print("Successfully ran Hello World application");
    cleanup_platform();
    return 0;
}
