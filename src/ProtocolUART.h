// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_UART_H_
#define _INS_PROTOCOL_UART_H_

// Software UART
// Timing accumulator will overflow for baud rates below 300

// As UART stands for "Universal asynchronous receiver-transmitter"
// TxUART/RxUART are kind of misnomers unless they would be combined into a single class :)

#include "ProtocolUtils.h"
#include "DebugUtils.h"

// Mark / Logic 1 is < -3V
// Space / Logic 0 is > 3V

namespace inseparates
{

enum Parity {
	kNone,
	kEven,
	kOdd
};

#if !defined(INS_UART_FRACTIONAL_TIME) && !AVR
//#define INS_UART_FRACTIONAL_TIME 1
#endif

class TxUART : public SteppedTask
{
	friend class RxUART;

private:
#if INS_UART_FRACTIONAL_TIME
	uint8_t _fractionalBits;
	uint16_t _bitWidthFixedPointMicros;
	uint16_t _accumulatedFixedPointTime;
#else
	uint16_t _bitWidthMicros;
#endif
	uint8_t _data;
	Parity _parity;
	uint8_t _bits;
	uint8_t _stopBits;
	uint8_t _parityValue;
	PinWriter *_pin;
	uint8_t _mark;
	uint8_t _count;

public:
	TxUART(PinWriter *pin, uint8_t mark) :
		_parity(Parity::kNone), _bits(8), _stopBits(1), _pin(pin), _mark(mark), _count(-1)
	{
		pin->write(_mark);
	}

	void setBaudrate(uint32_t baudRate)
	{
#if INS_UART_FRACTIONAL_TIME
		_bitWidthFixedPointMicros = TxUART::baudToFixedPointTime(baudRate, _fractionalBits);
#else
		_bitWidthMicros = (uint32_t(1000000) + (baudRate >> 1)) / baudRate;
#endif
	}

	void setFormat(Parity parity, uint8_t bits = 8, uint8_t stopBits = 1)
	{
		_parity = parity;
		_bits = bits;
		_stopBits = stopBits;
	}

	void prepare(uint8_t data)
	{
#if INS_UART_FRACTIONAL_TIME
		_accumulatedFixedPointTime = 0;
#endif
		_data = data;
		_parityValue = 0;
		_count = -1;
	}

	uint16_t SteppedTask_step() override
	{
		uint8_t sent = 0;
		uint8_t sentValue;
		for (;;)
		{
			++_count;
			uint8_t bitVal = 1;
			if (_count == 0)
			{
				bitVal = 0;
			}
			else if (_count < 1 + _bits)
			{
				uint8_t bitnum = _count - 1;
				bitVal = (_data >> bitnum) & 1;
			}
			else if (_count == 1 + _bits && _parity != Parity::kNone)
			{
				if (_parity == Parity::kEven)
				{
					bitVal = _parityValue;
				}
				else
				{
					bitVal = 1 ^ _parityValue;
				}
			}

			uint8_t stopBitStart = _bits + (_parity == Parity::kNone ? 1 : 2);

			if (_count > stopBitStart)
			{
				if (_count < stopBitStart + _stopBits)
				{
					uint8_t toEnd = stopBitStart + _stopBits - _count;
					sent += toEnd;
					_count += toEnd;
				}
				if (sent)
				{
					break;
				}
				prepare(_data);
				return SteppedTask::kInvalidDelta;
			}

			if (!sent)
			{
				_pin->write(bitVal ? _mark : 1 ^ _mark);
				sentValue = bitVal;
			}
			else if (sentValue != bitVal)
			{
				--_count;
				break;
			}
			++sent;

			// This will run in the stop bit too and be ignored.
			if (_parity != Parity::kNone && _count && _count < 1 + _bits)
				_parityValue ^= bitVal;
		}
#if INS_UART_FRACTIONAL_TIME
		_accumulatedFixedPointTime += sent * _bitWidthFixedPointMicros;
		uint16_t accumulatedMicros = fixedPointToUnsigned(_accumulatedFixedPointTime, _fractionalBits);
		_accumulatedFixedPointTime -= unsignedToFixedPoint(accumulatedMicros, _fractionalBits);
		return accumulatedMicros;
#else
		return sent * _bitWidthMicros;
#endif
	}

private:
#if INS_UART_FRACTIONAL_TIME
	static inline uint16_t baudToFixedPointTime(uint32_t baudRate, uint8_t &fractionalBits)
	{
		// Limit fractional bits to half
		fractionalBits = 8;

		// Calculate the maximum accumulated value for 16 bits for some margin.
		uint32_t bitTimeTimes16Micros = int32_t(16000000) / baudRate;

		uint32_t maxAccumulatedValue = 255;

		// Decrement the number of fractional bits until the maximum accumulated value fits.
		while (maxAccumulatedValue < bitTimeTimes16Micros)
		{
			--fractionalBits;
			maxAccumulatedValue = (maxAccumulatedValue << 1) | 1;
		}

		return (uint32_t(1000000) << fractionalBits) / baudRate;
	}

	static inline uint32_t fixedPointToUnsigned(uint32_t fixedPointNumber, uint8_t fractionalBits)
	{
		return fixedPointNumber >> fractionalBits;
	}

	static inline uint32_t unsignedToFixedPoint(uint32_t fixedPointNumber, uint8_t fractionalBits)
	{
		return fixedPointNumber << fractionalBits;
	}
#endif
};

class RxUART : public Decoder
{
public:
	class Delegate
	{
	public:
		virtual void RxUARTDelegate_data(uint8_t data, uint8_t bus) = 0;
		virtual void RxUARTDelegate_timingError(uint8_t bus) = 0;
		virtual void RxUARTDelegate_parityError(uint8_t bus) = 0;
	};

private:
#if INS_UART_FRACTIONAL_TIME
	uint8_t _fractionalBits;
	uint16_t _bitWidthFixedPointMicros;
	uint16_t _accumulatedFixedPointTime;
#else
	uint16_t _bitWidthMicros;
	uint16_t _accumulatedTime;
#endif
	uint8_t _data;
	uint8_t _mark;
	Delegate *_delegate;
	Parity _parity;
	uint8_t _bits;
	uint8_t _bus;
	uint8_t _parityValue;
	uint8_t _count;
public:
	// The main receive function is driven by input changes.
	// You need to run SteppedTask_step() to get the last byte from a stream of bytes!
	RxUART(uint8_t mark, Delegate *delegate, uint8_t bus = 0) :
		_mark(mark), _delegate(delegate), _parity(Parity::kNone), _bits(8), _bus(bus)
	{
		reset();
	}

	void setBaudrate(uint32_t baudRate)
	{
#if INS_UART_FRACTIONAL_TIME
		_bitWidthFixedPointMicros = TxUART::baudToFixedPointTime(baudRate, _fractionalBits);
#else
		_bitWidthMicros = (uint32_t(1000000) + (baudRate >> 1)) / baudRate;
#endif
	}

	void setFormat(Parity parity, uint8_t bits = 8)
	{
		_parity = parity;
		_bits = bits;
	}

	void reset()
	{
		_data = 0;
		_parityValue = 0;
		_count = -1;
	}

	void Decoder_timeout(uint8_t pinState) override
	{
		if (_count == uint8_t(-1))
		{
			INS_DEBUGF("timeout %hhd %hhX\n", pinState, _data);
			INS_ASSERT(0);
			return;
		}

		if (pinState != _mark || _count < 1)
		{
			INS_DEBUGF("timeout %hhd %hhX\n", pinState, _data);
			if (_delegate)
				_delegate->RxUARTDelegate_timingError(_bus);
		}

		for (;;)
		{
			++_count;
			if (_count < _bits + 2)
			{
				// Data
				_data |= 1 << (_count - 2);
				if (_parity == Parity::kNone)
					continue;
			}
			if (_count >= _bits + 2 + (_parity == Parity::kNone ? 0 : 1))
			{
				// Stop
				if (_delegate)
				{
					_delegate->RxUARTDelegate_data(_data, _bus);
				}
				reset();
				return;
			}
			if (_count < _bits + 3)
			{
				// Parity
				_parityValue ^= 1;
			}
			if (_count == _bits + 2 && (_parity == kEven ? 0 : 1) != _parityValue)
			{
				if (_delegate)
					_delegate->RxUARTDelegate_parityError(_bus);
				reset();
				return;
			}
		}
	}

	uint16_t Decoder_pulse(uint8_t pulseState, uint16_t pulseWidth) override
	{
		bool mark = pulseState == _mark;

#if INS_UART_FRACTIONAL_TIME
		uint16_t pulseWidthFixedPoint = TxUART::unsignedToFixedPoint(pulseWidth, _fractionalBits);
		// Allow a quarter bit timing error
		int16_t maxErrorFixedPoint = _bitWidthFixedPointMicros >> 2;
		_accumulatedFixedPointTime += pulseWidthFixedPoint;
#else
		// Allow a quarter bit timing error
		int16_t maxError = _bitWidthMicros >> 2;
		_accumulatedTime += pulseWidth;
#endif

		bool atLeastOne = false;
		for (;;)
		{
			if (_count == uint8_t(-1))
			{
				// First mark check
				// pulseWidth might have wrapped while idle and cannot be used here to check timing.
				if (mark)
				{
					return kInvalidTimeout;
				}
#if INS_UART_FRACTIONAL_TIME
				_accumulatedFixedPointTime = pulseWidthFixedPoint;
#else
				_accumulatedTime = pulseWidth;
#endif
				_data = 0;
				_parityValue = 0;
				_count = 0;
			}

#if INS_UART_FRACTIONAL_TIME
			uint32_t previousBitBoundry = _count * uint32_t(_bitWidthFixedPointMicros);
			int32_t distanceToNextBitBoundry = (previousBitBoundry + _bitWidthFixedPointMicros) - _accumulatedFixedPointTime;
			if (distanceToNextBitBoundry > maxErrorFixedPoint)
			{
				if (distanceToNextBitBoundry < maxErrorFixedPoint + (_bitWidthFixedPointMicros >> 1))
#else
			 uint16_t previousBitBoundry = _count * _bitWidthMicros;
			 int16_t distanceToNextBitBoundry = (previousBitBoundry + _bitWidthMicros) - _accumulatedTime;
			 if (distanceToNextBitBoundry > maxError)
			 {
				if (distanceToNextBitBoundry < maxError + (_bitWidthMicros >> 1))
#endif
				{
					// Less than three quarter bits but more than a quarter bit left
					INS_DEBUGF("%hhd %d %hhd %d\n", pulseState, (int)pulseWidth, _count, (int)distanceToNextBitBoundry);
					if (_delegate)
						_delegate->RxUARTDelegate_timingError(_bus);
					reset();
					return kInvalidTimeout;
				}
				if (atLeastOne)
				{
					break;
				}
				INS_DEBUGF("%hhd %d %hhd %d\n", pulseState, (int)pulseWidth, _count, (int)distanceToNextBitBoundry);
				if (_delegate)
					_delegate->RxUARTDelegate_timingError(_bus);
				reset();
				return kInvalidTimeout;
			}

			atLeastOne = true;
			++_count;

			uint8_t bitValue = mark ? 1 : 0;

			if (_count == 1)
			{
				// Start Bit
				continue;
			}
			if (_count < _bits + 2)
			{
				// Data
				_data |= bitValue << (_count - 2);
				if (_parity == Parity::kNone)
					continue;
			}
			if (_count >= _bits + 2 + (_parity == Parity::kNone ? 0 : 1))
			{
				// Stop
				if (!mark)
				{
					INS_DEBUGF("%hhd %d %hhd\n", pulseState, (int)pulseWidth, _count);
					_delegate->RxUARTDelegate_timingError(_bus);
				}
				else if (_delegate)
				{
					_delegate->RxUARTDelegate_data(_data, _bus);
				}
				_count = -1;
				return kInvalidTimeout;
			}

			if (_count < _bits + 3)
			{
				// Parity
				_parityValue ^= bitValue;
			}

			if (_count == _bits + 2 && (_parity == kEven ? 0 : 1) != _parityValue)
			{
				INS_DEBUGF("%hhX %hhd\n", _data, _parityValue);
				if (_delegate)
					_delegate->RxUARTDelegate_parityError(_bus);
				_count = -1;
				return kInvalidTimeout;
			}
		}
		return timeToComplete();
	}

private:
	uint16_t timeToComplete()
	{
#if INS_UART_FRACTIONAL_TIME
		uint16_t _bitWidthMicros = TxUART::fixedPointToUnsigned(_bitWidthFixedPointMicros, _fractionalBits);
#endif
		return (4 + _bits - _count) * _bitWidthMicros;
	}
};

}

#endif
