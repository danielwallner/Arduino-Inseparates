// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolUART.h"

#include <array>

using namespace inseparates;

TEST(TxTest, UART)
{
	uint8_t pin = 3;

	resetLogs();
	TxUART tx1(pin, HIGH);
	tx1.setBaudrate(10000);
	tx1.prepare(0x55);
	Scheduler::run(&tx1);
	std::array<uint8_t, 11> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 11> wt1 { 0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(1000, totalDelay());

	resetLogs();
	TxUART tx2(pin, HIGH);
	tx2.setBaudrate(10000);
	tx2.prepare(0x0);
	Scheduler::run(&tx2);
	std::array<uint8_t, 3> ws2 { 1, 0, 1 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 3> wt2 { 0, 0, 900 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(1000, totalDelay());

	resetLogs();
	TxUART tx3(pin, HIGH);
	tx3.setBaudrate(10000);
	tx3.prepare(0xFF);
	Scheduler::run(&tx3);
	std::array<uint8_t, 3> ws3 { 1, 0, 1 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 3> wt3 { 0, 0, 100 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(1000, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	std::array<uint8_t, 2> ws4 { 0, 1 };
	EXPECT_THAT(ws4, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 2> wt4 { 0, 100 };
	EXPECT_THAT(wt4, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(1000, totalDelay());
}

TEST(RxTest, UART)
{
	class Delegate : public RxUART::Delegate
	{
		uint8_t &receivedData;
	public:
		Delegate(uint8_t &receivedData_) : receivedData(receivedData_) {}

		void RxUARTDelegate_data(uint8_t data) override
		{
			receivedData = data;
		}

		void RxUARTDelegate_timingError() override
		{
			FAIL() << "RxUARTDelegate_timingError";
		}

		void RxUARTDelegate_parityError() override
		{
			FAIL() << "RxUARTDelegate_parityError";
		}
	};

	uint8_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

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
	EXPECT_EQ(1000 + startDelay, totalDelay());
	EXPECT_EQ(data, receivedData);

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
	EXPECT_EQ(100 * (1 + 5 + 1 + 6) + startDelay, totalDelay());
	EXPECT_EQ(data, receivedData);

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
	EXPECT_EQ(1200 + startDelay, totalDelay());
	EXPECT_EQ(data, receivedData);

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
	EXPECT_EQ(1200 + startDelay, totalDelay());
	EXPECT_EQ(data, receivedData);
}
