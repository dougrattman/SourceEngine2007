// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_AVI_INCLUDE_IBINK_MEDIA_PLAYER_H_
#define SOURCE_AVI_INCLUDE_IBINK_MEDIA_PLAYER_H_

#ifdef _WIN32
#pragma once
#endif

#include <memory>
#include <tuple>
#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "base/include/macros.h"
#include "tier1/interface.h"

namespace source {
// Bink Media Player.
src_interface IBinkMediaPlayer {
  virtual ~IBinkMediaPlayer() = 0;

  // Has some frames to continue playing?
  virtual bool HasFrames() const noexcept = 0;

  // Can present frame on screen?
  virtual bool CanPresent() const noexcept = 0;
  // Do present frame.
  virtual void Present(bool do_present_old_frame) const noexcept = 0;

  // Continue to play.
  virtual bool Play() const noexcept = 0;
  // Pause playing.
  virtual bool Pause() const noexcept = 0;

  // Autoscale frames to dimensions.
  virtual bool AutoscaleTo(u32 width, u32 height) const noexcept = 0;
  // Correct window pos during window move.
  virtual void AdjustWindowPos(i32 & x, i32 & y) const noexcept = 0;
  // Set frames window offset.
  virtual bool SetWindowOffset(i32 x, i32 y) const noexcept = 0;
};

SOURCE_FORWARD_DECLARE_HANDLE(HWND);

constexpr inline ch kValveAviBinkMediaFactoryInterfaceVersion[]{
    "VBinkMediaFactory001"};

// Bink Media Player factory.
src_interface IBinkMediaFactory : public IAppSystem {
  virtual std::tuple<std::unique_ptr<IBinkMediaPlayer>, const ch *> Open(
      const ch *media_path, u32 bink_flags, HWND window, u32 buffer_flags) = 0;
};
}  // namespace source

#endif  // SOURCE_AVI_INCLUDE_IBINK_MEDIA_PLAYER_H_
