#include "d3d10_1_device.h"
#include "d3d10_1_buffer.h"
#include "d3d10_1_texture.h"
#include "d3d10_1_view_srv.h"

namespace dxvk {
  
  D3D10ShaderResourceView::D3D10ShaderResourceView(
          D3D10Device*                      device,
          ID3D10Resource*                   resource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC1&  desc,
    const Rc<DxvkBufferView>&               bufferView)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_bufferView(bufferView) { }
  
  
  D3D10ShaderResourceView::D3D10ShaderResourceView(
          D3D10Device*                      device,
          ID3D10Resource*                   resource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC1&  desc,
    const Rc<DxvkImageView>&                imageView)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_imageView(imageView) { }
  
  
  D3D10ShaderResourceView::~D3D10ShaderResourceView() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D10ShaderResourceView::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10View)
     || riid == __uuidof(ID3D10ShaderResourceView)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D10ShaderResourceView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDevice(ID3D10Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetResource(ID3D10Resource** ppResource) {
    *ppResource = m_resource.ref();
  }
  
  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDesc(D3D10_SHADER_RESOURCE_VIEW_DESC* pDesc) {
	  pDesc->Buffer = m_desc.Buffer;
	  pDesc->Format = m_desc.Format;
	  pDesc->Texture1D = m_desc.Texture1D;
	  pDesc->Texture1DArray = m_desc.Texture1DArray;
	  pDesc->Texture2D = m_desc.Texture2D;
	  pDesc->Texture2DMS = m_desc.Texture2DMS;
	  pDesc->Texture2DMSArray = m_desc.Texture2DMSArray;
	  pDesc->Texture2DArray = m_desc.Texture2DArray;
	  pDesc->Texture3D = m_desc.Texture3D;
	  pDesc->TextureCube = m_desc.TextureCube;
	  pDesc->ViewDimension = m_desc.ViewDimension;
  }
  
  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDesc1(D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D10ShaderResourceView::GetDescFromResource(
          ID3D10Resource*                   pResource,
          D3D10_SHADER_RESOURCE_VIEW_DESC1*  pDesc) {
    D3D10_RESOURCE_DIMENSION resourceDim = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D10_RESOURCE_DIMENSION_BUFFER: {
        D3D10_BUFFER_DESC bufferDesc;
        static_cast<D3D10Buffer*>(pResource)->GetDesc(&bufferDesc);
        
		pDesc->Format = DXGI_FORMAT_UNKNOWN;
		pDesc->ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
		pDesc->Buffer.ElementOffset = 0;
		pDesc->Buffer.ElementWidth = bufferDesc.ByteWidth / sizeof(uint32_t); //bufferDesc.StructureByteStride;
      } return S_OK;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D10_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D10Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MostDetailedMip = 0;
          pDesc->Texture1D.MipLevels       = resourceDesc.MipLevels;
        } else {
          pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MostDetailedMip = 0;
          pDesc->Texture1DArray.MipLevels       = resourceDesc.MipLevels;
          pDesc->Texture1DArray.FirstArraySlice = 0;
          pDesc->Texture1DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D10_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D10Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.SampleDesc.Count == 1) {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MostDetailedMip = 0;
            pDesc->Texture2D.MipLevels       = resourceDesc.MipLevels;
          } else {
            pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MostDetailedMip = 0;
            pDesc->Texture2DArray.MipLevels       = resourceDesc.MipLevels;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D10_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D10Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        pDesc->ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
        pDesc->Texture3D.MostDetailedMip = 0;
        pDesc->Texture3D.MipLevels       = resourceDesc.MipLevels;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D10: Unsupported dimension for shader resource view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D10ShaderResourceView::NormalizeDesc(
          ID3D10Resource*                   pResource,
          D3D10_SHADER_RESOURCE_VIEW_DESC1*  pDesc) {
    D3D10_RESOURCE_DIMENSION resourceDim = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t mipLevels = 0;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D10_RESOURCE_DIMENSION_BUFFER: {
        if (pDesc->ViewDimension != D3D10_SRV_DIMENSION_BUFFER) {
          Logger::err("D3D10: Incompatible view dimension for Buffer");
          return E_INVALIDARG;
        }
      } break;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D10_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D10Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D10: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D10_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D10Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY
         && pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURECUBE
         && pDesc->ViewDimension != D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY) {
          Logger::err("D3D10: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D10_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D10Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D10_SRV_DIMENSION_TEXTURE3D) {
          Logger::err("D3D10: Incompatible view dimension for Texture3D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = 1;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D10_SRV_DIMENSION_TEXTURE1D:
        if (pDesc->Texture1D.MipLevels > mipLevels - pDesc->Texture1D.MostDetailedMip)
          pDesc->Texture1D.MipLevels = mipLevels - pDesc->Texture1D.MostDetailedMip;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.MipLevels > mipLevels - pDesc->Texture1DArray.MostDetailedMip)
          pDesc->Texture1DArray.MipLevels = mipLevels - pDesc->Texture1DArray.MostDetailedMip;
        if (pDesc->Texture1DArray.ArraySize > numLayers - pDesc->Texture1DArray.FirstArraySlice)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURE2D:
        if (pDesc->Texture2D.MipLevels > mipLevels - pDesc->Texture2D.MostDetailedMip)
          pDesc->Texture2D.MipLevels = mipLevels - pDesc->Texture2D.MostDetailedMip;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.MipLevels > mipLevels - pDesc->Texture2DArray.MostDetailedMip)
          pDesc->Texture2DArray.MipLevels = mipLevels - pDesc->Texture2DArray.MostDetailedMip;
        if (pDesc->Texture2DArray.ArraySize > numLayers - pDesc->Texture2DArray.FirstArraySlice)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize > numLayers - pDesc->Texture2DMSArray.FirstArraySlice)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURECUBE:
        if (pDesc->TextureCube.MipLevels > mipLevels - pDesc->TextureCube.MostDetailedMip)
          pDesc->TextureCube.MipLevels = mipLevels - pDesc->TextureCube.MostDetailedMip;
        break;
      
      case D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY:
        if (pDesc->TextureCubeArray.MipLevels > mipLevels - pDesc->TextureCubeArray.MostDetailedMip)
          pDesc->TextureCubeArray.MipLevels = mipLevels - pDesc->TextureCubeArray.MostDetailedMip;
        if (pDesc->TextureCubeArray.NumCubes > (numLayers - pDesc->TextureCubeArray.First2DArrayFace) / 6)
          pDesc->TextureCubeArray.NumCubes = (numLayers - pDesc->TextureCubeArray.First2DArrayFace) / 6;
        break;
      
      case D3D10_SRV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.MipLevels > mipLevels - pDesc->Texture3D.MostDetailedMip)
          pDesc->Texture3D.MipLevels = mipLevels - pDesc->Texture3D.MostDetailedMip;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
}
