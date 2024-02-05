// Copyright (c) 2024 Daniel Wallner

// Command line host application for easier debugging in source-level debuggers.

#define INS_DEBUGGING 1
#define INS_ENABLE_INPUT_FILTER 1
#define INS_UART_FRACTIONAL_TIME 1

#include "../src/ProtocolUART.h"
#include "../src/DebugUtils.h"

#include <stdio.h>
#include <assert.h>

using namespace inseparates;

class Delegate : public RxUART::Delegate
{
public:
	std::vector<uint8_t> receivedData;
	uint16_t dataDelay = 0;

	void RxUARTDelegate_data(uint8_t data) override
	{
		receivedData.push_back(data);
		dataDelay = totalDelay();
	}

	void RxUARTDelegate_timingError() override
	{
		assert(0);
	}

	void RxUARTDelegate_parityError() override
	{
		assert(0);
	}
};

void runUART(RxUART &uart, uint8_t pin)
{
	if (g_digitalWriteStateLog[pin].size())
	{
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			uint8_t pinValue = g_digitalWriteStateLog[pin][i];
			uint16_t timeValue = g_digitalWriteTimeLog[pin][i];
			printf("%d %d\n", int(pinValue), int(timeValue));
			if (timeValue)
				uart.Decoder_pulse(1 ^ pinValue, timeValue);
		}
	}
}

int main()
{
	{
		DebugPrinter printer;

		printer.print("hej");
		printer.SteppedTask_step();
		printer.SteppedTask_step();
		printer.println("hopp");
		printer.flush();

		printer.SteppedTask_step();
		printer.SteppedTask_step();
		printer.print("hej");
		printer.println("hopp");
		printer.SteppedTask_step();
		printer.SteppedTask_step();

		printer.SteppedTask_step();
		printer.SteppedTask_step();

		printer.print("hej");
		printer.SteppedTask_step();
		printer.SteppedTask_step();
		printer.println("hopp");
		printer.flush();
	}
	{
		uint8_t pin = 3;

		resetLogs();
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(10000);
		tx1.setFormat(Parity::kEven);
		tx1.prepare(0x55);
		Scheduler::run(&tx1);
		assert(1100 == totalDelay());

		resetLogs();
		TxUART tx2(pin, HIGH);
		tx2.setBaudrate(10000);
		tx2.setFormat(Parity::kNone, 7, 3);
		tx2.prepare(0x0);
		Scheduler::run(&tx2);
		assert(1100 == totalDelay());

		resetLogs();
		TxUART tx3(pin, HIGH);
		tx3.setBaudrate(10000);
		tx3.prepare(0xFF);
		Scheduler::run(&tx3);
		assert(1000 == totalDelay());

		resetLogs();
		Scheduler::run(&tx3);
		assert(1000 == totalDelay());
	}

	{
		uint8_t pin = 6;

		Delegate delegate;

		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(10000);

		uint16_t startDelay = 4321;
		uint8_t data = 0x81;
		resetLogs();
		TxUART tx1(pin, HIGH);
		delayMicroseconds(startDelay);
		tx1.setBaudrate(10000);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 1; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			 rxUART.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(1000 + startDelay ==  totalDelay());
		assert(data = delegate.receivedData.back());

		rxUART.setFormat(Parity::kOdd, 5);
		tx1.setFormat(Parity::kOdd, 5, 6);

		data = 0x00;
		rxUART.reset();
		resetLogs();
		delayMicroseconds(startDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rxUART.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(100 * (1 + 5 + 1 + 6) + startDelay == totalDelay());
		assert(data == delegate.receivedData.back());

		rxUART.setFormat(Parity::kEven, 8);
		tx1.setFormat(Parity::kEven, 8, 2);

		data = 0x3C;
		rxUART.reset();
		resetLogs();
		delayMicroseconds(startDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rxUART.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(1200 + startDelay == totalDelay());
		assert(data == delegate.receivedData.back());

		data = 0xFF;
		rxUART.reset();
		resetLogs();
		delayMicroseconds(startDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rxUART.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(1200 + startDelay == totalDelay());
		assert(data == delegate.receivedData.back());

		rxUART.setFormat(Parity::kOdd, 5);
		tx1.setFormat(Parity::kOdd, 5, 6);

		data = 0xFF;
		rxUART.reset();
		resetLogs();
		delayMicroseconds(startDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
		{
			rxUART.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
		}
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(100 * (1 + 5 + 1 + 6) + startDelay == totalDelay());
		assert(0x1F == delegate.receivedData.back());
	}

	{
		Delegate delegate;

		uint16_t uartStartDelay = 1234;
		uint8_t pin = 7;
		uint32_t baudRate = 230400;
		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);

		uint8_t data = 0x81;
		resetLogs();
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.prepare(data);
		Scheduler::run(&tx1);

		data = 0x0;
		//delayMicroseconds(uartStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		assert(delegate.receivedData.size() && delegate.receivedData[0] == 0x81);
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() && delegate.receivedData.back() == data);
		rxUART.reset();

		data = 0xFF;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(uartStartDelay);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() && delegate.receivedData.back() == data);

		RxUART rxUART2(HIGH, &delegate);
		rxUART2.setBaudrate(baudRate);
		data = 0x5A;
		resetLogs();
		digitalWrite(pin, HIGH);
		delayMicroseconds(uartStartDelay);
		tx1.setFormat(Parity::kEven, 8);
		rxUART2.setFormat(Parity::kEven, 8);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART2, pin);
		rxUART2.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() && delegate.receivedData.back() == data);
	}

	{
		class ParityDelegate : public TxUART::Delegate, public RxUART::Delegate
		{
		public:
			std::vector<uint8_t> receivedData;
			bool parityError = false;

			void TxUARTDelegate_timingError() override
			{
				assert(0);
			}

			void RxUARTDelegate_data(uint8_t data) override
			{
				receivedData.push_back(data);
			}

			void RxUARTDelegate_timingError() override
			{
				assert(0);
			}

			void RxUARTDelegate_parityError() override
			{
				parityError = true;
			}
		};

		ParityDelegate delegate;

		uint16_t uartStartDelay = 4321;
		uint8_t pin = 1;
		uint32_t baudRate = 230400;
		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);

		uint8_t data = 0x81;
		resetLogs();
		delayMicroseconds(uartStartDelay);
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.setFormat(Parity::kOdd, 8);
		rxUART.setFormat(Parity::kEven, 8);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		rxUART.Decoder_timeout(HIGH);
		assert(delegate.parityError);
		assert(!delegate.receivedData.size());
	}

	{
		Delegate delegate;

		uint16_t uartStartDelay = 5678;
		uint8_t pin = 4;
		uint32_t baudRate = 230400;
		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);

		uint8_t data = 0x81;
		resetLogs();
		delayMicroseconds(uartStartDelay);
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.setFormat(Parity::kOdd, 8);
		rxUART.setFormat(Parity::kOdd, 8);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() && delegate.receivedData.back() == data);
	}

	{
		Delegate delegate;

		uint16_t uartStartDelay = 5678;
		uint8_t pin = 4;
		uint8_t bits = 3;
		uint32_t baudRate = 230400;
		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);

		uint8_t data = 0x2;
		resetLogs();
		delayMicroseconds(uartStartDelay);
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.setFormat(Parity::kOdd, bits);
		rxUART.setFormat(Parity::kOdd, bits);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() == 2 &&
			   delegate.receivedData[delegate.receivedData.size() - 2] == data &&
			   delegate.receivedData.back() == data);
	}

	{
		Delegate delegate;

		uint16_t uartStartDelay = 5678;
		uint8_t pin = 4;
		uint8_t bits = 8;
		uint32_t baudRate = 300;
		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);

		uint8_t data = 0x2;
		resetLogs();
		delayMicroseconds(uartStartDelay);
		TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.setFormat(Parity::kOdd, bits);
		rxUART.setFormat(Parity::kOdd, bits);
		tx1.prepare(data);
		Scheduler::run(&tx1);
		runUART(rxUART, pin);
		rxUART.Decoder_timeout(g_digitalWriteStateLog[pin].back());
		assert(delegate.receivedData.size() && delegate.receivedData.back() == data);
	}

	{
		resetLogs();

		static Scheduler scheduler;

		static uint8_t pin = 4;
		uint8_t bits = 8;
		uint32_t baudRate = 1200;
		static uint8_t data = 0x2;
		static TxUART tx1(pin, HIGH);
		tx1.setBaudrate(baudRate);
		tx1.setFormat(Parity::kOdd, bits);

		class Delegate2 : public RxUART::Delegate, public Scheduler::Delegate
		{
		public:
			std::vector<uint8_t> receivedData;
			uint16_t dataDelay = 0;

			void RxUARTDelegate_data(uint8_t data) override
			{
				receivedData.push_back(data);
				dataDelay = totalDelay();
			}

			void RxUARTDelegate_timingError() override
			{
				assert(0);
			}

			void RxUARTDelegate_parityError() override
			{
				assert(0);
			}

			void SchedulerDelegate_done(SteppedTask */*task*/) override
			{
				tx1.prepare(~data);
				scheduler.add(&tx1, this);
			}
		};

		Delegate2 delegate;

		RxUART rxUART(HIGH, &delegate);
		rxUART.setBaudrate(baudRate);
		rxUART.setFormat(Parity::kOdd, bits);

		scheduler.add(&rxUART, pin);

		for (int i = 0; delegate.receivedData.size() < 2; ++i)
		{
			scheduler.poll();
			safeDelayMicros(10);
			if (i == 10)
			{
				tx1.prepare(data);
				scheduler.add(&tx1, &delegate);
			}
		}

		assert(delegate.receivedData.size() && delegate.receivedData.back() == uint8_t(~data));
	}

	return 0;
}
