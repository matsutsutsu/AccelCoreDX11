#include "Camera.h"

// コンストラクタ
Camera::Camera()
{
	// カメラ設定
	SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		1280.0f / 720.0f,					// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	SetLookAt(
		{ 0, 0, -5 },		// 視点
		{ 0, 0, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
}

// 指定方向を向く
void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
	// 視点、注視点、上方向からビュー行列を作成
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&eye);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&focus);
	DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&up);
	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Eye, Focus, Up);
	DirectX::XMStoreFloat4x4(&view, View);

	// ビューを逆行列化し、ワールド行列に戻す
	DirectX::XMMATRIX World = DirectX::XMMatrixInverse(nullptr, View);
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, World);

	// カメラの方向を取り出す
	this->right.x = world._11;
	this->right.y = world._12;
	this->right.z = world._13;

	this->up.x = world._21;
	this->up.y = world._22;
	this->up.z = world._23;

	this->front.x = world._31;
	this->front.y = world._32;
	this->front.z = world._33;

	// 視点、注視点を保存
	this->eye = eye;
	this->focus = focus;

	// 行列が変わったのでFrustumを更新
	UpdateFrustum();
}

// パースペクティブ設定
void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
    // 画角、画面比率、クリップ距離からプロジェクション行列を作成
    DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    DirectX::XMStoreFloat4x4(&projection, Projection);

    // 計算に使った値をメンバ変数として保存する処理を追加！
    // これがないと GetAspect() や GetNear() が正しい値を返せません
    this->fovY   = fovY;
    this->aspect = aspect;
    this->nearZ  = nearZ;
    this->farZ   = farZ;

	// 行列が変わったのでFrustumを更新
	UpdateFrustum();
}


//正射影設定（追加）
void Camera::SetOrthographic(float width, float height, float nearZ, float farZ)
{
	// 幅・高さ・クリップ距離から正射影行列を作成
	DirectX::XMMATRIX Ortho = DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ);
	DirectX::XMStoreFloat4x4(&projection, Ortho);

	// パラメーターだけ保存 
	this->nearZ = nearZ;
	this->farZ = farZ;

	this->fovY = fovY;
	this->aspect = aspect;
}

// ====================================================================
// ローカルFrustumを作成し、最新のビュー逆行列でワールド空間へ配置する
// ====================================================================
void Camera::UpdateFrustum()
{
	// 1. プロジェクション行列（画角や遠近感）から、原点ベースの視界（ローカルFrustum）を作成
	DirectX::BoundingFrustum frustumLocal;
	DirectX::BoundingFrustum::CreateFromMatrix(frustumLocal, DirectX::XMLoadFloat4x4(&projection));

	// 2. ビュー行列の「逆行列」を計算（カメラのワールドTransformを表す行列）
	DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view));

	// 3. ローカルFrustumをカメラの位置・角度に合わせてワールド空間に変換
	frustumLocal.Transform(frustum, invView);
}