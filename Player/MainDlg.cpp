// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainDlg.h"	
#include "../libsilk/CSilkPlayer.h"

#include <shellapi.h>

class CDropTargetBase :public IDropTarget
{
public:
	CDropTargetBase()
	{
		nRef = 0;
	}

	virtual ~CDropTargetBase() {}

	//////////////////////////////////////////////////////////////////////////
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ __RPC__deref_out void __RPC_FAR* __RPC_FAR* ppvObject)
	{
		HRESULT hr = S_FALSE;
		if (riid == __uuidof(IUnknown))
			*ppvObject = (IUnknown*)this, hr = S_OK;
		else if (riid == __uuidof(IDropTarget))
			*ppvObject = (IDropTarget*)this, hr = S_OK;
		if (SUCCEEDED(hr)) AddRef();
		return hr;

	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) { return ++nRef; }

	virtual ULONG STDMETHODCALLTYPE Release(void) {
		ULONG uRet = --nRef;
		if (uRet == 0) delete this;
		return uRet;
	}

	//////////////////////////////////////////////////////////////////////////
	// IDropTarget

	virtual HRESULT STDMETHODCALLTYPE DragEnter(
		/* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		*pdwEffect = DROPEFFECT_LINK;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DragOver(
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		*pdwEffect = DROPEFFECT_LINK;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DragLeave(void)
	{
		return S_OK;
	}


protected:
	int nRef;
};

class CTestDropTarget1 : public CDropTargetBase
{
protected:
	IPlayer* m_player = NULL;
public:
	CTestDropTarget1(IPlayer* player) :m_player(player)
	{
		SASSERT(m_player);
	}
	~CTestDropTarget1()
	{
	}
public:
	virtual HRESULT STDMETHODCALLTYPE Drop(
		/* [unique][in] */ __RPC__in_opt IDataObject* pDataObj,
		/* [in] */ DWORD grfKeyState,
		/* [in] */ POINTL pt,
		/* [out][in] */ __RPC__inout DWORD* pdwEffect)
	{
		FORMATETC format =
		{
			CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL
		};
		STGMEDIUM medium;
		if (FAILED(pDataObj->GetData(&format, &medium)))
		{
			return S_FALSE;
		}

		HDROP hdrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));

		if (!hdrop)
		{
			return S_FALSE;
		}

		bool success = false;
		TCHAR filename[MAX_PATH];
		success = !!DragQueryFile(hdrop, 0, filename, MAX_PATH);
		DragFinish(hdrop);
		GlobalUnlock(medium.hGlobal);

		if (success && m_player->InitSuceesed())
		{
			m_player->Open(filename);
			m_player->Play();
		}

		*pdwEffect = DROPEFFECT_LINK;
		return S_OK;
	}
};


CMainDlg::CMainDlg() : SHostWnd(_T("LAYOUT:XML_MAINWND"))
{
	m_bLayoutInited = FALSE;
	m_player = CreateSilkPlayer();
}

CMainDlg::~CMainDlg()
{
	ReleaseSilkPlayer(m_player);
}

int CMainDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	SetMsgHandled(FALSE);
	return 0;
}

BOOL CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_bLayoutInited = TRUE;

	if(S_OK == ::RegisterDragDrop(m_hWnd, GetDropTarget()))
		RegisterDragDrop(FindChildByName(L"dropwnd")->GetSwnd(), new CTestDropTarget1(m_player));

	return 0;
}

void CMainDlg::OnTest()
{
	if (m_player && m_player->InitSuceesed())
	{
		m_player->Open(L"test.amr");
		m_player->Play();
	}
}

//TODO:消息映射
void CMainDlg::OnClose()
{
	DestroyWindow();
}

void CMainDlg::OnMaximize()
{
	SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE);
}
void CMainDlg::OnRestore()
{
	SendMessage(WM_SYSCOMMAND, SC_RESTORE);
}
void CMainDlg::OnMinimize()
{
	SendMessage(WM_SYSCOMMAND, SC_MINIMIZE);
}

void CMainDlg::OnSize(UINT nType, CSize size)
{
	SetMsgHandled(FALSE);
	if (!m_bLayoutInited) return;

	SWindow* pBtnMax = FindChildByName(L"btn_max");
	SWindow* pBtnRestore = FindChildByName(L"btn_restore");
	if (!pBtnMax || !pBtnRestore) return;

	if (nType == SIZE_MAXIMIZED)
	{
		pBtnRestore->SetVisible(TRUE);
		pBtnMax->SetVisible(FALSE);
	}
	else if (nType == SIZE_RESTORED)
	{
		pBtnRestore->SetVisible(FALSE);
		pBtnMax->SetVisible(TRUE);
	}
}

