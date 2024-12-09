// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_ESI_H_
#define _INS_PROTOCOL_ESI_H_

// Philips ESI bus protocol
// Measured waveforms can be found in extras/pictures/ESI*.png
// The ESI bus is active high (5V) open collector!
// Set mark to HIGH to replicate this.

// No public specification exists for the ESI protocol and the information below was found by measuring the communication between FA931 and FT930

// The repeat rate is unknown but is set here to 50 ms which is about twice the message length.

// Messages of length 4,20,28 and 36 bits have been observed.

// RC5 repeat messages are sent as a 0x0F 4 bit message.

// 28 bit messages:

//	0x2110081	Wake tuner
//	0x010118c	Tuner standby

//	RC5 translated messages as encoded by encodeRC5()
//	  RC-5 messages received by FA931 was forwarded with upper byte set to 0x10 which seems to imply that this is a source id.
//	  The amplifier itself did react to any value in the upper byte.
//	  Note that bits 24 to 27 must be 0.

// 36 bit messages:

//	0x210401111ULL	Select Tuner (As sent by FT930)
//	0x210401200ULL	Select Tape (The lower byte appears to be a source id and FA931 seems to accept any value here.)
//	0x210401400ULL	Select CD
//	0x210401500ULL	Select Phono
//	0x210401700ULL	Select DCC
//	0x210401900ULL	Select TV/AUX


#include "ProtocolUtils.h"
#include "DebugUtils.h"

namespace inseparates
{

class TxESI : public SteppedTask
{
	friend class RxESI;
	static const uint16_t kStepMicros = 444;
	static const uint16_t kRepeatInterval = 50000;

	uint64_t _data;
	PinWriter *_pin;
	uint8_t _mark;
	bool _state; // true -> mark
	bool _current;
	uint8_t _bits;
	uint8_t _count;
	uint16_t _microsAccumulator;
	bool _sleepUntilRepeat;
public:
	TxESI(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _state(false), _count(-1)
	{
	}

	void prepare(uint64_t data, uint8_t bits, bool sleepUntilRepeat = true)
	{
		_data = data;
		_state = false;
		_current = true;
		_bits = bits;
		_count = -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	// No safety belts here, can overflow!
	static const uint8_t kRC5MessageBits = 28;
	static inline uint32_t encodeRC5(uint8_t upper, uint8_t toggle, uint8_t address, uint8_t command) { return (uint32_t(upper) << 16) | (address << 8) | (toggle << 7) | command; }

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count == 0)
		{
			_microsAccumulator = 0;
		}
		if (_count > _bits * 2 + 2)
		{
			prepare(_data, _bits);
			return SteppedTask::kInvalidDelta;
		}
		bool bitBoundry = !(_count & 1);
		if (bitBoundry)
		{
			_state = !_state;
			_pin->write(_state ? _mark : 1 ^ _mark);
			if (_count == _bits * 2)
			{
				if (_state)
				{
					_microsAccumulator += kStepMicros;
					return kStepMicros;
				}
				if (!_sleepUntilRepeat)
				{
					prepare(_data, _bits);
					return SteppedTask::kInvalidDelta;
				}
				_count = _bits * 2 + 2;
				uint16_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
				_microsAccumulator += microsUntilRepeat;
				return microsUntilRepeat;
			}
			uint8_t bitnum = (_bits * 2 - 1 - _count) >> 1;
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
		if (_count >= _bits * 2 + 1)
		{
			_state = !_state;
			_pin->write(_state ? _mark : 1 ^_mark);
			if (!_sleepUntilRepeat)
			{
				prepare(_data, _bits);
				return SteppedTask::kInvalidDelta;
			}
			_count = _bits * 2 + 2;
			uint16_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
			_microsAccumulator += microsUntilRepeat;
			return microsUntilRepeat;
		}
		uint8_t bitnum = (_bits * 2 - 1 - _count) >> 1;
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
		virtual void RxESIDelegate_data(uint64_t data, uint8_t bits, uint8_t bus) = 0;
	};

private:
	uint8_t _mark;
	Delegate *_delegate;
	uint64_t _data;
	uint8_t _bus;
	bool _toggled;
	bool _current;
	uint8_t _count;

public:
	RxESI(uint8_t mark, Delegate *delegate, uint8_t bus = 0) :
		_mark(mark), _delegate(delegate), _bus(bus)
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
				_delegate->RxESIDelegate_data(_data, _count >> 1, _bus);
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
