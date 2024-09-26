#include "Camera.h"
#include <cmath>

void Math::BaseCamera::Update()
{
	m_PreviousViewProjMatrix = m_ViewProjMatrix;

	m_ViewMatrix = Matrix4(~m_CameraToWorld);

	m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;

	m_ReprojectMatrix = m_PreviousViewProjMatrix * Invert(GetViewProjMatrix());

	m_FrustumVS = Frustum(m_ProjMatrix);

	m_FrustumWS = m_CameraToWorld * m_FrustumVS;
}

void Math::BaseCamera::SetEyeAtUp(Vector3 eye, Vector3 at, Vector3 up)
{
	SetLookDirection(at - eye, up);
	SetPosition(eye);
}

void Math::BaseCamera::SetLookDirection(Vector3 forward, Vector3 up)
{
	// right hand coordinate for bulid view matrix
	Scalar forwardLenSq = LengthSquare(forward);
	
	forward = Select(forward * RecipSqrt(forwardLenSq), -Vector3(kZUnitVector), forwardLenSq < Scalar(0.000001f));

	Vector3 right = Cross(forward, up);
	Scalar rightLenSq = LengthSquare(right);
	right = Select(right * RecipSqrt(rightLenSq), Quaternion(Vector3(kYUnitVector), -XM_PIDIV2) * forward, rightLenSq < Scalar(0.000001f));

	// Compute actual up vector
	up = Cross(right, forward);

	// Finish constructing basis
	m_Basis = Matrix3(right, up, forward); // left hand coordinate
	m_CameraToWorld.SetRotation(Quaternion(m_Basis));
}

void Math::BaseCamera::SetRotaion(Quaternion basisRotation)
{
	m_CameraToWorld.SetRotation(Normalize(basisRotation));
	m_Basis = Matrix3(m_CameraToWorld.GetRotation());
}

void Math::BaseCamera::SetPosition(Vector3 worldPos)
{
	m_CameraToWorld.SetTranslation(worldPos);
}

void Math::BaseCamera::SetTransform(const AffineTransform& xform)
{
	// By using these functions, we rederive an orthogonal transform.
	SetLookDirection(-xform.GetZ(), xform.GetY());
	SetPosition(xform.GetTranslation());
}

void Math::BaseCamera::SetTransform(const OrthogonalTransform& xform)
{
}

Math::Camera::Camera()
	: m_ReverseZ(false), m_InfiniteZ(false)
{
	SetPerspectiveMatrix(XM_PIDIV4, 9.0f / 16.0f, 1.0f, 1000.0f);
}

void Math::Camera::SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip)
{
	m_VerticalFOV = verticalFovRadians;
	m_AspectRatio = aspectHeightOverWidth;
	m_NearClip = nearZClip;
	m_FarClip = farZClip;

	UpdateProjMatrix();

	m_PreviousViewProjMatrix = m_ViewProjMatrix;
}

void Math::Camera::UpdateProjMatrix(void)
{
	// calculate X,Y by fov and z=1.0 as default
	float Y = 1.0f / std::tanf(m_VerticalFOV * 0.5f);
	float X = Y * m_AspectRatio;

	float Q1, Q2;
	// ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
	// actually a great idea with F32 depth buffers to redistribute precision more evenly across
	// the entire range.  It requires clearing Z to 0.0f and using a GREATER variant depth test.
	// Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
	if (m_ReverseZ)
	{
		if (m_InfiniteZ)
		{
			Q1 = 0.0f;
			Q2 = m_NearClip;
		}
		else
		{
			Q1 = m_NearClip / (m_FarClip - m_NearClip);
			Q2 = Q1 * m_FarClip;
		}
	}
	else
	{
		if (m_InfiniteZ) 
		{
			Q1 = -1.0; // rihgt hand z is negative, camera look at -z in OpenGL
			Q2 = -m_NearClip;
		}
		else
		{
			Q1 = m_FarClip / (m_NearClip - m_FarClip);
			Q2 = Q1 * m_NearClip;
		}
	}

	// major-column matrix
	SetProjMatrix(Matrix4(
		Vector4(X,    0.0f, 0.0f, 0.0f),
		Vector4(0.0f, Y,    0.0f, 0.0f),
		Vector4(0.0f, 0.0f, Q1,  -1.0f), // reverse Z axis for left hand
		Vector4(0.0f, 0.0f, Q2,   0.0f)
	));

}
