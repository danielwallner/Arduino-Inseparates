// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PLATFORM_TIMERS_H_
#define _INS_PLATFORM_TIMERS_H_

// HWTimer only supports one instance at a time!

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
		if (!_timer)
			InsError(*(uint32_t*)"timr");
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
#ifndef INS_USE_TC4
#define INS_USE_TC4 1
#endif

class HWTimer
{
	Tc* _tc;
public:
	typedef void (*CallbackFunction)();

	static CallbackFunction s_callback;

	HWTimer()
	{
#if INS_USE_TC4
		_tc = TC4;
		uint32_t gclkId = TC4_GCLK_ID;
		IRQn_Type irq = TC4_IRQn;
#else
		_tc = TC3;
		uint32_t gclkId = TC3_GCLK_ID;
		IRQn_Type irq = TC3_IRQn;
#endif
		while (GCLK->STATUS.bit.SYNCBUSY);
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclkId) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
		while (GCLK->STATUS.bit.SYNCBUSY);
		// Reset the timer.
		_tc->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		while (_tc->COUNT16.CTRLA.bit.SWRST);
		// 16-bit mode.
		// Prescaler: DIV64.
		// Match Frequency (MFRQ) waveform generation mode.
		_tc->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64 | TC_CTRLA_WAVEGEN_MFRQ;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		// Disable all TC interrupts until we set the period.
		_tc->COUNT16.INTENCLR.reg = 0xFF;
		// Clear the counter.
		_tc->COUNT16.COUNT.reg = 0;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		// Enable the chosen timer’s IRQ in NVIC.
		NVIC_EnableIRQ(irq);
	}

	void attachInterruptInterval(const unsigned long interval, CallbackFunction callback)
	{
		s_callback = callback;
		uint32_t tickFrequency = 48000000UL / 64UL;
		uint16_t period = (uint16_t)((interval * tickFrequency) / 1000000UL);
		if (period == 0)
			period = 1;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		_tc->COUNT16.CC[0].reg = period;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		// Enable the match compare interrupt.
		_tc->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
		// Enable the timer.
		_tc->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
		while (_tc->COUNT16.STATUS.bit.SYNCBUSY);
	}
};
#endif

}

#endif
