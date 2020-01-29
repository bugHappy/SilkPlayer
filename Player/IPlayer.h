#pragma once
struct IPlayer
{
	virtual bool Play()=0;
	virtual bool Stop() = 0;
	virtual bool Open(LPCWSTR filepath) = 0;
	virtual bool Close() = 0;
	virtual bool Pause() = 0;
	virtual bool Resume() = 0;
	virtual DWORD GetAudioLength()=0;
	virtual bool SetVolume(DWORD dwVolume) = 0;
	virtual DWORD GetVolume() = 0;
	virtual bool InitSuceesed() = 0;
};

enum PlayState
{
	Play_Stop = 0,
	Play_Pause,
	Play_Playing,
};