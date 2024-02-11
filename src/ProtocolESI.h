// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_ESI_H_
#define _INS_PROTOCOL_ESI_H_

// Philips ESI bus protocol
// Measured waveforms can be found in extra/pictures/ESI*.png
// Note that these waveforms are inverted compared to what's on the actual connector.
// The ESI bus is active high (5V) open collector!
// Set mark to HIGH to replicate this.

// The repeat rate is unknown but is set here to 50 ms which is about twice the message length.

#include "ProtocolUtils.h"
#include "DebugUtils.h"

namespace inseparates
{

class TxESI : public SteppedTask
{
	friend class RxESI;
	static const uint16_t kStepMicros = 444;
	static const uint16_t kRepeatInterval = 50000;

	uint32_t _data;
	PinWriter *_pin;
	uint8_t _mark;
	bool _state; // true -> mark
	bool _current;
	uint8_t _count;
	uint16_t _microsAccumulator;
	bool _sleepUntilRepeat;
public:
	TxESI(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _state(false), _count(-1)
	{
	}

	void prepare(uint32_t data, bool sleepUntilRepeat = true)
	{
		_data = data;
		_state = false;
		_current = true;
		_count = -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	// No safety belts here, can overflow!
	// The meaning of the upper byte is unknown!
	// RC-5 messages received by a 900-series amplifier was forwarded with upper byte set to 0x10 but that does not seem to be required as it itself reacts when this is any random number.
	// Note that bits 24 to 27 must be 0.
	static inline uint32_t encodeRC5(uint8_t upper, uint8_t toggle, uint8_t address, uint8_t command) { return (uint32_t(upper) << 16) | (address << 8) | (toggle << 7) | command; }

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count == 0)
		{
			_microsAccumulator = 0;
		}
		if (_count > 58)
		{
			prepare(_data);
			return SteppedTask::kInvalidDelta;
		}
		bool bitBoundry = !(_count & 1);
		if (bitBoundry)
		{
			_state = !_state;
			_pin->write(_state ? _mark : 1 ^ _mark);
			if (_count == 56)
			{
				if (_state)
				{
					_microsAccumulator += kStepMicros;
					return kStepMicros;
				}
				if (!_sleepUntilRepeat)
				{
					prepare(_data);
					return SteppedTask::kInvalidDelta;
				}
				_count = 58;
				uint16_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
				_microsAccumulator += microsUntilRepeat;
				return microsUntilRepeat;
			}
			uint8_t bitnum = (55 - _count) >> 1;
			bool bit = (_data >> bitnum) & 1;
			if (bit != _current)
			{
				_current = bit;
				++_count;
				_microsAccumulator += 2 * kStepMicros;
				return 2 * kStepMicros;
			}
			_microsAccumulator += kStepMicros;
			return kStepMicros;
		}
		if (_count >= 57)
		{
			_state = !_state;
			_pin->write(_state ? _mark : 1 ^_mark);
			if (!_sleepUntilRepeat)
			{
				prepare(_data);
				return SteppedTask::kInvalidDelta;
			}
			_count = 58;
			uint16_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
			_microsAccumulator += microsUntilRepeat;
			return microsUntilRepeat;
		}
		uint8_t bitnum = (55 - _count) >> 1;
		bool bit = (_data >> bitnum) & 1;
		if (bit == _current)
		{
			_state = !_state;
			_pin->write(_state ? _mark : 1 ^_mark);
		}
		else
		{
			_current = bit;
		}
		_microsAccumulator += kStepMicros;
		return kStepMicros;
	}
};


class RxESI : public Decoder
{
	static const uint16_t kTimeout = 3 * TxESI::kStepMicros;

public:
	class Delegate
	{
	public:
		virtual void RxESIDelegate_data(uint32_t data) = 0;
	};

private:
	uint8_t _mark;
	Delegate *_delegate;
	uint32_t _data;
	bool _toggled;
	bool _current;
	uint8_t _count;

public:
	RxESI(uint8_t mark, Delegate *delegate) :
		_mark(mark), _delegate(delegate)
	{
		reset();
	}

	void reset()
	{
		_data = 0;
		_toggled = false;
		_current = true;
		_count = -1;
	}

	void Decoder_timeout(uint8_t /*pinState*/) override
	{
		if (_count == uint8_t(-1))
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
			if (!mark)
			{
				return Decoder::kInvalidTimeout;
			}
			// First mark.
			++_count;
		}

		uint8_t steps = validatePulseWidth(pulseWidth) ? 1 : validatePulseWidth(pulseWidth >> 1) ? 2 : 0;

		if (steps == 0)
		{
			reset();
			return Decoder::kInvalidTimeout;
		}

		_count += steps;

		bool atBitBoundry = !(_count & 1);
		if (atBitBoundry)
		{
			_data <<= 1;
			if (!_toggled)
				_current = !_current;
			_data |= _current ? 1 : 0;
			_toggled = false;
		}
		else
		{
			if (steps == 2)
			{
				reset();
				return Decoder::kInvalidTimeout;
			}
			_toggled = true;
		}

		if (mark && _count >= 56)
		{
			if (_delegate)
				_delegate->RxESIDelegate_data(_data);
			reset();
			return Decoder::kInvalidTimeout;
		}
		return kTimeout;
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		static const uint16_t minLimit = 350;
		static const uint16_t maxLimit = 600;
		return (pulseWidth >= minLimit && pulseWidth <= maxLimit);
	}
};

}

#endif
