#include "transcoder.h"
//#include "transcode_utils.h"


Encoder::Encoder() : m_bIsVPP(false), 
                    m_hdl(NULL),
                    m_pBSProcessor(NULL),
                    m_AsyncDepth(4)
{
 
    MSDK_ZERO_MEMORY(m_mfxEncParams); 
    MSDK_ZERO_MEMORY(m_mfxVPPParams);
    MSDK_ZERO_MEMORY(m_EncOpaqueAlloc);
    MSDK_ZERO_MEMORY(m_VPPOpaqueAlloc);
     

}
Encoder::~Encoder()
{
      if(m_pmfxENC.get())
          m_pmfxENC.reset();
      
      if(m_pmfxVPP.get())
          m_pmfxVPP.reset();
      
      if(m_pBSStore.get())
          m_pBSStore.reset(); 
          
      
 
}

 mfxStatus Encoder::Init(sInputParams* pParams,void* hdl, SafetySurfaceBuffer  *ExtBuffer )
 {
    mfxStatus sts; 
    mfxVersion ver = {{0,1}};
    
    MSDK_CHECK_POINTER(ExtBuffer, MFX_ERR_NULL_PTR);
    m_pBuffer = ExtBuffer;
      
    
 
    m_BitstreamWriter = new FileBitstreamProcessor();
    if(!m_BitstreamWriter)  
        return MFX_ERR_NULL_PTR;
    
    sts = m_BitstreamWriter->Init(NULL,pParams->strDstFile);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    
    //create session
    m_pmfxEncSession.reset(new MFXVideoSession);
    sts = m_pmfxEncSession->Init(MFX_IMPL_HARDWARE,&ver);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    
    mfxHandleType handleType = (mfxHandleType)0; 
    handleType = MFX_HANDLE_VA_DISPLAY; 
    sts = m_pmfxEncSession->SetHandle(handleType, hdl);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    m_hdl = hdl; 
    
    m_AsyncDepth = 4;
    
    m_pBSStore.reset( new ExtendedBSStore(m_AsyncDepth));
      
      
      
      
      
      
      
    
    if((m_Decoder->m_mfxDecParams.mfx.FrameInfo.CropW != pParams->nDstWidth && pParams->nDstWidth) ||
          (m_Decoder->m_mfxDecParams.mfx.FrameInfo.CropH != pParams->nDstHeight && pParams->nDstHeight))  //no deinterlacing for now 
        {
            m_bIsVPP = true;   //Init VPP...
            
            m_pmfxVPP.reset(new MFXVideoVPP(*m_pmfxEncSession.get()));
            memset(&m_mfxVPPParams, 0, sizeof(m_mfxVPPParams));
            
            
            //In
           MSDK_MEMCPY_VAR(m_mfxVPPParams.vpp.In, &(m_Decoder->m_mfxDecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));   //CHECK - May need the info from the parent pipeline
            
            m_mfxVPPParams.AsyncDepth = m_AsyncDepth; 
            m_mfxVPPParams.vpp.In.FourCC = MFX_FOURCC_NV12; 
            m_mfxVPPParams.vpp.In.ChromaFormat  = MFX_CHROMAFORMAT_YUV420; 
            m_mfxVPPParams.vpp.In.CropX = 0;
            m_mfxVPPParams.vpp.In.CropY = 0; 
            m_mfxVPPParams.vpp.In.CropW = m_Decoder->m_mfxDecParams.mfx.FrameInfo.CropW;
            m_mfxVPPParams.vpp.In.CropH = m_Decoder->m_mfxDecParams.mfx.FrameInfo.CropH; 
            m_mfxVPPParams.vpp.In.FrameRateExtN = 30; 
            m_mfxVPPParams.vpp.In.FrameRateExtD = 1; 
            //width must be multiple of 16
            
            m_mfxVPPParams.vpp.In.Width = MSDK_ALIGN16(m_mfxVPPParams.vpp.In.Width);
            m_mfxVPPParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVPPParams.vpp.In.PicStruct) ? 
                MSDK_ALIGN16(m_mfxVPPParams.vpp.In.CropH) : MSDK_ALIGN32(m_mfxVPPParams.vpp.In.CropH);
            
            //Out 
            
            MSDK_MEMCPY_VAR(m_mfxVPPParams.vpp.Out, &m_mfxVPPParams.vpp.In, sizeof(mfxFrameInfo));
            
            m_mfxVPPParams.vpp.Out.FourCC = MFX_FOURCC_NV12; 
            m_mfxVPPParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420; 
            
            //scaling 
            if(pParams->nDstWidth)
            {
                m_mfxVPPParams.vpp.Out.CropW = pParams->nDstWidth;
                m_mfxVPPParams.vpp.Out.Width = MSDK_ALIGN16(pParams->nDstWidth);    
            }
            
            if(pParams->nDstHeight)
            {
                m_mfxVPPParams.vpp.Out.CropH = pParams->nDstHeight;
                m_mfxVPPParams.vpp.Out.Height = MSDK_ALIGN32(pParams->nDstHeight);
            }
                
            m_mfxVPPParams.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
        }
        
        // Setup Encode 
        
        m_pmfxENC.reset(new MFXVideoENCODE(*m_pmfxEncSession.get()));
        memset(&m_mfxEncParams, 0, sizeof(m_mfxEncParams)); 
        
        m_mfxEncParams.mfx.CodecId = MFX_CODEC_AVC; //pParams->EncodeId;
        m_mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED; //pParams->nTargetUsage;
        m_mfxEncParams.AsyncDepth = m_AsyncDepth;
      //  m_mfxEncParams.mfx.GopPicSize = (mfxU16)pParams->dFrameRate *2;
        m_mfxEncParams.mfx.RateControlMethod = (mfxU16)MFX_RATECONTROL_VBR;
        
      //  m_mfxEncParams.mfx.NumSlice =  0; 
        if(m_bIsVPP)
        {
            MSDK_MEMCPY_VAR( m_mfxEncParams.mfx.FrameInfo, &m_mfxVPPParams.vpp.Out, sizeof(mfxFrameInfo));
        }
        else
        {
            MSDK_MEMCPY_VAR( m_mfxEncParams.mfx.FrameInfo, &(m_Decoder->m_mfxDecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));   //Check - may need decparms from parent 
        }
        
        if(pParams->dFrameRate)
        {
            ConvertFrameRate(pParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
        }
        
        //bitrate
        
        if(pParams->nBitRate == 0)
        {
            pParams->nBitRate = CalculateDefaultBitrate(pParams->EncodeId, pParams->nTargetUsage, m_mfxEncParams.mfx.FrameInfo.Width, m_mfxEncParams.mfx.FrameInfo.Height,
                                                        1.0 * m_mfxEncParams.mfx.FrameInfo.FrameRateExtN / m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
            
            m_mfxEncParams.mfx.TargetKbps = (mfxU16)(pParams->nBitRate);
            
        }
            
        //memory pattern 
        
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY; 
        
        //Join 
        
        sts = m_Decoder->Join(m_pmfxEncSession.get());
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
        
        //Memory Allocation 
        mfxFrameAllocRequest EncRequest;
        mfxFrameAllocRequest VPPRequest[2]; 
       
        MSDK_ZERO_MEMORY(EncRequest);
        MSDK_ZERO_MEMORY(VPPRequest);
        
        if(m_bIsVPP)
        {
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxVPPParams, VPPRequest);
            MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);                       
        }
       
        sts = m_pmfxENC.get()->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
        mfxU16 nSurfNumVPPEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested + m_mfxVPPParams.AsyncDepth;
        
        mfxFrameSurface1** pSurface = new mfxFrameSurface1 *[nSurfNumVPPEnc];
        MSDK_CHECK_POINTER(pSurface, MFX_ERR_MEMORY_ALLOC);
        for(int i= 0; i < nSurfNumVPPEnc; i++)
        {
            pSurface[i] = new mfxFrameSurface1; 
            MSDK_CHECK_POINTER(pSurface[i], MFX_ERR_MEMORY_ALLOC);
            memset(pSurface[i],0,sizeof(mfxFrameSurface1));
            memcpy(&(pSurface[i]->Info), &(EncRequest.Info), sizeof(mfxFrameInfo));
           // pSurface[i]->Data.MemId = 0;  // opaque 
            m_pSurfaceEncPool.push_back(pSurface[i]);
            
        }
        
        
       
         m_EncOpaqueAlloc.Header.BufferId  =  MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION; 
         m_EncOpaqueAlloc.Header.BufferSz =   sizeof(mfxExtOpaqueSurfaceAlloc);
    
         m_EncOpaqueAlloc.In.Surfaces = &m_pSurfaceEncPool[0]; 
         m_EncOpaqueAlloc.In.NumSurface = (mfxU16)m_pSurfaceEncPool.size(); 
         m_EncOpaqueAlloc.In.Type = EncRequest.Type; 
    
         if(m_bIsVPP) 
         {
            //m_EncOpaqueAlloc.In = m_VPPOpaqueAlloc.Out; 
            m_VPPOpaqueAlloc.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION; 
            m_VPPOpaqueAlloc.Header.BufferSz = sizeof(mfxExtOpaqueSurfaceAlloc);
            m_VPPOpaqueAlloc.In = m_Decoder->m_DecOpaqueAlloc.Out; 
            m_VPPOpaqueAlloc.Out = m_EncOpaqueAlloc.In; 
            
            m_VPPExtParams.push_back((mfxExtBuffer *)&m_VPPOpaqueAlloc);
            m_mfxVPPParams.ExtParam = &m_VPPExtParams[0];
            m_mfxVPPParams.NumExtParam = m_VPPExtParams.size(); 
         }
        else 
        {
             m_EncOpaqueAlloc.In = m_Decoder->m_DecOpaqueAlloc.Out; 
        }
         
        m_EncExtParams.push_back((mfxExtBuffer *)&m_EncOpaqueAlloc); 
        m_mfxEncParams.ExtParam = &m_EncExtParams[0];
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size(); 
        
       
   
        sts = m_pmfxENC->Init(&m_mfxEncParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts); 
        
        sts = m_pmfxVPP->Init(&m_mfxVPPParams);
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
   
        return sts;   
 }


 
 mfxStatus Encoder::Run()
 {
  mfxStatus sts = MFX_ERR_NONE; 
    ExtendedSurface DecExtSurface={0}; 
    ExtendedSurface VPPExtSurface={0}; 
    ExtendedBS *pBS = NULL; 
    bool isQuit = false; 
    
    
    time_t start = time(0);
    while(MFX_ERR_NONE == sts || MFX_ERR_MORE_DATA == sts)
    {
        
        while(MFX_ERR_MORE_SURFACE == m_pBuffer->GetSurface(DecExtSurface) && !isQuit)
            MSDK_SLEEP(1);
        
        if(NULL == DecExtSurface.pSurface)
            isQuit = true; 
        
        if(m_pmfxVPP.get())
        {
            sts = VPPOneFrame(&DecExtSurface, &VPPExtSurface);
            VPPExtSurface.pCtrl = DecExtSurface.pCtrl;
        }
        else
        {
            VPPExtSurface.pSurface = DecExtSurface.pSurface; 
            VPPExtSurface.pCtrl = DecExtSurface.pCtrl;
            VPPExtSurface.Syncp = DecExtSurface.Syncp; 
        }
        
        if(MFX_ERR_MORE_DATA == sts)
        {
            if(isQuit)
            {
                VPPExtSurface.pSurface = NULL; 
                sts = MFX_ERR_NONE; 
            }
            else
            {
                m_pBuffer->ReleaseSurface(DecExtSurface.pSurface);
                continue;
            }            
        }
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
        pBS = m_pBSStore->GetNext(); 
        if(!pBS)
            return MFX_ERR_NOT_FOUND; 
        
        m_BSPool.push_back(pBS);
        
        sts = EncodeOneFrame(&VPPExtSurface, &m_BSPool.back()->Bitstream);
        m_pBuffer->ReleaseSurface(DecExtSurface.pSurface);
        
        if(MFX_ERR_MORE_DATA == sts) 
        {
            m_BSPool.pop_back(); 
            m_pBSStore->Release(pBS);
            if(NULL == VPPExtSurface.pSurface)
            {
                break;
            }
            else
            {
                sts = MFX_ERR_NONE; 
                continue; 
            }
        }
        
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
        m_BSPool.back()->Syncp = VPPExtSurface.Syncp;
        m_BSPool.back()->pCtrl = VPPExtSurface.pCtrl; 
        if(m_BSPool.size() == m_AsyncDepth)
        {
            sts = PutBS(); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else
        {
            continue;
        }
        
    }
    
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    
    if(MFX_ERR_NONE == sts)
    {
        while(m_BSPool.size())
        {
            sts = PutBS(); 
            MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        }
    }
    if(MFX_ERR_NONE == sts)
    {
        sts = MFX_WRN_VALUE_NOT_CHANGED; 
    }
    
    return sts; 
 }
 

mfxStatus Encoder::EncodeOneFrame(ExtendedSurface *pExtSurface, mfxBitstream *pBS)
 {
    mfxStatus sts = MFX_ERR_NONE; 
    mfxEncodeCtrl *pCtrl = (pExtSurface->pCtrl) ? &pExtSurface->pCtrl->encCtrl : NULL; 
    
    for(;;)
    {
        sts = m_pmfxENC->EncodeFrameAsync(pCtrl,pExtSurface->pSurface,pBS,&pExtSurface->Syncp);
        if(MFX_ERR_NONE < sts && !pExtSurface->Syncp)
        {
            if(MFX_WRN_DEVICE_BUSY == sts)
                MSDK_SLEEP(1);
        }
        else if ( MFX_ERR_NONE  < sts && pExtSurface->Syncp)
        {
            sts = MFX_ERR_NONE; 
            break;
        }
        else if ( MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer(pBS);
            MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        }
        else
        {
            break;
        }
    }
    return sts;
 }
 
mfxStatus Encoder::VPPOneFrame(ExtendedSurface *pSurfaceIn, ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);
    mfxFrameSurface1 *pmfxSurface = NULL; 
    
    for(mfxU32 i = 0; i < MSDK_WAIT_INTERVAL; i++)
    {
        pmfxSurface = GetFreeSurface();
        if(pmfxSurface)
            break; 
        else
        {
            MSDK_SLEEP(1);
        }   
    }
    
    MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC);
    
    pmfxSurface->Info.PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct; 
    pExtSurface->pSurface = pmfxSurface; 
    
    mfxStatus sts = MFX_ERR_NONE; 
    for(;;)
    {
        sts = m_pmfxVPP->RunFrameVPPAsync(pSurfaceIn->pSurface, pmfxSurface, NULL,&pExtSurface->Syncp);
        if(MFX_ERR_NONE < sts && !pExtSurface->Syncp)
        {
            if(MFX_WRN_DEVICE_BUSY == sts) 
                MSDK_SLEEP(1); 
        }
       else if(MFX_ERR_NONE < sts && pExtSurface->Syncp)
            {
                sts = MFX_ERR_NONE; 
                break;
            }
       else
            {
                break;
            }            
    }
    return sts;
}


 mfxFrameSurface1* Encoder::GetFreeSurface()
 {
    SurfPointersArray *pArray; 
 
    pArray = &m_pSurfaceEncPool;
    
    for(mfxU32 i = 0;  i < pArray->size(); i++)
    {
        if(!(*pArray)[i]->Data.Locked)
            return((*pArray)[i]);
        
    }
    return NULL;    
 }
 
 mfxStatus Encoder::PutBS()
 {
    mfxStatus sts = MFX_ERR_NONE; 
    ExtendedBS *pBitstreamEx = m_BSPool.front(); 
    
    sts = m_pmfxEncSession->SyncOperation(pBitstreamEx->Syncp, MSDK_WAIT_INTERVAL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    sts = m_BitstreamWriter->ProcessOutputBitstream(&pBitstreamEx->Bitstream);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    
    pBitstreamEx->Bitstream.DataLength = 0;
    pBitstreamEx->Bitstream.DataOffset = 0;
    
    m_BSPool.pop_front();
    m_pBSStore->Release(pBitstreamEx);
    return sts; 
 }
 
mfxStatus Encoder::AllocateSufficientBuffer(mfxBitstream *pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    
    mfxVideoParam par; 
    MSDK_ZERO_MEMORY(par); 
    
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    mfxU32 new_size = 0; 
    
    if(0 != par.mfx.BufferSizeInKB)
    {
        new_size = par.mfx.BufferSizeInKB * 1000; 
        
    }
    else
    {
        new_size = (0 == pBS->MaxLength) ? 4 + (par.mfx.FrameInfo.Width  * par.mfx.FrameInfo.Height * 3 + 1023) : 2 * pBS->MaxLength; 
    }
    
    sts = ExtendMfxBitstream(pBS, new_size);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE,sts, WipeMfxBitstream(pBS));
    
    return MFX_ERR_NONE;    
}