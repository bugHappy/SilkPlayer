#include "pch.h"
#include "CSilkPlayer.h"
#include <mutex>
#include <cassert>

#include <atomic>
#include <thread>
#include <mutex>


#include <MMReg.h>
#include <MMSystem.h>
#pragma comment(lib, "winmm.lib")
#ifdef _DEBUG
#pragma comment(lib,"Dec_SDKd.lib")
#pragma comment(lib,"SKP_Silk_FLP_Win32_debug.lib")
#else
#pragma comment(lib,"Dec_SDK.lib")
#pragma comment(lib,"SKP_Silk_FLP_Win32_mt.lib")
#endif
#include "..\SILK_SDK_SRC_FLP_v1.0.9\decoder\SILKFileDecoder.h"
#define MAX_BUFFER_SIZE 1024


class CSilkPlayer :public IPlayer
{
#define MAX_BUF_BLOCK 3
	struct ItemRepository {
		WAVEHDR WaveHdr[MAX_BUF_BLOCK];
		size_t read_position = 0;
		size_t write_position = 0;
		size_t item_counter = 0;
		std::mutex mtx;
		std::mutex item_counter_mtx;
		std::condition_variable repo_not_full;
		std::condition_variable repo_not_empty;
		std::atomic_bool bDecodeFinished = false;
	} playJobRepository;
public:
	CSilkPlayer();
	~CSilkPlayer();
	// 通过 IPlayer 继承
	virtual bool Play() override;
	virtual bool Stop() override;
	virtual bool Open(LPCWSTR lpszFile) override;
	virtual bool Close() override;
	virtual bool Pause() override;
	virtual bool Resume() override;
	virtual DWORD GetAudioLength() override;
	virtual bool SetVolume(DWORD dwVolume)override;
	virtual DWORD GetVolume() override;
	virtual bool InitSuceesed()override;

	BOOL WaveOutProcImpl(HWAVEOUT hwo, UINT uMsg, DWORD dwParam1, DWORD dwParam2);

protected:
	static BOOL CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	bool InitPlayer();
	void PostPlayBuf(LPSTR buf, DWORD bufsize);	
	bool ConsumePlayBuf();
	void PlayThread();
	void DecodeThread();

	WAVEFORMATEX			m_wfmtx;
	bool					m_bInitSucessed = false;
	SILKFileDecoder*		m_pDecoder = NULL;
	HWAVEOUT				m_hWaveoutDev = NULL;
	std::thread				m_playThread, m_decodeThread;
	std::atomic_bool		m_bRunPlayTask;
};


CSilkPlayer::CSilkPlayer() {
	m_bInitSucessed = InitPlayer();
}

CSilkPlayer::~CSilkPlayer()
{
	Stop();
	Close();
}

bool CSilkPlayer::Play()
{
	Stop();
	m_bRunPlayTask = true;
	playJobRepository.bDecodeFinished = false;
	m_playThread = std::thread(&CSilkPlayer::PlayThread, this);
	m_decodeThread = std::thread(&CSilkPlayer::DecodeThread, this);
	return true;
}

bool CSilkPlayer::Stop()
{
	m_bRunPlayTask = false;
	waveOutReset(m_hWaveoutDev);
	if (m_playThread.joinable())
	{
		playJobRepository.repo_not_empty.notify_all();		
		m_playThread.join();
	}
	if (m_decodeThread.joinable())
	{
		playJobRepository.repo_not_full.notify_all();
		m_decodeThread.join();
	}
	
	{
		playJobRepository.read_position = 0;
		playJobRepository.write_position = 0;
		playJobRepository.item_counter = 0;
		playJobRepository.bDecodeFinished = false;
		for (int i = 0; i < MAX_BUF_BLOCK; i++)
		{
			delete playJobRepository.WaveHdr[i].lpData;
			playJobRepository.WaveHdr[i].lpData = NULL;
		}
	}
	return true;
}

bool CSilkPlayer::Open(LPCWSTR lpszFile)
{
	// 设置解码器
	if (m_pDecoder == NULL)
	{
		m_pDecoder = new SILKFileDecoder(lpszFile);
	}
	else
	{
		m_pDecoder->SetFilePathName(lpszFile);
	}
	return true;
}

bool CSilkPlayer::Close()
{
	if (m_hWaveoutDev)
	{
		waveOutReset(m_hWaveoutDev);
		waveOutClose(m_hWaveoutDev);
		m_hWaveoutDev = NULL;
	}
	return false;
}

bool CSilkPlayer::Pause()
{
	if (m_hWaveoutDev)
		return MMSYSERR_NOERROR == waveOutPause(m_hWaveoutDev);
	return false;
}

bool CSilkPlayer::Resume()
{
	if (m_hWaveoutDev)
		return MMSYSERR_NOERROR == waveOutRestart(m_hWaveoutDev);
	return false;
}

DWORD CSilkPlayer::GetAudioLength()
{
	return 0;
}

bool CSilkPlayer::SetVolume(DWORD dwVolume)
{
	if (m_hWaveoutDev)
		return MMSYSERR_NOERROR == waveOutSetVolume(m_hWaveoutDev, dwVolume);
	return false;
}

DWORD CSilkPlayer::GetVolume()
{
	DWORD dwVolume = 0;
	if (m_hWaveoutDev && (MMSYSERR_NOERROR == waveOutGetVolume(m_hWaveoutDev, &dwVolume)))
		return dwVolume;
	return 0;
}

bool CSilkPlayer::InitSuceesed()
{
	return m_bInitSucessed;
}

BOOL CSilkPlayer::WaveOutProcImpl(HWAVEOUT hwo, UINT uMsg, DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WOM_DONE)
	{
		std::unique_lock<std::mutex> lock(playJobRepository.mtx);
		{
			std::unique_lock<std::mutex> lock(playJobRepository.item_counter_mtx);
			--(playJobRepository.item_counter);
			playJobRepository.repo_not_full.notify_all();
		}
		if (playJobRepository.bDecodeFinished && (playJobRepository.write_position == playJobRepository.read_position))
		{
			m_bRunPlayTask = false;
			(playJobRepository.repo_not_empty).notify_all();
		}
	}
	return TRUE;
}

BOOL CSilkPlayer::WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	CSilkPlayer* pPlayer = (CSilkPlayer*)dwInstance;
	return pPlayer->WaveOutProcImpl(hwo, uMsg, dwParam1, dwParam2);
}
bool CSilkPlayer::InitPlayer()
{
	m_pDecoder = new SILKFileDecoder();
	MMRESULT mmres = waveOutOpen(&m_hWaveoutDev, WAVE_MAPPER, &(m_pDecoder->GetWaveFromatX()), (DWORD_PTR)WaveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
	return mmres == MMSYSERR_NOERROR;
}

void CSilkPlayer::PostPlayBuf(LPSTR buf, DWORD bufsize)
{
	std::unique_lock<std::mutex> lock(playJobRepository.mtx);	
	WAVEHDR& wavehdr = (playJobRepository.WaveHdr)[playJobRepository.write_position];
	if (wavehdr.dwFlags & WHDR_PREPARED)
	{
		waveOutUnprepareHeader(m_hWaveoutDev, &wavehdr, sizeof(WAVEHDR));
		delete (playJobRepository.WaveHdr)[playJobRepository.write_position].lpData;
	}
	wavehdr.lpData = buf;
	wavehdr.dwBufferLength = bufsize;
	wavehdr.dwFlags = WHDR_PREPARED;
	wavehdr.dwLoops =0;
	waveOutPrepareHeader(m_hWaveoutDev, &wavehdr, sizeof(WAVEHDR));
	++(playJobRepository.write_position);

	if (playJobRepository.write_position == MAX_BUF_BLOCK)
		playJobRepository.write_position = 0;

	(playJobRepository.repo_not_empty).notify_all();
}

bool CSilkPlayer::ConsumePlayBuf()
{
	std::unique_lock<std::mutex> lock(playJobRepository.mtx);
	// item buffer is empty, just wait here.
	while (playJobRepository.write_position == playJobRepository.read_position) {
		(playJobRepository.repo_not_empty).wait(lock);
		if (!m_bRunPlayTask)
		{
			return true;
		}
	}
	waveOutWrite(m_hWaveoutDev, &(playJobRepository.WaveHdr)[playJobRepository.read_position], sizeof(WAVEHDR));
	++(playJobRepository.read_position);
	if (playJobRepository.read_position == MAX_BUF_BLOCK)
		playJobRepository.read_position = 0;
	return true;
}

void CSilkPlayer::PlayThread()
{
	while (m_bRunPlayTask) {
		ConsumePlayBuf();
	}
}

//// HELPER MARCOs for wav header
//#define WRITE_U32(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
//                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);\
//                          *((buf)+2) = (unsigned char)(((x)>>16)&0xff);\
//                          *((buf)+3) = (unsigned char)(((x)>>24)&0xff);
//
//#define WRITE_U16(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
//                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);
//static int write_prelim_header(FILE* out, int channels, int samplerate) {
//
//	int bits = 16;
//	unsigned char headbuf[44];  /* The whole buffer */
//
//	int knownlength = 0;
//
//	int bytespersec = channels * samplerate * bits / 8;
//	int align = channels * bits / 8;
//	int samplesize = bits;
//
//
//	/*
//	here's a good ref...
//	http://www.lightlink.com/tjweber/StripWav/Canon.html
//	Based on the link above,
//	Actually, we are writting a simplified version of Wav.
//	*/
//	memcpy(headbuf, "RIFF", 4);
//	WRITE_U32(headbuf + 4, 0); // fileLength - 8 (4 bytes)
//	memcpy(headbuf + 8, "WAVE", 4);
//	memcpy(headbuf + 12, "fmt ", 4);
//	WRITE_U32(headbuf + 16, 16); // length of fmt data
//	WRITE_U16(headbuf + 20, 1);  /* format , 1==PCM */
//	WRITE_U16(headbuf + 22, channels); // 1==miss mono, 2== stereo 
//	WRITE_U32(headbuf + 24, samplerate);
//	WRITE_U32(headbuf + 28, bytespersec);
//	WRITE_U16(headbuf + 32, align);
//	WRITE_U16(headbuf + 34, samplesize);  // 16 or 8 
//	memcpy(headbuf + 36, "data", 4);
//	WRITE_U32(headbuf + 40, 0); // length of data block  fileLength - 44
//
//	if (fwrite(headbuf, 1, 44, out) != 44)	{
//		
//		return -1;
//	}
//
//	return 0;
//}
//
//
//// Update the wave header
//// Now....
//// The size is known..
//static void update_wave_header(FILE* fstream, unsigned int dataSize) {
//
//	unsigned char buffer[4]; // four bytes
//	fseek(fstream, 4, SEEK_SET);
//	WRITE_U32(buffer, dataSize + 44 - 8);
//	if (fwrite(buffer, sizeof(unsigned char), 4, fstream) != 4) {
//		printf("ERROR: Failed to update header info \n");
//		exit(-1);
//	}
//
//	fseek(fstream, 40, SEEK_SET);
//	WRITE_U32(buffer, dataSize);
//	if (4 != fwrite(buffer, sizeof(unsigned char), 4, fstream)) {
//		printf("ERROR: Failed to update header info \n");
//		exit(-1);
//	}
//
//}

void CSilkPlayer::DecodeThread()
{
	if (m_pDecoder == NULL || !m_pDecoder->IsVaild())
		return;

	// 开始解码，初始化解码器
	if (!m_pDecoder->BeginDecode())
	{
		return;
	}
	// 申请临时内存块，存储解码后的波形数据
	DWORD dwFrameMaxSize = m_pDecoder->GetDecodedFrameMaxSize();
	
	/*FILE* bitOutFile;
	_wfopen_s(&bitOutFile, L"e:\\test.wav", L"wb+");
	WAVEFORMATEX FORMAT= m_pDecoder->GetWaveFromatX();
	write_prelim_header(bitOutFile, FORMAT.nChannels, FORMAT.nSamplesPerSec);
	UINT datasize=0;*/
	while (!m_pDecoder->IsEOF() && m_bRunPlayTask)
	{
		std::unique_lock<std::mutex> lock(playJobRepository.item_counter_mtx);
		if (playJobRepository.item_counter < MAX_BUF_BLOCK) {
			LPSTR pBufferBase = new char[dwFrameMaxSize];
			assert(pBufferBase);
			DWORD dwSize = m_pDecoder->Decode(pBufferBase);
			/*datasize += dwSize;
			fwrite(pBufferBase, 1, dwSize, bitOutFile);*/
			PostPlayBuf(pBufferBase, dwSize);
			++(playJobRepository.item_counter);
		}
		else//坑满了，等播放完再放
		{
			playJobRepository.repo_not_full.wait(lock);
		}
	}
	/*update_wave_header(bitOutFile, datasize);
	fclose(bitOutFile);*/
	playJobRepository.bDecodeFinished = true;
	// 解码结束
	m_pDecoder->EndDecode();
}

PLAYER_API IPlayer* CreateSilkPlayer()
{
	return new CSilkPlayer();
}

PLAYER_API void ReleaseSilkPlayer(IPlayer* player)
{
	delete player;
	player = NULL;
}
