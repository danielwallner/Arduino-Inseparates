// Copyright (c) 2024 Daniel Wallner

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/ProtocolTechnicsSC.h"

#include <array>

using namespace inseparates;

TEST(TxTest, TechnicsSC)
{
	uint8_t dataPin = 3;
	uint8_t clockPin = 4;

	resetLogs();
	PushPullPinWriter dataPinWriter(dataPin);
	PushPullPinWriter clockPinWriter(clockPin);
	TxTechnicsSC tx1(&dataPinWriter, &clockPinWriter, dataPin, clockPin, LOW);
	tx1.prepare(0x1);
	Scheduler::runFor(&tx1, 140);
	std::array<uint8_t, 5> wsd1 { 0, 1, 0, 1, 0 };
	EXPECT_THAT(wsd1, testing::ElementsAreArray(g_digitalWriteStateLog[dataPin]));
	std::array<uint32_t, 5> wtd1 { 0, 0, 426, 26412, 1278 };
	EXPECT_THAT(wtd1, testing::ElementsAreArray(g_digitalWriteTimeLog[dataPin]));
	std::array<uint8_t, 67> wsc { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };
	EXPECT_THAT(wsc, testing::ElementsAreArray(g_digitalWriteStateLog[clockPin]));
	std::array<uint32_t, 67> wtc { 0, 0, 639, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426, 426 };
	EXPECT_THAT(wtc, testing::ElementsAreArray(g_digitalWriteTimeLog[clockPin]));

	resetLogs();
	TxTechnicsSC tx2(&dataPinWriter, &clockPinWriter, dataPin, clockPin, LOW);
	tx2.prepare(0x40000001);
	Scheduler::runFor(&tx2, 140);
	std::array<uint8_t, 7> wsd2 { 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(wsd2, testing::ElementsAreArray(g_digitalWriteStateLog[dataPin]));
	std::array<uint32_t, 7> wtd2 { 0, 0, 426, 852, 852, 24708, 1278 };
	EXPECT_THAT(wtd2, testing::ElementsAreArray(g_digitalWriteTimeLog[dataPin]));
	EXPECT_THAT(wsc, testing::ElementsAreArray(g_digitalWriteStateLog[clockPin]));
	EXPECT_THAT(wtc, testing::ElementsAreArray(g_digitalWriteTimeLog[clockPin]));

	resetLogs();
	TxTechnicsSC tx3(&dataPinWriter, &clockPinWriter, dataPin, clockPin, LOW);
	tx3.prepare(0x59);
	Scheduler::runFor(&tx3, 140);
	std::array<uint8_t, 9> wsd3 { 0, 1, 0, 1, 0, 1, 0, 1, 0 };
	EXPECT_THAT(wsd3, testing::ElementsAreArray(g_digitalWriteStateLog[dataPin]));
	std::array<uint32_t, 9> wtd3 { 0, 0, 426, 21300, 852, 852, 1704, 1704, 1278 };
	EXPECT_THAT(wtd3, testing::ElementsAreArray(g_digitalWriteTimeLog[dataPin]));
	EXPECT_THAT(wsc, testing::ElementsAreArray(g_digitalWriteStateLog[clockPin]));
	EXPECT_THAT(wtc, testing::ElementsAreArray(g_digitalWriteTimeLog[clockPin]));

	resetLogs();
	digitalWrite(dataPin, LOW);
	digitalWrite(clockPin, HIGH);
	tx3.prepare(0x59);
	Scheduler::runFor(&tx3, 140);
	EXPECT_THAT(wsd3, testing::ElementsAreArray(g_digitalWriteStateLog[dataPin]));
	EXPECT_THAT(wtd3, testing::ElementsAreArray(g_digitalWriteTimeLog[dataPin]));
	EXPECT_THAT(wsc, testing::ElementsAreArray(g_digitalWriteStateLog[clockPin]));
	EXPECT_THAT(wtc, testing::ElementsAreArray(g_digitalWriteTimeLog[clockPin]));
}

TEST(RxTest, TechnicsSC)
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
	uint32_t data = 0x80000001;
	resetLogs();
	delayMicroseconds(startDelay);
	PushPullPinWriter dataPinWriter(dataPin);
	PushPullPinWriter clockPinWriter(clockPin);
	TxTechnicsSC tx1(&dataPinWriter, &clockPinWriter, dataPin, clockPin, HIGH);
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
	EXPECT_EQ(data, receivedData);
}
