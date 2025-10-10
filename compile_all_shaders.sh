#!/bin/bash
cd "externals/DirectXTex/Shaders/Compiled"

FXC="/c/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/fxc.exe"

echo "Compiling BC7 shaders..."
"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -ETryMode456CS -FhBC7Encode_TryMode456CS.inc -FdBC7Encode_TryMode456CS.pdb -VnBC7Encode_TryMode456CS
"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -ETryMode456CS -FhBC7Encode_TryMode456CS_cs40.inc -FdBC7Encode_TryMode456CS_cs40.pdb -VnBC7Encode_TryMode456CS

"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -ETryMode137CS -FhBC7Encode_TryMode137CS.inc -FdBC7Encode_TryMode137CS.pdb -VnBC7Encode_TryMode137CS
"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -ETryMode137CS -FhBC7Encode_TryMode137CS_cs40.inc -FdBC7Encode_TryMode137CS_cs40.pdb -VnBC7Encode_TryMode137CS

"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -ETryMode02CS -FhBC7Encode_TryMode02CS.inc -FdBC7Encode_TryMode02CS.pdb -VnBC7Encode_TryMode02CS
"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -ETryMode02CS -FhBC7Encode_TryMode02CS_cs40.inc -FdBC7Encode_TryMode02CS_cs40.pdb -VnBC7Encode_TryMode02CS

"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -EEncodeBlockCS -FhBC7Encode_EncodeBlockCS.inc -FdBC7Encode_EncodeBlockCS.pdb -VnBC7Encode_EncodeBlockCS
"$FXC" ../BC7Encode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -EEncodeBlockCS -FhBC7Encode_EncodeBlockCS_cs40.inc -FdBC7Encode_EncodeBlockCS_cs40.pdb -VnBC7Encode_EncodeBlockCS

echo "Compiling BC6H shaders..."
"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -ETryModeG10CS -FhBC6HEncode_TryModeG10CS.inc -FdBC6HEncode_TryModeG10CS.pdb -VnBC6HEncode_TryModeG10CS
"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -ETryModeG10CS -FhBC6HEncode_TryModeG10CS_cs40.inc -FdBC6HEncode_TryModeG10CS_cs40.pdb -VnBC6HEncode_TryModeG10CS

"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -ETryModeLE10CS -FhBC6HEncode_TryModeLE10CS.inc -FdBC6HEncode_TryModeLE10CS.pdb -VnBC6HEncode_TryModeLE10CS
"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -ETryModeLE10CS -FhBC6HEncode_TryModeLE10CS_cs40.inc -FdBC6HEncode_TryModeLE10CS_cs40.pdb -VnBC6HEncode_TryModeLE10CS

"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_5_0 -EEncodeBlockCS -FhBC6HEncode_EncodeBlockCS.inc -FdBC6HEncode_EncodeBlockCS.pdb -VnBC6HEncode_EncodeBlockCS
"$FXC" ../BC6HEncode.hlsl -nologo -WX -Ges -Zi -Zpc -Qstrip_reflect -Qstrip_debug -Tcs_4_0 -DEMULATE_F16C -EEncodeBlockCS -FhBC6HEncode_EncodeBlockCS_cs40.inc -FdBC6HEncode_EncodeBlockCS_cs40.pdb -VnBC6HEncode_EncodeBlockCS

echo "All shaders compiled successfully!"
