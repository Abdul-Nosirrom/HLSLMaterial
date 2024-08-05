// Fill out your copyright notice in the Description page of Project Settings.


#include "HLSLShaderParser.h"

#include "HLSLShader.h"
#include "HLSLShaderLibrary.h"
#include "HLSLShaderMessages.h"
#include "ShaderCore.h"
#include "Internationalization/Regex.h"
#include "Misc/Paths.h"

FString FHLSLShaderParser::Parse(const UHLSLShaderLibrary& Library, FString Text, TArray<FHLSLMaterialShader>& OutFunctions,
	TArray<FHLSLStruct>& OutStructs)
{
	enum class EScope
	{
		Global,
		Preprocessor,
		GlobalComment,
		FunctionReturn,
		FunctionName,
		FunctionArgs,
		FunctionBodyStart,
		FunctionBody,
		StructStart,
		StructName,
		StructBodyStart,
		StructBody,
	};

	EScope Scope = EScope::Global;
	int32 Index = 0;
	int32 ScopeDepth = 0; // Increment when encountering {, decrement when encoutnering }
	int32 ArgParenthesisScopeDepth = 0; // Increment when encountering ( decrement when ) in FunctionArgs scope
	int32 LineNumber = 0;

	// Simplify line breaks handling
	Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

	while (Index < Text.Len())
	{
		FString Token;
		for (int32 TokenIndex = Index; TokenIndex < Text.Len() && !FChar::IsWhitespace(Text[TokenIndex]); TokenIndex++)
		{
			Token += Text[TokenIndex];
		}
		
		const TCHAR Char = Text[Index++];
		
		if (FChar::IsLinebreak(Char))
		{
			LineNumber++;
		}

		// State machine
		switch (Scope)
		{
		case EScope::Global:
		{
			ensure(ScopeDepth == 0);

			// Skip over line breaks and white space
			if (FChar::IsWhitespace(Char) || FChar::IsLinebreak(Char))
			{
				continue;
			}

			if (OutFunctions.Num() == 0 || !OutFunctions.Last().ReturnType.IsEmpty())
			{
				OutFunctions.Emplace();
			}

			// Decrement the index so when we go into the next states, we're not skipping thee first character
			Index--;

			if (Char == TEXT('#'))
			{
				Scope = EScope::Preprocessor;
			}
			else if (Char == TEXT('/'))
			{
				Scope = EScope::GlobalComment;
			}
			else if (Token == TEXT("struct"))
			{
				Scope = EScope::StructStart;
				OutStructs.Emplace();
			}
			else
			{
				Scope = EScope::FunctionReturn;
			}
		}
		break;
		case EScope::Preprocessor:
		case EScope::GlobalComment:
		{
			// Don't care for global comments & we've already preprocessed # directives, so skip till the next line
			if (!FChar::IsLinebreak(Char)) continue;
			Scope = EScope::Global;
		}
		break;
		case EScope::FunctionReturn:
		{
			if (!FChar::IsWhitespace(Char))
			{
				OutFunctions.Last().ReturnType += Char;
				continue;
			}

			Scope = EScope::FunctionName;
		}
		break;
		case EScope::FunctionName:
		{
			// Stop when we encounter the function args 
			if (Char != TEXT('('))
			{
				if (!FChar::IsWhitespace(Char))
				{
					OutFunctions.Last().Name += Char;
				}
				continue;
			}

			Scope = EScope::FunctionArgs;

			ensure(ArgParenthesisScopeDepth == 0);
			ArgParenthesisScopeDepth++;
		}
		break;
		case EScope::FunctionArgs:
		{
			// We don't worry about checking parenthesis scope depth here as we should never encoutner an open scope, function arguments should always be of the form:
			// (FMaterialPixel/VertexParameters Parameters, VS/PS/NormInput input, out VS/PS/NormOutput output)

			// What we care about here is the names of the input/out structs, because we're gonna be stripping them from the function body

			if (Char == TEXT('(')) ArgParenthesisScopeDepth++;
			if (Char == TEXT(')'))
			{
				ArgParenthesisScopeDepth--;
				ensure(ArgParenthesisScopeDepth >= 0);
			}

			if (ArgParenthesisScopeDepth > 0)
			{
				if (Char == TEXT(',') && ArgParenthesisScopeDepth == 1)
				{
					OutFunctions.Last().Arguments.Emplace();
				}
				else
				{
					if (OutFunctions.Last().Arguments.Num() == 0) OutFunctions.Last().Arguments.Emplace();

					OutFunctions.Last().Arguments.Last() += Char;
				}
				continue;
			}
			
			Scope = EScope::FunctionBodyStart;
		}
		break;
		case EScope::FunctionBodyStart:
		{
			ensure(ScopeDepth == 0);

			if (Char != TEXT('{')) continue;

			/* Fix above for comments right after function args
			if (FChar::IsWhitespace(Char)) continue;

			if (Char != TEXT('{'))
			{
				return FString::Printf(TEXT("Invalid function body for %s: missing {"), *OutFunctions.Last().Name);
			}
			*/
			
			if (Library.bAccurateErrors)
			{
				OutFunctions.Last().StartLine = LineNumber;
			}

			Scope = EScope::FunctionBody;
			ScopeDepth++;
		}
		break;
		case EScope::FunctionBody:
		{
			ensure(ScopeDepth > 0);

			if (Char == TEXT('{'))
			{
				ScopeDepth++;
			}
			if (Char == TEXT('}'))
			{
				ScopeDepth--;
			}

			if (ScopeDepth > 0)
			{
				OutFunctions.Last().Body += Char;
				continue;
			}
			if (ScopeDepth < 0)
			{
				return FString::Printf(TEXT("Invalid function body for %s: too many }"), *OutFunctions.Last().Name);
			}

			ensure(ScopeDepth == 0);
			Scope = EScope::Global;
		}
		break;
		case EScope::StructStart:
		{
			if (!FChar::IsWhitespace(Char)) continue;

			Scope = EScope::StructName;
		}
		break;
		case EScope::StructName:
		{
			if (Char != TEXT('{') && Char != TEXT('/'))
			{
				if (!FChar::IsWhitespace(Char) || !FChar::IsLinebreak(Char))
				{
					OutStructs.Last().Name += Char;
				}
				continue;
			}

			if (Char == TEXT('{'))
			{
				Scope = EScope::StructBody;
				ScopeDepth++;
			}
			else
			{
				Scope = EScope::StructBodyStart;
			}
		}
		break;
		case EScope::StructBodyStart:
		{
			ensure(ScopeDepth == 0);
			
			if (Char != TEXT('{')) continue;

			/* Fix above for comment after struct name, we know enter the structbody state earlier than before.
			if (FChar::IsWhitespace(Char) || FChar::IsLinebreak(Char)) continue;

			if (Char != TEXT('{'))
			{
				return FString::Printf(TEXT("Invalid struct body for %s: missing {"), *OutStructs.Last().Name);
			}
			*/
			
			Scope = EScope::StructBody;
			ScopeDepth++;
		}
		break;
		case EScope::StructBody:
		{
			if (Char == TEXT(';') && ScopeDepth == 0)
			{
				Scope = EScope::Global;
				continue;
			}
			
			ensure(ScopeDepth > 0);

			if (Char == TEXT('{')) ScopeDepth++;
			if (Char == TEXT('}')) ScopeDepth--;
			
			if (ScopeDepth > 0)
			{
				OutStructs.Last().Body += Char;
				continue;
			}
			if (ScopeDepth < 0)
			{
				return FString::Printf(TEXT("Invalid struct body for %s: too many }"), *OutFunctions.Last().Name);
			}

			// We exit when scope depth == 0 & we encounter a ;
			ensure(ScopeDepth == 0);
		}
		break;
		default: ensure(false);
		}
	}

	// We're always adding 1 additional function to the array, so lets remove that
	if (OutFunctions.Last().ReturnType.IsEmpty())
		OutFunctions.RemoveAtSwap(OutFunctions.Num()-1);

	if (Scope != EScope::Global && Scope != EScope::GlobalComment)
	{
		return TEXT("Parsing Error");
	}

	ensure(ScopeDepth == 0);
	ensure(ArgParenthesisScopeDepth == 0);

	return {};
}

TArray<FHLSLShaderParser::FInclude> FHLSLShaderParser::GetIncludes(const FString& FilePath, const FString& Text)
{
	FString VirtualFolder;
	if (UHLSLShaderLibrary::TryConvertFilenameToShaderPath(FilePath, VirtualFolder))
	{
		VirtualFolder = FPaths::GetPath(VirtualFolder);
	}

	TArray<FInclude> OutIncludes;

	FRegexPattern RegexPattern(R"_((\A|\v)\s*#include "([^"]+)")_");
	FRegexMatcher RegexMatcher(RegexPattern, Text);
	while (RegexMatcher.FindNext())
	{
		FString VirtualPath = RegexMatcher.GetCaptureGroup(2);
		// Nice includes to have for intellisense but dont actually include them as it wont work
		if (VirtualPath.Contains("MaterialTemplate.ush", ESearchCase::CaseSensitive)) continue;
		
		if (!VirtualPath.StartsWith(TEXT("/")) && !VirtualFolder.IsEmpty())
		{
			// Relative path
			VirtualPath = VirtualFolder / VirtualPath;
		}

		FString DiskPath = GetShaderSourceFilePath(VirtualPath);
		if (DiskPath.IsEmpty())
		{
			FHLSLShaderMessages::ShowError(TEXT("Failed to map include %s"), *VirtualPath);
		}
		else
		{
			DiskPath = FPaths::ConvertRelativePathToFull(DiskPath);
		}

		OutIncludes.Add({ VirtualPath, DiskPath });
	}

	return OutIncludes;
}

TArray<FHLSLShaderParser::FSetting> FHLSLShaderParser::GetSettings(const FString& Text)
{
	TArray<FHLSLShaderParser::FSetting> OutDefines;

	FRegexPattern RegexPattern(R"_((\A|\v)\s*#define (\w*) (.*))_");
	FRegexMatcher RegexMatcher(RegexPattern, Text);
	while (RegexMatcher.FindNext())
	{
		OutDefines.Add({ RegexMatcher.GetCaptureGroup(2), RegexMatcher.GetCaptureGroup(3) });
	}

	return OutDefines;
}

TArray<FHLSLShaderParser::FPragmaDeclarations> FHLSLShaderParser::GetPragmaDeclarations(const FString& Text)
{
	TArray<FHLSLShaderParser::FPragmaDeclarations> OutDefines;

	FRegexPattern RegexPattern(R"_((\A|\v)\s*#pragma (\w*) (\w*))_");
	FRegexMatcher RegexMatcher(RegexPattern, Text);
	while (RegexMatcher.FindNext())
	{
		OutDefines.Add({ RegexMatcher.GetCaptureGroup(2), RegexMatcher.GetCaptureGroup(3) });
	}

	return OutDefines;
}

FString FHLSLShaderParser::ParseInputStructs(const UHLSLShaderLibrary& Library, const FHLSLStruct& Struct, TArray<FHLSLShaderInput>& Inputs)
{
	FRegexPattern RegexPattern(""
		R"_(\s*)_"                      // Start
		R"_((?:\[(.*)\])?)_"             // [Metadata]
		R"_(\s*)_"                       // Spaces
		R"_((?:(\/\/\s*))?)_"			// Possible comment that we wanna check if its empty (unless theres a newline following it)
		R"_((?:(const\s+)?))_"			// Either const or out
		R"_((\w*))_"                     // Type
		R"_(\s+)_"                       // Spaces
		R"_((\w*))_"                     // Name
		R"_((?:\s*=\s*(.+))?)_"          // Optional default value
		R"_(\s*;)_");                    // End
	
	FRegexMatcher RegexMatcher(RegexPattern, Struct.Body);

	while (RegexMatcher.FindNext())
	{
		if (!RegexMatcher.GetCaptureGroup(2).IsEmpty() && !RegexMatcher.GetCaptureGroup(2).Contains("\n")) continue;

		// If we're dealing with a SamplerState, then skip it since it gets autogenerated for texture objects
		if (RegexMatcher.GetCaptureGroup(4) == "SamplerState") continue;
		
		FHLSLShaderInput& Input = Inputs.Emplace_GetRef();
		Input.MetaStringRaw = RegexMatcher.GetCaptureGroup(1);
		Input.bIsConst = !RegexMatcher.GetCaptureGroup(3).IsEmpty();
		Input.Type = RegexMatcher.GetCaptureGroup(4);
		Input.Name = RegexMatcher.GetCaptureGroup(5);
		Input.DefaultValue = RegexMatcher.GetCaptureGroup(6);

		FString Error = FHLSLShaderInput::ParseMetaAndDefault(Library, Input);
		if (!Error.IsEmpty()) return Error;
	}

	return "";
}

FString FHLSLShaderParser::ParseOutputStructs(const UHLSLShaderLibrary& Library, const FHLSLStruct& Struct, TArray<FHLSLShaderOutput>& Outputs)
{
	FRegexPattern RegexPattern(""
		R"_(\s*)_"                      // Start
		R"_((\w+))_"					// Type
		R"_(\s+)_"						// Spaces
		R"_((\w+))_"                       // Name
		R"_(\s*:\s*)_"						// Semantic divider
		R"_((\w+))_"			// Semantic
		R"_(\s*;)_");                     // End
	
	FRegexMatcher RegexMatcher(RegexPattern, Struct.Body);

	while (RegexMatcher.FindNext())
	{
		FHLSLShaderOutput& Output = Outputs.Emplace_GetRef();
		Output.Type = RegexMatcher.GetCaptureGroup(1);
		Output.Name = RegexMatcher.GetCaptureGroup(2);
		Output.Semantic = RegexMatcher.GetCaptureGroup(3);

		FString Error = FHLSLShaderOutput::ParseTypeAndSemantic(Output);
		if (!Error.IsEmpty()) return Error;
	}
	
	return "";
}
