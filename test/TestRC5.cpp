// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolRC5.h"

#include <array>
#include <iostream>

using namespace inseparates;

TEST(TxTest, RC5)
{
	uint8_t pin = 5;

	EXPECT_EQ(0x3000, TxRC5::encodeRC5(0, 0, 0));
	EXPECT_EQ(0x3175, TxRC5::encodeRC5(0, 0x05, 0x35));
	EXPECT_EQ(0x3800, TxRC5::encodeRC5(1, 0, 0));
	EXPECT_EQ(0x3084, TxRC5::encodeRC5(0, 0x02, 0x04));
	EXPECT_EQ(0x3FFF, TxRC5::encodeRC5(1, 0x1F, 0x3F));

	EXPECT_EQ(0x2000, TxRC5::encodeRC5X(0, 0, 0));
	EXPECT_EQ(0x2175, TxRC5::encodeRC5X(0, 0x05, 0x35));
	EXPECT_EQ(0x2800, TxRC5::encodeRC5X(1, 0, 0));
	EXPECT_EQ(0x3084, TxRC5::encodeRC5X(0, 0x02, 0x44));
	EXPECT_EQ(0x3FFF, TxRC5::encodeRC5X(1, 0x1F, 0x7F));

	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxRC5 tx1(&pinWriter, HIGH);
	tx1.prepare(0x2000);
	Scheduler::run(&tx1);
	std::array<uint8_t, 26> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 26> wt1 { 0, 1778, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(114000, totalDelay());

	resetLogs();
	TxRC5 tx2(&pinWriter, HIGH);
	tx2.prepare(0x1);
	Scheduler::run(&tx2);
	std::array<uint8_t, 28> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 28> wt2 { 0, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 889, 1778, 889 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(114000 + 889, totalDelay());

	resetLogs();
	TxRC5 tx3(&pinWriter, HIGH);
	tx3.prepare(TxRC5::encodeRC5(0, 0x05, 0x35));
	Scheduler::run(&tx3);
	std::array<uint8_t, 20> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 20> wt3 { 0, 889, 889, 1778, 889, 889, 889, 889, 1778, 1778, 1778, 889, 889, 889, 889, 1778, 1778, 1778, 1778, 889 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(114000, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(114000, totalDelay());
}


TEST(RxTest, RC5)
{
	class Delegate : public RxRC5::Delegate
	{
		uint16_t &receivedData;
	public:
		Delegate(uint16_t &receivedData_) : receivedData(receivedData_) {}

		void RxRC5Delegate_data(uint16_t data) override
		{
			receivedData = data;
		}
	};

	uint16_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

	RxRC5 rc5Decoder(HIGH, &delegate);

	uint16_t startDelay = 4321;
	uint16_t data = 0x2000;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWriter(pin);
	TxRC5 tx1(&pinWriter, HIGH);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);

	data = 0x3011;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
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
		rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
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
		rc5Decoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
}
