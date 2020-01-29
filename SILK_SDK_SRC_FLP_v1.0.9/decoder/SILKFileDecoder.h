#pragma once

#include <Windows.h>
#include <atlstr.h>
#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2


extern "C"
{

#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FLP.h"
}

class SILKFileDecoder
{
public:
    SILKFileDecoder(void);
    SILKFileDecoder(LPCTSTR lpszFile);
    virtual ~SILKFileDecoder(void);

private: // 屏蔽拷贝构造函数和赋值运算
    SILKFileDecoder(const SILKFileDecoder& )
    {
        ATLASSERT(FALSE);
    }
    SILKFileDecoder& operator=(const SILKFileDecoder&)
    {
        ATLASSERT(FALSE);
        return *this;
    }
	bool initDeCoder();
public:
    /// 设置需解码文件路径
    virtual void SetFilePathName(LPCTSTR lpszFile);
    /// 获取总时间长度，单位ms
    virtual ULONGLONG GetTimeLength();
    /// 获取解码后的波形格式
    virtual WAVEFORMATEX GetWaveFromatX();
    /// 开始解码，初始化解码器
    virtual BOOL BeginDecode();
    /// 解码，每解码一帧，游标后移至下一帧,返回解码后的帧大小，输出解码后的波形数据
    virtual DWORD Decode(LPSTR& pData);
    /// 判断是否解码结束
    virtual bool IsEOF();
	DWORD decodeLastPacket(LPSTR& pData);
	/// 结束解码，销毁解码器
    virtual void EndDecode();
    /// 判断解码器是否正常
    virtual bool IsVaild();
    /// 获取解码后的波形数据大小，单位byte
    virtual DWORD GetDecodedMaxSize();
    /// 获取解码后的波形数据帧大小，单位byte
    virtual DWORD GetDecodedFrameMaxSize();

private:
    DWORD GetFrameCount();

private:
    FILE                          *m_pbitInFile;
    CString                       m_sFilePathName;        // 解码文件路径
	SKP_SILK_SDK_DecControlStruct m_DecControl;
	void                          *m_psDec;//解码器
	size_t                        m_counter;//文件当前读位置
	SKP_uint8                     m_payload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * (MAX_LBRR_DELAY + 1)];
	SKP_uint8                     *m_pPayloadEnd , *m_pPayloadToDec ;
	SKP_uint8                     m_FECpayload[MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES], *payloadPtr;
	SKP_int16                     m_nBytesPerPacket[MAX_LBRR_DELAY + 1], totBytes;
	SKP_int16                     m_OutBuf[((FRAME_LENGTH_MS * MAX_API_FS_KHZ) << 1) * MAX_INPUT_FRAMES], *outPtr;	
	bool                          m_bEnd;
	DWORD                         m_FrameCount;
};
