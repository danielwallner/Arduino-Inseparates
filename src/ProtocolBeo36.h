// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_BEO_OLD_H_
#define _INS_PROTOCOL_BEO_OLD_H_

#include "ProtocolUtils.h"

// Old 36 kHz IR format.

// This protocol is distance encoded, LSB first and only six bits + start bit.
// It looks like the start bit is always zero and never used to send data.
// If you need to be able to send a one in the start bit the _data shifts must be removed below.

// A Beovision remote did output 6 pulses at 35.7 kHz for each mark.

namespace inseparates
{

class TxBeo36 : public SteppedTask
{
	friend class RxBeo36;

	static const uint16_t kMarkMicros = 154;
	static const uint16_t kT1 = 5100 - kMarkMicros; // 0
	static const uint16_t kT2 = 7100 - kMarkMicros; // 1
	static const uint16_t kIdleMicros = 14100;

	uint8_t _data;
	PinWriter *_pin;
	uint8_t _markVal;
	uint8_t _count;
public:
	TxBeo36(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _count(-1)
	{
	}

	void prepare(uint8_t data)
	{
		_data = data << 1;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t /*now*/) override
	{
		++_count;
		if (_count >= 16)
		{
			_count = -1;
			return Scheduler::kInvalidDelta;
		}
		bool startOfMark = !(_count & 1);
		if (startOfMark)
		{
			_pin->write(_markVal);
			return kMarkMicros;
		}
		_pin->write(1 ^ _markVal);
		if (_count >= 15)
		{
			return 13700;
		}
		uint8_t bitnum = _count >> 1;
		bool thisBit = (_data >> bitnum) & 1;
		if (!thisBit)
			return kT1;
		return kT2;
	}
};

class RxBeo36 : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxBeo36Delegate_data(uint8_t data) = 0;
	};

private:
	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint8_t _lastBit;
	uint8_t _data;
	uint8_t _count;

public:
	RxBeo36(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_lastBit = 1;
		_data = 0;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count != uint8_t(-1) && _inputHandler.getTimeSinceLastTransition(now) > 10000)
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
			// Wait for mark
			if (!pinState)
			{
				return;
			}
			// Do not check for enough idle time here because pulseTime could have wrapped.
			_count = 0;
			return;
		}

		++_count;

		if (!pinState)
		{
			if (!validMarkPulseWidth(pulseTime))
			{
				reset();
				return;
			}
			if (_count == 15)
			{
				if (_delegate)
					_delegate->RxBeo36Delegate_data(_data >> 1);
				reset();
			}
			return;
		}

		uint8_t t = validDistance(pulseTime);

		if (!t)
		{
			reset();
			return;
		}

		_data |= (t - 1) << (_count - 2) / 2;
		return;
	}

private:
	bool validMarkPulseWidth(uint16_t pulseWidth)
	{
		if (pulseWidth > 100 && pulseWidth < 250)
		{
			return true;
		}
		return false;
	}

	uint8_t validDistance(uint16_t distance)
	{
		if (distance < 4500)
			return 0;
		if (distance < 5700)
			return 1;
		if (distance < 6500)
			return 0;
		if (distance > 7700)
			return 0;
		return 2;
	}
};

}

#endif
