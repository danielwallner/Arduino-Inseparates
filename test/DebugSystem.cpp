// Copyright (c) 2024 Daniel Wallner

// Command line host application for easier debugging in source-level debuggers.

#define INS_DEBUGGING 1
#define INS_ENABLE_INPUT_FILTER 1
#define INS_UART_FRACTIONAL_TIME 1

#include "../src/ProtocolUART.h"
#include "../src/DebugUtils.h"

#include <assert.h>

#include <thread>
#include <vector>
#include <atomic>

using namespace inseparates;

#define FIFO_SIZE 8

#define FIFO_TEST_ITEMS 0x2FFFFFF

void push_to_fifo(LockFreeFIFO<unsigned, FIFO_SIZE>& fifo, unsigned num_items)
{
    for (unsigned i = 0; i < num_items; ++i)
    {
        while (fifo.full());
        fifo.writeRef() = i;
        fifo.push();
    }
}

int main()
{
    {
        LockFreeFIFO<unsigned, FIFO_SIZE> fifo;

        std::thread push_thread(push_to_fifo, std::ref(fifo), FIFO_TEST_ITEMS);

        for (unsigned i = 0; i < FIFO_TEST_ITEMS; ++i)
        {
            while (fifo.empty());
            unsigned item = fifo.readRef();
            fifo.pop();
            assert(item == i);
        }

        push_thread.join();
    }

    {
        resetLogs();

        InterruptScheduler scheduler;

        uint8_t pin = 4;
        uint8_t bits = 8;
        uint32_t baudRate = 1200;
        uint8_t data = 0x2;

        class Delegate : public RxUART::Delegate, public Scheduler::Delegate
        {
            Scheduler &_scheduler;
            uint8_t &_data;
            PushPullPinWriter _pinWriter;
            TxUART _tx1;
        public:
            std::vector<uint8_t> receivedData;
            uint16_t dataDelay = 0;

            Delegate(Scheduler &scheduler, uint8_t &data, uint8_t pin, uint8_t bits, uint32_t baudRate) :
                _scheduler(scheduler),
                _data(data),
                _pinWriter(pin),
                _tx1(&_pinWriter, HIGH)
            {
                _tx1.setBaudrate(baudRate);
                _tx1.setFormat(Parity::kOdd, bits);
            }

            void start()
            {
                _tx1.prepare(_data);
                _scheduler.add(&_tx1, this);
            }

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
                _tx1.prepare(~_data);
                _scheduler.add(&_tx1, this);
            }
        };

        Delegate delegate(scheduler, data, pin, bits, baudRate);

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
                delegate.start();
            }
        }

        assert(delegate.receivedData.size() && delegate.receivedData.back() == uint8_t(~data));
    }

    {
        resetLogs();

        InterruptScheduler scheduler;

        uint8_t pin = 4;
        uint8_t bits = 8;
        uint32_t baudRate = 1200;
        uint8_t data = 0x2;

        class Delegate2 : public RxUART::Delegate, public Scheduler::Delegate
        {
            Scheduler &_scheduler;
            uint8_t &_data;
            InterruptWriteScheduler _writeScheduler;
            InterruptPinWriter _pinWriter;
            TxUART _tx1;
        public:
            std::vector<uint8_t> receivedData;
            uint16_t dataDelay = 0;

            Delegate2(Scheduler &scheduler, uint8_t &data, uint8_t pin, uint8_t bits, uint32_t baudRate) :
                _scheduler(scheduler),
                _data(data),
                _writeScheduler(5, 1000),
                _pinWriter(&_writeScheduler, pin),
                _tx1(&_pinWriter, HIGH)
            {
                _scheduler.add(&_writeScheduler);

                _tx1.setBaudrate(baudRate);
                _tx1.setFormat(Parity::kOdd, bits);
            }

            void start()
            {
                _tx1.prepare(_data);
                _writeScheduler.add(&_tx1, this);
            }

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
                _tx1.prepare(~_data);
                _writeScheduler.add(&_tx1, this);
            }
        };

        Delegate2 delegate(scheduler, data, pin, bits, baudRate);

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
                delegate.start();
            }
        }

        assert(delegate.receivedData.size() && delegate.receivedData.back() == uint8_t(~data));
    }

    return 0;
}
