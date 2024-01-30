// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_ESI_H_
#define _INS_PROTOCOL_ESI_H_

// Philips ESI bus protocol
// Measured waveforms can be found in extra/pictures/ESI*.png
// Note that these waveforms are inverted compared to what's on the actual connector.
// The ESI bus is active high (5V) open collector!

// The repeat rate is unknown but is set here to 50 ms which is about twice the message length.

#include "ProtocolUtils.h"

namespace inseparates
{

class TxESI : public SteppedTask
{
	friend class RxESI;
	static const uint16_t kStepMicros = 444;
	static const uint16_t kRepeatInterval = 50000;

	uint32_t _data;
	PinWriter *_pin;
	uint8_t _markVal;
	bool _state; // true -> mark
	bool _current;
	uint8_t _count;
	uint16_t _startMicros;
public:
	TxESI(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _state(false), _count(-1)
	{
	}

	void prepare(uint32_t data)
	{
		_data = data;
		_state = false;
		_current = true;
		_count = -1;
	}

	// No safety belts here, can overflow!
	// The meaning of the upper byte is unknown!
	// RC-5 messages received by a 900-series amplifier was forwarded with upper byte set to 0x10 but that does not seem to be required as it itself reacts when this is any random number.
	// Note that bits 24 to 27 must be 0.
	static inline uint32_t encodeRC5(uint8_t upper, uint8_t toggle, uint8_t address, uint8_t command) { return (uint32_t(upper) << 16) | (address << 8) | (toggle << 7) | command; }

	uint16_t SteppedTask_step(uint32_t now) override
	{
		++_count;
		if (_count == 0)
		{
			_startMicros = now;
		}
		if (_count > 58)
		{
			prepare(_data);
			return Scheduler::kInvalidDelta;
		}
		bool bitBoundry = !(_count & 1);
		if (bitBoundry)
		{
			_state = !_state;
			_pin->write(_state ? _markVal : 1 ^ _markVal);
			if (_count == 56)
			{
				if (_state)
					return kStepMicros;
				_count = 58;
				uint16_t microsUntilRepeat = kRepeatInterval - (uint16_t(now) - _startMicros);
				return microsUntilRepeat;
			}
			uint8_t bitnum = (55 - _count) >> 1;
			bool bit = (_data >> bitnum) & 1;
			if (bit != _current)
			{
				_current = bit;
				++_count;
				return 2 * kStepMicros;
			}
			return kStepMicros;
		}
		if (_count >= 57)
		{
			_state = !_state;
			_pin->write(_state ? _markVal : 1 ^_markVal);
			_count = 58;
			uint16_t microsUntilRepeat = kRepeatInterval - (uint16_t(now) - _startMicros);
			return microsUntilRepeat;
		}
		uint8_t bitnum = (55 - _count) >> 1;
		bool bit = (_data >> bitnum) & 1;
		if (bit == _current)
		{
			_state = !_state;
			_pin->write(_state ? _markVal : 1 ^_markVal);
		}
		else
		{
			_current = bit;
		}
		return kStepMicros;
	}
};


// Does not handle a zero start bit (which is not valid RC-5)
class RxESI : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxESIDelegate_data(uint32_t data) = 0;
	};

private:
	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint32_t _data;
	bool _toggled;
	bool _current;
	uint8_t _count;

public:
	RxESI(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_toggled = false;
		_current = true;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count != uint8_t(-1) && _inputHandler.getTimeSinceLastTransition(now) > TxESI::kStepMicros * 4)
			{
				reset();
			}
			return 20;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return 20;
	}

	void inputChanged(bool pinState, uint16_t pulseTime)
	{
		if (_count == uint8_t(-1))
		{
			if (!pinState)
			{
				return;
			}
			// First mark.
			_count = 0;
			return;
		}

		uint8_t steps = validatePulseWidth(pulseTime) ? 1 : validatePulseWidth(pulseTime >> 1) ? 2 : 0;

		if (steps == 0)
		{
			reset();
			return;
		}

		_count += steps;

		bool atBitBoundry = !(_count & 1);
		if (atBitBoundry)
		{
			_data <<= 1;
			if (!_toggled)
				_current = !_current;
			_data |= _current ? 1 : 0;
			_toggled = false;
		}
		else
		{
			if (steps == 2)
			{
				reset();
				return;
			}
			_toggled = true;
		}

		if (!pinState && _count >= 56)
		{
			if (_delegate)
				_delegate->RxESIDelegate_data(_data);
			reset();
		}
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		static const uint16_t minLimit = 350;
		static const uint16_t maxLimit = 600;
		return (pulseWidth >= minLimit && pulseWidth <= maxLimit);
	}
};

}

#endif
