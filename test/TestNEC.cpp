// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolNEC.h"

#include <array>

using namespace inseparates;

TEST(TxTest, NEC)
{
	uint8_t pin = 4;

	EXPECT_EQ(0xFF00FF00, TxNEC::encodeNEC(0, 0));
	EXPECT_EQ(0xFF0000FF, TxNEC::encodeNEC(0xFF, 0));
	EXPECT_EQ(0x00FFFF00, TxNEC::encodeNEC(0, 0xFF));
	EXPECT_EQ(0xFD02FE01, TxNEC::encodeNEC(1, 2));
	EXPECT_EQ(0x02FD01FE, TxNEC::encodeNEC(0xFE, 0XFD));

	EXPECT_EQ(0xFF000000, TxNEC::encodeExtendedNEC(0, 0));
	EXPECT_EQ(0x00FF0000, TxNEC::encodeExtendedNEC(0, 0xFF));
	EXPECT_EQ(0x00FF1234, TxNEC::encodeExtendedNEC(0x1234, 0xFF));
	EXPECT_EQ(0xFE015678, TxNEC::encodeExtendedNEC(0x5678, 0x01));
	EXPECT_EQ(0x01FE9ABC, TxNEC::encodeExtendedNEC(0x9ABC, 0xFE));

	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxNEC tx1(&pinWriter, HIGH);
	tx1.prepare(0x0);
	Scheduler::run(&tx1);
	std::array<uint8_t, 4> ws1 { 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 4> wt1 { 0, 9000, 2250, 562 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(110000, totalDelay());

	resetLogs();
	TxNEC tx2(&pinWriter, HIGH);
	tx2.prepare(0x80000001);
	Scheduler::run(&tx2);
	std::array<uint8_t, 68> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 68> wt2 { 0, 9000, 4500, 562, 1688, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 563, 562, 1688, 562 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(110000, totalDelay());

	resetLogs();
	TxNEC tx3(&pinWriter, HIGH);
	tx3.prepare(TxNEC::encodeNEC(0x59, 0x16));
	Scheduler::run(&tx3);
	std::array<uint8_t, 68> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 68> wt3 { 0, 9000, 4500, 562, 1688, 562, 563, 562, 563, 562, 1688, 562, 1688, 562, 563, 562, 1688, 562, 563, 562, 563, 562, 1688, 562, 1688, 562, 563, 562, 563, 562, 1688, 562, 563, 562, 1688, 562, 563, 562, 1688, 562, 1688, 562, 563, 562, 1688, 562, 563, 562, 563, 562, 563, 562, 1688, 562, 563, 562, 563, 562, 1688, 562, 563, 562, 1688, 562, 1688, 562, 1688, 562 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(110000, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(110000, totalDelay());
}

TEST(RxTest, NEC)
{
	class Delegate : public RxNEC::Delegate
	{
		uint32_t &receivedData;
	public:
		Delegate(uint32_t &receivedData_) : receivedData(receivedData_) {}

		void RxNECDelegate_data(uint32_t data) override
		{
			receivedData = data;
		}
	};

	uint32_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

	RxNEC necDecoder(pin, HIGH, &delegate);

	uint16_t startDelay = 14321;
	uint32_t data = 0x80000001;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWriter(pin);
	TxNEC tx1(&pinWriter, HIGH);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		necDecoder.inputChanged(g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x7000000F;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		necDecoder.inputChanged(g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x3FFF;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		necDecoder.inputChanged(g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x355A;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		necDecoder.inputChanged(g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
}
