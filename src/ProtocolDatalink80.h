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

namespace inseparates
{

class TxDatalink80 : public SteppedTask
{
	friend class RxDatalink80;

	static const uint16_t kBitWidthMicros = 3125;

	uint8_t _data;
	PinWriter *_pin;
	uint8_t _markVal;
	uint8_t _count;
public:
	TxDatalink80(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _count(-1)
	{
	}

	void prepare(uint8_t data)
	{
		_data = data;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t /*now*/) override
	{
		uint8_t sent = 0;
		uint8_t sentValue;
		for (;;)
		{
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

			if (_count >= 18)
			{
				if (_count == 18)
				{
					break;
				}
				prepare(_data);
				return Scheduler::kInvalidDelta;
			}

			if (!sent)
			{
				_pin->write(bitVal ? 1 ^ _markVal : _markVal);
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

class RxDatalink80 : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxDatalink80Delegate_data(uint8_t data) = 0;
		virtual void RxDatalink80Delegate_timingError() = 0;
	};

private:
	InputFilter _inputHandler;
	uint16_t _startTime;
	uint16_t _accumulatedTime;
	uint8_t _data;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint8_t _parityValue;
	uint8_t _count;
public:
	// The main receive function is driven by input changes.
	// You need to run SteppedTask_step() to get the last byte from a stream of bytes!
	RxDatalink80(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_accumulatedTime = 0;
		_data = 0;
		_parityValue = 0;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count == uint8_t(-1))
			{
				return 15;
			}
			uint16_t passed = uint16_t(now) - _startTime;
			if (passed > 9 * TxDatalink80::kBitWidthMicros)
			{
				// Finish or reset previous byte if not done.
				inputChanged(true, passed - _accumulatedTime, true);
			}
			return 15;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return 15;
	}

	void inputChanged(bool pinState, uint16_t pulseTime, bool force = false)
	{
		// Allow a quarter bit timing error
		int16_t maxError = TxDatalink80::kBitWidthMicros >> 2;

		if (pulseTime > 25000)
		{
			// Prevent overflow
			pulseTime = 25000;
		}

		bool atLeastOne = false;
		for (;;)
		{
			if (_count == uint8_t(-1))
			{
				if (!pinState)
				{
					// Not mark
					// Do not check for enough idle time here because pulseTime can have wrapped.
					return;
				}
				_count = 0;
				_startTime = fastMicros();
				return;
			}

			uint16_t previousBitBoundry = _count * TxDatalink80::kBitWidthMicros;
			int16_t distanceToNextBitBoundry = (previousBitBoundry + TxDatalink80::kBitWidthMicros) - (_accumulatedTime + pulseTime);
			if (distanceToNextBitBoundry > maxError)
			{
				if (distanceToNextBitBoundry < TxDatalink80::kBitWidthMicros >> 1)
				{
					// Less than half a bit but more than a quarter bit left
					if (_delegate)
						_delegate->RxDatalink80Delegate_timingError();
					reset();
					return;
				}
				if (atLeastOne)
				{
					break;
				}
				if (_delegate)
					_delegate->RxDatalink80Delegate_timingError();
				reset();
				return;
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
				uint8_t bitValue = pinState ? 0 : 1;
				_data |= bitValue << (8 - _count);
				continue;
			}
			// Stop
			if (!pinState)
			{
				_delegate->RxDatalink80Delegate_timingError();
			}
			else if (_delegate)
			{
				_delegate->RxDatalink80Delegate_data(_data);
			}
			reset();
			if (force)
				return;
			continue;
		}
		_accumulatedTime += pulseTime;
	}
};

}

#endif
