#include "d3d10_1_device.h"
#include "d3d10_1_buffer.h"
#include "d3d10_1_texture.h"
#include "d3d10_1_view_dsv.h"

namespace dxvk {
  
  D3D10DepthStencilView::D3D10DepthStencilView(
          D3D10Device*                      device,
          ID3D10Resource*                   resource,
    const D3D10_DEPTH_STENCIL_VIEW_DESC&    desc,
    const Rc<DxvkImageView>&                view)
  : m_device(device), m_resource(resource),
    m_desc(desc), m_view(view) { }
  
  
  D3D10DepthStencilView::~D3D10DepthStencilView() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D10DepthStencilView::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10View)
     || riid == __uuidof(ID3D10DepthStencilView)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D10DepthStencilView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D10DepthStencilView::GetDevice(ID3D10Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D10DepthStencilView::GetResource(ID3D10Resource** ppResource) {
    *ppResource = m_resource.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D10DepthStencilView::GetDesc(D3D10_DEPTH_STENCIL_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D10DepthStencilView::GetDescFromResource(
          ID3D10Resource*                   pResource,
          D3D10_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D10_RESOURCE_DIMENSION resourceDim = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D10_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D10_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D10Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MipSlice        = 0;
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
            pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MipSlice = 0;
          } else {
            pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MipSlice        = 0;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
        
      default:
        Logger::err(str::format(
          "D3D10: Unsupported dimension for depth stencil view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D10DepthStencilView::NormalizeDesc(
          ID3D10Resource*                   pResource,
          D3D10_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D10_RESOURCE_DIMENSION resourceDim = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D10_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D10_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D10Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D10: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D10_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D10_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D10Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY) {
          Logger::err("D3D10: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D10_DSV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize > numLayers - pDesc->Texture1DArray.FirstArraySlice)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize > numLayers - pDesc->Texture2DArray.FirstArraySlice)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize > numLayers - pDesc->Texture2DMSArray.FirstArraySlice)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
}
