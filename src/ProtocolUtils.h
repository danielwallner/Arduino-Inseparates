// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_UTILS_H_
#define _INS_PROTOCOL_UTILS_H_

#include "Inseparates.h"

#if ESP32 && !INS_ESP32_PWM_CHANNEL
#define INS_ESP32_PWM_CHANNEL 0
#endif

namespace inseparates
{

class PinWriter
{
public:
	virtual void write(uint8_t value) = 0;
};

class PushPullPinWriter : public PinWriter
{
	uint8_t _pin;
public:
	PushPullPinWriter(uint8_t pin) :
		_pin(pin)
	{
		pinMode(pin, OUTPUT);
	}

	void write(uint8_t value) override
	{
		digitalWrite(_pin, value);
	}
};

// Pseudo open drain that uses pinMode to float pin
class OpenDrainPinWriter : public PinWriter
{
	uint8_t _pin;
	uint8_t _onState;
	uint8_t _offState;
public:
	OpenDrainPinWriter(uint8_t pin, uint8_t onState, uint8_t offState = INPUT) :
		_pin(pin), _onState(onState), _offState(offState)
	{
		pinMode(pin, _offState);
		digitalWrite(pin, onState);
	}

	void write(uint8_t value) override
	{
		if (value == _onState)
			pinMode(_pin, OUTPUT);
		else
			pinMode(_pin, _offState);
	}
};

// There can only be one PWMPinWriter active at a time!
class PWMPinWriter : public PinWriter
{
	const uint8_t _pin;
	const uint8_t _onState;
	uint32_t _frequency = 0;
	uint8_t _dutyCycle;

public:
	PWMPinWriter(uint8_t pin, uint8_t onState) :
		_pin(pin),
		_onState(onState)
	{
		pinMode(_pin, OUTPUT);
		digitalWrite(_pin, 1 ^ _onState);
	}

	void prepare(uint32_t frequency, uint8_t dutyCycle)
	{
	_frequency = frequency;
	_dutyCycle = dutyCycle;
#if AVR
		uint16_t divisor = 1;
		uint16_t pwmTop;
		uint8_t p = 1;
		for (; p <= 2; ++p)
		{
			Serial.println(divisor);
			int32_t divFreq = divisor * _frequency;
			pwmTop = (F_CPU + divFreq / 2) / divFreq;
			if (pwmTop < 256)
			{
				break;
			}
			divisor = 8;
		}

		TIMSK2 &= ~(1 << OCIE2A); // Disable Timer 2 Output Compare Match Interrupt
		TCCR2A = (1 << WGM21) | (1 << WGM20);
		TCCR2B = (1 << WGM22) | p; // Fast PWM mode
		OCR2A = pwmTop - 1;
		OCR2B = (_dutyCycle * (OCR2A + 1)) / 100;
#elif ESP8266
		// ESP8266 analogWrite is a software PWM that doesn't work for high frequencies.
		if (frequency > 50000)
			InsError(*(uint32_t*)"pfrq");
		analogWriteRange(255);
#elif ESP32
		// ESP32 ledcSetup has an upper limit.
		if (frequency > 300000)
			InsError(*(uint32_t*)"pfrq");
		ledcSetup(INS_ESP32_PWM_CHANNEL, _frequency, 8);
		ledcAttachPin(_pin, INS_ESP32_PWM_CHANNEL);
#endif
	}

	void write(uint8_t value) override
	{
		if (!_frequency)
			InsError(*(uint32_t*)"hefr");
		if (value == _onState)
		{
#if AVR
			uint8_t pinmode;
			if (_onState == HIGH)
			{
				pinmode = (1 << COM2A1);
			}
			else
			{
				pinmode = (1 << COM2A1) | (1 << COM2A0);
			}
			if (_pin == 11)
			{
				TCCR2A |= pinmode;
			}
			else if (_pin == 3)
			{
				TCCR2A |= pinmode >> 2;
			}
			else
			{
				InsError(*(uint32_t*)"tpin");
			}
#elif ESP8266
			analogWriteFreq(_frequency);
			if (_onState == HIGH)
			{
				analogWrite(_pin, _dutyCycle * 255L / 100);
			}
			else
			{
				analogWrite(_pin, (100 - _dutyCycle) * 255L / 100);
			}
#elif ESP32
			if (_onState == HIGH)
			{
				ledcWrite(INS_ESP32_PWM_CHANNEL, _dutyCycle * 255L / 100);
			}
			else
			{
				ledcWrite(INS_ESP32_PWM_CHANNEL, (100 - _dutyCycle) * 255L / 100);
			}
#else
			// This fallback may not work as some platforms limit the frequency to 20 kHz.
			// Also, tone does not support duty cycle.
			tone(_pin, _frequency);
#endif
		}
		else
		{
#if AVR
			TCCR2A &= 0xF;
#elif ESP8266
			digitalWrite(_pin, 1 ^ _onState);
#elif ESP32
			ledcWrite(INS_ESP32_PWM_CHANNEL, (1 ^ _onState) * 255);
#else
			noTone(_pin);
			digitalWrite(_pin, 1 ^ _onState);
#endif
		}
	}
};

// This should only be used as a fallback.
// Will attempt to run a soft PWM in parallel with all other running tasks.
// That will only work in low load conditions and with a fast enough MCU.
// It is unlikely to ever work on AVR and similar MCUs.
class SoftPWMPinWriter : public PinWriter, public SteppedTask
{
	static const uint8_t kFractionalBits = 8;
	const uint8_t _pin;
	const uint8_t _onState;
	uint8_t _modulating = false;
	uint8_t _state;
	uint16_t _onTime = 0;
	uint16_t _offTime;
	uint16_t _accumulator;
public:
	SoftPWMPinWriter(uint8_t pin, uint8_t onState) :
		_pin(pin),
		_onState(onState)
	{
		pinMode(pin, OUTPUT);
	}
	void prepare(uint32_t frequency, uint8_t dutyCycle)
	{
		uint16_t periodMicros = ((1000000UL << kFractionalBits) + (frequency / 2)) / frequency;
		_onTime = periodMicros * dutyCycle / 100;
		_offTime = periodMicros - _onTime;
	}
	void write(uint8_t value) override
	{
		if (!_onTime)
			InsError(*(uint32_t*)"sefr");
		digitalWrite(_pin, value);
		_modulating = value == _onState;
		_state = value;
		if (_modulating)
		{
			_accumulator = 0;
		}
	}
	uint16_t SteppedTask_step() override
	{
		if (!_modulating)
		{
			return _offTime >> kFractionalBits;
		}
		_state = 1 ^ _state;
		digitalWrite(_pin, _state);
		if (_state == _onState)
			_accumulator += _onTime;
		else
			_accumulator += _offTime;
		uint16_t micros = _accumulator >> kFractionalBits;
		_accumulator -= micros << kFractionalBits;
		return micros;
	}
};

// Open drain pin writer with collision detection
class CheckingPinWriter : public PinWriter, public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void CheckingPinWriterDelegate_error(uint8_t pin) = 0;
	};

private:
	uint16_t _step;
	uint8_t _pin;
	Delegate *_delegate;
	uint8_t _onState;
	uint8_t _pinState;
	bool _didTransition = false;

public:
	CheckingPinWriter(uint8_t pin, uint16_t step, Delegate *delegate, uint8_t onState) : _step(step), _pin(pin), _delegate(delegate), _onState(onState), _pinState(1 & (~onState)) {}

	void write(uint8_t value) override
	{
		if (value == _onState)
			pinMode(_pin, OUTPUT);
		else
			pinMode(_pin, INPUT);
		if (_pin != _pinState)
		{
			_didTransition = true;
		}
		_pinState = value;
	}

	uint16_t SteppedTask_step() override
	{
		if (_didTransition)
		{
			_didTransition = false;
		}
		else if (digitalRead(_pin) != _pinState)
		{
			_delegate->CheckingPinWriterDelegate_error(_pin);
		}
		return _step;
	}
};

// Task that sends a jam signal.
class TxJam : public SteppedTask
{
	PinWriter *_pin;
	uint8_t _mark;
	uint32_t _length;
	uint8_t _count;
	uint32_t _microsSinceStart;
public:
	TxJam(PinWriter *pin, uint8_t mark, uint32_t length) :
		_pin(pin), _mark(mark), _length(length), _count(-1)
	{
		prepare(length);
	}

	void prepare(uint32_t length)
	{
		_length = length;
		_count = -1;
	}

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (!_count)
		{
			_microsSinceStart = 0;
			_pin->write(_mark);
			return _length;
		}
		if (_microsSinceStart >= _length)
		{
			_pin->write(1 ^ _mark);
			_count = -1;
			return kInvalidDelta;
		}
		_microsSinceStart += kMaxSleepMicros;
		if (_length - _microsSinceStart > kMaxSleepMicros)
			return kMaxSleepMicros;
		return _length - _microsSinceStart;
	}
};

// Input filter and timekeeper
class InputFilter
{
	uint8_t _ones = 0;
	unsigned long _lastTransitionTime = 0;

public:
	bool setState(bool pinState)
	{
#if INS_ENABLE_INPUT_FILTER // Removes single glitches
		bool oldPinState = getPinState();
		if (pinState && _ones < 3)
			 ++_ones;
		else if (!pinState && _ones > 0)
			--_ones;
		bool newPinState = getPinState();
		return oldPinState != newPinState;
#else
		bool oldPinState = !!_ones;
		_ones = pinState;
		return oldPinState != pinState;
#endif
	}

	bool getPinState()
	{
#if INS_ENABLE_INPUT_FILTER
		return _ones > 1;
#else
		return !!_ones;
#endif
	}

	uint32_t getAndUpdateTimeSinceLastTransition(unsigned long us)
	{
		uint32_t timeOffset = uint32_t(us - _lastTransitionTime);
		_lastTransitionTime = us;
		return timeOffset;
	}

	uint32_t getTimeSinceLastTransition(unsigned long us)
	{
		uint32_t timeOffset = uint32_t(us - _lastTransitionTime);
		return timeOffset;
	}
};

}

#endif
