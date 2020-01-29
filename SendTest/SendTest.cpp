// SendTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include<stdio.h>
#include<windows.h>  

#pragma comment(lib, "winmm.lib")

// HELPER MARCOs for wav header
#define WRITE_U32(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);\
                          *((buf)+2) = (unsigned char)(((x)>>16)&0xff);\
                          *((buf)+3) = (unsigned char)(((x)>>24)&0xff);

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)((x)&0xff);\
                          *((buf)+1) = (unsigned char)(((x)>>8)&0xff);
static int write_prelim_header(FILE* out, int channels, int samplerate) {

	int bits = 16;
	unsigned char headbuf[44];  /* The whole buffer */

	int knownlength = 0;

	int bytespersec = channels * samplerate * bits / 8;
	int align = channels * bits / 8;
	int samplesize = bits;


	/*
	here's a good ref...
	http://www.lightlink.com/tjweber/StripWav/Canon.html
	Based on the link above,
	Actually, we are writting a simplified version of Wav.
	*/
	memcpy(headbuf, "RIFF", 4);
	WRITE_U32(headbuf + 4, 0); // fileLength - 8 (4 bytes)
	memcpy(headbuf + 8, "WAVE", 4);
	memcpy(headbuf + 12, "fmt ", 4);
	WRITE_U32(headbuf + 16, 16); // length of fmt data
	WRITE_U16(headbuf + 20, 1);  /* format , 1==PCM */
	WRITE_U16(headbuf + 22, channels); // 1==miss mono, 2== stereo 
	WRITE_U32(headbuf + 24, samplerate);
	WRITE_U32(headbuf + 28, bytespersec);
	WRITE_U16(headbuf + 32, align);
	WRITE_U16(headbuf + 34, samplesize);  // 16 or 8 
	memcpy(headbuf + 36, "data", 4);
	WRITE_U32(headbuf + 40, 0); // length of data block  fileLength - 44

	if (fwrite(headbuf, 1, 44, out) != 44) {

		return -1;
	}

	return 0;
}

static void update_wave_header(FILE* fstream, unsigned int dataSize) {

	unsigned char buffer[4]; // four bytes
	fseek(fstream, 4, SEEK_SET);
	WRITE_U32(buffer, dataSize + 44 - 8);
	if (fwrite(buffer, sizeof(unsigned char), 4, fstream) != 4) {
		printf("ERROR: Failed to update header info \n");
		exit(-1);
	}

	fseek(fstream, 40, SEEK_SET);
	WRITE_U32(buffer, dataSize);
	if (4 != fwrite(buffer, sizeof(unsigned char), 4, fstream)) {
		printf("ERROR: Failed to update header info \n");
		exit(-1);
	}

}


int main()
{
	FILE* pf;

	HWAVEIN hWaveIn;  //输入设备
	WAVEFORMATEX waveform; //采集音频的格式，结构体
	BYTE* pBuffer1;//采集音频时的数据缓存
	WAVEHDR wHdr1; //采集音频时包含数据缓存的结构体
	HANDLE         wait;

	waveform.wFormatTag = WAVE_FORMAT_PCM;//声音格式为PCM

	waveform.nSamplesPerSec = 8000;//采样率，16000次/秒

	waveform.wBitsPerSample = 16;//采样比特，16bits/次

	waveform.nChannels = 1;//采样声道数，2声道

	waveform.nAvgBytesPerSec = 16000;//每秒的数据率，就是每秒能采集多少字节的数据

	waveform.nBlockAlign = 2;//一个块的大小，采样bit的字节数乘以声道数

	waveform.cbSize = 0;//一般为0
	fopen_s(&pf, "录音测试.wav", "wb");
	write_prelim_header(pf, waveform.nChannels, waveform.nSamplesPerSec);
	UINT datasize = 0;

	wait = CreateEvent(NULL, 0, 0, NULL);
	//使用waveInOpen函数开启音频采集

	waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, (DWORD_PTR)wait, 0L, CALLBACK_EVENT);

	//建立两个数组（这里可以建立多个数组）用来缓冲音频数据

	DWORD bufsize = 1024 * 100;//每次开辟10k的缓存存储录音数据

	
	int cd = 10;
	while (cd--)
	{
		pBuffer1 = new  BYTE[bufsize];

		wHdr1.lpData = (LPSTR)pBuffer1;

		wHdr1.dwBufferLength = bufsize;

		wHdr1.dwBytesRecorded = 0;

		wHdr1.dwUser = 0;

		wHdr1.dwFlags = 0;

		wHdr1.dwLoops = 1;

		waveInPrepareHeader(hWaveIn, &wHdr1, sizeof(WAVEHDR));//准备一个波形数据块头用于录音

		waveInAddBuffer(hWaveIn, &wHdr1, sizeof(WAVEHDR));//指定波形数据块为录音输入缓存

		waveInStart(hWaveIn);//开始录音

		Sleep(1000);//等待声音录制1s

		waveInReset(hWaveIn);//停止录音

		fwrite(pBuffer1, 1, wHdr1.dwBytesRecorded, pf);

		datasize += wHdr1.dwBytesRecorded;
		delete pBuffer1;

	}
	update_wave_header(pf, datasize);
	fclose(pf);
	waveInClose(hWaveIn);

	return  0;

}