// Copyright (c) 2024 Daniel Wallner

// Command line host application for easier debugging in source-level debuggers.

#define INS_DEBUGGING 1

#include "../src/ProtocolBeo36.h"
#include "../src/ProtocolDatalink80.h"
#include "../src/ProtocolDatalink86.h"
#include "../src/ProtocolESI.h"
#include "../src/ProtocolNEC.h"
#include "../src/ProtocolRC5.h"
#include "../src/ProtocolSIRC.h"
#include "../src/ProtocolTechnicsSC.h"

#include <assert.h>

using namespace inseparates;

int main()
{
	// B&O Old
	{
		class Delegate : public RxBeo36::Delegate
		{
		public:
			uint8_t receivedData = 0;
			uint8_t receivedBus = 0;
			uint32_t dataDelay = 0;

			void RxBeo36Delegate_data(uint8_t data, uint8_t bus) override
			{
				receivedData = data;
				receivedBus = bus;
				dataDelay = totalDelay();
			}
		};

		uint16_t datalinkStartDelay = 4321;

		Delegate delegate;

		RxBeo36 beoDecoder(LOW, &delegate);

		uint8_t pin = 7;

		uint32_t data = 0x01;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxBeo36 tx1(&pinWriter, LOW);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			beoDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x31;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			beoDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x15;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			beoDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x3F;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			beoDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
	}

	// B&O Datalink 80
	{
		uint8_t pin = 1;
		// All zeros
		resetLogs();
		PushPullPinWriter pinWriter(pin);
		TxDatalink80 tx1(&pinWriter, LOW);
		tx1.prepare(0x0);
		Scheduler::run(&tx1);
		assert(50000 == totalDelay());
	}
	{
		class Delegate : public RxDatalink80::Delegate
		{
		public:
			std::vector<uint8_t> receivedData;
			uint32_t dataDelay = 0;

			void RxDatalink80Delegate_data(uint8_t data, uint8_t bus) override
			{
				assert(bus == 0);
				receivedData.push_back(data);
				dataDelay = totalDelay();
			}

			void RxDatalink80Delegate_timingError() override
			{
				assert(0);
			}
		};

		uint16_t datalinkStartDelay = 4321;

		Delegate delegate;

		uint8_t pin = 7;
		RxDatalink80 datalinkDecoder(LOW, &delegate);

		uint32_t data = 0x41;
		resetLogs();
		delayMicroseconds(datalinkStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxDatalink80 tx1(&pinWriter, LOW);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(totalDelay() - datalinkStartDelay == 50000);
		assert(delegate.receivedData.size() == 1 &&
			   delegate.receivedData[0] == data);

		data = 0x0;
		resetLogs();
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		datalinkDecoder.reset();
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(totalDelay() - datalinkStartDelay == 50000);
		assert(delegate.receivedData.size() == 2 &&
			   delegate.receivedData[1] == data);

		data = 0x7F;
		resetLogs();
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		datalinkDecoder.reset();
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(totalDelay() - datalinkStartDelay == 50000);
		assert(delegate.receivedData.size() == 3 &&
			   delegate.receivedData[2] == data);

		data = 0x2A;
		resetLogs();
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		datalinkDecoder.reset();
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(totalDelay() - datalinkStartDelay == 50000);
		assert(delegate.receivedData.size() == 4 &&
			   delegate.receivedData[3] == data);
	}

	// B&O Datalink 86
	{
		class Delegate : public RxDatalink86::Delegate
		{
		public:
			uint64_t receivedData = 0;
			uint64_t receivedBits = 0;
			uint32_t dataDelay = 0;

			void RxDatalink86Delegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
			{
				assert(bus == 0);
				receivedData = data;
				receivedBits = bits;
				dataDelay = totalDelay();
			}
		};

		uint16_t datalinkStartDelay = 4321;

		Delegate delegate;

		RxDatalink86 datalinkDecoder(LOW, &delegate);

		uint8_t bits = 16;
		uint8_t pin = 7;

		uint32_t data = 0x8001;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxDatalink86 tx1(&pinWriter, LOW);
		tx1.prepare(data, bits, false, false);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == bits);

		data = 0x3011;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data, bits, false, false);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == bits);

		data = 0x3A5F;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data, bits, false, false);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == bits);

		bits = 63;
		data = 0xF355A28F;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(datalinkStartDelay);
		tx1.prepare(data, bits, false, false);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == bits);
	}

	// ESI
	{
		class Delegate : public RxESI::Delegate
		{
		public:
			uint64_t receivedData = 0;
			uint8_t receivedBits = 0;
			uint8_t receivedBus = 0;
			uint32_t dataDelay = 0;

			void RxESIDelegate_data(uint64_t data, uint8_t bits, uint8_t bus) override
			{
				receivedData = data;
				receivedBits = bits;
				receivedBus = bus;
				dataDelay = totalDelay();
			}
		};

		uint16_t esiStartDelay = 4321;

		Delegate delegate;

		RxESI esiDecoder(HIGH, &delegate);

		uint8_t pin = 7;

		uint32_t data = 0x8000001;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(esiStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxESI tx1(&pinWriter, HIGH);
		tx1.prepare(data, 28);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		esiDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 28);
		assert(delegate.dataDelay == 50000 + esiStartDelay);

		data = 0x30011;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(esiStartDelay);
		tx1.prepare(data, 28);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		esiDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 28);
		assert(delegate.dataDelay == 50000 + esiStartDelay);

		data = 0x30FFF;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(esiStartDelay);
		tx1.prepare(data, 28);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		esiDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 28);
		assert(delegate.dataDelay == 50000 + esiStartDelay);

		data = 0x355A2;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(esiStartDelay);
		tx1.prepare(data, 28);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		esiDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 28);
		assert(delegate.dataDelay == 50000 + esiStartDelay);
	}

	// NEC
	{
		class Delegate : public RxNEC::Delegate
		{
		public:
			uint32_t receivedData = 0;
			uint8_t receivedBus = 0;
			uint32_t dataDelay = 0;

			void RxNECDelegate_data(uint32_t data, uint8_t bus) override
			{
				receivedData = data;
				receivedBus = bus;
				dataDelay = totalDelay();
			}
		};

		Delegate delegate;

		RxNEC necDecoder(LOW, &delegate);

		uint16_t necStartDelay = 12345;
		uint8_t pin = 7;

		uint32_t data = TxNEC::encodeNEC(0, 0);
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(necStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxNEC tx1(&pinWriter, LOW);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 1; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			if (i + 1 == g_digitalWriteStateLog[pin].size())
			{
				// Dummy line for adding breakpoint
				necStartDelay = 12345;
			}
			necDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.dataDelay == 110000 + necStartDelay);

		data = TxNEC::encodeNEC(0xFF, 0);
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(necStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			necDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.dataDelay == 110000 + necStartDelay);

		data = TxNEC::encodeNEC(0, 0xFF);
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(necStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			necDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.dataDelay == 110000 + necStartDelay);

		data = TxNEC::encodeNEC(0xAA, 0x55);
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(necStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			necDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);
		assert(delegate.dataDelay == 110000 + necStartDelay);
	}

	// RC-5
	{
		class Delegate : public RxRC5::Delegate
		{
		public:
			uint16_t receivedData = 0;
			uint8_t receivedBus = 0;
			uint32_t dataDelay = 0;

			void RxRC5Delegate_data(uint16_t data, uint8_t bus) override
			{
				receivedData = data;
				receivedBus = bus;
				dataDelay = totalDelay();
			}
		};

		Delegate delegate;

		RxRC5 rc5Decoder(HIGH, &delegate);

		uint16_t rc5StartDelay = 4321;
		uint8_t pin = 6;

		// Example RC5 data (14 bits)
		bool rc5Data[20] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0  };
		uint16_t pulseWidths[20] = { 5678, 888, 888, 1776, 888, 888, 888, 888, 1776, 1776, 1776, 888, 888, 888, 888, 1776, 1776, 1776, 1776, 888 };

		for (unsigned i = 0; i < 20; i++)
		{
			rc5Decoder.Decoder_pulse(1 ^ rc5Data[i], pulseWidths[i]);
		}

		assert(delegate.receivedData == 0x3175);

		uint16_t data = 0x2000;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(rc5StartDelay);
		PushPullPinWriter pinWriter(pin);
		TxRC5 tx1(&pinWriter, HIGH);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x3011;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(rc5StartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x3FFF;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(rc5StartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		data = 0x355A;
		resetLogs();
		digitalWrite(pin, LOW);
		delayMicroseconds(rc5StartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		assert(delegate.receivedData == data);

		resetLogs();
		TxRC5 tx2(&pinWriter, HIGH);
		tx2.prepare(0x1);
		Scheduler::run(&tx2);
		auto pinLog = g_digitalWriteStateLog[pin];
		auto timeLog = g_digitalWriteTimeLog[pin];
		assert(114000 + 889 == totalDelay());
	}

	// SIRC
	{
		class Delegate : public RxSIRC::Delegate
		{
		public:
			uint32_t receivedData = 0;
			uint8_t receivedBits = 0;
			uint8_t receivedBus = 0;
			uint32_t dataDelay = 0;

			void RxSIRCDelegate_data(uint32_t data, uint8_t bits, uint8_t bus) override
			{
				receivedData = data;
				receivedBits = bits;
				receivedBus = bus;
				dataDelay = totalDelay();
			}
		};

		uint16_t sircStartDelay = 4321;
		Delegate delegate;

		RxSIRC sircDecoder(LOW, &delegate);

		uint8_t pin = 7;

		uint32_t data = 0x80001;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(sircStartDelay);
		PushPullPinWriter pinWriter(pin);
		TxSIRC tx1(&pinWriter, LOW);
		tx1.prepare(data, 20);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			if (i + 1 == g_digitalWriteStateLog[pin].size())
			{
				// Dummy line for adding breakpoint
				sircStartDelay = 4321;
			}
			sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 20);

		data = 0x8A1;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(sircStartDelay);
		tx1.prepare(data, 12);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		delayMicroseconds(sircStartDelay);
		sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 12);

		data = 0x30FFF;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(sircStartDelay);
		tx1.prepare(data, 20);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 20);

		data = 0x355A2;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(sircStartDelay);
		tx1.prepare(data, 20);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData == data);
		assert(delegate.receivedBits == 20);
	}

	// Technics System Control
	{
		class Delegate : public RxTechnicsSC::Delegate
		{
			uint32_t &receivedData;
		public:
			Delegate(uint32_t &receivedData_) : receivedData(receivedData_) {}

			void RxTechnicsSCDelegate_data(uint32_t data) override
			{
				receivedData = data;
			}
		};

		uint32_t receivedData;
		uint8_t dataPin = 3;
		uint8_t clockPin = 4;

		Delegate delegate(receivedData);

		RxTechnicsSC technicsSCDecoder(dataPin, clockPin, HIGH, &delegate);

		uint16_t startDelay = 14321;
		uint32_t data = 0x80AA5501;
		resetLogs();
		PushPullPinWriter dataPinWriter(dataPin);
		PushPullPinWriter clockPinWriter(clockPin);
		TxTechnicsSC tx1(&dataPinWriter, &clockPinWriter, dataPin, clockPin, HIGH);
		delayMicroseconds(startDelay);
		tx1.prepare(data);
		Scheduler::runFor(&tx1, 200);
		uint32_t clockTime = 0;
		for (unsigned i = 0; i < g_digitalWriteStateLog[clockPin].size(); i++)
		{
			clockTime += g_digitalWriteTimeLog[clockPin][i];
			bool dataState;
			uint32_t dataTime = 0;
			for (unsigned j = 0; j < g_digitalWriteTimeLog[dataPin].size(); ++j)
			{
				dataTime += g_digitalWriteTimeLog[dataPin][j];
				if (dataTime > clockTime)
					break;
				dataState = g_digitalWriteStateLog[dataPin][j];
			}
			technicsSCDecoder.inputChanged(dataState, g_digitalWriteStateLog[clockPin][i], g_digitalWriteTimeLog[clockPin][i]);
		}
		assert(data == receivedData);
	}

	return 0;
}
