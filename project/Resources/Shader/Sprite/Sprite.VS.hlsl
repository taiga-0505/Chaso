struct SpriteVSInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

struct SpriteVSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
SpriteVSOutput main(SpriteVSInput input)
{
    SpriteVSOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
