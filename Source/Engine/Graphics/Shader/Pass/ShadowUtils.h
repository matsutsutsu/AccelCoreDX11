#include <DirectXMath.h>
#include <vector>
#include <algorithm>

using namespace DirectX;

// ヘルパー関数：指定した View-Projection 行列から、ワールド空間での視錐台の8頂点を取得する
inline std::vector<XMFLOAT3> GetFrustumCornersWorldSpace(const XMMATRIX& view, const XMMATRIX& proj)
{
    XMMATRIX viewProj = view * proj;
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

    std::vector<XMFLOAT3> corners(8);
    // NDC(正規化デバイス座標)空間での箱の8頂点
    const XMFLOAT3 ndcCorners[8] = {
        XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f,  1.0f, 0.0f),
        XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT3(-1.0f, -1.0f, 0.0f),
        XMFLOAT3(-1.0f,  1.0f, 1.0f), XMFLOAT3(1.0f,  1.0f, 1.0f),
        XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, -1.0f, 1.0f)
    };

    for (int i = 0; i < 8; ++i) {
        XMVECTOR v = XMLoadFloat3(&ndcCorners[i]);
        // W=1.0 として座標変換
        XMVECTOR transformed = XMVector3TransformCoord(v, invViewProj);
        XMStoreFloat3(&corners[i], transformed);
    }
    return corners;
}

// CSMの行列を計算するメイン関数
// ※ cameraView: 現在のカメラのView行列
// ※ fov, aspectRatio: カメラの設定
// ※ cascadeSplits: float[3] { 15.0f, 50.0f, 200.0f } などの境界値
inline void CalculateCascadeMatrices(
    const XMMATRIX& cameraView,
    float fov, float aspectRatio,
    const XMFLOAT3& lightDir,
    const float cascadeSplits[3],
    XMMATRIX outLightVP[3])
{
    // カメラのZ方向の分割点を設定（Nearクリップを含めた4点）
    float frustumIntervals[4] = { 0.1f, cascadeSplits[0], cascadeSplits[1], cascadeSplits[2] };

    // ライトのView行列を作成 (原点からライトの逆方向を見る)
    XMVECTOR lDir = XMLoadFloat3(&lightDir);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    // ライトが真上や真下を向いている場合の例外処理
    if (abs(XMVectorGetY(lDir)) > 0.999f) up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookToLH(XMVectorZero(), lDir, up);

    // 3つのカスケード層について計算
    for (int i = 0; i < 3; ++i)
    {
        // 1. このカスケード層専用のProjection行列を作る
        XMMATRIX cascadeProj = XMMatrixPerspectiveFovLH(fov, aspectRatio, frustumIntervals[i], frustumIntervals[i + 1]);

        // 2. その層の視錐台の8頂点をワールド座標で取得
        auto corners = GetFrustumCornersWorldSpace(cameraView, cascadeProj);

        // 3. ライト視点での頂点の最小値・最大値（AABB）を求める
        float minX = 100000.0f, maxX = -100000.0f;
        float minY = 100000.0f, maxY = -100000.0f;
        float minZ = 100000.0f, maxZ = -100000.0f;

        for (const auto& corner : corners) {
            XMVECTOR v = XMLoadFloat3(&corner);
            // 頂点をライト空間に変換
            XMVECTOR lightSpaceV = XMVector3TransformCoord(v, lightView);

            minX = min(minX, XMVectorGetX(lightSpaceV));
            maxX = max(maxX, XMVectorGetX(lightSpaceV));
            minY = min(minY, XMVectorGetY(lightSpaceV));
            maxY = max(maxY, XMVectorGetY(lightSpaceV));
            minZ = min(minZ, XMVectorGetZ(lightSpaceV));
            maxZ = max(maxZ, XMVectorGetZ(lightSpaceV));
        }

        // シャドウが欠けないようにZの範囲に余裕を持たせる（Z-Pullback現象対策）
        minZ -= 1000.0f;
        maxZ += 1000.0f;

        // =======================================================
        //  影の揺れ(シマリング)と回転時の粗さを防ぐテクセルスナップ
        // =======================================================
        // 1. AABBを「常に正方形」に固定し、カメラ回転による影の伸び縮みを防ぐ
        float frustumWidth = maxX - minX;
        float frustumHeight = maxY - minY;
        float maxLength = (std::max)(frustumWidth, frustumHeight);
        float halfLength = maxLength * 0.5f;

        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;

        // 2. シャドウマップの解像度（ShadowMapクラスの生成時と同じ値）
        const float SHADOW_MAP_SIZE = 2048.0f;

        // 3. 1テクセルあたりのワールド空間の大きさ（ミリメートル単位）
        float worldUnitsPerTexel = maxLength / SHADOW_MAP_SIZE;

        // 4. 中心座標をテクセルのサイズ単位で丸める（スナップ）
        // ※これにより、カメラが動いても光のカメラはテクセル単位でしか動かなくなり、影が地面に吸着する
        centerX = std::floor(centerX / worldUnitsPerTexel) * worldUnitsPerTexel;
        centerY = std::floor(centerY / worldUnitsPerTexel) * worldUnitsPerTexel;

        // 5. スナップした中心からAABBを再構築
        minX = centerX - halfLength;
        maxX = centerX + halfLength;
        minY = centerY - halfLength;
        maxY = centerY + halfLength;

        // 4. ライト用の正投影（Orthographic）行列を作成
        XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);


        // 5. 結果を保存
        outLightVP[i] = lightView * lightProj;
    }
}