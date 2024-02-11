// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolBeo36.h"

#include <array>

using namespace inseparates;

TEST(TxTest, Beo36)
{
	uint8_t pin = 6;

	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxBeo36 tx1(&pinWriter, HIGH);
	tx1.prepare(0x20);
	Scheduler::run(&tx1);
	std::array<uint8_t, 16> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 16> wt1 { 0, 154, 4946, 154, 4946, 154, 4946, 154, 4946, 154, 4946, 154, 4946, 154, 6946, 154 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(51954, totalDelay());
	resetLogs();
	TxBeo36 tx2(&pinWriter, HIGH);
	tx2.prepare(0x01);
	Scheduler::run(&tx2);
	std::array<uint8_t, 16> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 16> wt2 { 0, 154, 4946, 154, 6946, 154, 4946, 154, 4946, 154, 4946, 154, 4946, 154, 4946, 154 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(51954, totalDelay());

	resetLogs();
	TxBeo36 tx3(&pinWriter, HIGH);
	tx3.prepare(0x15);
	Scheduler::run(&tx3);
	std::array<uint8_t, 16> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 16> wt3 { 0, 154, 4946, 154, 6946, 154, 4946, 154, 6946, 154, 4946, 154, 6946, 154, 4946, 154 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(55954, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(55954, totalDelay());
}

TEST(RxTest, Beo36)
{
	class Delegate : public RxBeo36::Delegate
	{
		uint8_t &receivedData;
	public:
		Delegate(uint8_t &receivedData_) : receivedData(receivedData_) {}

		void RxBeo36Delegate_data(uint8_t data) override
		{
			receivedData = data;
		}
	};

	uint8_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

	RxBeo36 beo36Decoder(LOW, &delegate);

	uint16_t startDelay = 4321;
	uint32_t data = 0x01;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWriter(pin);
	TxBeo36 tx1(&pinWriter, LOW);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		beo36Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x3F;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		beo36Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x2A;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		beo36Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x15;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		beo36Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
}
