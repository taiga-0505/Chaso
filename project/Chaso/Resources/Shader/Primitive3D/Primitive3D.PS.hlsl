struct PSIn
{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PSIn i) : SV_TARGET
{
    return i.color;
}
