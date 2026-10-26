#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/UEFrameConversion-rationale.md · docs/UEFrameConversion-guards.md

#include "CoreMinimal.h"
#include "OGSimulation/SimulationFrame.h"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"
// ⛔G-05  docs/UEFrameConversion-guards.md

namespace ueFrameDetail
{
	struct TransformParts
	{
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
	};

	OGSIMULATIONUNREAL_API glm::vec3 narrow(double x, double y, double z);
	OGSIMULATIONUNREAL_API glm::quat narrow(double w, double x, double y, double z);
	OGSIMULATIONUNREAL_API glm::mat4 composeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);
	OGSIMULATIONUNREAL_API TransformParts decomposeTransform(const glm::mat4& transform);
} // namespace ueFrameDetail

namespace ueNative
{
	inline glm::vec3 toGlm(const FVector& v)
	{
		return ueFrameDetail::narrow(v.X, v.Y, v.Z);
	}

	inline FVector toFVector(const glm::vec3& v)
	{
		return FVector(v.x, v.y, v.z);
	}

	inline glm::quat toGlmQuat(const FQuat& q)
	{
		return ueFrameDetail::narrow(q.W, q.X, q.Y, q.Z);
	}

	inline FQuat toFQuat(const glm::quat& q)
	{
		return FQuat(q.x, q.y, q.z, q.w);
	}

	inline glm::mat4 toGLMMat4(const FTransform& transform)
	{
		return ueFrameDetail::composeTransform(toGlm(transform.GetTranslation()), toGlmQuat(transform.GetRotation()),
			toGlm(transform.GetScale3D()));
	}

	inline FTransform toFTransform(const glm::mat4& transform)
	{
		const ueFrameDetail::TransformParts parts = ueFrameDetail::decomposeTransform(transform);
		return FTransform(toFQuat(parts.rotation), toFVector(parts.translation), toFVector(parts.scale));
	}
} // namespace ueNative

template <class Frame>
struct BasicUeFrame
{
	static constexpr bool kReflects = glm::dot(glm::cross(Frame::kForward, Frame::kRight), Frame::kUp) < 0.f;
	static constexpr double kUeCentimetresPerUnit = static_cast<double>(Frame::kUeCentimetresPerUnit);

	static constexpr bool reversesWinding(bool mirroredScale)
	{
		return mirroredScale != kReflects;
	}

	static float lengthToSim(double centimetres)
	{
		return static_cast<float>(toUnits(centimetres, kUeCentimetresPerUnit));
	}

	static double lengthToUe(float length)
	{
		return toCentimetres(static_cast<double>(length), kUeCentimetresPerUnit);
	}

	static glm::vec3 vectorToSim(const FVector& v)
	{
		return narrowed(toSim<false>(toUnits(v.X, kUeCentimetresPerUnit), toUnits(v.Y, kUeCentimetresPerUnit),
			toUnits(v.Z, kUeCentimetresPerUnit)));
	}

	static glm::dvec3 vectorToSimD(const FVector& v)
	{
		return toSim<false>(toUnits(v.X, kUeCentimetresPerUnit), toUnits(v.Y, kUeCentimetresPerUnit),
			toUnits(v.Z, kUeCentimetresPerUnit));
	}

	static FVector vectorToUe(const glm::vec3& p)
	{
		const glm::dvec3 ue = toUe<false>(p);
		return FVector(toCentimetres(ue.x, kUeCentimetresPerUnit), toCentimetres(ue.y, kUeCentimetresPerUnit),
			toCentimetres(ue.z, kUeCentimetresPerUnit));
	}

	static glm::vec3 directionToSim(const FVector& n)
	{
		return narrowed(toSim<false>(n.X, n.Y, n.Z));
	}

	static FVector directionToUe(const glm::vec3& n)
	{
		const glm::dvec3 ue = toUe<false>(n);
		return FVector(ue.x, ue.y, ue.z);
	}

	static glm::vec3 axialToSim(const FVector& w)
	{
		return narrowed(toSim<true>(w.X, w.Y, w.Z));
	}

	static FVector axialToUe(const glm::vec3& w)
	{
		const glm::dvec3 ue = toUe<true>(w);
		return FVector(ue.x, ue.y, ue.z);
	}

	static glm::vec3 torqueToSim(const FVector& t)
	{
		return narrowed(toSim<true>(toUnits(t.X, kUeSquareCentimetresPerSquareUnit), toUnits(t.Y, kUeSquareCentimetresPerSquareUnit),
			toUnits(t.Z, kUeSquareCentimetresPerSquareUnit)));
	}

	static FVector torqueToUe(const glm::vec3& t)
	{
		const glm::dvec3 ue = toUe<true>(t);
		return FVector(toCentimetres(ue.x, kUeSquareCentimetresPerSquareUnit), toCentimetres(ue.y, kUeSquareCentimetresPerSquareUnit),
			toCentimetres(ue.z, kUeSquareCentimetresPerSquareUnit));
	}

	static glm::vec3 inertiaToSim(const FVector& diagonal)
	{
		return narrowed(toSimUnsigned(toUnits(diagonal.X, kUeSquareCentimetresPerSquareUnit),
			toUnits(diagonal.Y, kUeSquareCentimetresPerSquareUnit), toUnits(diagonal.Z, kUeSquareCentimetresPerSquareUnit)));
	}

	static FVector inertiaToUe(const glm::vec3& diagonal)
	{
		return FVector(toCentimetres(diagonal[kForwardIndex], kUeSquareCentimetresPerSquareUnit),
			toCentimetres(diagonal[kRightIndex], kUeSquareCentimetresPerSquareUnit),
			toCentimetres(diagonal[kUpIndex], kUeSquareCentimetresPerSquareUnit));
	}

	static glm::vec3 halfExtentsToSim(const FVector& halfExtents)
	{
		return narrowed(toSimUnsigned(toUnits(halfExtents.X, kUeCentimetresPerUnit), toUnits(halfExtents.Y, kUeCentimetresPerUnit),
			toUnits(halfExtents.Z, kUeCentimetresPerUnit)));
	}

	static FVector halfExtentsToUe(const glm::vec3& halfExtents)
	{
		return FVector(lengthToUe(halfExtents[kForwardIndex]), lengthToUe(halfExtents[kRightIndex]), lengthToUe(halfExtents[kUpIndex]));
	}

	static glm::vec3 scaleToSim(const FVector& scale)
	{
		return narrowed(toSimUnsigned(scale.X, scale.Y, scale.Z));
	}

	static FVector scaleToUe(const glm::vec3& scale)
	{
		return FVector(scale[kForwardIndex], scale[kRightIndex], scale[kUpIndex]);
	}

	static glm::quat rotationToSim(const FQuat& q)
	{
		const glm::dvec3 axial = toSim<true>(q.X, q.Y, q.Z);
		return ueFrameDetail::narrow(q.W, axial.x, axial.y, axial.z);
	}

	static FQuat rotationToUe(const glm::quat& q)
	{
		const glm::dvec3 ue = toUe<true>(glm::vec3(q.x, q.y, q.z));
		return FQuat(ue.x, ue.y, ue.z, q.w);
	}

	static glm::mat4 transformToSim(const FTransform& transform)
	{
		checkf(transform.GetScale3D().GetAbs().Equals(FVector::OneVector, UE_KINDA_SMALL_NUMBER),
			TEXT("ueFrame::transformToSim: a rigid transform crosses with unit scale; got %s. Bake the scale into the shape, ")
			TEXT("or convert a UE-native matrix with ueNative::toGLMMat4."),
			*transform.GetScale3D().ToString());
		return ueFrameDetail::composeTransform(vectorToSim(transform.GetTranslation()), rotationToSim(transform.GetRotation()),
			scaleToSim(transform.GetScale3D()));
	}

	static FTransform transformToUe(const glm::mat4& transform)
	{
		const ueFrameDetail::TransformParts parts = ueFrameDetail::decomposeTransform(transform);
		return FTransform(rotationToUe(parts.rotation), vectorToUe(parts.translation), scaleToUe(parts.scale));
	}

private:
	static constexpr double kUeSquareCentimetresPerSquareUnit = kUeCentimetresPerUnit * kUeCentimetresPerUnit;
	static constexpr glm::length_t kForwardIndex = simFrameDetail::axisIndex(Frame::kForward);
	static constexpr glm::length_t kRightIndex = simFrameDetail::axisIndex(Frame::kRight);
	static constexpr glm::length_t kUpIndex = simFrameDetail::axisIndex(Frame::kUp);
	static constexpr bool kForwardNegative = simFrameDetail::axisIsNegative(Frame::kForward);
	static constexpr bool kRightNegative = simFrameDetail::axisIsNegative(Frame::kRight);
	static constexpr bool kUpNegative = simFrameDetail::axisIsNegative(Frame::kUp);

	static_assert(kUeCentimetresPerUnit == 1.0 || kUeCentimetresPerUnit == 100.0,
		"UEFrameConversion: the og unit is the centimetre or the metre");
	static_assert(kForwardIndex != kRightIndex && kForwardIndex != kUpIndex && kRightIndex != kUpIndex,
		"UEFrameConversion: UE's forward, right and up land on three different og components");

	static double toUnits(double centimetres, double centimetresPerUnit)
	{
		if constexpr (kUeCentimetresPerUnit == 1.0)
		{
			return centimetres;
		}
		else
		{
			return centimetres / centimetresPerUnit;
		}
	}

	static double toCentimetres(double units, double centimetresPerUnit)
	{
		if constexpr (kUeCentimetresPerUnit == 1.0)
		{
			return units;
		}
		else
		{
			return units * centimetresPerUnit;
		}
	}

	static glm::vec3 narrowed(const glm::dvec3& v)
	{
		return ueFrameDetail::narrow(v.x, v.y, v.z);
	}

	template <bool Negate>
	static constexpr double signedValue(double value)
	{
		if constexpr (Negate)
		{
			return -value;
		}
		else
		{
			return value;
		}
	}

	template <bool Axial>
	static constexpr glm::dvec3 toSim(double forward, double right, double up)
	{
		glm::dvec3 out(0.0);
		out[kForwardIndex] = signedValue<kForwardNegative != (Axial && kReflects)>(forward);
		out[kRightIndex] = signedValue<kRightNegative != (Axial && kReflects)>(right);
		out[kUpIndex] = signedValue<kUpNegative != (Axial && kReflects)>(up);
		return out;
	}

	template <bool Axial>
	static constexpr glm::dvec3 toUe(const glm::vec3& v)
	{
		return glm::dvec3(signedValue<kForwardNegative != (Axial && kReflects)>(static_cast<double>(v[kForwardIndex])),
			signedValue<kRightNegative != (Axial && kReflects)>(static_cast<double>(v[kRightIndex])),
			signedValue<kUpNegative != (Axial && kReflects)>(static_cast<double>(v[kUpIndex])));
	}

	static constexpr glm::dvec3 toSimUnsigned(double forward, double right, double up)
	{
		glm::dvec3 out(0.0);
		out[kForwardIndex] = forward;
		out[kRightIndex] = right;
		out[kUpIndex] = up;
		return out;
	}
};

using ueFrame = BasicUeFrame<simFrame>;
