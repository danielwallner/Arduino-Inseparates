// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_NEC_H_
#define _INS_PROTOCOL_NEC_H_

// NEC protocol
// https://www.sbprojects.net/knowledge/ir/nec.php
// The same protocol is used on both IR/SR connectors and remotes.
// IR modulation is 38 kHz.
// R/SR I/O are usually active low.

#include "ProtocolUtils.h"

namespace inseparates
{

class TxNEC : public SteppedTask
{
	static const uint16_t kStartMarkMicros = 9000;
	static const uint16_t kStartSpaceMicros = 4500;
	static const uint16_t kMarkMicros = 562;
	static const uint16_t kOneSpaceMicros = 1688;
	static const uint16_t kZeroSpaceMicros = 563;
	static const uint32_t kRepeatInterval = 110000;

	uint32_t _data;
	PinWriter *_pin;
	uint8_t _markVal;
	uint8_t _count;
	uint32_t _startMicros;
public:
	TxNEC(PinWriter *pin, uint8_t markVal) :
	_pin(pin), _markVal(markVal), _count(-1)
	{
	}

	// Setting data to 0 sends a repeat code
	void prepare(uint32_t data)
	{
		_data = data;
		_count = -1;
	}

	static inline uint32_t encodeNEC(uint8_t address, uint8_t command) { return address | ((0xFF & ~address) << 8) | ((uint32_t)command << 16) | ((uint32_t)~command) << 24; }
	static inline uint32_t encodeExtendedNEC(uint16_t address, uint8_t command) { return address | ((uint32_t)command << 16) | ((uint32_t)~command) << 24; }

	uint16_t SteppedTask_step(uint32_t now) override
	{
		++_count;
		if (_count > 67)
		{
			return idleTimeLeft(now);
		}
		bool bitBoundry = !(_count & 1);
		_pin->write(bitBoundry ? _markVal : 1 ^ _markVal);
		if (_count < 2)
		{
			if (_count == 0)
			{
				_startMicros = now;
				return kStartMarkMicros;
			}
			return _data == 0 ? (kStartSpaceMicros / 2) : kStartSpaceMicros;
		}
		if (bitBoundry)
		{
			return kMarkMicros;
		}
		if (_data == 0 || _count == 67)
		{
			_count = 67;
			return idleTimeLeft(now);
		}
		uint8_t bitnum = (_count - 2) >> 1;
		bool bitVal = (_data >> bitnum) & 1;
		return bitVal ? kOneSpaceMicros : kZeroSpaceMicros;
	}

private:
	uint16_t idleTimeLeft(uint32_t now)
	{
		int32_t microsUntilRepeat = kRepeatInterval - (now - _startMicros);
		if (microsUntilRepeat <= 0)
		{
			_count = -1;
			return Scheduler::kInvalidDelta;
		}
		if (microsUntilRepeat > Scheduler::kMaxSleepMicros)
			return Scheduler::kMaxSleepMicros;
		return microsUntilRepeat;
	}
};

class RxNEC : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxNECDelegate_data(uint32_t data) = 0;
	};

private:
	static const uint16_t kNECStartMarkMinMicros = 8000;
	static const uint16_t kNECStartMarkMaxMicros = 10000;
	static const uint16_t kNECRepeatSpaceMinMicros = 2000;
	static const uint16_t kNECStartSpaceMinMicros = 4000;
	static const uint16_t kNECStartSpaceMaxMicros = 5000;
	static const uint16_t kNECMarkWidthMinMicros = 450;
	static const uint16_t kNECMarkWidthMaxMicros = 750;
	static const uint16_t kNECPeriodMinMicros = 1000;
	static const uint16_t kNECPeriodMaxMicros = 1300;

	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint32_t _data;
	uint16_t _markLength;
	uint8_t _count;
	bool _repeat;

public:
	RxNEC(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_count = -1;
		_repeat	= false;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count != uint8_t(-1) && _inputHandler.getTimeSinceLastTransition(now) > kNECStartMarkMaxMicros * 2)
			{
				if (_repeat)
					_delegate->RxNECDelegate_data(0);
				reset();
			}
			return 15;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return 15;
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

		_count += 1;

		if (_count == 1)
		{
			if (pulseTime < kNECStartMarkMinMicros || pulseTime > kNECStartMarkMaxMicros)
			{
				reset();
				return;
			}
			return;
		}

		if (_count == 2)
		{
			if (pulseTime < kNECStartSpaceMinMicros || pulseTime > kNECStartSpaceMaxMicros)
			{
				if (pulseTime < kNECRepeatSpaceMinMicros)
				{
					reset();
					return;
				}
				_repeat	= true;
			}
			return;
		}

		if (!pinState)
		{
			if (!validMarkPulseWidth(pulseTime))
			{
				reset();
				return;
			}
			_markLength = pulseTime;
			if (_count >= 66)
			{
				if (_delegate)
					_delegate->RxNECDelegate_data(_data);
				reset();
			}
			return;
		}

		if (_repeat && _count > 3)
		{
			reset();
			return;
		}

		uint8_t distance = validDistance(_markLength + pulseTime);
		switch (distance)
		{
		case 2:
			// Long pulse => 1
			_data |= uint32_t(1) << ((_count - 4) >> 1);
			return;
		default:
			reset();
		case 1:
			// Short pulse => 0
			return;
		}
	}

private:
	bool validMarkPulseWidth(uint16_t pulseWidth)
	{
		return (pulseWidth >= kNECMarkWidthMinMicros && pulseWidth <= kNECMarkWidthMaxMicros);
	}

	int8_t validDistance(uint16_t distance)
	{
		if (distance >= kNECPeriodMinMicros && distance <= kNECPeriodMaxMicros)
			return 1;
		distance >>= 1;
		if (distance >= kNECPeriodMinMicros && distance <= kNECPeriodMaxMicros)
			return 2;
		return 0;
	}
};

}

#endif
