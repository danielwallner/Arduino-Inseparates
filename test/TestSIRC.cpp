// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolSIRC.h"

#include <array>

using namespace inseparates;

TEST(TxTest, SIRC)
{
	uint8_t pin = 6;

	EXPECT_EQ(0x0000, TxSIRC::encodeSIRC(0, 0));
	EXPECT_EQ(0x017F, TxSIRC::encodeSIRC(0x02, 0x7F));
	EXPECT_EQ(0x0093, TxSIRC::encodeSIRC(0x01, 0x13));
	EXPECT_EQ(0x7FFF, TxSIRC::encodeSIRC(0xFF, 0x7F));

	EXPECT_EQ(0x00000, TxSIRC::encodeSIRC20(0, 0, 0));
	EXPECT_EQ(0x0017F, TxSIRC::encodeSIRC20(0x0, 0x02, 0x7F));
	EXPECT_EQ(0xFF000, TxSIRC::encodeSIRC20(0xFF, 0, 0));
	EXPECT_EQ(0x01104, TxSIRC::encodeSIRC20(0x01, 0x02, 0x04));
	EXPECT_EQ(0xFFFFF, TxSIRC::encodeSIRC20(0xFF, 0x1F, 0x7F));

	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxSIRC tx1(&pinWriter, HIGH);
	tx1.prepare(0x4000, 15);
	Scheduler::run(&tx1);
	std::array<uint8_t, 32> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 32> wt1 { 0, 2400, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 1200 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(45000, totalDelay());

	resetLogs();
	TxSIRC tx2(&pinWriter, HIGH);
	tx2.prepare(0x01, 12);
	Scheduler::run(&tx2);
	std::array<uint8_t, 26> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 26> wt2 { 0, 2400, 600, 1200, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600, 600 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(45000, totalDelay());

	resetLogs();
	TxSIRC tx3(&pinWriter, HIGH);
	tx3.prepare(TxSIRC::encodeSIRC(0x01, 0x13), 12);
	Scheduler::run(&tx3);
	std::array<uint8_t, 26> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 26> wt3 { 0, 2400, 600, 1200, 600, 1200, 600, 600, 600, 600, 600, 1200, 600, 600, 600, 600, 600, 1200, 600, 600, 600, 600, 600, 600, 600, 600 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(45000, totalDelay());

	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(45000, totalDelay());
}

TEST(RxTest, SIRC)
{
	class Delegate : public RxSIRC::Delegate
	{
		uint32_t &receivedData;
		uint8_t &receivedBits;
		uint8_t &receivedBus;
	public:
		Delegate(uint32_t &receivedData_, uint8_t &receivedBits_, uint8_t &receivedBus_) : receivedData(receivedData_), receivedBits(receivedBits_), receivedBus(receivedBus_) {}

		void RxSIRCDelegate_data(uint32_t data, uint8_t bits, uint8_t bus) override
		{
			receivedData = data;
			receivedBits = bits;
			receivedBus = bus;
		}
	};

	uint32_t receivedData;
	uint8_t receivedBits;
	uint8_t receivedBus;
	uint8_t pin = 6;
	uint8_t bus = 4;

	Delegate delegate(receivedData, receivedBits, receivedBus);

	RxSIRC sircDecoder(HIGH, &delegate, bus);

	uint16_t startDelay = 4321;
	uint32_t data = 0x80001;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWriter(pin);
	TxSIRC tx1(&pinWriter, HIGH);
	tx1.prepare(data, 20);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(20, receivedBits);
	EXPECT_EQ(bus, receivedBus);

	data = 0x7000F;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data, 20);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(20, receivedBits);
	EXPECT_EQ(bus, receivedBus);

	data = 0x3FFF;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data, 20);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(20, receivedBits);
	EXPECT_EQ(bus, receivedBus);

	data = 0x355A;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	tx1.prepare(data, 20);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		sircDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	sircDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(20, receivedBits);
	EXPECT_EQ(bus, receivedBus);
}
