#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

void DEFAULT_DESCRIPTOR_SET_LAYOUT(vk::raii::DescriptorSetLayout &descriptorSetLayout, vk::raii::Device &device);
void DEFAULT_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool, vk::raii::Device &device);

void ANIMATED_DESCRIPTOR_SET_LAYOUT(vk::raii::DescriptorSetLayout &descriptorSetLayout, vk::raii::Device &device);
void ANIMATED_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool, vk::raii::Device &device);