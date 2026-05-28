#ifndef CONVERTMESH_CORE_PROGRESS_H
#define CONVERTMESH_CORE_PROGRESS_H

#include "core/Error.h"

#include <atomic>
#include <functional>
#include <string_view>

namespace cmesh
{

/**
 * Progress callback. `fraction` is in [0, 1]. `stage` is an optional
 * human-readable label for the current sub-step ("smoothing", "decimating",
 * etc.) that callers can surface in a status line.
 */
using ProgressFn = std::function<void(double fraction, std::string_view stage)>;

/**
 * Cancellation token. Holds a non-owning pointer to an atomic flag that
 * external callers (UIs, supervisors) can set asynchronously. Core
 * functions check the token at safe points and raise AbortError when it
 * fires. A default-constructed token never aborts.
 */
class AbortToken
{
public:
  AbortToken() = default;
  explicit AbortToken(const std::atomic<bool> *flag) : m_Flag(flag) {}

  bool IsRequested() const noexcept
  {
    return m_Flag && m_Flag->load(std::memory_order_acquire);
  }

  void ThrowIfRequested() const
  {
    if(IsRequested()) throw AbortError();
  }

private:
  const std::atomic<bool> *m_Flag = nullptr;
};

} // namespace cmesh

#endif
