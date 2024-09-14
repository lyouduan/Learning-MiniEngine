#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

struct MyBoundingFrustum
{
	MyBoundingFrustum(){}

	inline bool Intersection(DirectX::BoundingBox& bounds, DirectX::XMMATRIX ViewToWorld);
	inline void Transform(MyBoundingFrustum& out, DirectX::XMMATRIX ViewToWorld);

	XMVECTOR planeNormal[4];

	DirectX::XMFLOAT3 Origin;
	DirectX::XMFLOAT4 Orientation;

	float RightSlope;
	float LeftSlope;
	float TopSlope;
	float BottomSlope;
	float Near;
	float Far;

};
class FrustumCulling
{
public:

	inline void CreateFromMatrix(MyBoundingFrustum& out, DirectX::XMMATRIX Proj);
	inline void Transform(MyBoundingFrustum& out, DirectX::XMMATRIX ViewToWorld);

private:

	static const size_t CORNER_COUNT = 8;

};
bool MyBoundingFrustum::Intersection(DirectX::BoundingBox& bounds, DirectX::XMMATRIX ViewToWorld)
{
	// 计算boundingbox的PQ向量

	XMVECTOR vCenter = XMLoadFloat3(&bounds.Center);
	XMVECTOR vExtents = XMLoadFloat3(&bounds.Extents);

	auto distance = XMVectorGetX(XMVector4Length(vExtents));

	BoundingFrustum f;
	f.Origin = Origin;
	f.Orientation = Orientation;
	f.Near = Near;
	f.Far = Far;
	f.RightSlope = RightSlope;
	f.LeftSlope = LeftSlope;
	f.TopSlope = TopSlope;
	f.BottomSlope = BottomSlope;
	f.Transform(f, ViewToWorld);

	return f.Contains(bounds) != DirectX::DISJOINT;
}

void MyBoundingFrustum::Transform(MyBoundingFrustum& out, DirectX::XMMATRIX ViewToWorld)
{
	// Load the frustum.
	XMVECTOR vOrigin = XMLoadFloat3(&Origin);
	XMVECTOR vOrientation = XMLoadFloat4(&Orientation);

	assert(DirectX::Internal::XMQuaternionIsUnit(vOrientation));

	// Composite the frustum rotation and the transform rotation
	XMMATRIX nM;
	nM.r[0] = XMVector3Normalize(ViewToWorld.r[0]);
	nM.r[1] = XMVector3Normalize(ViewToWorld.r[1]);
	nM.r[2] = XMVector3Normalize(ViewToWorld.r[2]);
	nM.r[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMVECTOR Rotation = XMQuaternionRotationMatrix(nM);
	vOrientation = XMQuaternionMultiply(vOrientation, Rotation);

	// Transform the center.
	vOrigin = XMVector3Transform(vOrigin, ViewToWorld);

	// Store the frustum.
	XMStoreFloat3(&out.Origin, vOrigin);
	XMStoreFloat4(&out.Orientation, vOrientation);

	// Scale the near and far distances (the slopes remain the same).
	XMVECTOR dX = XMVector3Dot(ViewToWorld.r[0], ViewToWorld.r[0]);
	XMVECTOR dY = XMVector3Dot(ViewToWorld.r[1], ViewToWorld.r[1]);
	XMVECTOR dZ = XMVector3Dot(ViewToWorld.r[2], ViewToWorld.r[2]);

	XMVECTOR d = XMVectorMax(dX, XMVectorMax(dY, dZ));
	float Scale = sqrtf(XMVectorGetX(d));

	out.Near *= Scale;
	out.Far  *= Scale;

	// keep the slopes.

	// Copy the slopes.
	out.RightSlope = RightSlope;
	out.LeftSlope = LeftSlope;
	out.TopSlope = TopSlope;
	out.BottomSlope = BottomSlope;
}
void FrustumCulling::CreateFromMatrix(MyBoundingFrustum& out, DirectX::XMMATRIX Proj)
{

	// DNC space 转变到View space
	// 1. 齐次除法的逆变换
	// 2. 投影矩阵的逆变换
	static DirectX::XMVECTOR HomogenousPoints[6] =
	{
		// 计算斜率
		{1.0, 0.0, 1.0, 1.0}, // 右平面，远平面右边中点
		{-1.0, 0.0, 1.0, 1.0}, // 左平面，远平面左边中点
		{0.0, 1.0, 1.0, 1.0}, // 顶平面，远平面上边中点
		{0.0, -1.0, 1.0, 1.0}, // 底平面，远平面下边中点

		// 计算距离
		{0.0, 0.0, 0.0, 1.0}, // 计算近平面到原点的距离，近平面中心点
		{0.0, 0.0, 1.0, 1.0} // 计算远平面到原点的距离，远平面中心点
	};

	// 投影矩阵的逆变换
	XMVECTOR Determinant;
	XMMATRIX invProj = XMMatrixInverse(&Determinant, Proj);
	XMVECTOR Points[6];
	for (size_t i = 0; i < 6; ++i)
	{
		// trasform the points to view space
		Points[i] = XMVector4Transform(HomogenousPoints[i], invProj);
	}

	// 齐次除法的逆变换: w=z
	Points[0] = Points[0] * XMVectorReciprocal(XMVectorSplatZ(Points[0]));
	Points[1] = Points[1] * XMVectorReciprocal(XMVectorSplatZ(Points[1]));
	Points[2] = Points[2] * XMVectorReciprocal(XMVectorSplatZ(Points[2]));
	Points[3] = Points[3] * XMVectorReciprocal(XMVectorSplatZ(Points[3]));

	// the origin of view space
	out.Origin = XMFLOAT3(0.0, 0.0, 0.0);
	out.Orientation = XMFLOAT4(0.0, 0.0, 0.0, 1.0);

	out.RightSlope = XMVectorGetX(Points[0]);
	out.LeftSlope  = XMVectorGetX(Points[1]);
	out.TopSlope   = XMVectorGetY(Points[2]);
	out.BottomSlope = XMVectorGetY(Points[3]);

	// 远近平面到z轴到原点的距离，令齐次坐标w=1
	Points[4] = Points[4] * XMVectorReciprocal(XMVectorSplatW(Points[4]));
	Points[5] = Points[5] * XMVectorReciprocal(XMVectorSplatW(Points[5]));

	out.Near = XMVectorGetZ(Points[4]);
	out.Far = XMVectorGetZ(Points[5]);
}

