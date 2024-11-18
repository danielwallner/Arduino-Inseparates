// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_DATALINK_86_H_
#define _INS_PROTOCOL_DATALINK_86_H_

#include "ProtocolUtils.h"

// "New" datalink format.
// Mark is active low.
// Used for multi room connectivity.
// https://www.mikrocontroller.net/attachment/33137/datalink.pdf

// A variant of this protocol is used for IR remotes with a modulation frequency of 455 kHz.

//   You can build your own IR receiver as Bang & Olufsen did (check old schematics) or use a TSOP7000.
//   Vishay stopped producing TSOP7000 a long time ago so you will likely only find counterfeits:
//   https://www.vishay.com/files/whatsnew/doc/ff_FastFacts_CounterfeitTSOP7000_Dec72018.pdf
//   It is also likely that you will need an oscilloscope to debug a counterfeit TSOP7000.
//   The specimen used to test this code was very noisy and had a very low output current.
//   A somewhat working fix was to put a 4n7 capacitor across the output and ground followed by a pnp emitter follower.
//   Other samples may require a different treatment.
//   This particular receiver also did receive lower frequencies but rather poorly and with a lower delay than usual.
//   This makes it hard to create a functional universal receiver by paralleling a TSOP7000 with another receiver.
//
//   If the transmitter is close enough receivers will still pick up the signal even if the modulation frequency is 200 kHz or so.

//  IOREF -----------------*---
//                         |
//                      R pull-up
//                         |
//                         *---- OUT
//                   PNP |v E
//  TSOP out -> ----*----|  B
//                  |    |\ C
//                C 4n7   |
//                  |     |
//  GND ------------*-----*---

namespace inseparates
{

class TxDatalink86 : public SteppedTask
{
	friend class RxDatalink86;

	static const uint16_t kIRMarkMicros = 200;
	static const uint16_t kDatalinkMarkMicros = 1562;
	static const uint16_t kT1 = 3125 - kDatalinkMarkMicros; // 1 -> 0
	static const uint16_t kT2 = 6250 - kDatalinkMarkMicros; // Same
	static const uint16_t kT3 = 9375 - kDatalinkMarkMicros; // 0 -> 1
	static const uint16_t kT4 = 12500 - kDatalinkMarkMicros; // Stop
	static const uint16_t kT5 = 15625 - kDatalinkMarkMicros; // Start

	uint64_t _data;
	uint8_t _bits;
	PinWriter *_pin;
	uint8_t _mark;
	bool _ir;
	bool _repeat;
	uint8_t _count;
public:
	TxDatalink86(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _count(-1)
	{
	}

	void prepare(uint64_t data, uint8_t bits, bool ir, bool repeat)
	{
		_data = data;
		_bits = bits;
		_ir = ir;
		_repeat = repeat;
		_count = -1;
	}

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_repeat && _count == 0)
		{
			// Skip first mark
			++_count;
		}
		bool startOfMark = !(_count & 1);
		if (startOfMark)
		{
			// _count == even
			_pin->write(_mark);
			return _ir ? kIRMarkMicros : kDatalinkMarkMicros;
		}
		// _count == odd
		_pin->write(1 ^ _mark);
		if (_count <= 3)
		{
			return kT1;
		}
		if (_count == 5)
		{
			return kT5;
		}
		if (_count == (_bits << 1) + 9)
		{
			return kT4;
		}
		if (_count == (_bits << 1) + 11)
		{
			_count = -1;
			return SteppedTask::kInvalidDelta;
		}
		uint8_t bitnum = _bits - ((_count - 7) >> 1);
		bool thisBit = (_data >> bitnum) & 1;
		bool oldBit = (_count == 7) ? 1 : (_data >> (bitnum + 1)) & 1;
		if (oldBit && !thisBit)
			return kT1;
		if (!oldBit && thisBit)
			return kT3;
		return kT2;
	}
};

class RxDatalink86 : public Decoder
{
	static const uint16_t kT6 = 18750; // Timeout
public:
	class Delegate
	{
	public:
		virtual void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) = 0;
	};

private:
	bool _irMark;
	uint8_t _mark;
	Delegate *_delegate;
	uint8_t _lastBit;
	uint64_t _data;
	uint8_t _bus;
	uint8_t _count;

public:
	RxDatalink86(uint8_t mark, Delegate *delegate, uint8_t bus = 0) :
		_mark(mark), _delegate(delegate), _bus(bus)
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
		if (_count != uint8_t(-1))
		{
			INS_ASSERT(0);
			return;
		}
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
				return kInvalidTimeout;
			}
			// Do not check for enough idle time here because pulseWidth could have wrapped.
			++_count;
		}

		if (mark)
		{
			if (!validMarkPulseWidth(pulseWidth, !_count))
			{
				reset();
				return kInvalidTimeout;
			}
			return kT6;
		}

		_count += 1;

		uint8_t t = validDistance(pulseWidth);

		if (!t)
		{
			goto distance_error;
		}

		if (_count < 3)
		{
			if (t == 1)
			{
				return kT6;
			}
			if (t == 5)
			{
				_count = 3;
				return kT6;
			}
			goto distance_error;
		}
		if (_count == 3)
		{
			if (t != 5)
				goto distance_error;
			return kT6;
		}
		if (t == 4)
		{
			// If this is the start of a repeat message the first mark for the next message will be swallowed.
			// This will anyway be accepted as a new message.
			if (_delegate)
				_delegate->RxDatalink86Delegate_data(_data, _count - 5, _bus);
			goto distance_error;
		}

		if (_lastBit == 0 && t == 3)
		{
			_lastBit = 1;
		}
		else if (_lastBit == 1 && t == 1)
		{
			_lastBit = 0;
		}
		else if (t != 2)
		{
			goto distance_error;
		}

		_data <<= 1;
		_data |= _lastBit;
		return kT6;

	distance_error:
		reset();
		return kInvalidTimeout;
	}

private:
	bool validMarkPulseWidth(uint16_t pulseWidth, bool set = false)
	{
		if ((set || _irMark) && pulseWidth > 80 && pulseWidth < 550)
		{
			_irMark = true;
			return true;
		}
		if ((set || !_irMark) && pulseWidth > 1000 && pulseWidth < 2000)
		{
			_irMark = false;
			return true;
		}
		return false;
	}

	uint8_t validDistance(uint16_t distance)
	{
		for (uint8_t type = 1; type <= 5; ++type)
		{
			int16_t diff = distance - uint16_t(3125) * type;
			if (diff > 500)
				continue;
			if (diff < 500)
				return type;
		}
		return 0;
	}
};

}

#endif
