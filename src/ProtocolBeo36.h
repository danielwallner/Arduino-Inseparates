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
	uint8_t _mark;
	uint8_t _count;
	bool _sleepUntilRepeat;
public:
	TxBeo36(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _count(-1)
	{
	}

	void prepare(uint8_t data, bool sleepUntilRepeat = true)
	{
		_data = data << 1;
		_count = -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count >= 16)
		{
			_count = -1;
			return SteppedTask::kInvalidDelta;
		}
		bool startOfMark = !(_count & 1);
		if (startOfMark)
		{
			_pin->write(_mark);
			return kMarkMicros;
		}
		_pin->write(1 ^ _mark);
		if (_count >= 15)
		{
			if (!_sleepUntilRepeat)
			{
				_count = -1;
				return SteppedTask::kInvalidDelta;
			}
			return kIdleMicros;
		}
		uint8_t bitnum = _count >> 1;
		bool thisBit = (_data >> bitnum) & 1;
		if (!thisBit)
			return kT1;
		return kT2;
	}
};

class RxBeo36 : public Decoder
{
	static const uint16_t kTimeoutMicros = 14100;

public:
	class Delegate
	{
	public:
		virtual void RxBeo36Delegate_data(uint8_t data) = 0;
	};

private:
	uint8_t _mark;
	Delegate *_delegate;
	uint8_t _lastBit;
	uint8_t _data;
	uint8_t _count;

public:
	RxBeo36(uint8_t mark, Delegate *delegate) :
		_mark(mark), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_lastBit = 1;
		_data = 0;
		_count = -1;
	}

	void Decoder_timeout(uint8_t /*pinState*/) override
	{
		reset();
	}

	uint16_t Decoder_pulse(uint8_t pulseState, uint16_t pulseWidth) override
	{
		bool mark = pulseState == _mark;
		if (_count == uint8_t(-1))
		{
			// Wait for mark
			if (!mark)
			{
				return Decoder::kInvalidTimeout;
			}
			// Do not check for enough idle time here because pulseWidth could have wrapped.
			++_count;
		}

		++_count;

		if (mark)
		{
			if (!validMarkPulseWidth(pulseWidth))
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			if (_count == 15)
			{
				if (_delegate)
					_delegate->RxBeo36Delegate_data(_data >> 1);
				reset();
				return Decoder::kInvalidTimeout;
			}
			return kTimeoutMicros;
		}

		uint8_t t = validDistance(pulseWidth);

		if (!t)
		{
			reset();
			return Decoder::kInvalidTimeout;
		}

		_data |= (t - 1) << (_count - 2) / 2;
		return kTimeoutMicros;
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
