// SPDX-License-Identifier: MPL-2.0
// docs/UEFrameConversion-rationale.md · docs/UEFrameConversion-guards.md

#include "UEFrameConversion.h"

#include "OGSimulation/CompilerControl.h"
#include "glm/ext/matrix_transform.hpp"

OGSIM_OPTIMIZE_OFF

namespace ueFrameDetail
{
	// ⛔G-03  docs/UEFrameConversion-guards.md
	FORCENOINLINE glm::vec3 narrow(double x, double y, double z)
	{
		glm::vec3 out;
		out.x = static_cast<float>(x);
		out.y = static_cast<float>(y);
		out.z = static_cast<float>(z);
		return out;
	}

	// ⛔G-04  docs/UEFrameConversion-guards.md
	FORCENOINLINE glm::quat narrow(double w, double x, double y, double z)
	{
		glm::quat out;
		out.x = static_cast<float>(x);
		out.y = static_cast<float>(y);
		out.z = static_cast<float>(z);
		out.w = static_cast<float>(w);
		return out;
	}

	// ⛔G-01  docs/UEFrameConversion-guards.md
	FORCENOINLINE glm::mat4 composeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation);
		transform *= glm::mat4_cast(rotation);
		transform = glm::scale(transform, scale);
		return transform;
	}

	// ⛔G-02  docs/UEFrameConversion-guards.md
	FORCENOINLINE TransformParts decomposeTransform(const glm::mat4& transform)
	{
		TransformParts parts;
		parts.translation = glm::vec3(transform[3]);
		parts.rotation = glm::quat_cast(transform);
		parts.scale = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
		return parts;
	}
} // namespace ueFrameDetail

OGSIM_OPTIMIZE_ON
