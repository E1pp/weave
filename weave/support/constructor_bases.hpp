#pragma once

namespace weave::support {

struct NonCopyableBase {
  NonCopyableBase() = default;

  NonCopyableBase(const NonCopyableBase&) = delete;
  NonCopyableBase& operator=(const NonCopyableBase&) = delete;

  NonCopyableBase(NonCopyableBase&&) = default;
  NonCopyableBase& operator=(NonCopyableBase&&) = default;
};

struct NonMovableBase{
  NonMovableBase() = default;

  NonMovableBase(const NonMovableBase&) = default;
  NonMovableBase& operator=(const NonMovableBase&) = default;

  NonMovableBase(NonMovableBase&&) = delete;
  NonMovableBase& operator=(NonMovableBase&&) = delete;
};

struct PinnedBase {
  PinnedBase() = default;

  PinnedBase(const PinnedBase&) = delete;
  PinnedBase& operator=(const PinnedBase&) = delete;

  PinnedBase(PinnedBase&&) = delete;
  PinnedBase& operator=(PinnedBase&&) = delete;
};



} // namespace weave::support