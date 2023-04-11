/*
    Copyright 2022-2023 Hydr8gon

    This file is part of rokuyon.

    rokuyon is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    rokuyon is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with rokuyon. If not, see <https://www.gnu.org/licenses/>.
*/

#include <atomic>
#include <cstring>
#include <queue>
#include <thread>
#include <vector>

#include "ai.h"
#include "core.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

#define MAX_BUFFERS 4
#define SAMPLE_COUNT 1024
#define OUTPUT_RATE 48000
#define OUTPUT_SIZE SAMPLE_COUNT * sizeof(uint32_t)

struct Samples
{
    uint32_t address;
    uint32_t count;
};

namespace AI
{
    uint32_t bufferOut[SAMPLE_COUNT];
    std::atomic<bool> ready;

    Samples samples[2];
    std::queue<std::vector<uint32_t>> buffers;
    uint32_t offset;

    uint32_t dramAddr;
    uint32_t control;
    uint32_t frequency;
    uint32_t status;

    void createBuffer();
    void submitBuffer();
    void processBuffer();
}

void AI::fillBuffer(uint32_t *out)
{
    // Try to wait until a buffer is ready, but don't stall the audio callback too long
    std::chrono::steady_clock::time_point waitTime = std::chrono::steady_clock::now();
    while (!ready.load())
    {
        if (std::chrono::steady_clock::now() - waitTime > std::chrono::microseconds(1000000 / 60))
        {
            // If a buffer isn't ready in time, fill the output with the last played sample
            for (int i = 0; i < SAMPLE_COUNT; i++)
                out[i] = bufferOut[SAMPLE_COUNT - 1];
            return;
        }
    }

    // Output the buffer and mark it as used
    memcpy(out, bufferOut, OUTPUT_SIZE);
    ready.store(false);
}

void AI::reset()
{
    // Reset the AI to its initial state
    dramAddr = 0;
    control = 0;
    frequency = 0;
    status = 0;

    // Schedule the first audio buffer to output
    Core::schedule(createBuffer, (uint64_t)SAMPLE_COUNT * (93750000 * 2) / OUTPUT_RATE);
}

uint32_t AI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        case 0x450000C: // AI_STATUS
            // Get the status register
            return status;

        default:
            LOG_WARN("Unknown AI register read: 0x%X\n", address);
            return 0;
    }
}

void AI::write(uint32_t address, uint32_t value)
{
    // Write to an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4500000: // AI_DRAM_ADDR
            // Set the RDRAM DMA address
            dramAddr = value & 0xFFFFFF;
            return;

        case 0x4500004: // AI_LENGTH
            if (control) // DMA enabled
            {
                if (status & (1 << 30)) // Busy
                {
                    // Queue a second set of samples while the first is processed
                    status |= (1 << 31); // Full
                    samples[1].address = dramAddr;
                    samples[1].count = (value & ~0x7) / 4;
                }
                else
                {
                    // Queue a set of samples and submit them as an audio buffer
                    status |= (1 << 30); // Busy
                    samples[0].address = dramAddr;
                    samples[0].count = (value & ~0x7) / 4;
                    submitBuffer();
                }
            }
            return;

        case 0x4500008: // AI_CONTROL
            // Set the control register
            control = value & 0x1;
            return;

        case 0x450000C: // AI_STATUS
            // Acknowledge an AI interrupt
            MI::clearInterrupt(2);
            return;

        case 0x4500010: // AI_DAC_RATE
            // Set the audio frequency based on the NTSC DAC rate
            frequency = 48681812 / (value & 0x3FFF);
            return;

        default:
            LOG_WARN("Unknown AI register write: 0x%X\n", address);
            return;
    }
}

void AI::createBuffer()
{
    // Wait until the previous buffer has been used
    while (Core::running && ready.load())
        std::this_thread::yield();

    memset(bufferOut, 0, OUTPUT_SIZE);
    size_t count = 0;

    while (!buffers.empty() && count < OUTPUT_SIZE)
    {
        // Get the current queued buffer and the size of its remaining samples
        std::vector<uint32_t> &buffer = buffers.front();
        size_t size = (buffer.size() - offset) * sizeof(uint32_t);

        if (size <= OUTPUT_SIZE - count)
        {
            // Copy all of the remaining queued samples to the output buffer
            memcpy(&bufferOut[count / sizeof(uint32_t)], &buffer[offset], size);
            count += size;
            offset = 0;
            buffers.pop();
        }
        else
        {
            // Copy as many queued samples that can fit to the output buffer
            memcpy(&bufferOut[count / sizeof(uint32_t)], &buffer[offset], OUTPUT_SIZE - count);
            offset += (OUTPUT_SIZE - count) / sizeof(uint32_t);
            break;
        }
    }

    // Mark the buffer as ready and schedule the next one
    ready.store(true);
    Core::schedule(createBuffer, (uint64_t)SAMPLE_COUNT * (93750000 * 2) / OUTPUT_RATE);
}

void AI::submitBuffer()
{
    LOG_INFO("Submitting %d AI samples from RDRAM 0x%X at frequency %dHz\n",
        samples[0].count, samples[0].address, frequency);

    if (buffers.size() < MAX_BUFFERS)
    {
        // Create a new audio buffer based on sample count and frequency
        size_t count = samples[0].count * OUTPUT_RATE / frequency;
        std::vector<uint32_t> buffer(count);

        // Copy samples to the buffer, scaled from their original frequency
        for (size_t i = 0; i < count; i++)
        {
            uint32_t address = samples[0].address + (i * samples[0].count / count) * 4;
            uint32_t value = Memory::read<uint32_t>(0xA0000000 + address);
            buffer[i] = (value << 16) | (value >> 16);
        }

        // Add the buffer to the output queue
        buffers.push(buffer);
    }

    // Schedule the logical completion of the AI DMA based on sample count and frequency
    Core::schedule(processBuffer, (uint64_t)samples[0].count * (93750000 * 2) / frequency);
}

void AI::processBuffer()
{
    if (status & (1 << 31)) // Full
    {
        // Submit the next queued samples and trigger an AI interrupt to request more
        status &= ~(1 << 31); // Not full
        samples[0] = samples[1];
        submitBuffer();
        MI::setInterrupt(2);
    }
    else
    {
        // Stop running because there are no more samples to submit
        status &= ~(1 << 30); // Not busy
    }
}
