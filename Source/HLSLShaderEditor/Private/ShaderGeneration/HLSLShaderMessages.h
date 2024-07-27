// Copyright Phyronnaz

#pragma once

#include "CoreMinimal.h"

class UHLSLShaderLibrary;

class FHLSLShaderMessages
{
public:
	template <typename FmtType, typename... Types>
	static void ShowError(const FmtType& Fmt, Types... Args)
	{
		ShowErrorImpl(FString::Printf(Fmt, Args...));
	}

	class FLibraryScope
	{
	public:
		explicit FLibraryScope(UHLSLShaderLibrary& InLibrary)
			: Guard(Library, &InLibrary)
		{
		}

	private:
		static UHLSLShaderLibrary* Library;
		TGuardValue<UHLSLShaderLibrary*> Guard;

		friend class FHLSLShaderMessages;
	};

private:
	static void ShowErrorImpl(FString Message);
};