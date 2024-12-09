// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_RC_5_H_
#define _INS_PROTOCOL_RC_5_H_

// Philips RC-5 protocol
// https://www.sbprojects.net/knowledge/ir/rc5.php
// The same protocol is used on both RC-5 connectors and remotes.
// IR modulation is 36 kHz.
// On RC-5 connectors a mark is 5V active high open collector!
// Most other IR inputs are active low!
// Connecting an RC-5 I/O to a normal IR I/O will likely destroy at least one of them!

#include "ProtocolUtils.h"
#include "DebugUtils.h"

namespace inseparates
{

class TxRC5 : public SteppedTask
{
	friend class RxRC5;
	static const uint16_t kStepMicros = 889;
	static const uint32_t kRepeatInterval = 114000;

	uint16_t _data;
	PinWriter *_pin;
	uint8_t _mark;
	uint8_t _count;
	uint32_t _microsAccumulator;
	bool _sleepUntilRepeat;
public:
	TxRC5(PinWriter *pin, uint8_t mark) :
		_pin(pin), _mark(mark), _count(-1)
	{
	}

	void prepare(uint32_t data, bool sleepUntilRepeat = true)
	{
		_data = data;
		_count = (_data >> 13) & 1 ? 0 : -1;
		_sleepUntilRepeat = sleepUntilRepeat;
	}

	// No safety belts here, can overflow!
	static inline uint16_t encodeRC5(uint8_t toggle, uint8_t address, uint8_t command) { return (uint16_t(0xC0 | (toggle << 5) | address) << 6) | command; }
	static inline uint16_t encodeRC5X(uint8_t toggle, uint8_t address, uint8_t command) { return (uint16_t(0x80 | (command & 0x40) | (toggle << 5) | address) << 6) | (command & 0x3F); }

	uint16_t SteppedTask_step() override
	{
		++_count;
		if (_count <= 1) // Will be wrong for start bit == 0 but simplifies the logic.
		{
			_microsAccumulator = 0;
		}
		if (_count > 28)
		{
			return idleTimeLeft();
		}
		uint8_t bitnum = 13 - (_count >> 1);
		bool bitVal = _count < 28 ? (_data >> bitnum) & 1 : true;
		bool bitBoundry = !(_count & 1);
		bool value = bitVal ^ bitBoundry;
		_pin->write(value ? _mark : 1 ^ _mark);
		if ((!value && _count == 27) || _count == 28)
		{
			_count = 28;
			return idleTimeLeft();
		}
		if (!bitBoundry && _count < 27)
		{
			bool nextBitVal = (_data >> (bitnum - 1)) & 1;
			if (bitVal != nextBitVal)
			{
				++_count;
				_microsAccumulator += kStepMicros << 1;
				return kStepMicros << 1;
			}
		}
		_microsAccumulator += kStepMicros;
		return kStepMicros;
	}

private:
	uint16_t idleTimeLeft()
	{
		if (!_sleepUntilRepeat)
		{
			prepare(_data);
			return SteppedTask::kInvalidDelta;
		}
		int32_t microsUntilRepeat = kRepeatInterval - _microsAccumulator;
		if (microsUntilRepeat <= 0)
		{
			prepare(_data);
			return SteppedTask::kInvalidDelta;
		}
		if (microsUntilRepeat > SteppedTask::kMaxSleepMicros)
			microsUntilRepeat = SteppedTask::kMaxSleepMicros;
		_microsAccumulator += microsUntilRepeat;
		return microsUntilRepeat;
	}
};

// Does not handle a zero start bit (which is not valid RC-5)
class RxRC5 : public Decoder
{
public:
	class Delegate
	{
	public:
		virtual void RxRC5Delegate_data(uint16_t data, uint8_t bus) = 0;
	};

private:
	static const uint16_t kTimeout = 3 * TxRC5::kStepMicros;

	uint8_t _mark;
	Delegate *_delegate;
	uint16_t _data;
	uint8_t _bus;
	uint8_t _count;

public:
	RxRC5(uint8_t mark, Delegate *delegate, uint8_t bus = 0) :
		_mark(mark), _delegate(delegate), _bus(bus)
	{
		reset();
	}

	void reset()
	{
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
			// Wait for mark.
			if (!mark)
			{
				return Decoder::kInvalidTimeout;
			}
			_data = 0x1;
			++_count;
		}

		uint8_t steps = validatePulseWidth(pulseWidth) ? 1 : 0;

		if (!steps && !(steps = (validatePulseWidth(pulseWidth >> 1) ? 2 : 0)))
		{
			_count = -1;
			return Decoder::kInvalidTimeout;
		}

		_count += steps;

		bool atBitCenter = !(_count & 1);

		if (atBitCenter)
		{
			_data <<= 1;
			_data |= mark ? 0 : 1;
		}
		else if (steps != 1)
		{
			_count = -1;
			return Decoder::kInvalidTimeout;
		}

		if (mark && _count >= 26)
		{
			if (_delegate)
				_delegate->RxRC5Delegate_data(_data, _bus);
			_count = -1;
			return Decoder::kInvalidTimeout;
		}

		return kTimeout;
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		static const uint16_t minLimit = 750;
		static const uint16_t maxLimit = 1100;
		return (pulseWidth >= minLimit && pulseWidth <= maxLimit);
	}
};

}

#endif
