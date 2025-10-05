#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>
#include "WinApp.h"
#define XBOX_BUTTON_A 0x1000
#define XBOX_BUTTON_B 0x2000
#define XBOX_BUTTON_X 0x4000
#define XBOX_BUTTON_Y 0x8000
#define XBOX_BUTTON_LB 0x0100
#define XBOX_BUTTON_RB 0x0200
#define XBOX_BUTTON_BACK 0x0020
#define XBOX_BUTTON_START 0x0010
#define XBOX_BUTTON_LSTICK 0x0040
#define XBOX_BUTTON_RSTICK 0x0080
#define XBOX_DPAD_UP 0x0001
#define XBOX_DPAD_DOWN 0x0002
#define XBOX_DPAD_LEFT 0x0004
#define XBOX_DPAD_RIGHT 0x0008

class Input
{

public:

	template <class T>using ComPtr = Microsoft::WRL::ComPtr<T>;

	void Initialize(WinApp* winApp);
	void Update();
	void Finalize();

	bool PushKey(BYTE keyNumber);
	bool TriggerKey(BYTE keyNumber);

	HRESULT GetMouseState(DIMOUSESTATE* mouseState);
	void SetMouseCursor(bool visible);
	void GetMouseMovement(float& deltaX, float& deltaY);
	void ResetMouseCenter();
	bool IsMouseButtonTriggered(int button);  // 0:左, 1:右, 2:中
	bool IsXboxControllerConnected(int playerIndex = 0);
	bool IsXboxButtonPressed(int button, int playerIndex = 0);
	bool IsXboxButtonTriggered(int button, int playerIndex = 0);
	float GetXboxLeftStickX(int playerIndex = 0);
	float GetXboxLeftStickY(int playerIndex = 0);
	float GetXboxRightStickX(int playerIndex = 0);
	float GetXboxRightStickY(int playerIndex = 0);
	float GetXboxLeftTrigger(int playerIndex = 0);
	float GetXboxRightTrigger(int playerIndex = 0);

private:
	BYTE key[256] = {};
	BYTE preKey[256] = {};
	ComPtr<IDirectInputDevice8>keyboard;
	ComPtr<IDirectInputDevice8>mouse;
	ComPtr<IDirectInput8>directInput = nullptr;
	WinApp* winApp_ = nullptr;
	
	DIMOUSESTATE mouseState_;
	DIMOUSESTATE previousMouseState_;
	POINT windowCenter_;
	
	XINPUT_STATE xboxControllerState_[XUSER_MAX_COUNT];
	XINPUT_STATE previousXboxControllerState_[XUSER_MAX_COUNT];
	bool xboxControllerConnected_[XUSER_MAX_COUNT];
	
	static const float XBOX_STICK_DEADZONE;
};