// Copyright Phyronnaz

#include "HLSLShaderMessages.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLShaderLibrary.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void FHLSLShaderMessages::ShowErrorImpl(FString Message)
{
	if (FLibraryScope::Library)
	{
		Message = FLibraryScope::Library->File.FilePath + ": " + Message;
	}

	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = 10.f;
	Info.CheckBoxState = ECheckBoxState::Unchecked;
	FSlateNotificationManager::Get().AddNotification(Info);

	UE_LOG(LogHLSLMaterial, Error, TEXT("%s"), *Message);
}

UHLSLShaderLibrary* FHLSLShaderMessages::FLibraryScope::Library;