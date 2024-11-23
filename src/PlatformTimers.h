// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PLATFORM_TIMERS_H_
#define _INS_PLATFORM_TIMERS_H_

#ifdef ARDUINO_ARCH_SAMD
#include <SAMDTimerInterrupt.hpp>
#endif

namespace inseparates
{

#ifdef ESP8266

#define INS_HAVE_HW_TIMER 1

class HWTimer
{
public:
	~HWTimer()
	{
		timer1_disable();
	}

	void attachInterruptInterval(const unsigned long &interval, void (*callback)(void))
	{
		timer1_disable();
		timer1_attachInterrupt(callback);
		timer1_isr_init();
		timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
		timer1_write(interval * 5);
	}
};
#endif

#ifdef ESP32
#define INS_HAVE_HW_TIMER 1

class HWTimer
{
	hw_timer_t *_timer = nullptr;

public:
	HWTimer() :
#if ESP_IDF_VERSION_MAJOR < 5
		_timer(timerBegin(0, 80, true))
#else
		_timer(timerBegin(1000000))
#endif
	{
	}

	~HWTimer()
	{
		if (_timer)
		{
			timerEnd(_timer);
		}
	}

	void attachInterruptInterval(const unsigned long &interval, void (*callback)(void))
	{
#if ESP_IDF_VERSION_MAJOR < 5
		timerAttachInterrupt(_timer, callback, true);
		timerAlarmWrite(_timer, interval, true);
		timerAlarmEnable(_timer);
#else
		timerAttachInterrupt(_timer, callback);
		timerAlarm(_timer, interval, true, 0);
#endif
	}
};
#endif

#ifdef ARDUINO_ARCH_SAMD
#define INS_HAVE_HW_TIMER 1

class HWTimer
{
	SAMDTimerInterrupt _timer;
public:
	HWTimer() : _timer(TIMER_TC3) {}
	void attachInterruptInterval(const unsigned long &interval, void (*callback)(void))
	{
		attachInterruptInterval(interval, callback);
	}
};
#endif

}

#endif
