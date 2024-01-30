// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_SIRC_H_
#define _INS_PROTOCOL_SIRC_H_

// Sony SIRC protocol
// https://www.sbprojects.net/knowledge/ir/sirc.php
// The same protocol is used on both "CONTROL S" connectors and remotes.
// IR modulation is 40 kHz.
// CONTROL S signals are active low.
// Note that one of the CONTROL S pins is +5V!
// To make sure you don't break anything measure and check the service manual!

#include "ProtocolUtils.h"

namespace inseparates
{

class TxSIRC : public SteppedTask
{
	friend class RxSIRC;
	static const uint16_t kStartMarkMicros = 2400;
	static const uint16_t kStepMicros = 600;
	static const uint16_t kRepeatInterval = 45000;

	uint32_t _data;
	uint8_t _bits;
	PinWriter *_pin;
	uint8_t _markVal;
	uint8_t _count;
	uint16_t _startMicros;
public:
	TxSIRC(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _count(-1)
	{
	}

	void prepare(uint32_t data, uint8_t bits)
	{
		_data = data;
		_bits = bits;
		_count = -1;
	}

	// No safety belts here, can overflow!
	static inline uint16_t encodeSIRC(uint8_t address, uint8_t command) { return (uint16_t(address) << 7) | command; }
	static inline uint32_t encodeSIRC20(uint8_t extended, uint8_t address, uint8_t command) { return (uint32_t(extended) << 12) | (uint16_t(address) << 7) | command; }

	uint16_t SteppedTask_step(uint32_t now) override
	{
		++_count;
		if (_count > (_bits << 1) + 1)
		{
			_count = -1;
			return Scheduler::kInvalidDelta;
		}
		bool bitBoundry = !(_count & 1);
		_pin->write(bitBoundry ? _markVal : 1 ^ _markVal);
		if (!_count)
		{
			_startMicros = now;
			return kStartMarkMicros;
		}
		if (_count == (_bits << 1) + 1)
		{
			uint16_t microsUntilRepeat = kRepeatInterval - (uint16_t(now) - _startMicros);
			return microsUntilRepeat;
		}
		if (!bitBoundry)
		{
			return kStepMicros;
		}
		uint8_t bitNum = (_count >> 1) - 1;
		bool bitVal = (_data >> bitNum) & 1;
		return bitVal ? kStepMicros << 1 : kStepMicros;
	}
};

class RxSIRC : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxSIRCDelegate_data(uint32_t data, uint8_t bits) = 0;
	};

private:
	static const uint16_t kSIRCStartMarkMinMicros = 2200;
	static const uint16_t kSIRCStartMarkMaxMicros = 3000;
	static const uint16_t kSIRCShortMinMicros = 500;
	static const uint16_t kSIRCShortMaxMicros = 800;
	static const uint16_t kSIRCLongMinMicros = 1050;
	static const uint16_t kSIRCLongMaxMicros = 1550;

	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint32_t _data;
	uint8_t _maxBits;
	uint8_t _count;

public:
	// If messages are less than 20 bits it is impossible to know if the message is complete when the last bit is received!
	// You need to specify number of bits or poll by running step to trigger receive callback!
	// Otherwise 12-bit or 15-bit messages will not be received!
	RxSIRC(uint8_t pin, uint8_t markVal, Delegate *delegate, uint8_t maxBits = 20) :
		_pin(pin), _markVal(markVal), _delegate(delegate), _maxBits(maxBits)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count == uint8_t(-1))
			{
				return kSIRCLongMinMicros >> 4;
			}
			if (_count >= 24 && _inputHandler.getTimeSinceLastTransition(now) > kSIRCStartMarkMinMicros)
			{
				if (_delegate)
					_delegate->RxSIRCDelegate_data(_data, _count >> 1);
				reset();
			}
			if (_count)
			{
				if (_inputHandler.getTimeSinceLastTransition(now) > kSIRCStartMarkMaxMicros * 2)
				{
					reset();
				}
			}
			return kSIRCLongMinMicros >> 4;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return kSIRCLongMinMicros >> 4;
	}

	void inputChanged(bool pinState, uint16_t pulseTime)
	{
		if (_count == uint8_t(-1))
		{
			// First mark check
			if (!pinState)
			{
				return;
			}
			_count = 0;
			return;
		}

		if (_count >= 24 && pulseTime > kSIRCStartMarkMinMicros)
		{
			if (_delegate)
				_delegate->RxSIRCDelegate_data(_data, _count >> 1);
			reset();
		}

		_count += 1;

		if (_count == 1)
		{
			if (pulseTime < kSIRCStartMarkMinMicros || pulseTime > kSIRCStartMarkMaxMicros)
			{
				reset();
				return;
			}
			return;
		}

		if (pinState)
		{
			if (!validatePulseWidth(pulseTime))
			{
				reset();
				return;
			}
			return;
		}

		if (pulseTime >= kSIRCShortMinMicros && pulseTime <= kSIRCShortMaxMicros)
		{
			// Short pulse, consider as 0
		}
		else if (pulseTime >= kSIRCLongMinMicros && pulseTime <= kSIRCLongMaxMicros)
		{
			// Long pulse, consider as 1
			_data |= uint32_t(1) << ((_count - 2) >> 1);
		}
		else
		{
			reset();
			return;
		}

		if (!pinState && _count >= (_maxBits << 1))
		{
			if (_delegate)
				_delegate->RxSIRCDelegate_data(_data, _count >> 1);
			reset();
		}
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		return (pulseWidth >= kSIRCShortMinMicros && pulseWidth <= kSIRCShortMaxMicros);
	}
};

}

#endif
