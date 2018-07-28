﻿/*
Name:		seeedtea.ino
Created:	2018/4/13 14:09:27
Author:	zxd
*/

// the setup function runs once when you press reset or power the board
#include "POWER_Ctrl.h"
#include "SHT1x.h"
#include <Wire.h>
#include <string.h>
#include "Digital_Light_TSL2561.h"
#include "seeed_Lora.h"
#include <LoRaWan.h>
#include "Dust_other.h"
#include "CO2.h"
#include <SoftwareSerial.h>
#include "BME280.h"
#include <EnergySaving.h>
#include "Oxygen.h"

//宏开关，注释后会停止相应模块工作
#define LORA_RUN
#define SENSOR_RUN

//系统运行时间，用于低功耗控制
unsigned long system_time = 0;		//存储上次运行时间
#define interval_time 600			//间隔时间，秒
#define Preheat_time 30000			//Dust Sensor预热时间

#define dataPin 6 //土壤水分和温度传感器数据引脚
#define clockPin 7 //土壤水分和温度传感器时钟引脚
SHT1x sht1x(dataPin, clockPin);

//电源控制引脚，低开高关
#define Air_CtrlPin 10
#define Soil_CtrlPin 11

#define bme280_error 0		//0000 0000
#define CO2_error 1			//0000 0001
#define dust_error 2
#define light_error 3
#define O2_error 4
#define soil_error 5

#define soil_temp_low 0
#define soil_temp_high 124
#define soil_humi_low 0
#define soil_humi_high 100
#define TSL2561_data_low 0
#define TSL2561_data_high 65535


BME280 bme280;
const unsigned char data_length = 15;
unsigned char Lora_data[15] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13,0,0 };
int temp; //温度
int hum;  //湿度
int pressure; //压力
int altitude; //海拔
			  //unsigned int pressure_left;
			  //unsigned int pressure_right;
unsigned int altitude_left;
unsigned int altitude_right;
int TSL2561_data; //光传感器数据
int Dust_value; //灰尘数据
unsigned char o2_value;  //本次氧气数值
unsigned char last_value = 207;//上次氧气数值
float soil_temp;//土壤温度
float soil_humi;//土壤湿度

void setup() {
	pinMode(2, OUTPUT);		//供外部单片机复位参考
	pinMode(Air_CtrlPin, OUTPUT);
	pinMode(Soil_CtrlPin, OUTPUT);
	pinMode(Dust_pin, INPUT);
	pinMode(O2_pin, INPUT);
	digitalWrite(Air_CtrlPin, 1);	//关电源
	digitalWrite(Soil_CtrlPin, 1);
	digitalWrite(2, 1);
	delay(500);
	digitalWrite(2, 0);
	Wire.begin();
	SerialUSB.begin(9600);		//USB
	power_ctrl_init();
	CO2_init();

#ifdef LORA_RUN
	Lora_begin();	//Lora_begin();
	memset(Lora_data, 0, sizeof(Lora_data));
	Lora_send(data_length);
#endif // LORA_RUN

	for (int i = 3; i > 0; i--)		//Blink LED
	{
		digitalWrite(2, 1);
		delay(500);
		digitalWrite(2, 0);
		delay(500);
	}
}


// the loop function runs over and over again until power down or reset
void loop()
{
	bool power_state;	//1为电池有电，0为电池没电
	power_state = power_ctrl();
	SerialUSB.print("power_state = ");
	SerialUSB.println(power_state);

#ifdef SENSOR_RUN
	unsigned long time_2 = 0;
	bool time_state = 0;	//1为时间足够，可以采集数据

	if (0 == system_time)
		time_state = 1;
	else
	{
		time_2 = (millis() - system_time) / 1000;
		if (time_2 >= interval_time)
			time_state = 1;
	}
	
	SerialUSB.print("time_state = ");
	SerialUSB.println(time_state);
	if (power_state == 1 && time_state == 1)
	{
		system_time = millis();
		SerialUSB.println("Sensor power on!");
		digitalWrite(Air_CtrlPin, 0);	//开电源
		digitalWrite(Soil_CtrlPin, 0);
		delay(3000);
		get_AllSensorData();
		SerialUSB.println("Sensor power off!");
		digitalWrite(Air_CtrlPin, 1);	//关电源
		digitalWrite(Soil_CtrlPin, 1);
#ifdef LORA_RUN
		Lora_send(data_length);
#endif // LORA_RUN		
	}
	else
	{
		delay(6000);
	}
	SerialUSB.println("finish loop");
#else
	delay(1000);
#endif // SENSOR_RUN
}







bool get_AllSensorData()	//获取所有传感器的数据
{
	unsigned long D_time;	//为Dust Sensor记录上电时间
	D_time = millis();
	SerialUSB.println("get_BME280_data");
	get_BME280_data();
	SerialUSB.println("get_CO2_data");
	get_CO2_data();
	SerialUSB.println(get_Light_data());
	
	SerialUSB.println("get_O2_data");
	get_O2_data();
	SerialUSB.println("get_soil_data");
	get_soil_data();
	SerialUSB.println("get_Dust_data");

	/*****************************************************************/
	//获取电压数据
	//Lora_data[13]:电压
	Lora_data[13] = get_voltage();
	/*****************************************************************/

	while ((millis() - D_time) < Preheat_time)		//等待预热时间
	{
		delay(100);
	}
	get_Dust_data();
	SerialUSB.println("get_Dust_data OK");
	//发送数据
	return 1;
	
}

void get_BME280_data()
{
	/*****************************************************************/
	//获取温湿度气压传感器数据
	//Lora_data[0]:温度,Lora_data[1]:湿度,Lora_data[2_3]:气压
	//错误代码Lora_data[14] bit0,1：error，0：no_error
	delay(2000);
	if (!bme280.init()) {
		//SerialUSB.println("bme280 init error!");
		Lora_data[14] = 1;		//错误代码1：BME280初始化失败
	}
	delay(200);
	temp = bme280.getTemperature();
	if (temp != 0)
	{
		hum = bme280.getHumidity();
		pressure = bme280.getPressure();
		altitude = bme280.calcAltitude(pressure);
		altitude_right = altitude & 0x00ff;
		altitude_left = altitude >> 8;
		Lora_data[0] = temp & 0x00ff;		//空气湿度
		Lora_data[1] = hum & 0x00ff;		//空气湿度
		Lora_data[2] = altitude_left;		//海拔高8位
		Lora_data[3] = altitude_right;		//海拔低8位
		Lora_data[14] = Lora_data[14] & (~(1 << bme280_error)); //错误位清0
	}
	else
	{
		Lora_data[0] = 0;		//空气湿度
		Lora_data[1] = 0;		//空气湿度
		Lora_data[2] = 0;		//气压高8位
		Lora_data[3] = 0;		//气压低8位
		Lora_data[14] = Lora_data[14] | (1 << bme280_error); //错误位置1
		SerialUSB.print("bme280_ERROR");
		SerialUSB.println();
	}
	delay(1000);
	/*****************************************************************/
}

void get_CO2_data()
{
	/*****************************************************************/
	//获取CO2传感器数据
	//Lora_data[4_5]:CO2浓度
	//错误代码Lora_data[14] bit1,1：error，0：no_error
	unsigned int CO2PPM = 0;
	CO2PPM = CO2_dataRecieve();
	if (CO2PPM != 0)
	{
		Lora_data[4] = CO2PPM >> 8;			//CO2高8位
		Lora_data[5] = CO2PPM & 0x00ff;		//CO2低8位
		Lora_data[14] = Lora_data[14] & (~(1 << CO2_error)); //错误位清0
	}
	else
	{
		Lora_data[4] = 0;
		Lora_data[5] = 0;
		Lora_data[14] = Lora_data[14] | (1 << CO2_error); //错误位置1
		SerialUSB.println("CO2_ERROR");
		SerialUSB.println();
	}
	delay(100);
	/*****************************************************************/
}

void get_Dust_data()
{
	/*****************************************************************/
	//获取灰尘传感器数据
	//Lora_data[6_7]:灰尘
	//错误代码Lora_data[14] bit2,1：error，0：no_error
	Dust_value = Dust_concentration();
	if (Dust_value != 0)
	{
		/*SerialUSB.print("Dust_value=");
		SerialUSB.println(Dust_value);*/

		Lora_data[6] = Dust_value >> 8;
		Lora_data[7] = Dust_value & 0x00ff;
		Lora_data[14] = Lora_data[14] & (~(1 << dust_error)); //错误位清0
	}
	else
	{
		Lora_data[6] = 0;
		Lora_data[7] = 0;
		Lora_data[14] = Lora_data[14] | (1 << dust_error); //错误位置1
		SerialUSB.println("dust_ERROR");
		SerialUSB.println(Dust_value);
	}
	delay(1000);
	/*****************************************************************/
}

int get_Light_data()
{
	/*****************************************************************/
	//获取光照传感器数据
	//Lora_data[8_9]:光照
	//错误代码Lora_data[14] bit3,1：error，0：no_error
	delay(2000);
	TSL2561.init();
	delay(100);
	TSL2561_data = TSL2561.readVisibleLux();
	//SerialUSB.println(TSL2561.readVisibleLux());
	if ((TSL2561_data >= TSL2561_data_low) && (TSL2561_data <= TSL2561_data_high))//判断光照是否在正常范围
	{
		Lora_data[8] = (unsigned  char)((TSL2561_data >> 8) & 0xff);//光照高8位
		Lora_data[9] = (unsigned  char)(TSL2561_data & 0x00ff);		//光照低8位
		Lora_data[14] = Lora_data[14] & (~(1 << light_error)); //错误位清0
	}
	else
	{
		Lora_data[8] = 0;//光照高8位
		Lora_data[9] = 0;//光照低8位
		Lora_data[14] = Lora_data[14] | (1 << light_error); //错误位置1
		SerialUSB.println("light_ERROR");
		SerialUSB.println(TSL2561_data);
		SerialUSB.println();
	}
	delay(1000);
	return TSL2561_data;
	/*****************************************************************/
}

void get_O2_data()
{
	/*****************************************************************/
	//获取氧气传感器数据
	//Lora_data[10]:氧气
	//错误代码Lora_data[14] bit4,1：error，0：no_error
	/*以下为氧气传感器的不正确时的数据暂行显示策略*/
	//   o2_value = (unsigned char)random(205, 210);
	//if (abs((o2_value-last_value)) <= 1)
	//{
	//	Lora_data[10] = o2_value;//氧气数据
	//	last_value = o2_value;
	//	Lora_data[14] = Lora_data[14] & (~(1 << O2_error)); //错误位清0
	//}



	/*以下为氧气数据正确时的代码*/
	o2_value = (unsigned char)O2_value();
	if ((o2_value >= 170) && (o2_value <= 215))//判断O2是否在正常范围
	{
		Lora_data[10] = o2_value;//氧气数据
		Lora_data[14] = Lora_data[14] & (~(1 << O2_error)); //错误位清0
	}
	else
	{
		Lora_data[10] = 0;
		O2_init_flag = 0; //重新校准O2传感器
		Lora_data[14] = Lora_data[14] | (1 << O2_error); //错误位置1
		SerialUSB.print("O2_ERROR");
		SerialUSB.println();
	}
	delay(1000);
	/*****************************************************************/
}

void get_soil_data()
{
	/*****************************************************************/
	//获取土壤温湿度传感器数据
	//Lora_data[11]:温度 Lora_data[12]:湿度
	//错误代码Lora_data[14] bit5,1：error，0：no_error
	soil_temp = sht1x.readTemperatureC();

	soil_humi = sht1x.readHumidity();
	SerialUSB.println();
	SerialUSB.println();
	SerialUSB.println(soil_temp, DEC);
	SerialUSB.println(soil_humi, DEC);
	if (((soil_temp >= soil_temp_low) && (soil_temp <= soil_temp_high)) && ((soil_humi >= soil_humi_low) && (soil_humi <= soil_humi_high))) //判断温湿度数据范围是否正确
	{
		Lora_data[11] = (unsigned char)(soil_temp);
		Lora_data[12] = (unsigned char)(soil_humi);
		Lora_data[14] = Lora_data[14] & (~(1 << soil_error)); //错误位清0
	}
	else
	{
		Lora_data[11] = 0;
		Lora_data[12] = 0;
		Lora_data[14] = Lora_data[14] | (1 << soil_error); //错误位置1
		SerialUSB.println("soil_ERROR");
		SerialUSB.println((unsigned char)soil_temp);
		SerialUSB.println((unsigned char)soil_humi);
		SerialUSB.println();
		SerialUSB.println();
	}
	delay(2000);
	/*****************************************************************/
}
