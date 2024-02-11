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
#include "DebugUtils.h"

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
	uint8_t _mark;
	uint8_t _count;
	uint16_t _microsAccumulator;
	bool _sleepUntilRepeat;
public:
	TxSIRC(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _count(-1)
	{
	}

	void prepare(uint32_t data, uint8_t bits, bool sleepUntilRepeat = true)
	{
		_data = data;
		_bits = bits;
		_count = -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	// No safety belts here, can overflow!
	static inline uint16_t encodeSIRC(uint8_t address, uint8_t command) { return (uint16_t(address) << 7) | command; }
	static inline uint32_t encodeSIRC20(uint8_t extended, uint8_t address, uint8_t command) { return (uint32_t(extended) << 12) | (uint16_t(address) << 7) | command; }

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count > (_bits << 1) + 1)
		{
			_count = -1;
			return SteppedTask::kInvalidDelta;
		}
		bool bitBoundry = !(_count & 1);
		_pin->write(bitBoundry ? _mark : 1 ^ _mark);
		if (!_count)
		{
			_microsAccumulator = kStartMarkMicros;
			return kStartMarkMicros;
		}
		if (_count == (_bits << 1) + 1)
		{
			if (!_sleepUntilRepeat)
			{
				_count = -1;
				return SteppedTask::kInvalidDelta;
			}
			uint16_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
			_microsAccumulator += microsUntilRepeat;
			return microsUntilRepeat;
		}
		if (!bitBoundry)
		{
			_microsAccumulator += kStepMicros;
			return kStepMicros;
		}
		uint8_t bitNum = (_count >> 1) - 1;
		bool bitVal = (_data >> bitNum) & 1;
		uint16_t sleepTime = bitVal ? kStepMicros << 1 : kStepMicros;
		_microsAccumulator += sleepTime;
		return sleepTime;
	}
};

class RxSIRC : public Decoder
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
	static const uint16_t kTimeout = kSIRCStartMarkMaxMicros + kSIRCShortMinMicros;

	uint8_t _mark;
	Delegate *_delegate;
	uint32_t _data;
	uint8_t _maxBits;
	uint8_t _count;

public:
	// If messages are less than 20 bits it is impossible to know if the message is complete when the last bit is received!
	// You need to specify number of bits or poll by running step to trigger receive callback!
	// Otherwise 12-bit or 15-bit messages will not be received!
	RxSIRC(uint8_t mark, Delegate *delegate, uint8_t maxBits = 20) :
		_mark(mark), _delegate(delegate), _maxBits(maxBits)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_count = -1;
	}

	void Decoder_timeout(uint8_t pinState) override
	{
		if (_count == uint8_t(-1))
		{
			INS_ASSERT(0);
			return;
		}

		if (pinState != _mark)
		{
			if (_delegate)
				_delegate->RxSIRCDelegate_data(_data, _count >> 1);
		}
		reset();
	}

	uint16_t Decoder_pulse(uint8_t pulseState, uint16_t pulseWidth) override
	{
		bool mark = pulseState == _mark;
		if (_count == uint8_t(-1))
		{
			// First mark check
			if (!mark)
			{
				return Decoder::kInvalidTimeout;
			}
		}

		_count += 1;

		if (_count == 0)
		{
			if (pulseWidth < kSIRCStartMarkMinMicros || pulseWidth > kSIRCStartMarkMaxMicros)
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			return kTimeout;
		}

		if (!mark)
		{
			if (!validatePulseWidth(pulseWidth))
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			return kTimeout;
		}

		if (pulseWidth >= kSIRCShortMinMicros && pulseWidth <= kSIRCShortMaxMicros)
		{
			// Short pulse, consider as 0
		}
		else if (pulseWidth >= kSIRCLongMinMicros && pulseWidth <= kSIRCLongMaxMicros)
		{
			// Long pulse, consider as 1
			_data |= uint32_t(1) << ((_count - 2) >> 1);
		}
		else
		{
			reset();
			return Decoder::kInvalidTimeout;
		}

		if (mark && _count >= (_maxBits << 1))
		{
			if (_delegate)
				_delegate->RxSIRCDelegate_data(_data, _count >> 1);
			reset();
			return Decoder::kInvalidTimeout;
		}
		return kTimeout;
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		return (pulseWidth >= kSIRCShortMinMicros && pulseWidth <= kSIRCShortMaxMicros);
	}
};

}

#endif
