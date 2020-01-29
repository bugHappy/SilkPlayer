#include "SILKFileDecoder.h"

static SKP_int32 rand_seed = 1;
#define sampleRate 24000

SILKFileDecoder::SILKFileDecoder(void) :m_bEnd(false), m_psDec(NULL), m_pPayloadEnd(NULL), m_pPayloadToDec(NULL), m_pbitInFile(NULL)
{
	initDeCoder();
}

SILKFileDecoder::SILKFileDecoder(LPCTSTR lpszFile) :
	m_sFilePathName(lpszFile), m_bEnd(false), m_psDec(NULL), m_pPayloadEnd(NULL), m_pPayloadToDec(NULL), m_pbitInFile(NULL)
{
	_wfopen_s(&m_pbitInFile, lpszFile, L"rb");
	initDeCoder();
}

bool SILKFileDecoder::initDeCoder()
{
	/* Create a Decoder */
	/* Set the samplingrate that is requested for the output */
	m_DecControl.API_sampleRate = sampleRate;
	/* Initialize to one frame per packet, for proper concealment before first packet arrives */
	m_DecControl.framesPerPacket = 1;
	SKP_int32 decSizeBytes;
	/* Create a decoder */
	SKP_int16 ret = SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes);
	if (ret) {
		return false;
	}
	m_psDec = malloc(decSizeBytes);
	/* Reset decoder */
	ret = SKP_Silk_SDK_InitDecoder(m_psDec);
	if (ret) {
		return false;
	}
	return true;
}

SILKFileDecoder::~SILKFileDecoder(void)
{
	EndDecode();
	if (m_psDec)
		free(m_psDec);
}

/*
@	判断amr音频文件的一个音频帧是否为可用帧
*/
bool SILKFileDecoder::IsVaild()
{
	if (m_pbitInFile == NULL || m_psDec == NULL)
		return false;

	char header_buf[50];
	// drop the first byte [02] for Wechat voice2 
	// @2016-02-07 Wechat Android 6.*
	// Voice files are in folder voice2
	fseek(m_pbitInFile, 0, 0);
	m_counter = fread(header_buf, sizeof(char), 1, m_pbitInFile);
	m_counter = fread(header_buf, sizeof(char), strlen("#!SILK_V3"), m_pbitInFile);
	header_buf[strlen("#!SILK_V3")] = (char)0; /* Terminate with a null character */
	if (strcmp(header_buf, "#!SILK_V3") != 0) {
		/* Non-equal strings */
		return false;
	}
	return true;
}


/*
@	一个amr音频的音频帧时长为20毫秒
@	GetFrameCount()获取的是音频帧的数量
@	GetTimeLength获取音频文件的播放时长(精确到毫秒)
*/
ULONGLONG SILKFileDecoder::GetTimeLength()
{
	if (!IsVaild())  return 0;
	return GetFrameCount();
}

/*
@	获取波形帧的辅助数据
*/
WAVEFORMATEX SILKFileDecoder::GetWaveFromatX()
{
	WAVEFORMATEX wfmtx;
	memset(&wfmtx, 0, sizeof(WAVEFORMATEX));
	wfmtx.wFormatTag = WAVE_FORMAT_PCM;
	wfmtx.nChannels = 1; // 单声道
	wfmtx.wBitsPerSample = 16;
	//(nChannels*wBitsPerSample)/8	
	wfmtx.nBlockAlign = 2;
	wfmtx.nSamplesPerSec = sampleRate;
	wfmtx.nAvgBytesPerSec = sampleRate*2 ;
	wfmtx.cbSize = 0;

	return wfmtx;
}

/*
@	开始解码：
@	1、先判断音频帧是否可用
@	2、初始化解码器
@	3、设置开始解码位置(游标位置+帧头前8位)
*/
BOOL SILKFileDecoder::BeginDecode()
{
	SKP_int16 nBytes = 0;

	if (!IsVaild())  return FALSE;

	//m_fLoss_prob = 0.0f;
	m_pPayloadEnd = m_payload;

	fseek(m_pbitInFile, strlen("#!SILK_V3") + sizeof(char), 0);
	
	/* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
	for (int i = 0; i < MAX_LBRR_DELAY; i++) {
		/* Read m_payload size */
		m_counter = fread(&nBytes, sizeof(SKP_int16), 1, m_pbitInFile);
		/* Read m_payload */
		m_counter = fread(m_pPayloadEnd, sizeof(SKP_uint8), nBytes, m_pbitInFile);

		if ((SKP_int16)m_counter < nBytes) {
			break;
		}
		m_nBytesPerPacket[i] = nBytes;
		m_pPayloadEnd += nBytes;
	}
	return TRUE;
}

/*
@	结束解码
*/
void SILKFileDecoder::EndDecode()
{
	if (m_pbitInFile)
	{
		fclose(m_pbitInFile);
		m_pbitInFile = NULL;
	}
}

/*
@	解析游标大于等于初始游标位置+文件大小（已经到文件末尾）
*/
bool SILKFileDecoder::IsEOF()
{
	return m_bEnd;
}

/*
@	解析音频帧中的音频数据块
*/
DWORD SILKFileDecoder::decodeLastPacket(LPSTR& pData)
{
	SKP_int16 ret_Len=0;
	SKP_int16 nBytes=0;
	SKP_int32 frames, lost;
	for (int k = 0; k < MAX_LBRR_DELAY; k++) {
		if (m_nBytesPerPacket[0] == 0) {
			/* Indicate lost packet */
			lost = 1;

			/* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
			payloadPtr = m_payload;
			for (int i = 0; i < MAX_LBRR_DELAY; i++) {
				if (m_nBytesPerPacket[i + 1] > 0) {
					SKP_int16 nBytesFEC=0;
					SKP_Silk_SDK_search_for_LBRR(payloadPtr, m_nBytesPerPacket[i + 1], i + 1, m_FECpayload, &nBytesFEC);
					if (nBytesFEC > 0) {
						m_pPayloadToDec = m_FECpayload;
						nBytes = nBytesFEC;
						lost = 0;
						break;
					}
				}
				payloadPtr += m_nBytesPerPacket[i + 1];
			}
		}
		else {
			lost = 0;
			nBytes = m_nBytesPerPacket[0];
			m_pPayloadToDec = m_payload;
		}

		/* Silk decoder */
		outPtr = m_OutBuf;
		SKP_int16 tot_len = 0;
		SKP_int16 len = 0;
		SKP_int16 ret=0;
		if (lost == 0) {
			/* No loss: Decode all frames in the packet */
			frames = 0;
			do {
				/* Decode 20 ms */
				ret = SKP_Silk_SDK_Decode(m_psDec, &m_DecControl, 0, m_pPayloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_SDK_Decode returned %d", ret);
				}

				frames++;
				outPtr += len;
				tot_len += len;
				if (frames > MAX_INPUT_FRAMES) {
					/* Hack for corrupt stream that could generate too many frames */
					outPtr = m_OutBuf;
					tot_len = 0;
					frames = 0;
				}
				/* Until last 20 ms frame of packet has been decoded */
			} while (m_DecControl.moreInternalDecoderFrames);
		}
		else {
			/* Loss: Decode enough frames to cover one packet duration */
			/* Generate 20 ms */
			for (int i = 0; i < m_DecControl.framesPerPacket; i++) {
				ret = SKP_Silk_SDK_Decode(m_psDec, &m_DecControl, 1, m_pPayloadToDec, nBytes, outPtr, &len);
				if (ret) {
					printf("\nSKP_Silk_Decode returned %d", ret);
				}
				outPtr += len;
				tot_len += len;
			}
		}
		// 输出
		if (pData)
		{
			memcpy(pData+ sizeof(SKP_int16) * ret_Len, m_OutBuf, sizeof(SKP_int16) * tot_len);
			ret_Len += tot_len;
		}
		/* Update Buffer */
		totBytes = 0;
		for (int i = 0; i < MAX_LBRR_DELAY; i++) {
			totBytes += m_nBytesPerPacket[i + 1];
		}
		SKP_memmove(m_payload, &m_payload[m_nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
		m_pPayloadEnd -= m_nBytesPerPacket[0];
		SKP_memmove(m_nBytesPerPacket, &m_nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));
	}

	return sizeof(SKP_int16) * ret_Len;
}

DWORD SILKFileDecoder::Decode(LPSTR& pData)
{
	SKP_int16 ret;
	SKP_int16 nBytes = 0;
	SKP_int32 frames, lost;
	/* Read m_payload size */
	m_counter = fread(&nBytes, sizeof(SKP_int16), 1, m_pbitInFile);

	if (nBytes < 0 || m_counter < 1) {
		m_bEnd = true;
		return decodeLastPacket(pData);
	}

	/* Read m_payload */
	m_counter = fread(m_pPayloadEnd, sizeof(SKP_uint8),(size_t) nBytes, m_pbitInFile);
	if ((SKP_int16)m_counter < nBytes) {
		m_bEnd = true;
		return decodeLastPacket(pData);
	}

	// Well.... I don't need to simulate loss
	m_nBytesPerPacket[MAX_LBRR_DELAY] = nBytes;
	m_pPayloadEnd += nBytes;

	if (m_nBytesPerPacket[0] == 0) {
		/* Indicate lost packet */
		lost = 1;

		/* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
		payloadPtr = m_payload;
		for (int i = 0; i < MAX_LBRR_DELAY; i++) {
			if (m_nBytesPerPacket[i + 1] > 0) {
				SKP_int16 nBytesFEC = 0;
				SKP_Silk_SDK_search_for_LBRR(payloadPtr, m_nBytesPerPacket[i + 1], i + 1, m_FECpayload, &nBytesFEC);
				if (nBytesFEC > 0) {
					m_pPayloadToDec = m_FECpayload;
					nBytes = nBytesFEC;
					lost = 0;
					break;
				}
			}
			payloadPtr += m_nBytesPerPacket[i + 1];
		}
	}
	else {
		lost = 0;
		nBytes = m_nBytesPerPacket[0];
		m_pPayloadToDec = m_payload;
	}


	//FIMXE , Now there're (MAX_LBRR_DELAY + 1) packets in m_payload

	/* Silk decoder */
	outPtr = m_OutBuf;
	SKP_int16 tot_len = 0, len = 0;
	if (lost == 0) {
		/* No Loss: Decode all frames in the packet */
		frames = 0;
		do {
			/* Decode 20 ms */
			ret = SKP_Silk_SDK_Decode(m_psDec, &m_DecControl, 0, m_pPayloadToDec, nBytes, outPtr, &len);
			if (ret) {
				printf("\nSKP_Silk_SDK_Decode returned %d", ret);
			}

			frames++;
			outPtr += len;
			tot_len += len;
			if (frames > MAX_INPUT_FRAMES) {
				/* Hack for corrupt stream that could generate too many frames */
				outPtr = m_OutBuf;
				tot_len = 0;
				frames = 0;
			}
			/* Until last 20 ms frame of packet has been decoded */
		} while (m_DecControl.moreInternalDecoderFrames);
	}
	else {
		/* Loss: Decode enough frames to cover one packet duration */
		for (int i = 0; i < m_DecControl.framesPerPacket; i++) {
			/* Generate 20 ms */
			ret = SKP_Silk_SDK_Decode(m_psDec, &m_DecControl, 1, m_pPayloadToDec, nBytes, outPtr, &len);
			if (ret) {
				printf("\nSKP_Silk_Decode returned %d", ret);
			}
			outPtr += len;
			tot_len += len;
		}
	}
	// 输出
	if (pData)
	{
		memcpy(pData, m_OutBuf, sizeof(SKP_int16) * tot_len);
	}
	/* Update buffer */
	totBytes = 0;
	for (int i = 0; i < MAX_LBRR_DELAY; i++) {
		totBytes += m_nBytesPerPacket[i + 1];
	}
	// drop the first frame... a Cycle buffer??
	SKP_memmove(m_payload, &m_payload[m_nBytesPerPacket[0]], totBytes * sizeof(SKP_uint8));
	m_pPayloadEnd -= m_nBytesPerPacket[0];
	SKP_memmove(m_nBytesPerPacket, &m_nBytesPerPacket[1], MAX_LBRR_DELAY * sizeof(SKP_int16));

	return sizeof(SKP_int16) * tot_len;
}

DWORD SILKFileDecoder::GetDecodedFrameMaxSize()
{
	return sizeof(m_OutBuf);
}

DWORD SILKFileDecoder::GetDecodedMaxSize()
{
	return sizeof(m_OutBuf);
}

void SILKFileDecoder::SetFilePathName(LPCTSTR lpszFile)
{
	// 关闭解码器
	EndDecode();
	m_bEnd = false;
	_wfopen_s(&m_pbitInFile, lpszFile, L"rb");
}

DWORD SILKFileDecoder::GetFrameCount()
{
	ATLASSERT(IsVaild());
	return m_FrameCount;
}