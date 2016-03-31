/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"
 

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#include <d3d9.h>
#include "d3d_allocator.h"
#include "d3d11_allocator.h"
#else
#include <stdarg.h>
#include "vaapi_allocator.h"
#endif

#include "sysmem_allocator.h"
//#include "transcode_utils.h"
#include "transcoder.h"

 

// parsing defines
#define IS_SEPARATOR(ch)  ((ch) <= ' ' || (ch) == '=')
#define VAL_CHECK(val, argIdx, argName) \
{ \
    if (val) \
    { \
        PrintHelp(NULL, MSDK_STRING("Input argument number %d \"%s\" require more parameters"), argIdx, argName); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}

#define SIZE_CHECK(cond) \
{ \
    if (cond) \
    { \
        PrintHelp(NULL, MSDK_STRING("Buffer is too small")); \
        return MFX_ERR_UNSUPPORTED; \
    } \
}

msdk_tick GetTick()
{
    return msdk_time_get_tick();
}

mfxF64 GetTime(msdk_tick start)
{
    static msdk_tick frequency = msdk_time_get_frequency();

    return MSDK_GET_TIME(msdk_time_get_tick(), start, frequency);
}

void PrintHelp(const msdk_char *strAppName, const msdk_char *strErrorMessage, ...)
{
    msdk_printf(MSDK_STRING("Multi Transcoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        va_list args;
        msdk_printf(MSDK_STRING("ERROR: "));
        va_start(args, strErrorMessage);
        msdk_vprintf(strErrorMessage, args);
        va_end(args);
        msdk_printf(MSDK_STRING("\n\n"));
    }

    msdk_printf(MSDK_STRING("Command line parameters\n"));

    if (!strAppName) strAppName = MSDK_STRING("sample_multi_transcode");
    msdk_printf(MSDK_STRING("Usage: %s [options] [--] pipeline-description\n"), strAppName);
    msdk_printf(MSDK_STRING("   or: %s [options] -par ParFile\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Options:\n"));
    //                     ("  ............xx
    msdk_printf(MSDK_STRING("  -?            Print this help and exit\n"));
    msdk_printf(MSDK_STRING("  -p <file-name>\n"));
    msdk_printf(MSDK_STRING("                Collect performance statistics in specified file\n"));
    msdk_printf(MSDK_STRING("  -timeout <seconds>\n"));
    msdk_printf(MSDK_STRING("                Set time to run transcoding in seconds\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (general options):\n"));
    msdk_printf(MSDK_STRING("  -i::h265|h264|mpeg2|vc1|mvc|jpeg|vp8 <file-name>\n"));
    msdk_printf(MSDK_STRING("                Set input file and decoder type\n"));
    msdk_printf(MSDK_STRING("  -o::h265|h264|mpeg2|mvc|jpeg|vp8 <file-name>\n"));
    msdk_printf(MSDK_STRING("                Set output file and encoder type\n"));
    msdk_printf(MSDK_STRING("  -sw|-hw|-hw_d3d11\n"));
    msdk_printf(MSDK_STRING("                SDK implementation to use: \n"));
    msdk_printf(MSDK_STRING("                      -hw - platform-specific on default display adapter (default)\n"));
    msdk_printf(MSDK_STRING("                      -hw_d3d11 - platform-specific via d3d11\n"));
    msdk_printf(MSDK_STRING("                      -sw - software\n"));
    msdk_printf(MSDK_STRING("  -async        Depth of asynchronous pipeline. default value 1\n"));
    msdk_printf(MSDK_STRING("  -join         Join session with other session(s), by default sessions are not joined\n"));
    msdk_printf(MSDK_STRING("  -priority     Use priority for join sessions. 0 - Low, 1 - Normal, 2 - High. Normal by default\n"));
    msdk_printf(MSDK_STRING("  -n            Number of frames to transcode \n"));
    msdk_printf(MSDK_STRING("  -fps <frames per second>\n"));
    msdk_printf(MSDK_STRING("                Transcoding frame rate limit\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (encoding options):\n"));
    msdk_printf(MSDK_STRING("  -b <Kbits per second>\n"));
    msdk_printf(MSDK_STRING("                Encoded bit rate, valid for H.264, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("  -f <frames per second>\n"));
    msdk_printf(MSDK_STRING("                Video frame rate for the whole pipelin, overwrites input stream's framerate is taken\n"));
    msdk_printf(MSDK_STRING("  -u 1|4|7      Target usage: quality (1), balanced (4) or speed (7); valid for H.264, MPEG2 and MVC encoders. Default is balanced\n"));
    msdk_printf(MSDK_STRING("  -q <quality>  Quality parameter for JPEG encoder; in range [1,100], 100 is the best quality\n"));
    msdk_printf(MSDK_STRING("  -l numSlices  Number of slices for encoder; default value 0 \n"));
    msdk_printf(MSDK_STRING("  -mss maxSliceSize \n"));
    msdk_printf(MSDK_STRING("                Maximum slice size in bytes. Supported only with -hw and h264 codec. This option is not compatible with -l option.\n"));
    msdk_printf(MSDK_STRING("  -la           Use the look ahead bitrate control algorithm (LA BRC) for H.264 encoder. Supported only with -hw option on 4th Generation Intel Core processors. \n"));
    msdk_printf(MSDK_STRING("  -lad depth    Depth parameter for the LA BRC, the number of frames to be analyzed before encoding. In range [10,100]. \n"));
    msdk_printf(MSDK_STRING("                May be 1 in the case when -mss option is specified \n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (vpp options):\n"));
    msdk_printf(MSDK_STRING("  -deinterlace  Forces VPP to deinterlace input stream\n"));
    msdk_printf(MSDK_STRING("  -angle 180    Enables 180 degrees picture rotation user module before encoding\n"));
    msdk_printf(MSDK_STRING("  -opencl       Uses implementation of rotation plugin (enabled with -angle option) through Intel(R) OpenCL\n"));
    msdk_printf(MSDK_STRING("  -w            Destination picture width, invokes VPP resize\n"));
    msdk_printf(MSDK_STRING("  -h            Destination picture height, invokes VPP resize\n"));
    msdk_printf(MSDK_STRING("  -pe           Set encoding plugin for this particular session. This setting overrides plugin settings defined by SET clause.\n"));
    msdk_printf(MSDK_STRING("  -pd           Set decoding plugin for this particular session. This setting overrides plugin settings defined by SET clause.\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("ParFile format:\n"));
    msdk_printf(MSDK_STRING("  ParFile is extension of what can be achieved by setting pipeline in the command\n"));
    msdk_printf(MSDK_STRING("line. For more information on ParFile format see readme-multi-transcode.pdf\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Examples:\n"));
    msdk_printf(MSDK_STRING("  %s -i::mpeg2 in.mpeg2 -o::h264 out.h264\n"), strAppName);
    msdk_printf(MSDK_STRING("  %s -i::mvc in.mvc -o::mvc out.mvc -w 320 -h 240\n"), strAppName);
}

void PrintInfo(mfxU32 session_number, sInputParams* pParams, mfxVersion *pVer)
{
    msdk_char buf[2048];
    MSDK_CHECK_POINTER_NO_RET(pVer);

    if ((MFX_IMPL_AUTO <= pParams->libType) && (MFX_IMPL_HARDWARE4 >= pParams->libType))
    {
        msdk_printf(MSDK_STRING("MFX %s Session %d API ver %d.%d parameters: \n"),
            (MFX_IMPL_SOFTWARE == pParams->libType)? MSDK_STRING("SOFTWARE") : MSDK_STRING("HARDWARE"),
                 session_number,
                 pVer->Major,
                 pVer->Minor);
        
    }

    if (0 == pParams->DecodeId)
        msdk_printf(MSDK_STRING("Input  video: From parent session\n"));
    else
        msdk_printf(MSDK_STRING("Input  video: %s\n"), CodecIdToStr(pParams->DecodeId).c_str());

    // means that source is parent session
    if (0 == pParams->EncodeId)
        msdk_printf(MSDK_STRING("Output video: To child session\n"));
    else
        msdk_printf(MSDK_STRING("Output video: %s\n"), CodecIdToStr(pParams->EncodeId).c_str());
 
    msdk_printf(MSDK_STRING("\n"));
}


CmdProcessor::CmdProcessor()
{
    m_SessionParamId = 0;
    m_SessionArray.clear();
    m_decoderPlugins.clear();
    m_encoderPlugins.clear();
    m_PerfFILE = NULL;
    m_parName = NULL;
    m_nTimeout = 0;

} //CmdProcessor::CmdProcessor()

CmdProcessor::~CmdProcessor()
{
    m_SessionArray.clear();
    m_decoderPlugins.clear();
    m_encoderPlugins.clear();
    if (m_PerfFILE)
        fclose(m_PerfFILE);

} //CmdProcessor::~CmdProcessor()

void CmdProcessor::PrintParFileName()
{
    if (m_parName && m_PerfFILE)
    {
        msdk_fprintf(m_PerfFILE, MSDK_STRING("Input par file: %s\n\n"), m_parName);
    }
}

mfxStatus CmdProcessor::ParseCmdLine(int argc, msdk_char *argv[])
{
    FILE *parFile = NULL;
    mfxStatus sts = MFX_ERR_UNSUPPORTED;

    if (1 == argc)
    {
       PrintHelp(argv[0], NULL);
       return MFX_ERR_UNSUPPORTED;
    }

    --argc;
    ++argv;

    while (argv[0])
    {
        if (0 == msdk_strcmp(argv[0], MSDK_STRING("-par")))
        {
            if (parFile)
            {
                msdk_printf(MSDK_STRING("error: too many parfiles\n"));
                return MFX_ERR_UNSUPPORTED;
            }
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-par' option\n"));
            }
            m_parName = argv[0];
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-timeout")))
        {
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-timeout' option\n"));
            }
            if (MFX_ERR_NONE != msdk_opt_read(argv[0], m_nTimeout))
            {
                msdk_printf(MSDK_STRING("error: -timeout \"%s\" is invalid"), argv[0]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-?")) )
        {
            PrintHelp(argv[0], NULL);
            return MFX_ERR_UNSUPPORTED;
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-p")))
        {
            if (m_PerfFILE)
            {
                msdk_printf(MSDK_STRING("error: only one performance file is supported"));
                return MFX_ERR_UNSUPPORTED;
            }
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-p' option\n"));
            }
            MSDK_FOPEN(m_PerfFILE, argv[0], MSDK_STRING("w"));
            if (NULL == m_PerfFILE)
            {
                msdk_printf(MSDK_STRING("error: performance file \"%s\" not found"), argv[0]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("--")))
        {
            // just skip separator "--" which delimits cmd options and pipeline settings
            break;
        }
        else
        {
            break;
        }
        --argc;
        ++argv;
    }

    msdk_printf(MSDK_STRING("Multi Transcoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    //Read pipeline from par file
    if (m_parName && !argv[0])
    {
        MSDK_FOPEN(parFile, m_parName, MSDK_STRING("r"));
        if (NULL == parFile)
        {
            msdk_printf(MSDK_STRING("error: ParFile \"%s\" not found\n"), m_parName);
            return MFX_ERR_UNSUPPORTED;
        }

        if (NULL != m_parName) msdk_printf(MSDK_STRING("Par file is: %s\n\n"), m_parName);

        sts = ParseParFile(parFile);

        if (MFX_ERR_NONE != sts)
        {
            PrintHelp(argv[0], MSDK_STRING("Command line in par file is invalid"));
            fclose(parFile);
            return sts;
        }

        fclose(parFile);
    }
    //Read pipeline from cmd line
    else if (!argv[0])
    {
        msdk_printf(MSDK_STRING ("error: pipeline description not found\n"));
        return MFX_ERR_UNSUPPORTED;
    }
    else if (argv[0] && m_parName)
    {
        msdk_printf(MSDK_STRING ("error: simultaneously enabling parfile and description pipeline from command line forbidden\n"));
        return MFX_ERR_UNSUPPORTED;
    }
    else
    {
        sts = ParseParamsForOneSession(argc, argv);
        if (MFX_ERR_NONE != sts)
        {
            msdk_printf(MSDK_STRING("error: pipeline description is invalid\n"));
            return sts;
        }
    }

    return sts;

} //mfxStatus CmdProcessor::ParseCmdLine(int argc, msdk_char *argv[])

mfxStatus CmdProcessor::ParseParFile(FILE *parFile)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED;
    if (!parFile)
        return MFX_ERR_UNSUPPORTED;

    mfxU32 currPos = 0;
    mfxU32 lineIndex = 0;

    // calculate file size
    fseek(parFile, 0, SEEK_END);
    mfxU32 fileSize = ftell(parFile) + 1;
    fseek(parFile, 0, SEEK_SET);

    // allocate buffer for parsing
    s_ptr<msdk_char, false> parBuf;
    parBuf.reset(new msdk_char[fileSize]);
    msdk_char *pCur;

    while(currPos < fileSize)
    {
        pCur = /*_fgetts*/msdk_fgets(parBuf.get(), fileSize, parFile);
        if (!pCur)
            return MFX_ERR_NONE;
        while(pCur[currPos] != '\n' && pCur[currPos] != 0)
        {
            currPos++;
            if  (pCur + currPos >= parBuf.get() + fileSize)
                return sts;
        }
        // zero string
        if (!currPos)
            continue;

        sts = TokenizeLine(pCur, currPos);
        if (MFX_ERR_NONE != sts)
            PrintHelp(NULL, MSDK_STRING("Error in par file parameters at line %d"), lineIndex);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        currPos = 0;
        lineIndex++;
    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::ParseParFile(FILE *parFile)

mfxStatus CmdProcessor::TokenizeLine(msdk_char *pLine, mfxU32 length)
{
    mfxU32 i;
    const mfxU8 maxArgNum = 255;
    msdk_char *argv[maxArgNum+1];
    mfxU32 argc = 0;
    s_ptr<msdk_char, false> pMemLine;

    pMemLine.reset(new msdk_char[length+2]);

    msdk_char *pTempLine = pMemLine.get();
    pTempLine[0] = ' ';
    pTempLine++;

    MSDK_MEMCPY_BUF(pTempLine,0 , length*sizeof(msdk_char), pLine, length*sizeof(msdk_char));

    // parse into command streams
    for (i = 0; i < length ; i++)
    {
        // check if separator
        if (IS_SEPARATOR(pTempLine[-1]) && !IS_SEPARATOR(pTempLine[0]))
        {
            argv[argc++] = pTempLine;
            if (argc > maxArgNum)
            {
                PrintHelp(NULL, MSDK_STRING("Too many parameters (reached maximum of %d)"), maxArgNum);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        if (pTempLine[0] == ' ')
        {
            pTempLine[0] = 0;
        }
        pTempLine++;
    }

    // EOL for last parameter
    pTempLine[0] = 0;

    return ParseParamsForOneSession(argc, argv);
}

mfxStatus CmdProcessor::ParseParamsForOneSession(mfxU32 argc, msdk_char *argv[])
{
    mfxStatus sts;
    mfxU32 i;
    mfxU32 skipped = 0;
    sInputParams InputParams;
    if (m_nTimeout)
        InputParams.nTimeout = m_nTimeout;

    if (0 == msdk_strcmp(argv[0], MSDK_STRING("set")))
    {
        if (argc != 3) {
            msdk_printf(MSDK_STRING("error: number of arguments for 'set' options is wrong"));
            return MFX_ERR_UNSUPPORTED;
        }
        sts = ParseOption__set(argv[1], argv[2]);
        return sts;
    }
    // default implementation
    InputParams.libType = MFX_IMPL_HARDWARE_ANY;

    for (i = 0; i < argc; i++)
    {
        // process multi-character options
        if ( (0 == msdk_strncmp(MSDK_STRING("-i::"), argv[i], msdk_strlen(MSDK_STRING("-i::")))) && (0 != msdk_strncmp(argv[i]+4, MSDK_STRING("source"), msdk_strlen(MSDK_STRING("source")))) )
        {
            sts = StrFormatToCodecFormatFourCC(argv[i]+4, InputParams.DecodeId);
            if (sts != MFX_ERR_NONE)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_opt_read(argv[i], InputParams.strSrcFile);
            if (InputParams.eMode == Source)
            {
                switch(InputParams.DecodeId)
                {
                    case MFX_CODEC_MPEG2:
                    case MFX_CODEC_HEVC:
                    case MFX_CODEC_AVC:
                    case MFX_CODEC_VC1:
//                    case MFX_CODEC_VP8:
                    case CODEC_MVC:
                    case MFX_CODEC_JPEG:
                        return MFX_ERR_UNSUPPORTED;
                }
            }
            if (InputParams.DecodeId == CODEC_MVC)
            {
                InputParams.DecodeId = MFX_CODEC_AVC;
                InputParams.bIsMVC = true;
            }
        }
        else if ( (0 == msdk_strncmp(MSDK_STRING("-o::"), argv[i], msdk_strlen(MSDK_STRING("-o::")))) && (0 != msdk_strncmp(argv[i]+4, MSDK_STRING("sink"), msdk_strlen(MSDK_STRING("sink")))) )
        {
            sts = StrFormatToCodecFormatFourCC(argv[i]+4, InputParams.EncodeId);
            if (sts != MFX_ERR_NONE)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_opt_read(argv[i], InputParams.strDstFile);
            if (InputParams.eMode == Sink || InputParams.bIsMVC)
            {
                switch(InputParams.EncodeId)
                {
                    case MFX_CODEC_MPEG2:
                    case MFX_CODEC_HEVC:
                    case MFX_CODEC_AVC:
                    case MFX_CODEC_JPEG:
//                    case MFX_CODEC_VP8:
                        return MFX_ERR_UNSUPPORTED;
                }
            }
            if (InputParams.EncodeId == CODEC_MVC)
            {
                if (InputParams.eMode == Sink)
                    return MFX_ERR_UNSUPPORTED;

                InputParams.EncodeId = MFX_CODEC_AVC;
                InputParams.bIsMVC = true;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-sw")))
        {
            InputParams.libType = MFX_IMPL_SOFTWARE;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-hw")))
        {
            InputParams.libType = MFX_IMPL_HARDWARE_ANY;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-hw_d3d11")))
        {
            InputParams.libType = MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-perf_opt")))
        {
            InputParams.bIsPerf = true;
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-f")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.dFrameRate))
            {
                PrintHelp(NULL, MSDK_STRING("frameRate \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-fps")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nFPS))
            {
                PrintHelp(NULL, MSDK_STRING("FPS limit \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-b")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nBitRate))
            {
                PrintHelp(NULL, MSDK_STRING("bitRate \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-u")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nTargetUsage))
            {
                PrintHelp(NULL, MSDK_STRING(" \"%s\" target usage is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-q")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nQuality))
            {
                PrintHelp(NULL, MSDK_STRING(" \"%s\" quality is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-w")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nDstWidth))
            {
                PrintHelp(NULL, MSDK_STRING("width \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-h")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nDstHeight))
            {
                PrintHelp(NULL, MSDK_STRING("height \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-l")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nSlices))
            {
                PrintHelp(NULL, MSDK_STRING("numSlices \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-mss")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nMaxSliceSize))
            {
                PrintHelp(NULL, MSDK_STRING("maxSliceSize \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-async")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nAsyncDepth))
            {
                PrintHelp(NULL, MSDK_STRING("async \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-join")))
        {
           InputParams.bIsJoin = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-priority")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.priority))
            {
                PrintHelp(NULL, MSDK_STRING("priority \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::source")))
        {
            if (InputParams.eMode != Native)
                return MFX_ERR_UNSUPPORTED;

            InputParams.eMode = Source;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::sink")))
        {
             if (InputParams.eMode != Native)
                return MFX_ERR_UNSUPPORTED;

            InputParams.eMode = Sink;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-n")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.MaxFrameNumber))
            {
                PrintHelp(NULL, MSDK_STRING("-n \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-angle")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nRotationAngle))
            {
                PrintHelp(NULL, MSDK_STRING("-angle \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
            if (InputParams.strVPPPluginDLLPath[0] == '\0') {
                msdk_opt_read(MSDK_CPU_ROTATE_PLUGIN, InputParams.strVPPPluginDLLPath);
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-timeout")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nTimeout))
            {
                PrintHelp(NULL, MSDK_STRING("-timeout \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
            skipped+=2;
        }

        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-opencl")))
        {
            msdk_opt_read(MSDK_OCL_ROTATE_PLUGIN, InputParams.strVPPPluginDLLPath);
            InputParams.bOpenCL = true;
        }

        // output PicStruct
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-deinterlace")))
        {
            InputParams.bEnableDeinterlacing = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-la_ext")))
        {
            InputParams.bEnableExtLA = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-la")))
        {
            InputParams.bLABRC = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-bpyr")))
        {
            InputParams.bEnableBPyramid = true;
        }

        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-lad")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (MFX_ERR_NONE != msdk_opt_read(argv[i], InputParams.nLADepth))
            {
                PrintHelp(NULL, MSDK_STRING("look ahead depth \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-pe")))
        {
            VAL_CHECK(i + 1 == argc, i, argv[i]);
            InputParams.encoderPluginParams = ParsePluginParameter(argv[i + 1]);
            i++;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-pd")))
        {
            VAL_CHECK(i + 1 == argc, i, argv[i]);
            InputParams.decoderPluginParams = ParsePluginParameter(argv[i + 1]);
            i++;
        }
        else
        {
            PrintHelp(NULL, MSDK_STRING("Invalid input argument number %d \"%s\""), i, argv[i]);
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if (skipped < argc)
    {
        sts = VerifyAndCorrectInputParams(InputParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_SessionArray.push_back(InputParams);
    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::ParseParamsForOneSession(msdk_char *pLine, mfxU32 length)

mfxStatus CmdProcessor::ParseOption__set(msdk_char* strCodecType, msdk_char* strPluginPath)
{
    mfxU32 codecid = 0;
    mfxU32 type = 0;
    sPluginParams pluginParams;

    //Parse codec type - decoder or encoder
    if (0 == msdk_strncmp(MSDK_STRING("-i::"), strCodecType, 4))
    {
        type = MSDK_VDECODE;
    }
    else if (0 == msdk_strncmp(MSDK_STRING("-o::"), strCodecType, 4))
    {
        type = MSDK_VENCODE;
    }
    else
    {
        msdk_printf(MSDK_STRING("error: incorrect definition codec type (must be -i:: or -o::)\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (StrFormatToCodecFormatFourCC(strCodecType+4, codecid) != MFX_ERR_NONE)
    {
        msdk_printf(MSDK_STRING("error: codec is unknown\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (!IsPluginCodecSupported(codecid))
    {
        msdk_printf(MSDK_STRING("error: codec is unsupported\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    pluginParams = ParsePluginParameter(strPluginPath);

    if (type == MSDK_VDECODE)
        m_decoderPlugins.insert(std::pair<mfxU32, sPluginParams>(codecid, pluginParams));
    else
        m_encoderPlugins.insert(std::pair<mfxU32, sPluginParams>(codecid, pluginParams));

    return MFX_ERR_NONE;
};

sPluginParams CmdProcessor::ParsePluginParameter(msdk_char* strPluginPath)
{
    sPluginParams pluginParams;
    if (MFX_ERR_NONE == ConvertStringToGuid(strPluginPath, pluginParams.pluginGuid))
    {
        pluginParams.type = MFX_PLUGINLOAD_TYPE_GUID;
    }
    else
    {
#if defined(_WIN32) || defined(_WIN64)
        msdk_char wchar[1024];
        msdk_opt_read(strPluginPath, wchar);
        std::wstring wstr(wchar);
        std::string str(wstr.begin(), wstr.end());

        strcpy(pluginParams.strPluginPath, str.c_str());
#else
        msdk_opt_read(strPluginPath, pluginParams.strPluginPath);
#endif
        pluginParams.type = MFX_PLUGINLOAD_TYPE_FILE;
    }

    return pluginParams;
}


mfxStatus CmdProcessor::VerifyAndCorrectInputParams(sInputParams &InputParams)
{
    if (0 == msdk_strlen(InputParams.strSrcFile) && (InputParams.eMode == Sink || InputParams.eMode == Native))
    {
        PrintHelp(NULL, MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == msdk_strlen(InputParams.strDstFile) && (InputParams.eMode == Source || InputParams.eMode == Native))
    {
        PrintHelp(NULL, MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (MFX_CODEC_JPEG != InputParams.EncodeId && MFX_CODEC_MPEG2 != InputParams.EncodeId &&
        MFX_CODEC_AVC != InputParams.EncodeId && MFX_CODEC_HEVC != InputParams.EncodeId &&
      /*  MFX_CODEC_VP8 != InputParams.EncodeId && */ InputParams.eMode != Sink)
    {
        PrintHelp(NULL, MSDK_STRING("Unknown encoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_CODEC_MPEG2 != InputParams.DecodeId &&
       MFX_CODEC_AVC != InputParams.DecodeId &&
       MFX_CODEC_HEVC != InputParams.DecodeId &&
       MFX_CODEC_VC1 != InputParams.DecodeId &&
       MFX_CODEC_JPEG != InputParams.DecodeId &&
     //  MFX_CODEC_VP8 != InputParams.DecodeId &&
       InputParams.eMode != Source)
    {
        PrintHelp(NULL, MSDK_STRING("Unknown decoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.nQuality && InputParams.EncodeId && (MFX_CODEC_JPEG != InputParams.EncodeId))
    {
        PrintHelp(NULL, MSDK_STRING("-q option is supported only for JPEG encoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((InputParams.nTargetUsage || InputParams.nBitRate) && (MFX_CODEC_JPEG == InputParams.EncodeId))
    {
        PrintHelp(NULL, MSDK_STRING("-b and -u options are supported only for H.264, MPEG2 and MVC encoders. For JPEG encoder use -q\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    // valid target usage range is: [MFX_TARGETUSAGE_BEST_QUALITY .. MFX_TARGETUSAGE_BEST_SPEED] (at the moment [1..7])
    if ((InputParams.nTargetUsage < MFX_TARGETUSAGE_BEST_QUALITY) ||
        (InputParams.nTargetUsage > MFX_TARGETUSAGE_BEST_SPEED) )
    {
        if (InputParams.nTargetUsage == MFX_TARGETUSAGE_UNKNOWN)
        {
            // if user did not specified target usage - use balanced
            InputParams.nTargetUsage = MFX_TARGETUSAGE_BALANCED;
        }
        else
        {
            // report error if unsupported target usage was set
            return MFX_ERR_UNSUPPORTED;
        }
    }

    // Ignoring user-defined Async Depth for LA
    if (InputParams.nMaxSliceSize)
    {
        InputParams.nAsyncDepth = 1;
    }

    if (InputParams.bLABRC && !(InputParams.libType & MFX_IMPL_HARDWARE_ANY))
    {
        PrintHelp(NULL, MSDK_STRING("Look ahead BRC is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.bLABRC && (InputParams.EncodeId != MFX_CODEC_AVC) && (InputParams.eMode == Source))
    {
        PrintHelp(NULL, MSDK_STRING("Look ahead BRC is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.nLADepth && (InputParams.nLADepth < 10 || InputParams.nLADepth > 100))
    {
        if ((InputParams.nLADepth != 1) || (!InputParams.nMaxSliceSize))
        {
            PrintHelp(NULL, MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100] or 1 in case of -mss option!"));
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if ((InputParams.nMaxSliceSize) && !(InputParams.libType & MFX_IMPL_HARDWARE_ANY))
    {
        PrintHelp(NULL, MSDK_STRING("MaxSliceSize option is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }
    if ((InputParams.nMaxSliceSize) && (InputParams.nSlices))
    {
        PrintHelp(NULL, MSDK_STRING("-mss and -l options are not compatible!"));
    }
    if ((InputParams.nMaxSliceSize) && (InputParams.EncodeId != MFX_CODEC_AVC))
    {
        PrintHelp(NULL, MSDK_STRING("MaxSliceSize option is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    std::map<mfxU32, sPluginParams>::iterator it;

    // Set decoder plugins parameters only if they were not set before
    if (!memcmp(InputParams.decoderPluginParams.pluginGuid.Data, MSDK_PLUGINGUID_NULL.Data, sizeof(MSDK_PLUGINGUID_NULL)) &&
         !strcmp(InputParams.decoderPluginParams.strPluginPath,""))
    {
        it = m_decoderPlugins.find(InputParams.DecodeId);
        if (it != m_decoderPlugins.end())
            InputParams.decoderPluginParams = it->second;
    }
    else
    {
        // Decoding plugin was set manually, so let's check if codec supports plugins
        if (!IsPluginCodecSupported(InputParams.DecodeId))
        {
            msdk_printf(MSDK_STRING("error: decoder does not support plugins\n"));
            return MFX_ERR_UNSUPPORTED;
        }

    }

    // Set encoder plugins parameters only if they were not set before
    if (!memcmp(InputParams.encoderPluginParams.pluginGuid.Data, MSDK_PLUGINGUID_NULL.Data, sizeof(MSDK_PLUGINGUID_NULL)) &&
        !strcmp(InputParams.encoderPluginParams.strPluginPath, ""))
    {
        it = m_encoderPlugins.find(InputParams.EncodeId);
        if (it != m_encoderPlugins.end())
            InputParams.encoderPluginParams = it->second;
    }
    else
    {
        // Decoding plugin was set manually, so let's check if codec supports plugins
        if (!IsPluginCodecSupported(InputParams.DecodeId))
        {
            msdk_printf(MSDK_STRING("error: encoder does not support plugins\n"));
            return MFX_ERR_UNSUPPORTED;
        }

    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::VerifyAndCorrectInputParams(TranscodingSample::sInputParams &InputParams)

bool  CmdProcessor::GetNextSessionParams(sInputParams &InputParams)
{
    if (!m_SessionArray.size())
        return false;
    if (m_SessionParamId == m_SessionArray.size())
    {
        return false;
    }
    InputParams = m_SessionArray[m_SessionParamId];

    m_SessionParamId++;
    return true;

} //bool  CmdProcessor::GetNextSessionParams(TranscodingSample::sInputParams &InputParams)





/////

enum
{
    TIME_TO_SLEEP = 1
};

sInputParams::sInputParams()
{
    memset(static_cast<__sInputParams*>(this),0,sizeof(__sInputParams));
    priority = MFX_PRIORITY_NORMAL; 
    libType = MFX_IMPL_HARDWARE; 
    MaxFrameNumber = 0xFFFFFFFF; 
}
void sInputParams::Reset()
{
    memset(static_cast<__sInputParams*>(this),0,sizeof(__sInputParams));
    priority = MFX_PRIORITY_NORMAL; 
    libType = MFX_IMPL_HARDWARE; 
    MaxFrameNumber = 0xFFFFFFFF; 
}


//BUFFERS
void IncreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_inc16((volatile mfxU16 *)(&ptr->Locked));
}

void DecreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_dec16((volatile mfxU16 *)&ptr->Locked);
}


SafetySurfaceBuffer::SafetySurfaceBuffer(SafetySurfaceBuffer *pNext):m_pNext(pNext)
{
} // SafetySurfaceBuffer::SafetySurfaceBuffer

SafetySurfaceBuffer::~SafetySurfaceBuffer()
{
} //SafetySurfaceBuffer::~SafetySurfaceBuffer()

void SafetySurfaceBuffer::AddSurface(ExtendedSurface Surf)
{
    AutomaticMutex  guard(m_mutex);

    SurfaceDescriptor sDescriptor;
    // Locked is used to signal when we can free surface
    sDescriptor.Locked     = 1;
    sDescriptor.ExtSurface = Surf;

    if (Surf.pSurface)
    {
        IncreaseReference(&Surf.pSurface->Data);
    }

    m_SList.push_back(sDescriptor);

} // SafetySurfaceBuffer::AddSurface(mfxFrameSurface1 *pSurf)

mfxStatus SafetySurfaceBuffer::GetSurface(ExtendedSurface &Surf)
{
    // no ready surfaces
    if (0 == m_SList.size())
    {
        return MFX_ERR_MORE_SURFACE;
    }

    AutomaticMutex guard(m_mutex);
    SurfaceDescriptor sDescriptor = m_SList.front();

    Surf = sDescriptor.ExtSurface;


    return MFX_ERR_NONE;

} // SafetySurfaceBuffer::GetSurface()

mfxStatus SafetySurfaceBuffer::ReleaseSurface(mfxFrameSurface1* pSurf)
{
    AutomaticMutex guard(m_mutex);
    std::list<SurfaceDescriptor>::iterator it;
    for (it = m_SList.begin(); it != m_SList.end(); it++)
    {
        if (pSurf == it->ExtSurface.pSurface)
        {
            it->Locked--;
            if (it->ExtSurface.pSurface)
                DecreaseReference(&it->ExtSurface.pSurface->Data);
            if (0 == it->Locked)
                m_SList.erase(it);
            return MFX_ERR_NONE;
        }
    }
    return MFX_ERR_UNKNOWN;

} // mfxStatus SafetySurfaceBuffer::ReleaseSurface(mfxFrameSurface1* pSurf)


//BITSTREAM 

FileBitstreamProcessor::FileBitstreamProcessor()
{
    MSDK_ZERO_MEMORY(m_Bitstream); 
}

FileBitstreamProcessor::~FileBitstreamProcessor()
{
    if(m_pFileReader.get())
        m_pFileReader->Close(); 
    if(m_pFileWriter.get())
        m_pFileWriter->Close();
    WipeMfxBitstream(&m_Bitstream);
}

mfxStatus FileBitstreamProcessor::Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile)
{
    mfxStatus sts; 
    if(pStrSrcFile)
    {
        m_pFileReader.reset(new CSmplBitstreamReader());
        sts = m_pFileReader->Init(pStrSrcFile);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    }
    
    if(pStrDstFile)
    {
        m_pFileWriter.reset(new CSmplBitstreamWriter());
        sts = m_pFileWriter->Init(pStrDstFile);
        MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    }
    sts = InitMfxBitstream(&m_Bitstream, 1024 * 1024); 
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    return MFX_ERR_NONE; 
}

mfxStatus FileBitstreamProcessor::GetInputBitstream(mfxBitstream** pBitstream)
{
    mfxStatus sts = m_pFileReader->ReadNextFrame(&m_Bitstream);
    if(MFX_ERR_NONE == sts)
    {
        *pBitstream = &m_Bitstream; 
        return sts; 
    }
    return sts; 
}

mfxStatus FileBitstreamProcessor::ProcessOutputBitstream(mfxBitstream *pBitstream)
{
    if(m_pFileWriter.get())
        return m_pFileWriter->WriteNextFrame(pBitstream, false);
    else
        return MFX_ERR_NONE;     
    
    
    
}

