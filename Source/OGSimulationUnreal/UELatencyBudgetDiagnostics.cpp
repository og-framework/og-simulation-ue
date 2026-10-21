// SPDX-License-Identifier: MPL-2.0
// docs/UELatencyBudgetDiagnostics-rationale.md · docs/UELatencyBudgetDiagnostics-guards.md

#include "UELatencyBudgetDiagnostics.h"

#if !UE_BUILD_SHIPPING
DEFINE_LOG_CATEGORY(LogOGLatencyBudget);
#endif
