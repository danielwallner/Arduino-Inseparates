// Copyright (c) 2024 Daniel Wallner

#ifndef _INS_PROTOCOL_TECHNICS_SC_H_
#define _INS_PROTOCOL_TECHNICS_SC_H_

// Technics System Control bus protocol
// Measured waveforms can be found in extra/pictures/TechnicsSC*.png
// There may be several variants of this protocol.
// This protocol was reverse engineered on ST-X902L / ST-X302L
// In systems with these tuners, the tuner has the IR eye and forwards commands on the system control bus to the other components.
// This is a synchronous protocol with clock and data.
// A 3.5 mm TRS connector is used where tip = data and ring = clock.
// Both pins are bidirectional and open collector.
// Here mark is considered to be LOW, which means that if you use inverting drivers, mark must be set to HIGH.

// In idle clock is +5V and data is 0V.
// As part of the handshake in the beginning of the transmission all masters must stop driving data to 0V.

// For reference, these are the raw Kaseikyo IR codes and the translated System Control output from a RAK-SC304W remote controlling a ST-X902L.
// IR repeat rate is about 130 ms. System control repeat rate is abount 50 ms.
// The remote repeats all commands indefinitely but some of the forwarded commands are only repeated four times.
// OFF/ON                 0x993D04A0  Not forwarded.
// SLEEP                  0x329604A0  Not forwarded.
// MUTE                   0x923200A0  0x00320001
// VOLUME DOWN            0x812100A0  0x00210001
// VOLUME UP              0x802000A0  0x00200001
// CD                     0x64C004A0  0x00940001
// TAPE                   0x65C104A0  0x00960001
// TUNER                  0x66C204A0  0x00920001
// TU 0                   0xBD1904A0  0x00920001
//    1                   0xB41004A0  0x00920001
//    2                   0xB51104A0  0x00920001
//    3                   0xB61204A0  0x00920001
//    4                   0xB71304A0  0x00920001
//    5                   0xB01404A0  0x00920001
//    6                   0xB11504A0  0x00920001
//    7                   0xB21604A0  0x00920001
//    8                   0xB31704A0  0x00920001
//    9                   0xBC1804A0  0x00920001
// EQ ON/FLAT             0x3F8F10A0  0x108F0001
//    PRESET              0x328210A0  0x10820001
// CD PLAY                0xA00A0AA0  0x0A0A0001
//    PAUSE               0xAC060AA0  0x0A060001
//    STOP                0xAA000AA0  0x0A000001
//    PREVIOUS            0xE3490AA0  0x0A490001
//    NEXT                0xE04A0AA0  0x0A4A0001
//    PROGRAM             0x208A0AA0  0x0A8A0001
//    TIME MODE           0xFF550AA0  0x0A550001
// TT PLAY                0xA40A0EA0  0x0E0A0001
//    STOP                0xAE000EA0  0x0E000001
// TA DECK1/2             0x3D9508A0  0x08950001
//    PAUSE               0xAE0608A0  0x08060001
//    REVERSE PLAY        0xA30B08A0  0x080B0001
//    STOP                0xA80008A0  0x08000001
//    PLAY                0xA20A08A0  0x080A0001
//    FREW                0xAA0208A0  0x08020001
//    FFWD                0xAB0308A0  0x08030001
//    D2 AUTO REC MUTE    0x2A8208A0  0x08820001
//       REC              0xA00808A0  0x08080001

#include "ProtocolUtils.h"

namespace inseparates
{

// This transmitter is unusual because it needs to read the output pins and must run continuously to sync with other masters.
// The simple handshake implemented here may not work under severe load.
class TxTechnicsSC : public SteppedTask
{
	friend class RxTechnicsSC;
	static const uint8_t kIdleState = -2;
	static const uint8_t kPreparedState = -1;
	static const uint16_t kQuarterStepMicros = 213;

	uint32_t _data;
	PinWriter *_dataPin;
	PinWriter *_clockPin;
	uint8_t _dataInputPin;
	uint8_t _clockInputPin;
	uint8_t _mark;
	bool _current;
	uint8_t _count;
	uint16_t _lastClock;
public:
	TxTechnicsSC(PinWriter *dataPin, PinWriter *clockPin, uint8_t dataInputPin, uint8_t clockInputPin, uint8_t mark) :
		_dataPin(dataPin), _clockPin(clockPin), _dataInputPin(dataInputPin), _clockInputPin(clockInputPin), _mark(mark), _count(kIdleState)
	{
		_clockPin->write(1 ^ _mark);
		_dataPin->write(_mark);
	}

	void prepare(uint32_t data)
	{
		_data = data;
		_count = kPreparedState;
	}

	// This is required here since we need to be able to abort without inactivating this instance.
	void abort(uint32_t /*data*/)
	{
		_clockPin->write(_mark);
		_dataPin->write(1 ^ _mark);
		_count = kIdleState;
	}

	bool done() { return _count == kIdleState; }

	// No safety belts here, can overflow!
	static inline uint32_t encodeIR(uint8_t address, uint8_t command) { return (uint32_t(address) << 24) | (uint32_t(command) << 16) | 1; }

	uint16_t SteppedTask_step() override
	{
		uint8_t clockPinState = digitalRead(_clockInputPin);
		if (clockPinState == _mark)
			_lastClock = fastMicros();
		if (_count == kIdleState || _count == kPreparedState)
		{
			if (_count == kIdleState)
			{
				if (clockPinState == _mark)
					_dataPin->write(1 ^ _mark);
				return kQuarterStepMicros;
			}
			if (_count == kPreparedState)
			{
				if (fastMicros() - _lastClock < 8 * kQuarterStepMicros)
					return kQuarterStepMicros;
			}
		}
		++_count;
		if (_count == 0)
		{
			_clockPin->write(_mark);
			_dataPin->write(1 ^ _mark);
			_current = true;
			return 2 * kQuarterStepMicros;
		}
		else if (_count == 1)
		{
			if (digitalRead(_dataInputPin) == _mark)
			{
				--_count;
				// Timeout?
				return kQuarterStepMicros;
			}
		}
		if (_count == 130)
		{
			_clockPin->write(1 ^ _mark);
			return kQuarterStepMicros;
		}
		if (_count >= 131)
		{
			if (_current)
			_dataPin->write(_mark);
			_count = kIdleState;
			// Normally SteppedTask::kInvalidDelta should be returned here but this instance must be kept active for handshake with other masters.
			return kQuarterStepMicros;
		}
		uint8_t clockState = _count & 3;
		bool bit = 0;
		if (clockState != 2)
		{
			if (_count < 128)
			{
				uint8_t bitnum = 31 - _count / 4;
				bit = _data & (1UL << bitnum);
			}
			else
			{
				bit = 1;
			}
		}
		switch (clockState)
		{
		case 0:
			{
				if (clockPinState == _mark)
				{
					// Some other master must be yanking the clock pin. Abort.
					_clockPin->write(1 ^ _mark);
					_dataPin->write(_mark);
					_count = kPreparedState;
					return kQuarterStepMicros;
				}
				_clockPin->write(_mark);
				if (bit == _current)
				{
					++_count;
					return 2 * kQuarterStepMicros;
				}
			}
			break;
		case 1:
			{
				uint8_t dataPinState = digitalRead(_dataInputPin);
				if ((dataPinState != _mark) != _current)
				{
					// Some other master must be yanking the data pin. Abort.
					_clockPin->write(1 ^ _mark);
					_dataPin->write(_mark);
					_count = kPreparedState;
					return kQuarterStepMicros;
				}
				_dataPin->write(bit ? 1 ^ _mark : _mark);
				_current = bit;
			}
			break;
		case 2:
			_clockPin->write(1 ^ _mark);
			++_count;
			return 2 * kQuarterStepMicros;
		}
		return kQuarterStepMicros;
	}
};

// Unlike other receivers, due to the two input pins, this is a regular SteppedTask instead of a Decoder.
class RxTechnicsSC : public SteppedTask
{
public:
	class Delegate
	{
	public:
		virtual void RxTechnicsSCDelegate_data(uint32_t data) = 0;
	};

private:
	InputFilter _dataInputHandler;
	InputFilter _clockInputHandler;
	uint8_t _dataPin;
	uint8_t _clockPin;
	uint8_t _mark;
	Delegate *_delegate;
	uint32_t _data;
	bool _toggled;
	bool _current;
	uint8_t _count;

public:
	RxTechnicsSC(uint8_t dataPin, uint8_t clockPin, uint8_t mark, Delegate *delegate) :
		_dataPin(dataPin), _clockPin(clockPin), _mark(mark), _delegate(delegate)
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

	uint16_t SteppedTask_step() override
	{
		uint8_t dataValue = digitalRead(_dataPin);
		uint8_t clockValue = digitalRead(_clockPin);
		_dataInputHandler.setState(dataValue == _mark);
		if (!_clockInputHandler.setState(clockValue == _mark))
		{
			if (_count != uint8_t(-1) && _clockInputHandler.getTimeSinceLastTransition(fastMicros()) > TxTechnicsSC::kQuarterStepMicros * 20)
			{
				reset();
			}
			return 15;
		}
		// Clock input has changed.
		bool dataState = _dataInputHandler.getPinState();
		bool clockState = _clockInputHandler.getPinState();
		uint16_t pulseLength = _clockInputHandler.getAndUpdateTimeSinceLastTransition(fastMicros());
		inputChanged(dataState, clockState, pulseLength);
		return 15;
	}

	// These are current values unlike for Decoder_pulse().
	void inputChanged(bool dataState, bool clockState, uint16_t pulseWidth)
	{
		if (_count == uint8_t(-1))
		{
			if (!clockState)
			{
				return;
			}
			_count = 0;
			return;
		}

		_count += 1;

		if (_count > 1 && !validatePulseWidth(pulseWidth))
		{
			reset();
			return;
		}

		if (!clockState && _count < 64)
		{
			_data <<= 1;
			_data |= dataState ? 0 : 1;
		}

		if (_count >= 65)
		{
			if (_delegate)
				_delegate->RxTechnicsSCDelegate_data(_data);
			reset();
		}
	}

private:
	bool validatePulseWidth(uint16_t pulseWidth)
	{
		static const uint16_t minLimit = 300;
		static const uint16_t maxLimit = 600;
		return (pulseWidth >= minLimit && pulseWidth <= maxLimit);
	}
};

}

#endif
