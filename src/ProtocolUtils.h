// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_UTILS_H_
#define _INS_PROTOCOL_UTILS_H_

#include "Inseparates.h"

#include "PlatformTimers.h"

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
	uint8_t _offMode;
public:
	OpenDrainPinWriter(uint8_t pin, uint8_t onState, uint8_t offMode = INPUT) :
		_pin(pin), _onState(onState), _offMode(offMode)
	{
		pinMode(pin, _offMode);
		if (_offMode == OUTPUT)
			digitalWrite(pin, onState ? 0 : 1);
		else
			digitalWrite(pin, onState);
	}

	void write(uint8_t value) override
	{
		if ( _offMode == OUTPUT)
		{
			digitalWrite(_pin, value);
		}
		else if (value == _onState)
		{
			pinMode(_pin, OUTPUT);
		}
		else
		{
			pinMode(_pin, _offMode);
		}
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
#if defined(AVR)
		uint16_t divisor = 1;
		uint16_t pwmTop;
		uint8_t p = 1;
		for (; p <= 2; ++p)
		{
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
		OCR2B = (_dutyCycle * pwmTop) / 100;
#elif defined(ESP8266)
		// ESP8266 analogWrite is a software PWM that doesn't work for high frequencies (Depending on CPU clock).
		if (esp_get_cpu_freq_mhz() < 100UL)
		{
			if (frequency > 200000)
				InsError(*(uint32_t*)"pfrq");
		}
		analogWriteRange(63);
#elif defined(ESP32)
		// ESP32 ledcAttach has an upper limit.
		if (frequency > 300000)
			InsError(*(uint32_t*)"pfrq");
		ledcAttach(_pin, _frequency, 6);
#endif
	}

	void write(uint8_t value) override
	{
		if (!_frequency)
			InsError(*(uint32_t*)"hefr");
		if (value == _onState)
		{
#if defined(AVR)
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
#elif defined(ESP8266)
			analogWriteFreq(_frequency);
			if (_onState == HIGH)
			{
				analogWrite(_pin, _dutyCycle * 63U / 100);
			}
			else
			{
				analogWrite(_pin, (100 - _dutyCycle) * 63U / 100);
			}
#elif defined(ESP32)
			if (_onState == HIGH)
			{
				ledcWrite(_pin, _dutyCycle * 63U / 100);
			}
			else
			{
				ledcWrite(_pin, (100 - _dutyCycle) * 63U / 100);
			}
#else
			// This fallback may not work as some platforms limit the frequency to 20 kHz.
			// Also, tone does not support duty cycle.
			tone(_pin, _frequency);
#endif
		}
		else
		{
#if defined(AVR)
			TCCR2A &= 0xF;
#elif defined(ESP8266)
			digitalWrite(_pin, 1 ^ _onState);
#elif defined(ESP32)
			ledcWrite(_pin, (1 ^ _onState) * 255);
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
	bool _modulating = false;
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
		if (!_onTime || !_offTime)
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
	uint8_t _offState;
	uint8_t _pinState;
	bool _enabled = false;
	bool _didTransition = false;

public:
	CheckingPinWriter(uint8_t pin, uint16_t step, Delegate *delegate, uint8_t onState, uint8_t offState = INPUT) :
		_step(step), _pin(pin), _delegate(delegate), _onState(onState), _offState(offState), _pinState(1 & (~onState))
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
		if (value != _pinState)
		{
			_didTransition = true;
		}
		_pinState = value;
	}

	void enable() { _enabled = true; }
	void disable() { _enabled = false; }

	bool enabled() { return _enabled; }

	uint16_t SteppedTask_step() override
	{
		if (_didTransition)
		{
			_didTransition = false;
		}
		else if (_enabled && digitalRead(_pin) != _pinState)
		{
			_enabled = false;
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

#if INS_HAVE_HW_TIMER || UNIT_TEST
class InterruptWriteScheduler : public SteppedTask
{
	friend class InterruptPinWriter;

	struct OutputData
	{
		uint16_t micros;
		uint8_t pin;
		uint8_t state;
		uint8_t mode;
	};

	uint16_t _pollIntervalMicros;
	uint16_t _futureMicros;
	uint16_t _currentMicros;
	uint16_t _currentDelta;
	bool _didWrite = false;
	SteppedTask *_task = nullptr;
	Scheduler::Delegate *_delegate;
	LockFreeFIFO<OutputData, INS_INPUT_FIFO_LENGTH> _outputFIFO;
#ifndef UNIT_TEST
	HWTimer _timer;
#endif
	static InterruptWriteScheduler *s_this;
public:
	InterruptWriteScheduler(uint16_t pollIntervalMicros, uint16_t futureMicros) :
		_pollIntervalMicros(pollIntervalMicros), _futureMicros(futureMicros)
	{
		s_this = this;
#ifdef UNIT_TEST
		attachInterruptInterval(pollIntervalMicros, timerISR);
#else
		_timer.attachInterruptInterval(pollIntervalMicros, timerISR);
#endif
	}

	bool add(SteppedTask *task, Scheduler::Delegate *delegate = nullptr)
	{
		if (_task)
		{
			InsError(*(uint32_t*)"txst");
			return false;
		}
		_delegate = delegate;
		scheduleInternal(task);
		return true;
	}

	bool active()
	{
		return !!_task;
	}

	uint16_t SteppedTask_step() override
	{
		if (_task)
			scheduleInternal(_task);
		return _pollIntervalMicros * (INS_INPUT_FIFO_LENGTH / 2);
	}

private:
	void scheduleInternal(SteppedTask *task)
	{
		if (!_task)
		{
			if (_currentMicros < fastMicros() + _futureMicros)
				_currentMicros = fastMicros() + _futureMicros;
			_currentDelta = 0;
			_task = task;
		}
		while (!_outputFIFO.full())
		{
			uint16_t delta = task->SteppedTask_step();
			// The task should have called write now.
			_currentMicros += _currentDelta;
			_currentDelta = delta;
			if (_didWrite)
			{
				_didWrite = false;
			}
			else
			{
				_outputFIFO.writeRef().pin = -1;
			}
			_outputFIFO.writeRef().micros = _currentMicros;
			_outputFIFO.push();
			if (delta == kInvalidDelta)
			{
				SteppedTask *task = _task;
				_task = nullptr;
				if (_delegate)
					_delegate->SchedulerDelegate_done(task);
				return;
			}
			if (delta == kInvalidDelta)
			{
				_currentDelta = 0;
			}
		}
	}

	void write(uint8_t pin, uint8_t state, uint8_t mode)
	{
		if (!_task)
		{
			// This can happen during initialization.
			if (mode == OUTPUT)
			{
				digitalWrite(pin, state);
				pinMode(pin, mode);
			}
			else
			{
				pinMode(pin, mode);
				digitalWrite(pin, state);
			}
			return;
		}
		_outputFIFO.writeRef().pin = pin;
		_outputFIFO.writeRef().state = state;
		_outputFIFO.writeRef().mode = mode;
		_didWrite = true;
	}

	void INS_IRAM_ATTR pollOutputISR()
	{
		if (_outputFIFO.empty())
		{
			return;
		}

		uint16_t now = fastMicros();
		uint16_t targetMicros = _outputFIFO.readRef().micros;
		int16_t timeLeft = targetMicros - now;
		if (timeLeft > _pollIntervalMicros / 2)
		{
			return;
		}

		uint8_t pin = _outputFIFO.readRef().pin;
		if (pin == (uint8_t)-1)
		{
			_outputFIFO.pop();
			return;
		}
		uint8_t state = _outputFIFO.readRef().state;
		uint8_t mode = _outputFIFO.readRef().mode;
		_outputFIFO.pop();
		if (mode == OUTPUT)
		{
			digitalWrite(pin, state);
			pinMode(pin, mode);
		}
		else
		{
			pinMode(pin, mode);
			digitalWrite(pin, state);
		}
	}

	static void INS_IRAM_ATTR timerISR()
	{
		s_this->pollOutputISR();
	}
};

// offMode == OUTPUT -> push-pull.
// offMode != OUTPUT -> pseudo open drain.
// When offMode == OUTPUT onState is ignored.
// IMPORTANT: Encoders using this pin writer must be added to the InterruptWriteScheduler not the normal Scheduler!
class InterruptPinWriter : public PinWriter
{
	InterruptWriteScheduler *_scheduler;
	uint8_t _pin;
	uint8_t _onState;
	uint8_t _offMode;
public:
	InterruptPinWriter(InterruptWriteScheduler *scheduler, uint8_t pin, uint8_t onState = HIGH, uint8_t offMode = OUTPUT) :
		_scheduler(scheduler), _pin(pin), _onState(onState), _offMode(offMode)
	{
		pinMode(pin, _offMode);
		if (offMode != OUTPUT)
			digitalWrite(pin, onState);
	}

	void write(uint8_t value) override
	{
		if (_offMode == OUTPUT || value == _onState)
		{
			_scheduler->write(_pin, value, OUTPUT);
			return;
		}
		_scheduler->write(_pin, _onState, _offMode);
	}
};
#endif

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
