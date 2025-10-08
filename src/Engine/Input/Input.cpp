#include "Input.h"
#include <cassert>
#include <cmath>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"xinput.lib")

const float Input::XBOX_STICK_DEADZONE = 0.2f;

void Input::Initialize(WinApp* winApp)
{
	winApp_ = winApp;
	HRESULT hr;

	hr = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(hr));

	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(hr));

	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	hr = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	hr = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
	assert(SUCCEEDED(hr));

	hr = mouse->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(hr));

	hr = mouse->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));
	
	RECT windowRect;
	GetClientRect(winApp->GetHwnd(), &windowRect);
	windowCenter_.x = (windowRect.right - windowRect.left) / 2;
	windowCenter_.y = (windowRect.bottom - windowRect.top) / 2;
	
	memset(&mouseState_, 0, sizeof(mouseState_));
	memset(&previousMouseState_, 0, sizeof(previousMouseState_));
	
	for (int i = 0; i < XUSER_MAX_COUNT; ++i) {
		memset(&xboxControllerState_[i], 0, sizeof(XINPUT_STATE));
		memset(&previousXboxControllerState_[i], 0, sizeof(XINPUT_STATE));
		xboxControllerConnected_[i] = false;
	}
}

void Input::Update()
{
	memcpy(preKey, key, sizeof(key));
	keyboard->Acquire();
	keyboard->GetDeviceState(sizeof(key), key);

	memcpy(&previousMouseState_, &mouseState_, sizeof(mouseState_));
	mouse->Acquire();
	mouse->GetDeviceState(sizeof(mouseState_), &mouseState_);
	
	for (int i = 0; i < XUSER_MAX_COUNT; ++i) {
		memcpy(&previousXboxControllerState_[i], &xboxControllerState_[i], sizeof(XINPUT_STATE));
		
		DWORD result = XInputGetState(i, &xboxControllerState_[i]);
		xboxControllerConnected_[i] = (result == ERROR_SUCCESS);
	}
}

void Input::Finalize()
{
	SetMouseCursor(true);
	if (mouse) {
		mouse->Unacquire();
		mouse->Release();
		mouse = nullptr;
	}

	if (keyboard) {
		keyboard->Unacquire();
		keyboard->Release();
		keyboard = nullptr;
	}

	if (directInput) {
		directInput->Release();
		directInput = nullptr;
	}
}

bool Input::PushKey(BYTE keyNumber)
{
	if (key[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	if (key[keyNumber] && !preKey[keyNumber]) {
		return true;
	}
	return false;
}

HRESULT Input::GetMouseState(DIMOUSESTATE* mouseState)
{
	return mouse->GetDeviceState(sizeof(DIMOUSESTATE), mouseState);
}

void Input::SetMouseCursor(bool visible)
{
	if (visible) {
		while (ShowCursor(TRUE) < 0) {}
		SetCursor(LoadCursor(NULL, IDC_ARROW));
	} else {
		while (ShowCursor(FALSE) >= 0) {}
		SetCursor(NULL);
	}
}

void Input::GetMouseMovement(float& deltaX, float& deltaY)
{
	deltaX = static_cast<float>(mouseState_.lX);
	deltaY = static_cast<float>(mouseState_.lY);
}

void Input::ResetMouseCenter()
{
	POINT centerPoint = windowCenter_;
	ClientToScreen(winApp_->GetHwnd(), &centerPoint);
	SetCursorPos(centerPoint.x, centerPoint.y);
}

void Input::UpdateWindowCenter()
{
	RECT windowRect;
	GetClientRect(winApp_->GetHwnd(), &windowRect);
	windowCenter_.x = (windowRect.right - windowRect.left) / 2;
	windowCenter_.y = (windowRect.bottom - windowRect.top) / 2;
}

bool Input::IsMouseButtonTriggered(int button)
{
	if (button < 0 || button >= 3) return false;

	// 現在押されていて、前フレームで押されていなければトリガー
	return (mouseState_.rgbButtons[button] & 0x80) && !(previousMouseState_.rgbButtons[button] & 0x80);
}

bool Input::IsXboxControllerConnected(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return false;
	return xboxControllerConnected_[playerIndex];
}

bool Input::IsXboxButtonPressed(int button, int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return false;
	if (!xboxControllerConnected_[playerIndex]) return false;
	
	return (xboxControllerState_[playerIndex].Gamepad.wButtons & button) != 0;
}

bool Input::IsXboxButtonTriggered(int button, int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return false;
	if (!xboxControllerConnected_[playerIndex]) return false;
	
	bool currentPressed = (xboxControllerState_[playerIndex].Gamepad.wButtons & button) != 0;
	bool previousPressed = (previousXboxControllerState_[playerIndex].Gamepad.wButtons & button) != 0;
	
	return currentPressed && !previousPressed;
}

float Input::GetXboxLeftStickX(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	float x = static_cast<float>(xboxControllerState_[playerIndex].Gamepad.sThumbLX) / 32767.0f;
	return (std::abs(x) < XBOX_STICK_DEADZONE) ? 0.0f : x;
}

float Input::GetXboxLeftStickY(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	float y = static_cast<float>(xboxControllerState_[playerIndex].Gamepad.sThumbLY) / 32767.0f;
	return (std::abs(y) < XBOX_STICK_DEADZONE) ? 0.0f : y;
}

float Input::GetXboxRightStickX(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	float x = static_cast<float>(xboxControllerState_[playerIndex].Gamepad.sThumbRX) / 32767.0f;
	return (std::abs(x) < XBOX_STICK_DEADZONE) ? 0.0f : x;
}

float Input::GetXboxRightStickY(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	float y = static_cast<float>(xboxControllerState_[playerIndex].Gamepad.sThumbRY) / 32767.0f;
	return (std::abs(y) < XBOX_STICK_DEADZONE) ? 0.0f : y;
}

float Input::GetXboxLeftTrigger(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	return static_cast<float>(xboxControllerState_[playerIndex].Gamepad.bLeftTrigger) / 255.0f;
}

float Input::GetXboxRightTrigger(int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= XUSER_MAX_COUNT) return 0.0f;
	if (!xboxControllerConnected_[playerIndex]) return 0.0f;
	
	return static_cast<float>(xboxControllerState_[playerIndex].Gamepad.bRightTrigger) / 255.0f;
}