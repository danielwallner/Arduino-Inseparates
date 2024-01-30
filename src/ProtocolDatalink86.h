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
	uint8_t _markVal;
	bool _ir;
	bool _repeat;
	uint8_t _count;
public:
	TxDatalink86(PinWriter *pin, uint8_t markVal) :
		_pin(pin), _markVal(markVal), _count(-1)
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

	uint16_t SteppedTask_step(uint32_t /*now*/) override
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
			_pin->write(_markVal);
			return _ir ? kIRMarkMicros : kDatalinkMarkMicros;
		}
		// _count == odd
		_pin->write(1 ^ _markVal);
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
			return Scheduler::kInvalidDelta;
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

class RxDatalink86 : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxDatalink86Delegate_data(uint64_t data, uint8_t bits) = 0;
	};

private:
	bool _irMark;
	InputFilter _inputHandler;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	uint8_t _lastBit;
	uint64_t _data;
	uint8_t _count;

public:
	RxDatalink86(uint8_t pin, uint8_t markVal, Delegate *delegate) :
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
			if (_count != uint8_t(-1) && _inputHandler.getTimeSinceLastTransition(now) > TxDatalink86::kT5 * 2)
			{
				reset();
			}
			return 10;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint16_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return 10;
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

		if (!pinState)
		{
			if (!validMarkPulseWidth(pulseTime, !_count))
			{
				reset();
				return;
			}
			return;
		}

		_count += 1;

		uint8_t t = validDistance(pulseTime);

		if (!t)
		{
			goto distance_error;
		}

		if (_count < 3)
		{
			if (t == 1)
			{
				return;
			}
			if (t == 5)
			{
				_count = 3;
				return;
			}
			goto distance_error;
		}
		if (_count == 3)
		{
			if (t != 5)
				goto distance_error;
			return;
		}
		if (t == 4)
		{
			// If this is the start of a repeat message the first mark for the next message will be swallowed.
			// This will anyway be accepted as a new message.
			if (_delegate)
				_delegate->RxDatalink86Delegate_data(_data, _count - 5);
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
		return;

	distance_error:
		reset();
		return;
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
