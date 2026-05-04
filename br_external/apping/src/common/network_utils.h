#pragma once

#include <QString>

namespace apping {

QString firstUsableIpv4Address(bool allowLoopbackFallback = true);

} // namespace apping
