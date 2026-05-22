#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

void CREATE_SIMPLE_DESCRIPTOR_SET_LAYOUT(vk::raii::DescriptorSetLayout &descriptorSetLayout, vk::raii::Device &device);
void CREATE_SIMPLE_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool, vk::raii::Device &device);