// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_DATALINK_80_H_
#define _INS_PROTOCOL_DATALINK_80_H_

// Old 7-bit datalink format.
// The format superficially looks like 320 baud RS232 with no parity, 7 data bits and 8 stop bits
// but it is not as it is MSB first and the bits are inverted.
// Mark = low = one. Start bit is low and stop bit is high.
// Messages are always sent two times.
// Used between units in a Beosystem, even when the system uses the new format for multi room connectivity.

#include "ProtocolUtils.h"
#include "DebugUtils.h"

namespace inseparates
{

class TxDatalink80 : public SteppedTask
{
	friend class RxDatalink80;

	static const uint16_t kBitWidthMicros = 3125;

	uint8_t _data;
	PinWriter *_pin;
	uint8_t _mark;
	uint8_t _count;
	bool _sendRepeatSpace;
public:
	TxDatalink80(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _count(-1)
	{
	}

	void prepare(uint8_t data)
	{
		_data = data;
		_count = -1;
		_sendRepeatSpace = false;
	}

	uint16_t SteppedTask_step() override
	{
		uint8_t sent = 0;
		uint8_t sentValue;
		for (;;)
		{
			if (_sendRepeatSpace)
			{
				_sendRepeatSpace = false;
				return 8 * kBitWidthMicros;
			}

			++_count;
			uint8_t bitCount = _count > 8 ? _count - 9 : _count;
			uint8_t bitVal = 1;
			if (bitCount == 0)
			{
				bitVal = 0;
			}
			else if (bitCount < 8)
			{
				uint8_t bitnum = 7 - bitCount;
				bitVal = (_data >> bitnum) & 1;
				bitVal = bitVal ? 0 : 1;
			}

			if (bitCount == 8 && sent > 1 && sentValue == 1)
			{
				// Prevent overflow.
				_sendRepeatSpace = true;
				return sent * kBitWidthMicros;
			}

			if (_count >= 18)
			{
				if (_count == 18)
				{
					break;
				}
				_count = -1;
				return SteppedTask::kInvalidDelta;
			}

			if (!sent)
			{
				_pin->write(bitVal ? 1 ^ _mark : _mark);
				sentValue = bitVal;
			}
			else if (sentValue != bitVal)
			{
				--_count;
				break;
			}

			sent += bitCount == 8 ? 8 : 1;
		}
		return sent * kBitWidthMicros;
	}
};

class RxDatalink80 : public Decoder
{
public:
	class Delegate
	{
	public:
		virtual void RxDatalink80Delegate_data(uint8_t data) = 0;
		virtual void RxDatalink80Delegate_timingError() = 0;
	};

private:
	uint16_t _accumulatedTime;
	uint8_t _data;
	uint8_t _mark;
	Delegate *_delegate;
	uint8_t _count;
public:
	// The main receive function is driven by input changes.
	// You need to run SteppedTask_step() to get the last byte from a stream of bytes!
	RxDatalink80(uint8_t mark, Delegate *delegate) :
		_mark(mark), _delegate(delegate)
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
		if (pinState == _mark || _count < 1)
		{
			if (_delegate)
				_delegate->RxDatalink80Delegate_timingError();
		}

		for (;;)
		{
			++_count;

			if (_count < 9)
			{
				// Data
				uint8_t bitValue = pinState ? 0 : 1;
				_data |= bitValue << (8 - _count);
				continue;
			}
			if (_delegate)
			{
				_delegate->RxDatalink80Delegate_data(_data);
			}
			reset();
			return;
		}
	}

	uint16_t Decoder_pulse(uint8_t pulseState, uint16_t pulseWidth) override
	{
		bool mark = pulseState == _mark;
		// Allow a quarter bit timing error
		int16_t maxError = TxDatalink80::kBitWidthMicros >> 2;

		if (pulseWidth > 25000)
		{
			// Prevent overflow
			pulseWidth = 25000;
		}
		_accumulatedTime += pulseWidth;

		bool atLeastOne = false;
		for (;;)
		{
			if (_count == uint8_t(-1))
			{
				if (!mark)
				{
					// Not mark
					// Do not check for enough idle time here because pulseWidth can have wrapped.
					return Decoder::kInvalidTimeout;
				}
				_data = 0;
				_accumulatedTime = pulseWidth;
				++_count;
			}

			uint16_t previousBitBoundry = _count * TxDatalink80::kBitWidthMicros;
			int16_t distanceToNextBitBoundry = (previousBitBoundry + TxDatalink80::kBitWidthMicros) - _accumulatedTime;
			if (distanceToNextBitBoundry > maxError)
			{
				if (distanceToNextBitBoundry < maxError + (TxDatalink80::kBitWidthMicros >> 1))
				{
					// Less than three quarter bits but more than a quarter bit left
					if (_delegate)
						_delegate->RxDatalink80Delegate_timingError();
					_count = -1;
					return Decoder::kInvalidTimeout;
				}
				if (atLeastOne)
				{
					break;
				}
				INS_DEBUGF("%d %d %d %d\n", (int)pulseState, (int)pulseWidth, (int)_count, (int)distanceToNextBitBoundry);
				if (_delegate)
					_delegate->RxDatalink80Delegate_timingError();
				_count = -1;
				return Decoder::kInvalidTimeout;
			}

			atLeastOne = true;
			++_count;

			if (_count == 1)
			{
				// Start Bit
				continue;
			}
			if (_count < 9)
			{
				// Data
				uint8_t bitValue = mark ? 1 : 0;
				_data |= bitValue << (8 - _count);
				continue;
			}
			// Stop
			if (mark)
			{
				_delegate->RxDatalink80Delegate_timingError();
			}
			else if (_delegate)
			{
				_delegate->RxDatalink80Delegate_data(_data);
			}
			_count = -1;
			return Decoder::kInvalidTimeout;
		}
		return (9 - _count) * TxDatalink80::kBitWidthMicros;
	}
};

}

#endif
