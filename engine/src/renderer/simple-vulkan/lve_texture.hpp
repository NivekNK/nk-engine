#pragma once

#include "lve_device.hpp"

// std
#include <memory>
#include <string>

namespace lve {

class LveTexture {
 public:
  LveTexture(LveDevice &device, const std::string &filepath);
  ~LveTexture();

  LveTexture(const LveTexture &) = delete;
  LveTexture &operator=(const LveTexture &) = delete;

  static std::unique_ptr<LveTexture> createTextureFromFile(
      LveDevice &device, const std::string &filepath);

  VkImageView imageView() const { return textureImageView; }
  VkSampler sampler() const { return textureSampler; }

 private:
  void createTextureImage(const std::string &filepath);
  void createTextureImageView();
  void createTextureSampler();
  void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

  LveDevice &lveDevice;
  VkImage textureImage;
  VkDeviceMemory textureImageMemory;
  VkImageView textureImageView;
  VkSampler textureSampler;
  uint32_t mipLevels;
  uint32_t width, height;
};

}  // namespace lve

