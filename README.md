# HLSL Material for Unreal Engine
This is an extension of [Phyronnazs work](https://github.com/Phyronnaz/HLSLMaterial) aiming to extend the functionality to also work with generating materials and not material functions. I'm just gonna focus on the material side of things instead of material functions so any modifications made to Phyronnazs code will just probably be relegated to getting it working with updated versions of Unreal Engine in case anything breaks. A lot of what worked with Phyronnazs plugin should also work here in the context of materials since a good deal of the code is based off of their original work.

## Features
* HLSL Shaders: Write your entire material in HLSL with a custom syntax reminiscent of raw HLSL code
* Custom editor for the HLSL Shader library assets that allow for realtime visualizing of the shader with a dummy material instance viewport to mess around with parameters. Never needing to open the original material.

![Screenshot 2024-09-19 194640](https://github.com/user-attachments/assets/90658114-56b4-46d7-bd42-6df8a71b3e62)

## How to
* Create a new `HLSL Shader Library` (right click Content Browser -> Material & Textures). This asset will be the link between your hlsl file and all the generated materials.
* Set the `File` on it to point to your HLSL file
* Material will be created when you save the HLSL file or click RecompileShader in the custom editor
* Clicking `Create Material Instance` will create a material instance of the generated material in the same path as the material
* **Alternatively**, import your hlsl files directly in the editor and it'll auto set everything up for you

## Syntax
For more information on the syntax used to generate the materials, check out the (hopefully not outdated) wiki.
* HLSL Materials use a custom syntax for specifying material parameters and outputs, using a syntax familiar to raw HLSL with input structs & output structs.
* Semantics on output structs determine which material attribute they end up being plugged into

```hlsl
#define DOMAIN PostProcess
#define BLENDABLELOCATION After_Tonemapping

#pragma fragment PSMain
#pragma psin PSInput
#pragma psout PSOutput

struct PSInput
{
	[Range(0, 1)]
	float NoiseIntensity = 0.5f;
	[Range(-10, 10)]
	float2 PanSpeed = float2(0.5f, -2.f);
	[SamplerType(Normal)]
	Texture2D NoiseTex;
	SamplerState NoiseTexSampler;
};

struct PSOutput
{
	float3 Color : EMISSIVE;
};

void PSMain(FMaterialPixelParameters Parameters, PSInput pin, out PSOutput pout)
{
	float2 UV = GetDefaultSceneTextureUV(Parameters, PPI_PostProcessInput0);
	float2 noiseSample = (pin.NoiseTex.Sample(pin.NoiseTexSampler, UV + pin.PanSpeed * View.GameTime).rg - 0.5f) * 2.f;
	noiseSample = lerp(0.f, noiseSample, pin.NoiseIntensity);

	UV += noiseSample;
	pout.Color = SceneTextureLookup(UV, PPI_PostProcessInput0, false).rgb;
};
```

## How it works

Similar to Phyronnazs work with Material Function generation, this extension works by parsing the HLSL file and then:
* Look for specific flags to specify what the material is (post process, surface, UI, etc...) as well as any additional settings specific to a material domain
* Find the input/output struct for each shader stage (vertex/normal/pixel) as specified at the top of the shader files (#pragma structType structName)
* * Based on those, generate the appropriate material parameters
* Find the function for each shader stage (vertex/normal/pixel) as specified at the top of the shader files (#pragma stage functionName)
* * Toss the function bodies into custom HLSL nodes in the generated material
Given the similarity in function, the HLSL code doesn't need to be shared after the material is generated and can be deleted with no issue as the materials themselves dont depend on the plugin or any HLSL files.

The generated material uses the same name as the HLSL Shader Library asset with the `M_` prefix added.

## TODOs
* Add developer setting options to specify the paths where materials and material instances should be generated
* When clicking `Create Material Instance` in the HLSL Shader Library editor, create the material instance with the same parameters as the preview instance in the editor
* Further work to modularize the shader output semantics handling to allow for outputs that are not just material attributes (e.g landscape layers)
* Support for different types of switches (e.g quality switches, shadow pass switches, etc...)
