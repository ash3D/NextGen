/*
\author		Arseny Lezin aka SleepWalk
\date		25.10.2012 (c)Arseny Lezin

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "Common.h"

void ClipCursor(uint left, uint top, uint right, uint bottom);

class CDirectInput : IInput
{
	friend void LogWrite(uint uiInstIdx, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

private:

	uint _uiInstIdx;
	IEngineCore *_pEngineCore;

	TWindowHandle _stWinHandle;

	LPDIRECTINPUT8 _pDirectInput;

	LPDIRECTINPUTDEVICE8 _pKeyboard;
	LPDIRECTINPUTDEVICE8 _pMouse;
	std::vector<LPDIRECTINPUTDEVICE8> _clJoystick;

	uint _uiJoyCount;

	std::vector<LPDIDEVICEINSTANCEA> _clDeviceInstance;
	std::vector<LPDIDEVCAPS> _clJoystickCaps;

	DIMOUSESTATE2 _pMouseState;
	DIJOYSTATE _pJoystickState;

	bool _bExclusive, _bHideCursor,
		_bFocused, _bCurBeyond;

	bool _bIsTxtInput;
	char *_pcBuffer;
	std::string _clInputTxt;
	uint _uiBufSize;

	bool _abKeys[256];
	TMouseStates _stMsts;

	std::vector<TJoystickStates> _stJoysts;

	void _ClipCursor();
	void _Update(uint uiDeltaTime);
	void _Init();
	void _Free();
	void _MsgProc(const TWindowMessage &stMsg);

	BOOL _EnumJoysticks(LPCDIDEVICEINSTANCE pdInst, LPVOID pvRef);

	static void CALLBACK _s_EventHandler(void *pParameter, IBaseEvent *pEvent);
	static void CALLBACK _s_Update(void *pParameter);
	static void CALLBACK _s_Init(void *pParameter);
	static void CALLBACK _s_Free(void *pParameter);

	static BOOL CALLBACK _s_EnumJoysticks(LPCDIDEVICEINSTANCE pdInst, LPVOID pvRef);

public:

	CDirectInput(IEngineCore *pEngineCore);
	~CDirectInput();

	DGLE_RESULT DGLE_API Configure(E_INPUT_CONFIGURATION_FLAGS eFlags = ICF_DEFAULT);

	DGLE_RESULT DGLE_API GetMouseStates(TMouseStates &stMStates);
	DGLE_RESULT DGLE_API GetKey(E_KEYBOARD_KEY_CODES eKeyCode, bool &bPressed);
	DGLE_RESULT DGLE_API GetKeyName(E_KEYBOARD_KEY_CODES eKeyCode, uchar &cASCIICode);

	DGLE_RESULT DGLE_API BeginTextInput(char* pcBuffer, uint uiBufferSize);
	DGLE_RESULT DGLE_API EndTextInput();

	DGLE_RESULT DGLE_API GetJoysticksCount(uint &uiCount);
	DGLE_RESULT DGLE_API GetJoystickName(uint uiJoyId, char *pcName, uint &uiCharsCount);
	DGLE_RESULT DGLE_API GetJoystickStates(uint uiJoyId, TJoystickStates &stJoyStates);

	DGLE_RESULT DGLE_API GetType(E_ENGINE_SUB_SYSTEM &eSubSystemType);

	IDGLE_BASE_IMPLEMENTATION(IInput)
};