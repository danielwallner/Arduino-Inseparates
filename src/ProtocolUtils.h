// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_UTILS_H_
#define _INS_PROTOCOL_UTILS_H_

#ifndef INS_OUTPUT_FIFO_CHANNEL_COUNT
#ifdef AVR
#define INS_OUTPUT_FIFO_CHANNEL_COUNT 0
#elif defined(ESP32)
#define INS_OUTPUT_FIFO_CHANNEL_COUNT 4
#else
#define INS_OUTPUT_FIFO_CHANNEL_COUNT 2
#endif
#endif

#ifndef INS_OUTPUT_FIFO_CHANNEL_LENGTH
#define INS_OUTPUT_FIFO_LENGTH 1024
#endif

#include "Inseparates.h"
#include "PlatformTimers.h"
#include "DebugUtils.h"

#if INS_OUTPUT_FIFO_CHANNEL_COUNT
#include <deque>
#include <map>
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
	uint8_t _offMode;
public:
	OpenDrainPinWriter(uint8_t pin, uint8_t onState, uint8_t offMode = INPUT) :
		_pin(pin), _onState(onState), _offMode(offMode)
	{
		if (_offMode == OUTPUT)
			digitalWrite(pin, onState ? 0 : 1);
		else
			digitalWrite(pin, onState);
		pinMode(pin, _offMode);
	}

	void write(uint8_t value) override
	{
		if (_offMode == OUTPUT)
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
#if ESP_IDF_VERSION_MAJOR < 5
		ledcSetup(0, _frequency, 6);
		ledcAttachPin(_pin, 0);
#else
		ledcAttach(_pin, _frequency, 6);
#endif
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
#if ESP_IDF_VERSION_MAJOR < 5
			if (_onState == HIGH)
			{
				ledcWrite(0, _dutyCycle * 63U / 100);
			}
			else
			{
				ledcWrite(0, (100 - _dutyCycle) * 63U / 100);
			}
#else
			if (_onState == HIGH)
			{
				ledcWrite(_pin, _dutyCycle * 63U / 100);
			}
			else
			{
				ledcWrite(_pin, (100 - _dutyCycle) * 63U / 100);
			}
#endif
#else
			// This fallback may not work as some platforms limit the frequency to 20 kHz or even lower (or break completely when too high).
			// SAMD tone() is scary buggy and does not work above 10 kHz! (It can even hang!)
			// Also, tone() does not support duty cycle.
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
#if ESP_IDF_VERSION_MAJOR < 5
			ledcWrite(0, (1 ^ _onState) * 63);
#else
			ledcWrite(_pin, (1 ^ _onState) * 63);
#endif
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

#if (INS_HAVE_HW_TIMER || UNIT_TEST) && INS_OUTPUT_FIFO_CHANNEL_COUNT
INS_IRAM_ATTR void timerISR();

class InterruptWriteScheduler : public SteppedTask
{
	friend class InterruptPinWriter;

	struct TaskData
	{
		SteppedTask *task;
		Scheduler::Delegate *delegate;
		ins_micros_t micros;
		uint8_t pin;
	};
	struct OutputData
	{
		ins_micros_t micros;
		uint8_t pin;
		uint8_t state;
		uint8_t mode;
	};

public:
	uint16_t _pollIntervalMicros;
private:
	LockFreeFIFO<OutputData, INS_OUTPUT_FIFO_LENGTH> _outputFIFO[INS_OUTPUT_FIFO_CHANNEL_COUNT];
	OutputData *_writeRef;
	TaskData _outputFIFO_current[INS_OUTPUT_FIFO_CHANNEL_COUNT];
	std::deque<TaskData> _waitlist;
	std::deque<TaskData> _donelist;
#ifndef UNIT_TEST
	HWTimer _timer;
#endif
public:
	InterruptWriteScheduler(uint16_t pollIntervalMicros) :
		_pollIntervalMicros(pollIntervalMicros)
	{
		instance(this);
		memset(_outputFIFO_current, 0, sizeof(_outputFIFO_current));
	}

	void begin()
	{
#ifdef UNIT_TEST
		attachInterruptInterval(_pollIntervalMicros, timerISR);
#else
		_timer.attachInterruptInterval(_pollIntervalMicros, timerISR);
#endif
	}

	void add(SteppedTask *task, uint8_t pin, Scheduler::Delegate *delegate = nullptr)
	{
		TaskData td = {task, delegate, 0, pin};
		if (!activate(td))
		{
			_waitlist.push_back(td);
		}
	}

	uint16_t SteppedTask_step() override
	{
		while (_donelist.size())
		{
			TaskData &td = _donelist.front();
			td.delegate->SchedulerDelegate_done(td.task);
			_donelist.pop_front();
		}

		for (uint8_t i = 0; i < INS_OUTPUT_FIFO_CHANNEL_COUNT; ++i)
		{
			if (_outputFIFO_current[i].task)
			{
				push(i, false);
			}
		}

		for (uint8_t i = 0; i < _waitlist.size(); ++i)
		{
			TaskData &td = _waitlist[i];
			if (activate(td))
			{
				_waitlist.erase(_waitlist.begin() + i);
				--i;
			}
		}
		return _pollIntervalMicros * 10;
	}

	INS_IRAM_ATTR LockFreeFIFO<OutputData, INS_OUTPUT_FIFO_LENGTH> &outputFIFO(uint8_t fifo) { return _outputFIFO[fifo]; }

	INS_IRAM_ATTR static InterruptWriteScheduler *instance(InterruptWriteScheduler *timer = nullptr)
	{
		static InterruptWriteScheduler *s_instance;
		if (timer)
		{
			INS_ASSERT(!s_instance);
			s_instance = timer;
		}
		return s_instance;
	}

private:
	bool activate(TaskData &td)
	{
		for (uint8_t i = 0; i < INS_OUTPUT_FIFO_CHANNEL_COUNT; ++i)
		{
			if (_outputFIFO_current[i].pin == td.pin)
			{
				if (_outputFIFO_current[i].task)
					return false;
				_outputFIFO_current[i].task = td.task;
				_outputFIFO_current[i].delegate = td.delegate;
				push(i, _outputFIFO[i].empty());
				return true;
			}
		}
		for (uint8_t i = 0; i < INS_OUTPUT_FIFO_CHANNEL_COUNT; ++i)
		{
			// If there's more parallel writes in progress than INS_OUTPUT_FIFO_CHANNEL_COUNT
			// there's a risk that we go back in time on a FIFO or pin if we don't wait for all FIFOs to be empty here!
			// That will cause overlapping or all writes to be sent in a burst!
			// It should also be safe if all FIFOs haven't been used since they all were empty last since an unsafe ABCA cannot happen then.
			if (_outputFIFO[i].empty())
			{
				_outputFIFO_current[i] = td;
				push(i, true);
				return true;
			}
		}
		return false;
	}

	bool push(uint8_t index, bool first)
	{
		if (first)
		{
			// It might be a good idea to not go back in time here.
			// But that must be protected from wraparound.
			_outputFIFO_current[index].micros = fastMicros() + _pollIntervalMicros;
		}
		while (!_outputFIFO[index].full())
		{
			auto &w = _outputFIFO[index].writeRef();
			_writeRef = &w;
			uint16_t delta = _outputFIFO_current[index].task->SteppedTask_step();
			// The task should have called write now.

			if (_writeRef)
			{
				// The task did not write.
				w.pin = -1;
			}
			else
			{
				INS_ASSERT(w.pin == _outputFIFO_current[index].pin);
			}
			w.micros = _outputFIFO_current[index].micros;
			_outputFIFO[index].push();

			if (delta == kInvalidDelta)
			{
				if (_outputFIFO_current[index].delegate)
					_donelist.push_back(_outputFIFO_current[index]);
				_outputFIFO_current[index].task = nullptr;
				return true;
			}
			_outputFIFO_current[index].micros += delta;
		}
		return false;
	}

	void write(uint8_t pin, uint8_t state, uint8_t mode)
	{
		if (!_writeRef)
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
		_writeRef->pin = pin;
		_writeRef->state = state;
		_writeRef->mode = mode;
		_writeRef = nullptr;
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
		if (_offMode == OUTPUT)
			digitalWrite(pin, onState ? 0 : 1);
		pinMode(pin, _offMode);
	}

	void write(uint8_t value) override
	{
		if (_offMode == OUTPUT || value == _onState)
		{
			_scheduler->write(_pin, value, OUTPUT);
			return;
		}
		_scheduler->write(_pin, value, _offMode);
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
