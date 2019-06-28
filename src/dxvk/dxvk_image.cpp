#include "dxvk_image.h"

namespace dxvk {
  
  DxvkImage::DxvkImage(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkImageCreateInfo&  createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(vkd), m_info(createInfo), m_memFlags(memFlags) {

    // Is resource sharing supported (ie. the extension)
    // Let's ask our allocator who actually has a DxvkDevice.
    bool sharingSupported = memAlloc.supportsResourceSharing();

    // Copy the compatible view formats to a persistent array
    m_viewFormats.resize(createInfo.viewFormatCount);
    for (uint32_t i = 0; i < createInfo.viewFormatCount; i++)
      m_viewFormats[i] = createInfo.viewFormats[i];
    m_info.viewFormats = m_viewFormats.data();

    // If a resource is shared, we should tell the image...
    VkExternalMemoryImageCreateInfo externalInfo;
    externalInfo.sType        = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalInfo.pNext        = nullptr;
    externalInfo.handleTypes  = createInfo.sharing.type;

    // If defined, we should provide a format list, which
    // allows some drivers to enable image compression
    VkImageFormatListCreateInfoKHR formatList;
    formatList.sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
    formatList.pNext           = (sharingSupported && createInfo.sharing.isShared()) ? &externalInfo : nullptr;
    formatList.viewFormatCount = createInfo.viewFormatCount;
    formatList.pViewFormats    = createInfo.viewFormats;
    
    VkImageCreateInfo info;
    info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext                 = &formatList;
    info.flags                 = createInfo.flags;
    info.imageType             = createInfo.type;
    info.format                = createInfo.format;
    info.extent                = createInfo.extent;
    info.mipLevels             = createInfo.mipLevels;
    info.arrayLayers           = createInfo.numLayers;
    info.samples               = createInfo.sampleCount;
    info.tiling                = createInfo.tiling;
    info.usage                 = createInfo.usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (m_vkd->vkCreateImage(m_vkd->device(),
          &info, nullptr, &m_image) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkImage: Failed to create image:",
        "\n  Type:            ", info.imageType,
        "\n  Format:          ", info.format,
        "\n  Extent:          ", "(", info.extent.width,
                                 ",", info.extent.height,
                                 ",", info.extent.depth, ")",
        "\n  Mip levels:      ", info.mipLevels,
        "\n  Array layers:    ", info.arrayLayers,
        "\n  Samples:         ", info.samples,
        "\n  Usage:           ", info.usage,
        "\n  Tiling:          ", info.tiling));
    }
    
    // Get memory requirements for the image. We may enforce strict
    // alignment on non-linear images in order not to violate the
    // bufferImageGranularity limit, which may be greater than the
    // required resource memory alignment on some GPUs.
    VkMemoryDedicatedRequirementsKHR dedicatedRequirements;
    dedicatedRequirements.sType                       = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;
    dedicatedRequirements.pNext                       = VK_NULL_HANDLE;
    dedicatedRequirements.prefersDedicatedAllocation  = VK_FALSE;
    dedicatedRequirements.requiresDedicatedAllocation = VK_FALSE;
    
    VkMemoryRequirements2KHR memReq;
    memReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
    memReq.pNext = &dedicatedRequirements;
    
    VkImageMemoryRequirementsInfo2KHR memReqInfo;
    memReqInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
    memReqInfo.image = m_image;
    memReqInfo.pNext = VK_NULL_HANDLE;

    VkExportMemoryAllocateInfo exportInfo;
    exportInfo.sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.pNext       = nullptr;
    exportInfo.handleTypes = createInfo.sharing.type;

    VkImportMemoryWin32HandleInfoKHR importInfo;
    importInfo.sType       = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importInfo.pNext       = nullptr;
    importInfo.handleType  = createInfo.sharing.type;
    importInfo.handle      = createInfo.sharing.handle;
    importInfo.name        = nullptr;

    const void* sharingInfo = createInfo.sharing.isImport() ? (const void*) &importInfo : (const void*) &exportInfo;

    VkMemoryDedicatedAllocateInfoKHR dedMemoryAllocInfo;
    dedMemoryAllocInfo.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
    dedMemoryAllocInfo.pNext  = (sharingSupported && createInfo.sharing.isShared()) ? sharingInfo : nullptr;
    dedMemoryAllocInfo.buffer = VK_NULL_HANDLE;
    dedMemoryAllocInfo.image  = m_image;
    
    m_vkd->vkGetImageMemoryRequirements2KHR(
      m_vkd->device(), &memReqInfo, &memReq);

    // We must force dedicated allocation for shared resources...
    if (sharingSupported && createInfo.sharing.isShared()) {
      dedicatedRequirements.prefersDedicatedAllocation  = VK_TRUE;
      dedicatedRequirements.requiresDedicatedAllocation = VK_TRUE;
    }
 
    if (info.tiling != VK_IMAGE_TILING_LINEAR) {
      memReq.memoryRequirements.size      = align(memReq.memoryRequirements.size,       memAlloc.bufferImageGranularity());
      memReq.memoryRequirements.alignment = align(memReq.memoryRequirements.alignment , memAlloc.bufferImageGranularity());
    }

    // Use high memory priority for GPU-writable resources
    bool isGpuWritable = (m_info.usage & (
      VK_IMAGE_USAGE_STORAGE_BIT          |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
    
    float priority = isGpuWritable ? 1.0f : 0.5f;

    // Ask driver whether we should be using a dedicated allocation
    m_memory = memAlloc.alloc(&memReq.memoryRequirements,
      dedicatedRequirements, dedMemoryAllocInfo, memFlags, priority);
    
    // Try to bind the allocated memory slice to the image
    if (m_vkd->vkBindImageMemory(m_vkd->device(),
          m_image, m_memory.memory(), m_memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkImage::DxvkImage: Failed to bind device memory");

    if (sharingSupported && createInfo.sharing.isExport()) {
      VkMemoryGetWin32HandleInfoKHR handleInfo;
      handleInfo.sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
      handleInfo.pNext      = nullptr;
      handleInfo.handleType = createInfo.sharing.type;
      handleInfo.memory     = m_memory.memory();

      if(m_vkd->vkGetMemoryWin32HandleKHR(m_vkd->device(), &handleInfo, &m_sharedHandle) != VK_SUCCESS)
        Logger::warn("DxvkImage::DxvkImage: Failed to get shared handle for image");
    }
  }
  
  
  DxvkImage::DxvkImage(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkImageCreateInfo&  info,
          VkImage               image)
  : m_vkd(vkd), m_info(info), m_image(image) {
    
  }
  
  
  DxvkImage::~DxvkImage() {
    // This is a bit of a hack to determine whether
    // the image is implementation-handled or not
    if (m_memory.memory() != VK_NULL_HANDLE)
      m_vkd->vkDestroyImage(m_vkd->device(), m_image, nullptr);
  }
  
  
  DxvkImageView::DxvkImageView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  info)
  : m_vkd(vkd), m_image(image), m_info(info) {
    // Since applications tend to bind views 
    for (uint32_t i = 0; i < ViewCount; i++)
      m_views[i] = VK_NULL_HANDLE;
    
    switch (info.type) {
      case VK_IMAGE_VIEW_TYPE_1D:
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY: {
        this->createView(VK_IMAGE_VIEW_TYPE_1D,       1);
        this->createView(VK_IMAGE_VIEW_TYPE_1D_ARRAY, info.numLayers);
      } break;
      
      case VK_IMAGE_VIEW_TYPE_2D:
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        this->createView(VK_IMAGE_VIEW_TYPE_2D, 1);
        /* fall through */

      case VK_IMAGE_VIEW_TYPE_CUBE:
      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: {
        this->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, info.numLayers);
        
        if (m_image->info().flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
          uint32_t cubeCount = info.numLayers / 6;
        
          if (cubeCount > 0) {
            this->createView(VK_IMAGE_VIEW_TYPE_CUBE,       6);
            this->createView(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, 6 * cubeCount);
          }
        }
      } break;
        
      case VK_IMAGE_VIEW_TYPE_3D: {
        this->createView(VK_IMAGE_VIEW_TYPE_3D, 1);
        
        if (m_image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR && info.numLevels == 1) {
          this->createView(VK_IMAGE_VIEW_TYPE_2D,       1);
          this->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_image->mipLevelExtent(info.minLevel).depth);
        }
      } break;
      
      default:
        throw DxvkError(str::format("DxvkImageView: Invalid view type: ", info.type));
    }
  }
  
  
  DxvkImageView::~DxvkImageView() {
    for (uint32_t i = 0; i < ViewCount; i++)
      m_vkd->vkDestroyImageView(m_vkd->device(), m_views[i], nullptr);
  }
  
  
  void DxvkImageView::createView(VkImageViewType type, uint32_t numLayers) {
    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = m_info.aspect;
    subresourceRange.baseMipLevel   = m_info.minLevel;
    subresourceRange.levelCount     = m_info.numLevels;
    subresourceRange.baseArrayLayer = m_info.minLayer;
    subresourceRange.layerCount     = numLayers;

    VkImageViewUsageCreateInfoKHR viewUsage;
    viewUsage.sType           = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR;
    viewUsage.pNext           = nullptr;
    viewUsage.usage           = m_info.usage;
    
    VkImageViewCreateInfo viewInfo;
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext            = &viewUsage;
    viewInfo.flags            = 0;
    viewInfo.image            = m_image->handle();
    viewInfo.viewType         = type;
    viewInfo.format           = m_info.format;
    viewInfo.components       = m_info.swizzle;
    viewInfo.subresourceRange = subresourceRange;

    if (m_info.usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      viewInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    }
    
    if (m_vkd->vkCreateImageView(m_vkd->device(),
          &viewInfo, nullptr, &m_views[type]) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkImageView: Failed to create image view:"
        "\n  View type:       ", viewInfo.viewType,
        "\n  View format:     ", viewInfo.format,
        "\n  Subresources:    ",
        "\n    Aspect mask:   ", std::hex, viewInfo.subresourceRange.aspectMask,
        "\n    Mip levels:    ", viewInfo.subresourceRange.baseMipLevel, " - ",
                                 viewInfo.subresourceRange.levelCount,
        "\n    Array layers:  ", viewInfo.subresourceRange.baseArrayLayer, " - ",
                                 viewInfo.subresourceRange.layerCount,
        "\n  Image properties:",
        "\n    Type:          ", m_image->info().type,
        "\n    Format:        ", m_image->info().format,
        "\n    Extent:        ", "(", m_image->info().extent.width,
                                 ",", m_image->info().extent.height,
                                 ",", m_image->info().extent.depth, ")",
        "\n    Mip levels:    ", m_image->info().mipLevels,
        "\n    Array layers:  ", m_image->info().numLayers,
        "\n    Samples:       ", m_image->info().sampleCount,
        "\n    Usage:         ", std::hex, m_image->info().usage,
        "\n    Tiling:        ", m_image->info().tiling));
    }
  }
  
}