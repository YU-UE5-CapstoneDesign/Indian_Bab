#include "SaveGame/SettingSaveGame.h"


USettingSaveGame::USettingSaveGame()
{
    // 게임의 기본 설정값 (초기화 시 돌아갈 값)
    MasterVolume = 0.5f;
    MouseSensitivity = 0.5f;
	ScreenBrightness = 0.5f;
	ResolutionIndex = 0;
	ResolutionWidth = 0;
	ResolutionHeight = 0;
	RefreshRate = 0;
	bHasDisplayModeValues = false;
	WindowModeIndex = 0; // 기본값을 '전체 화면'으로 설정

    // 저장 파일 이름과 인덱스
    SaveSlotName = TEXT("GameSettings");
    UserIndex = 0;
}
