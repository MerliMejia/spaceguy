#pragma once

#include "./types.h"

struct BufferWithMemory
{
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
};

extern uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

extern BufferWithMemory createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties);