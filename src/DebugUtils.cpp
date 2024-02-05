// Copyright (c) 2024 Daniel Wallner

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

void INS_DEBUGF(const char *format, ...)
{
#ifdef UNIT_TEST
	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);
#else
	static const uint8_t kBufferLength = 64;
	char string[kBufferLength];
	va_list argptr;
	int ret;
	va_start(argptr, format);
	ret = vsnprintf(string, kBufferLength, format, argptr);
	va_end(argptr);
	Serial.print(string);
#endif
}
