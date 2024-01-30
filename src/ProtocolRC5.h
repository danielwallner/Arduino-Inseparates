// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_RC_5_H_
#define _INS_PROTOCOL_RC_5_H_

// Philips RC-5 protocol
// https://www.sbprojects.net/knowledge/ir/rc5.php
// The same protocol is used on both RC-5 connectors and remotes.
// IR modulation is 36 kHz.
// On RC-5 connectors a mark is 5V active high open collector!
// Most other IR inputs are active low!
// Connecting an RC-5 I/O to a normal IR I/O will likely destroy at least one of them!

#include "ProtocolUtils.h"

namespace inseparates
{

class TxRC5 : public SteppedTask
{
	friend class RxRC5;
	static const uint16_t kStepMicros = 889;
	static const uint32_t kRepeatInterval = 114000;

	uint16_t _data;
	PinWriter *_pin;
	uint8_t _markVal;
	uint8_t _count;
	uint32_t _startMicros;
public:
	TxRC5(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _count(-1)
	{
	}

	void prepare(uint32_t data)
	{
		_data = data;
		_count = (_data >> 13) & 1 ? 0 : -1;
	}

	// No safety belts here, can overflow!
	static inline uint16_t encodeRC5(uint8_t toggle, uint8_t address, uint8_t command) { return (uint16_t(0xC0 | (toggle << 5) | address) << 6) | command; }
	static inline uint16_t encodeRC5X(uint8_t toggle, uint8_t address, uint8_t command) { return (uint16_t(0x80 | (command & 0x40) | (toggle << 5) | address) << 6) | (command & 0x3F); }

	uint16_t SteppedTask_step(uint32_t now) override
	{
		++_count;
		if (_count <= 1) // Will be wrong for start bit == 0 but simplifies the logic
		{
			_startMicros = now;
		}
		if (_count > 28)
		{
			return idleTimeLeft(now);
		}
		uint8_t bitnum = 13 - (_count >> 1);
		bool bitVal = _count < 28 ? (_data >> bitnum) & 1 : true;
		bool bitBoundry = !(_count & 1);
		bool value = bitVal ^ bitBoundry;
		_pin->write(value ? _markVal : 1 ^ _markVal);
		if ((!value && _count == 27) || _count == 28)
		{
			_count = 28;
			return idleTimeLeft(now);
		}
		if (!bitBoundry && _count < 27)
		{
			bool nextBitVal = (_data >> (bitnum - 1)) & 1;
			if (bitVal != nextBitVal)
			{
				++_count;
				return kStepMicros << 1;
			}
		}
		return kStepMicros;
	}

private:
	uint16_t idleTimeLeft(uint32_t now)
	{
		int32_t microsUntilRepeat = kRepeatInterval - (now - _startMicros);
		if (microsUntilRepeat <= 0)
		{
			prepare(_data);
			return Scheduler::kInvalidDelta;
		}
		if (microsUntilRepeat > Scheduler::kMaxSleepMicros)
			return Scheduler::kMaxSleepMicros;
		return microsUntilRepeat;
	}
};

// Does not handle a zero start bit (which is not valid RC-5)
class RxRC5 : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxRC5Delegate_data(uint16_t data) = 0;
	};

private:
	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint16_t _data;
	bool _state;
	uint8_t _bitCount;
	bool _atBitBoundry;

public:
	RxRC5(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_state = false;
		_bitCount = 0;
		_atBitBoundry = false;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_bitCount != 0 && _inputHandler.getTimeSinceLastTransition(now) > uint32_t(TxRC5::kStepMicros) * 4)
			{
				reset();
			}
			return 40;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return 40;
	}

	void inputChanged(bool pinState, uint16_t pulseTime)
	{
		if (!_atBitBoundry && _bitCount == 0)
		{
			// Wait for mark.
			if (!pinState)
			{
				return;
			}
		}

		_state = pinState;

		uint8_t steps = !_bitCount || validatePulseWidth(pulseTime) ? 1 : 0;

		if (steps == 0 &&
			!(_atBitBoundry && (steps = (validatePulseWidth(pulseTime >> 1) ? 2 : 0))))
		{
			reset();
			return;
		}

		if (!_atBitBoundry || steps == 2)
		{
			_data <<= 1;
			_data |= pinState ? 1 : 0;
			++_bitCount;
		}

		if (steps != 2)
			_atBitBoundry = !_atBitBoundry;

		if (_atBitBoundry && _bitCount == 14)
		{
			if (_delegate)
				_delegate->RxRC5Delegate_data(_data);
			reset();
		}
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		static const uint16_t minLimit = 750;
		static const uint16_t maxLimit = 1100;
		return (pulseWidth >= minLimit && pulseWidth <= maxLimit);
	}
};

}

#endif
