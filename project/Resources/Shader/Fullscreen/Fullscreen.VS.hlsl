#include "Fullscreen.hlsli"

static const uint32_t kNumVerTex = 3;
static const float4 kPositions[kNumVerTex] = {
    {-1.0f,1.0f,0.0f,1.0f}, // 左上
    {3.0f,1.0f,0.0f,1.0f}, // 右上
    {-1.0f,-3.0f,0.0f,1.0f}, //左下
};

static const float2 kTexCoords[kNumVerTex] = {
    {0.0f,0.0f}, // 左上
    {2.0f,0.0f}, // 右上
    {0.0f,2.0f}, //左下
};

VertexShaderOutput main(uint32_t vertexId : SV_VertexID) {
    VertexShaderOutput output;
    
    output.position = kPositions[vertexId];
    output.texcoord = kTexCoords[vertexId];
    return output;
}
