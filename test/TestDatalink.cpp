// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolDatalink80.h"
#include "../src/ProtocolDatalink86.h"

#include <array>

using namespace inseparates;

TEST(TxTest, Datalink80)
{
	uint8_t pin = 1;
	// All zeros
	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxDatalink80 tx1(&pinWriter, LOW);
	tx1.prepare(0x0);
	Scheduler::run(&tx1);
	std::array<uint8_t, 4> ws1 { 0, 1, 0, 1 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 4> wt1 { 0, 3125, 46875, 3125 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(100000, totalDelay());

	resetLogs();
	TxDatalink80 tx2(&pinWriter, LOW);
	tx2.prepare(0x4A);
	Scheduler::run(&tx2);
	std::array<uint8_t, 12> ws2 { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 12> wt2 { 0, 6250, 6250, 3125, 3125, 3125, 28125, 6250, 6250, 3125, 3125, 3125 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(100000, totalDelay());

	// All ones
	resetLogs();
	TxDatalink80 tx3(&pinWriter, LOW);
	tx3.prepare(0x7F);
	Scheduler::run(&tx3);
	std::array<uint8_t, 4> ws3 { 0, 1, 0, 1 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 4> wt3 { 0, 25000, 25000, 25000 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(100000, totalDelay());

	// Repeat
	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(100000, totalDelay());
}

TEST(RxTest, Datalink80)
{
	class Delegate : public RxDatalink80::Delegate
	{
		uint8_t &receivedData;
	public:
		Delegate(uint8_t &receivedData_) : receivedData(receivedData_) {}

		void RxDatalink80Delegate_data(uint8_t data) override
		{
			receivedData = data;
		}

		void RxDatalink80Delegate_timingError() override
		{
		}
	};

	uint8_t receivedData;
	uint8_t pin = 6;

	Delegate delegate(receivedData);

	RxDatalink80 datalinkDecoder(LOW, &delegate);

	uint16_t startDelay = 4321;
	uint32_t data = 0x41;
	resetLogs();
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWiter(pin);
	TxDatalink80 tx1(&pinWiter, LOW);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);

	data = 0x0;
	resetLogs();
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);

	data = 0x7F;
	resetLogs();
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);

	data = 0x5A;
	resetLogs();
	delayMicroseconds(startDelay);
	tx1.prepare(data);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	datalinkDecoder.Decoder_timeout(g_digitalWriteStateLog[pin].back());
	EXPECT_EQ(data, receivedData);
}

TEST(TxTest, Datalink86)
{
	uint8_t pin = 2;
	// Example from Datalink '86 document
	resetLogs();
	PushPullPinWriter pinWriter(pin);
	TxDatalink86 tx1(&pinWriter, HIGH);
	tx1.prepare(0x083E35, 21, false, false);
	Scheduler::run(&tx1);
	std::array<uint8_t, 54> ws1 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws1, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 54> wt1 { 0, 1562, 1563, 1562, 1563, 1562, 14063,1562,1563,1562, 4688, 1562, 7813, 1562, 1563, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 7813, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 1563, 1562, 4688, 1562, 4688, 1562, 7813, 1562, 4688, 1562, 1563, 1562, 7813, 1562, 1563, 1562, 7813, 1562, 10938, 1562 };
	EXPECT_THAT(wt1, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(173437, totalDelay());

	// Send '1' as start bit
	resetLogs();
	TxDatalink86 tx2(&pinWriter, HIGH);
	tx2.prepare(0x7, 2, false, false);
	Scheduler::run(&tx2);
	std::array<uint8_t, 16> ws2 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws2, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 16> wt2 { 0, 1562, 1563, 1562, 1563, 1562, 14063, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 10938, 1562 };
	EXPECT_THAT(wt2, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(54687, totalDelay());

	// All zeros
	resetLogs();
	TxDatalink86 tx3(&pinWriter, HIGH);
	tx3.prepare(0x0, 3, false, false);
	Scheduler::run(&tx3);
	std::array<uint8_t, 18> ws3 { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	std::array<uint32_t, 18> wt3 {  0, 1562, 1563, 1562, 1563, 1562, 14063, 1562, 1563, 1562, 4688, 1562, 4688, 1562, 4688, 1562, 10938, 1562 };
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(57812, totalDelay());

	// Repeat
	resetLogs();
	Scheduler::run(&tx3);
	EXPECT_THAT(ws3, testing::ElementsAreArray(g_digitalWriteStateLog[pin]));
	EXPECT_THAT(wt3, testing::ElementsAreArray(g_digitalWriteTimeLog[pin]));
	EXPECT_EQ(57812, totalDelay());
}

TEST(RxTest, Datalink86)
{
	class Delegate : public RxDatalink86::Delegate
	{
		uint64_t &receivedData;
		uint8_t &receivedBits;
	public:
		Delegate(uint64_t &receivedData_, uint8_t &receivedBits_) : receivedData(receivedData_), receivedBits(receivedBits_) {}

		void RxDatalink86Delegate_data(uint64_t data, uint8_t bits) override
		{
			receivedData = data;
			receivedBits = bits;
		}
	};

	uint64_t receivedData;
	uint8_t receivedBits;
	uint8_t pin = 6;

	Delegate delegate(receivedData, receivedBits);

	RxDatalink86 datalinkDecoder(HIGH, &delegate);

	uint16_t startDelay = 4321;
	uint32_t data = 0x8000001;
	uint8_t bits = 32;
	resetLogs();
	digitalWrite(pin, LOW);
	delayMicroseconds(startDelay);
	PushPullPinWriter pinWiter(pin);
	TxDatalink86 tx1(&pinWiter, HIGH);
	tx1.prepare(data, bits, false, false);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(bits, receivedBits);

	data = 0xAA55;
	bits = 16;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data, bits, false, false);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(bits, receivedBits);

	data = 0x3FFF;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data, bits, false, false);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(bits, receivedBits);

	data = 0x355A;
	resetLogs();
	digitalWrite(pin, HIGH);
	delayMicroseconds(startDelay);
	tx1.prepare(data, bits, false, false);
	Scheduler::run(&tx1);
	for (unsigned i = 0; i < g_digitalWriteStateLog[pin].size(); i++)
	{
		datalinkDecoder.Decoder_pulse(1 ^ g_digitalWriteStateLog[pin][i], g_digitalWriteTimeLog[pin][i]);
	}
	EXPECT_EQ(data, receivedData);
	EXPECT_EQ(bits, receivedBits);
}
