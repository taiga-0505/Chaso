#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <dxcapi.h>
#include <d3d12.h>
#pragma comment(lib, "dxcompiler.lib")

/// @brief シェーダーコンパイルの設定構造体
struct ShaderDesc {
    std::wstring path;          ///< ファイルパス (例: L"Shader/Object3D.VS.hlsl")
    std::wstring target;        ///< ターゲットプロファイル (例: L"vs_6_0", L"ps_6_0")
    std::wstring entry = L"main"; ///< エントリポイント関数名
    std::vector<DxcDefine> defines; ///< プリプロセッサ定義の配列
    bool optimize = true;       ///< 最適化を有効にするか (/O3 相当)
    bool debugInfo = false;     ///< デバッグ情報を付与するか (/Zi)
};

/// @brief コンパイル済みシェーダーバイナリとログを保持するクラス
class CompiledShader {
public:
    CompiledShader() = default;
    /// @brief コンストラクタ
    /// @param blob シェーダーバイナリ
    /// @param log コンパイルログ（エラーメッセージ等）
    CompiledShader(Microsoft::WRL::ComPtr<IDxcBlob> blob,
                   Microsoft::WRL::ComPtr<IDxcBlobUtf8> log)
        : blob_(std::move(blob)), log_(std::move(log)) {}

    /// @brief 失敗理由のみを保持するコンストラクタ
    /// @param error DXCに到達する前に失敗した場合の理由（ファイル未検出など）
    explicit CompiledShader(std::string error) : error_(std::move(error)) {}

    /// @brief シェーダーバイナリ(Blob)を取得
    /// @return IDxcBlob
    IDxcBlob* Blob() const { return blob_.Get(); }

    /// @brief コンパイルログを取得
    /// @return ログ文字列（DXCログが無い場合は失敗理由、どちらも無ければ空文字）
    const char* Log() const {
        if (log_ && log_->GetStringLength() > 0)
            return (const char*)log_->GetStringPointer();
        return error_.c_str();
    }

    /// @brief バイナリが正常に生成されているか確認
    /// @return 生成されていればtrue
    bool HasBlob() const { return blob_ != nullptr; }

    /// @brief D3D12_SHADER_BYTECODE 構造体として取得
    /// @return そのままPSO構築に使用可能なバイトコード
    D3D12_SHADER_BYTECODE Bytecode() const {
        D3D12_SHADER_BYTECODE bc{};
        bc.pShaderBytecode = blob_ ? blob_->GetBufferPointer() : nullptr;
        bc.BytecodeLength  = blob_ ? blob_->GetBufferSize() : 0;
        return bc;
    }
private:
    Microsoft::WRL::ComPtr<IDxcBlob> blob_; ///< シェーダーバイナリ本体
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> log_; ///< エラーメッセージ等のログ
    std::string error_; ///< DXC到達前に失敗した場合の理由
};

/// @brief DirectX Shader Compiler (DXC) を用いたシェーダーコンパイラクラス
class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    /// @brief DXC関連リソースの初期化
    /// @return 成功すればtrue
    bool Init();

    /// @brief リソースの解放
    void Term();

    /// @brief HLSLファイルをコンパイルする
    /// @param desc コンパイル設定
    /// @return コンパイル結果 (失敗時は HasBlob() が false になる)
    CompiledShader Compile(const ShaderDesc& desc) const;

private:
    Microsoft::WRL::ComPtr<IDxcUtils> utils_; ///< DXCユーティリティ
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_; ///< DXCコンパイラインスタンス
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_; ///< インクルードハンドラ
};
