// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_UART_H_
#define _INS_PROTOCOL_UART_H_

// Software UART
// Timing accumulator will overflow for baud rates below 300

// As UART stands for "Universal asynchronous receiver-transmitter"
// TxUART/RxUART are kind of misnomers unless they would be combined into a single class :)

#include "ProtocolUtils.h"

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
public:
	class Delegate
	{
	public:
		virtual void TxUARTDelegate_timingError() = 0;
	};

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
	uint8_t _pin;
	uint8_t _markVal;
	uint8_t _count;

public:
	TxUART(uint8_t pin, uint8_t markVal) :
		_parity(Parity::kNone), _bits(8), _stopBits(1), _pin(pin), _markVal(markVal), _count(-1)
	{
		pinMode(pin, OUTPUT);
		digitalWrite(pin, markVal);
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

	uint16_t SteppedTask_step(uint32_t /*now*/) override
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
				return Scheduler::kInvalidDelta;
			}

			if (!sent)
			{
				digitalWrite(_pin, bitVal ? _markVal : 1 ^ _markVal);
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

		// Increment the number of fractional bits until the maximum accumulated value fits.
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

class RxUART : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxUARTDelegate_data(uint8_t data) = 0;
		virtual void RxUARTDelegate_timingError() = 0;
		virtual void RxUARTDelegate_parityError() = 0;
	};

private:
	InputFilter _inputHandler;
#if INS_UART_FRACTIONAL_TIME
	uint8_t _fractionalBits;
	uint16_t _bitWidthFixedPointMicros;
	uint16_t _accumulatedFixedPointTime;
#else
	uint16_t _bitWidthMicros;
	uint16_t _accumulatedTime;
#endif
	uint8_t _data;
	uint8_t _pin;
	uint8_t _markVal;
	Delegate *_delegate;
	Parity _parity;
	uint8_t _bits;
	uint8_t _parityValue;
	uint8_t _count;
public:
	// The main receive function is driven by input changes.
	// You need to run SteppedTask_step() to get the last byte from a stream of bytes!
	RxUART(uint8_t pin, uint8_t markVal, Delegate *delegate) :
		_pin(pin), _markVal(markVal), _delegate(delegate), _parity(Parity::kNone), _bits(8)
	{
		uint8_t pinValue = digitalRead(_pin);
		_inputHandler.setState(pinValue == _markVal);
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
#if INS_UART_FRACTIONAL_TIME
		_accumulatedFixedPointTime = 0;
#else
		_accumulatedTime = 0;
#endif
		_data = 0;
		_parityValue = 0;
		_count = -1;
	}

	uint16_t SteppedTask_step(uint32_t now) override
	{
		uint8_t pinValue = digitalRead(_pin);
#if INS_UART_FRACTIONAL_TIME
		uint16_t _bitWidthMicros = TxUART::fixedPointToUnsigned(_bitWidthFixedPointMicros, _fractionalBits);
#endif
		if (!_inputHandler.setState(pinValue == _markVal))
		{
			if (_count == uint8_t(-1))
			{
				return _bitWidthMicros >> 4;
			}
			uint32_t pulseLength = _inputHandler.getTimeSinceLastTransition(now);
			if (pulseLength > timeToComplete())
			{
				// Time-out to trigger the data callback.
				// Would otherwise not happen until the next start bit.
				inputChanged(true, timeToComplete(), true);
			}
			return _bitWidthMicros >> 4;
		}
		// Input has changed.
		bool state = _inputHandler.getPinState();
		uint32_t pulseLength = _inputHandler.getAndUpdateTimeSinceLastTransition(now);
		inputChanged(state, pulseLength);
		return _bitWidthMicros >> 4;
	}

	void inputChanged(bool pinState, uint16_t pulseTime, bool force = false)
	{
#if INS_UART_FRACTIONAL_TIME
		uint16_t _bitWidthMicros = TxUART::fixedPointToUnsigned(_bitWidthFixedPointMicros, _fractionalBits);
#endif

		// Limit pulse time so we don't overflow
		if (_count != uint8_t(-1) && pulseTime > (5 + _bits - _count) * _bitWidthMicros)
		{
			pulseTime = (5 + _bits - _count) * _bitWidthMicros;
		}

#if INS_UART_FRACTIONAL_TIME
		uint16_t pulseWidthFixedPoint = TxUART::unsignedToFixedPoint(pulseTime, _fractionalBits);
		// Allow a quarter bit timing error
		int16_t maxErrorFixedPoint = _bitWidthFixedPointMicros >> 2;
#else
		// Allow a quarter bit timing error
		int16_t maxError = _bitWidthMicros >> 2;
#endif

		bool atLeastOne = false;
		for (;;)
		{
			if (_count == uint8_t(-1))
			{
				// First mark check
				// pulseTime might have wrapped while idle and cannot be used here to check timing.
				if (pinState)
				{
					return;
				}
				_count = 0;
				return;
			}

#if INS_UART_FRACTIONAL_TIME
			uint16_t previousBitBoundry = _count * _bitWidthFixedPointMicros;
			int16_t distanceToNextBitBoundry = (previousBitBoundry + _bitWidthFixedPointMicros) - (_accumulatedFixedPointTime + pulseWidthFixedPoint);
			if (distanceToNextBitBoundry > maxErrorFixedPoint)
			{
				if (distanceToNextBitBoundry < _bitWidthFixedPointMicros >> 1)
#else
			 uint16_t previousBitBoundry = _count * _bitWidthMicros;
			 int16_t distanceToNextBitBoundry = (previousBitBoundry + _bitWidthMicros) - (_accumulatedTime + pulseTime);
			 if (distanceToNextBitBoundry > maxError)
			 {
				if (distanceToNextBitBoundry < _bitWidthMicros >> 1)
#endif
				{
					// Less than half a bit but more than a quarter bit left
					if (_delegate)
						_delegate->RxUARTDelegate_timingError();
					reset();
					return;
				}
				if (atLeastOne)
				{
					break;
				}
				if (_delegate)
					_delegate->RxUARTDelegate_timingError();
				reset();
				return;
			}

			atLeastOne = true;
			++_count;

			uint8_t bitValue = pinState ? 0 : 1;

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
				if (!bitValue)
				{
					_delegate->RxUARTDelegate_timingError();
				}
				else if (_delegate)
				{
					_delegate->RxUARTDelegate_data(_data);
				}
				reset();
				if (force)
					return;
				continue;
			}

			if (_count < _bits + 3)
			{
				// Parity
				_parityValue ^= bitValue;
			}

			if (_count == _bits + 2 && (_parity == kEven ? 0 : 1) != _parityValue)
			{
				if (_delegate)
					_delegate->RxUARTDelegate_parityError();
				reset();
			}
		}
#if INS_UART_FRACTIONAL_TIME
		_accumulatedFixedPointTime += pulseWidthFixedPoint;
#else
		_accumulatedTime += pulseTime;
#endif
	}

private:
	uint16_t timeToComplete()
	{
#if INS_UART_FRACTIONAL_TIME
		uint16_t _bitWidthMicros = TxUART::fixedPointToUnsigned(_bitWidthFixedPointMicros, _fractionalBits);
#endif
		return (3 + _bits - _count) * _bitWidthMicros;
	}
};

}

#endif
