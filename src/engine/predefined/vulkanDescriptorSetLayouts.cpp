#include "vulkanDescriptorSetLayouts.h"
#include "../vulkanGlobals.h"

void PARTICLE_COMPUTE_DESCRIPTOR_SET_LAYOUT(
    vk::raii::DescriptorSetLayout &descriptorSetLayout,
    vk::raii::Device &device) {

  std::array<vk::DescriptorSetLayoutBinding, 5> bindings{
      vk::DescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex,
          .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding{
          .binding = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute |
                        vk::ShaderStageFlagBits::eVertex,
          .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding{
          .binding = 2,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute,
          .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding{
          .binding = 3,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute,
          .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding{
          .binding = 4,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute,
          .pImmutableSamplers = nullptr,
      },
  };

  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
};
void PARTICLE_COMPUTE_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool,
                                      vk::raii::Device &device) {

  std::array<vk::DescriptorPoolSize, 2> poolSizes{
      vk::DescriptorPoolSize{
          .type = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2),
      },
      vk::DescriptorPoolSize{
          .type = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 3),
      },
  };

  vk::DescriptorPoolCreateInfo poolInfo{
      .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void DEFAULT_DESCRIPTOR_SET_LAYOUT(
    vk::raii::DescriptorSetLayout &descriptorSetLayout,
    vk::raii::Device &device) {
  vk::DescriptorSetLayoutBinding uboLayoutBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags =
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers = nullptr};

  vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1,
                                               .pBindings = &uboLayoutBinding};

  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
};
void DEFAULT_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool,
                             vk::raii::Device &device) {
  vk::DescriptorPoolSize poolSize{
      .type = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

  vk::DescriptorPoolCreateInfo poolInfo{
      .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize};

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void ANIMATED_DESCRIPTOR_SET_LAYOUT(
    vk::raii::DescriptorSetLayout &descriptorSetLayout,
    vk::raii::Device &device) {
  std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
      vk::DescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex |
                        vk::ShaderStageFlagBits::eFragment,
          .pImmutableSamplers = nullptr,
      },
      vk::DescriptorSetLayoutBinding{
          .binding = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex,
          .pImmutableSamplers = nullptr,
      },
  };

  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}
void ANIMATED_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool,
                              vk::raii::Device &device) {
  std::array<vk::DescriptorPoolSize, 2> poolSizes{
      vk::DescriptorPoolSize{
          .type = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      },
      vk::DescriptorPoolSize{
          .type = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      },
  };

  vk::DescriptorPoolCreateInfo poolInfo{
      .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
};
