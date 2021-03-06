// Error_code_transform.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <ctype.h>
void print_error(unsigned int error_code);

int main()
{
	unsigned int error_code = 0;
	bool i = 1;		//循环标志位
	int x = 1;		//输入有效标志位
	char y;
	while (i) {
		printf("Please enter the hexadecimal error code. To exit, please enter FF or ff:\n");
		x = scanf_s("%x", &error_code);
		scanf_s("%c", &y);
		if (isspace(y) == 0)	//非空字符，循环运行清除输入缓冲区
		{
			printf("Please enter a number!\n");
			x = 0;
			for (int cc = 2; cc > 1; cc++)
			{
				scanf_s("%c", &y);
				if (isspace(y) != 0)	//空字符
					cc = 0;
			}
		}

		if (error_code == 0xff)
			i = 0;
		else if (error_code >= 0x40)
			printf("Out of range! Please enter again!\n");
		else if (error_code == 0 && x != 0)
			printf("All sensor OK!\n");
		else if (x != 0)
			print_error(error_code);
		printf("\n\n");
	}
	printf("Press Enter to exit:\n");
	return 0;
}

void print_error(unsigned int error_code)
{
	if ((error_code & 0x01) == 0x01)
		printf("BME280 erroe!\n");
	if ((error_code & 0x02) == 0x02)
		printf("CO2 erroe!\n");
	if ((error_code & 0x04) == 0x04)
		printf("Dust erroe!\n");
	if ((error_code & 0x08) == 0x08)
		printf("O2 erroe!\n");
	if ((error_code & 0x10) == 0x10)
		printf("Light erroe!\n");
	if ((error_code & 0x20) == 0x20)
		printf("Soil erroe!\n");
	if (((error_code & 0x40) == 0x40) || (((error_code & 0x80) == 0x80)))
		printf("Unknown erroe!\n");
}

