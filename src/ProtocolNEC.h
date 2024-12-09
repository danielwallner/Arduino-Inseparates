// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_NEC_H_
#define _INS_PROTOCOL_NEC_H_

// NEC protocol
// https://www.sbprojects.net/knowledge/ir/nec.php
// The same protocol is used on both IR/SR connectors and remotes.
// IR modulation is 38 kHz.
// R/SR I/O are usually active low.

#include "ProtocolUtils.h"
#include "DebugUtils.h"

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
	uint8_t _mark;
	uint8_t _count;
	uint32_t _microsAccumulator;
	bool _sleepUntilRepeat;
public:
	TxNEC(PinWriter *pin, uint8_t mark) :
	_pin(pin), _mark(mark), _count(-1)
	{
	}

	// Setting data to 0 sends a repeat code
	void prepare(uint32_t data, bool sleepUntilRepeat = true)
	{
		_data = data;
		_count = -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	static inline uint32_t encodeNEC(uint8_t address, uint8_t command) { return address | ((0xFF & ~address) << 8) | ((uint32_t)command << 16) | ((uint32_t)~command) << 24; }
	static inline uint32_t encodeExtendedNEC(uint16_t address, uint8_t command) { return address | ((uint32_t)command << 16) | ((uint32_t)~command) << 24; }

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count > 67)
		{
			return idleTimeLeft();
		}
		bool bitBoundry = !(_count & 1);
		_pin->write(bitBoundry ? _mark : 1 ^ _mark);
		if (_count < 2)
		{
			if (_count == 0)
			{
				_microsAccumulator = kStartMarkMicros;
				return kStartMarkMicros;
			}
			uint16_t sleepMicros = _data == 0 ? (kStartSpaceMicros / 2) : kStartSpaceMicros;
			_microsAccumulator += sleepMicros;
			return sleepMicros;
		}
		if (bitBoundry)
		{
			_microsAccumulator += kMarkMicros;
			return kMarkMicros;
		}
		if (_data == 0 || _count == 67)
		{
			_count = 67;
			return idleTimeLeft();
		}
		uint8_t bitnum = (_count - 2) >> 1;
		bool bitVal = (_data >> bitnum) & 1;
		uint16_t sleepMicros = bitVal ? kOneSpaceMicros : kZeroSpaceMicros;
		_microsAccumulator += sleepMicros;
		return sleepMicros;
	}

private:
	uint16_t idleTimeLeft()
	{
		if (!_sleepUntilRepeat)
		{
			_count = -1;
			return SteppedTask::kInvalidDelta;
		}
		int32_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
		if (microsUntilRepeat <= 0)
		{
			_count = -1;
			return SteppedTask::kInvalidDelta;
		}
		if (microsUntilRepeat > SteppedTask::kMaxSleepMicros)
		{
			microsUntilRepeat = SteppedTask::kMaxSleepMicros;
		}
		_microsAccumulator += microsUntilRepeat;
		return microsUntilRepeat;
	}
};

class RxNEC : public Decoder
{
public:
	class Delegate
	{
	public:
		virtual void RxNECDelegate_data(uint32_t data, uint8_t bus) = 0;
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
	static const uint16_t kNECTimeout = kNECStartMarkMaxMicros + kNECStartSpaceMinMicros;

	uint8_t _mark;
	Delegate *_delegate;
	uint32_t _data;
	uint16_t _markLength;
	uint8_t _bus;
	uint8_t _count;
	bool _repeat;

public:
	RxNEC(uint8_t mark, Delegate *delegate, uint8_t bus = 0) :
		_mark(mark), _delegate(delegate), _bus(bus)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_count = -1;
		_repeat	= false;
	}

	static inline bool checkParity(uint32_t data) { return ((0xFF & data) ^ ((0xFF & ~(data >> 8)))) || ((0xFF & (data >> 24)) ^ ((0xFF & ~(data >> 16)))); }

	void Decoder_timeout(uint8_t pinState) override
	{
		if (_count == uint8_t(-1))
		{
			INS_ASSERT(0);
			return;
		}

		if (pinState != _mark)
		{
			if (_repeat)
				_delegate->RxNECDelegate_data(0, _bus);
		}
		reset();
	}

	uint16_t Decoder_pulse(uint8_t pulseState, uint16_t pulseWidth) override
	{
		bool mark = pulseState == _mark;
		if (_count == uint8_t(-1))
		{
			if (!mark)
			{
				return Decoder::kInvalidTimeout;
			}
			// First mark.
		}

		_count += 1;

		if (_count == 0)
		{
			if (pulseWidth < kNECStartMarkMinMicros || pulseWidth > kNECStartMarkMaxMicros)
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			return kNECTimeout;
		}

		if (_count == 1)
		{
			if (pulseWidth < kNECStartSpaceMinMicros || pulseWidth > kNECStartSpaceMaxMicros)
			{
				if (pulseWidth < kNECRepeatSpaceMinMicros)
				{
					reset();
					return Decoder::kInvalidTimeout;
				}
				_repeat	= true;
			}
			return kNECTimeout;
		}

		if (mark)
		{
			if (!validMarkPulseWidth(pulseWidth))
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			_markLength = pulseWidth;
			if (_count >= 65)
			{
				if (_delegate)
					_delegate->RxNECDelegate_data(_data, _bus);
				reset();
				return Decoder::kInvalidTimeout;
			}
			return kNECTimeout;
		}

		if (_repeat && _count > 2)
		{
			reset();
			return Decoder::kInvalidTimeout;
		}

		uint8_t distance = validDistance(_markLength + pulseWidth);
		switch (distance)
		{
		case 2:
			// Long pulse => 1
			_data |= uint32_t(1) << ((_count - 3) >> 1);
			return kNECTimeout;
		default:
			reset();
			return Decoder::kInvalidTimeout;
		case 1:
			// Short pulse => 0
			return kNECTimeout;
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
