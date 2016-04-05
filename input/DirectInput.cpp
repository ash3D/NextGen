/*
\author		Arseny Lezin aka SleepWalk
\date		19.03.2016 (c)Arseny Lezin

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "DirectInput.h"

CDirectInput::CDirectInput(IEngineCore *pEngineCore):
_pEngineCore(pEngineCore), _bFocused(false), 
_bIsTxtInput(false), _pcBuffer(NULL)
{
	_pEngineCore->GetInstanceIndex(_uiInstIdx);

	_pEngineCore->AddProcedure(EPT_UPDATE, &_s_Update, (void*)this);
	_pEngineCore->AddProcedure(EPT_INIT, &_s_Init, (void*)this);
	_pEngineCore->AddProcedure(EPT_FREE, &_s_Free, (void*)this);
	_pEngineCore->AddEventListener(ET_ON_WINDOW_MESSAGE, &_s_EventHandler, (void*)this);

	_bExclusive	= false;
	_bHideCursor = true;
	_bCurBeyond = false;
}

CDirectInput::~CDirectInput()
{
	_pEngineCore->RemoveProcedure(EPT_UPDATE, &_s_Update, (void*)this);
	_pEngineCore->RemoveProcedure(EPT_INIT, &_s_Init, (void*)this);
	_pEngineCore->RemoveProcedure(EPT_FREE, &_s_Free, (void*)this);
	_pEngineCore->RemoveEventListener(ET_ON_WINDOW_MESSAGE, &_s_EventHandler, (void*)this);

	if (_bExclusive)
		ClipCursor(0, 0, 0, 0);

	for (uint i = 0; i < _uiJoyCount; ++i)
	{
		delete[] _clDeviceInstance[i];
		delete[] _clJoystickCaps[i];
	}

	LOG("DirectInput finalized.", LT_INFO);
}

void ClipCursor(uint left, uint top, uint right, uint bottom)
{
	if (left == 0 && top == 0 && right == 0 && bottom == 0)
		::ClipCursor(NULL);
	else
	{
		RECT rect;

		rect.left = left;
		rect.right = right;
		rect.top = top;
		rect.bottom = bottom;

		::ClipCursor(&rect);
	}
}

void CDirectInput::_ClipCursor()
{
	RECT rect;

	int iMetricBorder = 0;
	int iMetricTop    = 0;

	TEngineWindow stEngWindow;
	_pEngineCore->GetCurrentWindow(stEngWindow);

	if(stEngWindow.uiFlags == EWF_ALLOW_SIZEING)
		iMetricBorder = GetSystemMetrics(SM_CXFRAME);
	else
		iMetricBorder = GetSystemMetrics(SM_CXDLGFRAME);

	iMetricTop = GetSystemMetrics(SM_CYCAPTION) + iMetricBorder;

	GetWindowRect(_stWinHandle, &rect);

	ClipCursor(rect.left + iMetricBorder, rect.top + iMetricTop, rect.right - iMetricBorder + 1, rect.bottom - iMetricBorder + 1);
}

void CDirectInput::_Update(uint uiDeltaTime)
{
	if (_pKeyboard != NULL)
		switch(_pKeyboard->GetDeviceState(256 * sizeof(bool), (void*)_abKeys))
		{
			case DIERR_INPUTLOST:
				_pKeyboard->Acquire();
				break;
		}

	if (_pMouse != NULL)
		switch(_pMouse->GetDeviceState(sizeof(DIMOUSESTATE2), &_pMouseState))
		{
			case DI_OK:
				_stMsts.bLeftButton   = _pMouseState.rgbButtons[0] == 0x80;
				_stMsts.bRightButton  = _pMouseState.rgbButtons[1] == 0x80;
				_stMsts.bMiddleButton = _pMouseState.rgbButtons[2] == 0x80;

				_stMsts.iDeltaX	    = _pMouseState.lX;
				_stMsts.iDeltaY	    = _pMouseState.lY;
				_stMsts.iDeltaWheel	= _pMouseState.lZ;

				POINT p;
				GetCursorPos(&p);

				if(_bFocused)
					ScreenToClient(_stWinHandle, &p);

				_stMsts.iX = p.x;
				_stMsts.iY = p.y;

				break;
		
			case DIERR_INPUTLOST:
				_pMouse->Acquire();

				break;
		}

	for (uint i = 0; i < _uiJoyCount; ++i)
	{	
		switch(_clJoystick[i]->GetDeviceState(sizeof(DIJOYSTATE), &_pJoystickState))
		{
			case DI_OK:

				for (uint j = 0; j < _stJoysts[i].uiButtonsCount; ++j)
				{
					_stJoysts[i].bButtons[j] = _pJoystickState.rgbButtons[j] == 0x80 ;
				}

				_stJoysts[i].iXAxis = _pJoystickState.lX;
				_stJoysts[i].iYAxis = _pJoystickState.lY;
				_stJoysts[i].iZAxis = _pJoystickState.lZ;

				_stJoysts[i].iRAxis = _pJoystickState.lRz;
				_stJoysts[i].iUAxis = _pJoystickState.lRx;
				_stJoysts[i].iVAxis = _pJoystickState.lRy; 

				_stJoysts[i].iPOV   = _pJoystickState.rgdwPOV[0];

				break;

			case DIERR_INPUTLOST:
				_clJoystick[i]->Acquire();

				break;
		}
	}
}

void CDirectInput::_Init()
{
	_pEngineCore->GetWindowHandle(_stWinHandle);
	
	ZeroMemory(_abKeys, 256 * sizeof(bool));

	_stMsts.iDeltaX = 0;
	_stMsts.iDeltaY = 0;
	_stMsts.iX = 0;
	_stMsts.iY = 0;
	_stMsts.iDeltaWheel = 0;
	_stMsts.bLeftButton = false;
	_stMsts.bRightButton = false;
	_stMsts.bMiddleButton = false;

	_uiJoyCount = 0;

	if (DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&_pDirectInput, NULL) != DI_OK)
	{
		LOG("Can't create DirectInput8 device.", LT_FATAL);
		return;
	}

	if (_pDirectInput->CreateDevice(GUID_SysKeyboard, &_pKeyboard, NULL) != DI_OK)
		LOG("Can't create keyboard device.", LT_ERROR);
	else
		if (_pKeyboard->SetDataFormat(&c_dfDIKeyboard) != DI_OK)
			LOG("Can't set keyboard device data format.", LT_ERROR);
	
	if (_pDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, _s_EnumJoysticks , this, DIEDFL_ALLDEVICES) != DI_OK)
		LOG("Can't enum joystick device.", LT_INFO);

	if (_pDirectInput->CreateDevice(GUID_SysMouse, &_pMouse, NULL) != DI_OK)
		LOG("Can't create mouse device.", LT_ERROR);
	else
		if (_pMouse->SetDataFormat(&c_dfDIMouse2) != DI_OK)
			LOG("Can't set mouse device data format.", LT_ERROR);

	for (uint i = 0; i < _uiJoyCount; ++i)
		if (_clJoystick[i]->SetDataFormat(&c_dfDIJoystick) != DI_OK)
			LOG("Can't set joystick device data format.", LT_ERROR);

	for (uint i = 0; i < _uiJoyCount; ++i)
	{
		TJoystickStates stTempJoyState;

		ZeroMemory(stTempJoyState.bButtons, 32 * sizeof(bool));

		stTempJoyState.iPOV = 0;
		stTempJoyState.iRAxis = 0;
		stTempJoyState.iUAxis = 0;
		stTempJoyState.iVAxis = 0;
		stTempJoyState.iXAxis = 0;
		stTempJoyState.iYAxis = 0;
		stTempJoyState.iZAxis = 0;
		stTempJoyState.uiButtonsCount = 0;

		_stJoysts.push_back(stTempJoyState);
	}	

	if (_pKeyboard != NULL)
		if (_pKeyboard->SetCooperativeLevel(_stWinHandle, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND) != DI_OK)
			LOG("Can't set keyboard device cooperative level.", LT_ERROR);	
	
	if (_pMouse != NULL)
		if (_pMouse->SetCooperativeLevel(_stWinHandle, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND) != DI_OK)
			LOG("Can't set mouse device cooperative level.", LT_ERROR);	

	for (uint i = 0; i < _uiJoyCount; ++i)
	{
			DIPROPRANGE	pDiprg;
			DIPROPDWORD	pDipdw;

			_clDeviceInstance.push_back(new DIDEVICEINSTANCE);
			_clJoystickCaps.push_back(new DIDEVCAPS);

			_clDeviceInstance[i]->dwSize = sizeof(DIDEVICEINSTANCEA);

			if (_clJoystick[i]->GetDeviceInfo(_clDeviceInstance[i]) != DI_OK)
				LOG("Can't get joystick device info", LT_ERROR);

			_clJoystickCaps[i]->dwSize = sizeof(DIDEVCAPS);

			if (_clJoystick[i]->GetCapabilities(_clJoystickCaps[i]) != DI_OK)
				LOG("Can't get joystick capabilities", LT_ERROR);

			LOG("Initialize joystick: ", LT_INFO);
			LOG(_clDeviceInstance[i]->tszInstanceName, LT_INFO);

			pDiprg.diph.dwSize = sizeof(DIPROPRANGE);
			pDiprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			pDiprg.diph.dwHow = DIPH_BYOFFSET;
			pDiprg.lMin = -100;
			pDiprg.lMax = 100;
			
			pDiprg.diph.dwObj = DIJOFS_X;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support AxesX", LT_INFO);

			pDiprg.diph.dwObj = DIJOFS_Y;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support AxesY", LT_INFO);

			pDiprg.diph.dwObj = DIJOFS_Z;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support AxesZ", LT_INFO);

			pDiprg.diph.dwObj = DIJOFS_RZ;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support RotationZ", LT_INFO);

			pDiprg.diph.dwObj = DIJOFS_RY;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support RotationY", LT_INFO);

			pDiprg.diph.dwObj = DIJOFS_RX;
			if (_clJoystick[i]->SetProperty(DIPROP_RANGE, &pDiprg.diph) != DI_OK)
				LOG("Joystick is not support RotationX", LT_INFO);

			pDipdw.diph.dwSize = sizeof(DIPROPDWORD);
			pDipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			pDipdw.diph.dwHow = DIPH_DEVICE;
			pDipdw.dwData = 1000;

			pDipdw.diph.dwObj = DIJOFS_X;
			if (_clJoystick[i]->SetProperty(DIPROP_DEADZONE, &pDipdw.diph) != S_OK)
				LOG("Can't set property DIJOFS_X deadzone", LT_INFO);

			if (_clJoystick[i]->SetCooperativeLevel(_stWinHandle, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND) != DI_OK)
				LOG("Can't set joystick device cooperative level.", LT_ERROR);

			_stJoysts[i].uiButtonsCount = _clJoystickCaps[i]->dwButtons;			
		}

	LOG("DirectInput initialized.", LT_INFO);
}

void CDirectInput::_Free()
{
	if (_pKeyboard != NULL)
		_pKeyboard->Unacquire();

	if (_pMouse != NULL)
		_pMouse->Unacquire();

	for (uint i = 0; i < _uiJoyCount; ++i)
		_clJoystick[i]->Unacquire();
	
	_pKeyboard->Release();
	_pMouse->Release();

	for (uint i = 0; i < _uiJoyCount; ++i)
		_clJoystick[i]->Release();
	
	_pDirectInput->Release();
}

void CDirectInput::_MsgProc(const TWindowMessage &stMsg)	
{
	switch(stMsg.eMessage)
	{
		case WMT_CLOSE:
			
			if (_bHideCursor)
				ShowCursor(true);

			break;

		case WMT_DEACTIVATED:
			
			if (!_bFocused)
				break;
			
			memset(_abKeys, 0, 256 * sizeof(bool));

			_stMsts.bLeftButton = false;
			_stMsts.bRightButton = false;
			_stMsts.bMiddleButton = false;
			_bFocused = false;

			if (_bExclusive)
				ClipCursor(0, 0, 0, 0);
			
			if (_pKeyboard != NULL)
				_pKeyboard->Unacquire();

			if (_pMouse != NULL)
				_pMouse->Unacquire();

			for (uint i = 0; i < _uiJoyCount; ++i)
				_clJoystick[i]->Unacquire();

			break;

		case WMT_ACTIVATED:

			if (_pKeyboard != NULL)
				if (_pKeyboard->Acquire() != DI_OK)
					LOG("Can't acquire keyboard device.", LT_ERROR);

			if (_pMouse != NULL)
				if (_pMouse->Acquire() != DI_OK)
					LOG("Can't acquire mouse device.", LT_ERROR);
			
			for (uint i = 0; i < _uiJoyCount; ++i)
				if (_clJoystick[i]->Acquire() != DI_OK)
					LOG("Can't acquire joystick device.", LT_ERROR);

			if (_bFocused)
				break;
			
			_bFocused = true;

			break;

		case WMT_PRESENT:

			if (_bExclusive)
				_ClipCursor();

			break;

		case WMT_INPUT_CHAR:

			if (_bIsTxtInput)
			{
				if (stMsg.ui32Param1 > 31)
					_clInputTxt += (char)stMsg.ui32Param1;
				else
					if (stMsg.ui32Param1 == 8 /*Backspace*/ && _clInputTxt.length() > 0) 
						_clInputTxt.erase(_clInputTxt.length() - 1, 1);
				
				if (_uiBufSize > _clInputTxt.size())
					strcpy(_pcBuffer, _clInputTxt.c_str());
				else 
					EndTextInput();
			}			

			break;
	}
}

void CALLBACK CDirectInput::_s_Update(void *pParameter)
{
	uint dt;
	((CDirectInput *)pParameter)->_pEngineCore->GetLastUpdateDeltaTime(dt);
	((CDirectInput *)pParameter)->_Update(dt);
}

void CALLBACK CDirectInput::_s_Init(void *pParameter)
{
	((CDirectInput *)pParameter)->_Init();
}

void CALLBACK CDirectInput::_s_Free(void *pParameter)
{
	((CDirectInput *)pParameter)->_Free();
}

void CALLBACK CDirectInput::_s_EventHandler(void *pParameter, IBaseEvent *pEvent)
{
	E_EVENT_TYPE ev_type;
	TWindowMessage msg;

	pEvent->GetEventType(ev_type);

	switch(ev_type)
	{
	case ET_ON_WINDOW_MESSAGE:
		IEvWindowMessage *p_ev_msg;
		p_ev_msg = (IEvWindowMessage *)pEvent;
		p_ev_msg->GetMessage(msg);
		((CDirectInput *)pParameter)->_MsgProc(msg);
		break;
	}
}

DGLE_RESULT DGLE_API CDirectInput::Configure(E_INPUT_CONFIGURATION_FLAGS eFlags)
{
	_bExclusive	= (eFlags & ICF_EXCLUSIVE) != 0;

	_bCurBeyond = (eFlags & ICF_CURSOR_BEYOND_SCREEN) != 0;

	_bHideCursor = (eFlags & ICF_HIDE_CURSOR) != 0;

	if (_bFocused)
	{
		if (_bExclusive)
			_ClipCursor();
		else
			ClipCursor(0, 0, 0, 0);
	}

	ShowCursor(!_bHideCursor);

	return _bCurBeyond && !_bExclusive ? S_FALSE : S_OK;
} 

DGLE_RESULT DGLE_API CDirectInput::GetMouseStates(TMouseStates &stMStates)
{
	stMStates = _stMsts;
	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetKey(E_KEYBOARD_KEY_CODES eKeyCode, bool &bPressed)
{
	bPressed = _abKeys[eKeyCode];

	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetKeyName(E_KEYBOARD_KEY_CODES eKeyCode, uchar &cASCIICode)
{
	cASCIICode = EngKeyToASCIIKey(eKeyCode);
	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::BeginTextInput(char* pcBuffer, uint uiBufferSize)
{
	_pcBuffer = pcBuffer;
	_uiBufSize = uiBufferSize;
	_bIsTxtInput = true;
	_clInputTxt	= "";
	strcpy(_pcBuffer, "");
	
	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::EndTextInput()
{
	_bIsTxtInput = false;
	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetJoysticksCount(uint &uiCount)
{
	uiCount = _uiJoyCount;

	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetJoystickName(uint uiJoyId, char *pcName, uint &uiCharsCount)
{
	if (pcName)
		strcpy(pcName, " ");
	else
		uiCharsCount = 0;

	if (_uiJoyCount == 0)
	{
		LOG("Joysticks not found!", LT_WARNING);
		return E_NOTIMPL;
	}
	else
	{
		std::string strName = _clDeviceInstance[uiJoyId]->tszInstanceName;

		if (pcName == NULL)
		{
			uiCharsCount = strName.size();
			return S_OK;
		}

		if (uiCharsCount < strName.size())
		{
			LOG("Too small \"pcName\" buffer size.", LT_ERROR);
			return E_INVALIDARG;
		}

		strcpy(pcName, strName.c_str());
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetJoystickStates(uint uiJoyId, TJoystickStates &stJoyStates)
{
	stJoyStates = _stJoysts[uiJoyId];

	return S_OK;
}

DGLE_RESULT DGLE_API CDirectInput::GetType(E_ENGINE_SUB_SYSTEM &eSubSystemType)
{
	eSubSystemType = ESS_INPUT;
	return S_OK;
}

BOOL CDirectInput::_EnumJoysticks(LPCDIDEVICEINSTANCE pdInst, LPVOID pvRef)
{	
	_clJoystick.push_back(NULL);

	if (_pDirectInput->CreateDevice(pdInst->guidInstance, &_clJoystick[_uiJoyCount], NULL) != DI_OK)
	{
		LOG("Can't create joystick" + std::to_string(_uiJoyCount) + " device.", LT_INFO);

		return DIENUM_STOP;
	}

	LOG("Joystick " + std::to_string(_uiJoyCount) + " found.", LT_INFO);
	LOG(pdInst->tszInstanceName, LT_INFO);

	_uiJoyCount++;
	
	return DIENUM_CONTINUE;
}

BOOL CALLBACK CDirectInput::_s_EnumJoysticks(LPCDIDEVICEINSTANCE pdInst, LPVOID pvRef)
{
	return ((CDirectInput *)pvRef)->_EnumJoysticks(pdInst, pvRef);
}