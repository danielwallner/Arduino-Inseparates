// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolESI.h"

#include <array>

using namespace inseparates;

TEST(TxTest, ESI)
{
	uint8_t pin = 3;

	resetLogs();
	PushPullPinWriter pinWiter(pin);
	TxESI tx1(&pinWiter, HIGH);
	tx1.prepare(0x5555555);
	Scheduler::run(&tx1);
	std::array<uint8_t, 30> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 30> wt1 { 0, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 444 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(50000, totalDelay());

	resetLogs();
	TxESI tx2(&pinWiter, HIGH);
	tx2.prepare(0x5555554);
	Scheduler::run(&tx2);
	std::array<uint8_t, 30> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 30> wt2 { 0, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 888, 444, 444 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(50000, totalDelay());

	resetLogs();
	TxESI tx3(&pinWiter, HIGH);
	tx3.prepare(0x1 << 27);
	Scheduler::run(&tx3);
	std::array<uint8_t, 56> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 56> wt3 { 0, 444, 444, 888, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444, 444 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(50000, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(50000, totalDelay());
}

TEST(RxTest, ESI)
{
	class Delegate : public RxESI::Delegate
	{
		uint32_t &receivedData;
	public:
		Delegate(uint32_t &receivedData_) : receivedData(receivedData_) {}

		void RxESIDelegate_data(uint32_t data) override
		{
			receivedData = data;
		}
	};

	uint32_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

	RxESI esiDecoder(HIGH, &delegate);

	uint16_t startDelay = 4321;
	uint32_t data = 0x8000001;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWiter(pin);
	TxESI tx1(&pinWiter, HIGH);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x8000000;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
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
		esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
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
		esiDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
}
